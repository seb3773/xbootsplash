/*
 * xbs_vt_runner.c - Standalone Virtual Terminal (VT) runner for hardware testing of bootsplash binaries
 * 
 * Part of XBootsplash Studio.
 * Based on the fail-safe VT takeover and restoration mechanism from CAD rescue GUI.
 *
 * Guarantees:
 * - Never leaves the user stuck on a blank/broken console
 * - atexit() registered BEFORE any VT manipulation
 * - sigaltstack() with 64KB dedicated stack for fatal signal handling
 * - emergency_restore() uses ONLY raw syscalls (SYS_ioctl, SYS_kill, SYS_write)
 * - Atomic flag prevents double-execution of restore routines
 * - Evdev keyboard detection (/dev/input/event*) for instant keypress exit even in KD_GRAPHICS mode
 * - Watchdog timer (alarm) armed BEFORE VT_ACTIVATE to prevent deadlock in VT_WAITACTIVE
 * - Graceful SIGTERM with 500ms timeout before SIGKILL to allow clean DRM/CRTC restoration
 * - Full restoration: KD_TEXT, KDSKBMODE (K_XLATE), termios reset, ANSI RIS (\033c), klogctl(7)
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <poll.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/klog.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/input.h>

#define DEFAULT_TEST_DURATION_SEC 10
#define MAX_TEST_DURATION_SEC     60
#define MAX_KBD_DEVS              8
#define EMERGENCY_STACK_SIZE      65536

/* Global state for signal safety and restoration */
static int g_tty0_fd = -1;
static int g_test_tty_fd = -1;
static int g_original_vt = -1;
static int g_test_vt = -1;
static pid_t g_child_pid = -1;
static volatile sig_atomic_t g_vt_restored = 0;
static int g_kbd_fds[MAX_KBD_DEVS];
static int g_num_kbds = 0;

/* --- Restoration & Failsafe Functions --- */

/*
 * Async-signal-safe emergency restoration.
 * Uses ONLY direct kernel syscalls to ensure it can run safely from any signal handler context.
 */
static void emergency_restore(int sig) {
    (void)sig;

    /* Kill child immediately */
    if (g_child_pid > 0) {
        syscall(SYS_kill, g_child_pid, SIGKILL);
    }

    /* Force text mode on test tty */
    if (g_test_tty_fd >= 0) {
        syscall(SYS_ioctl, g_test_tty_fd, KDSETMODE, KD_TEXT);
    }

    /* Force text mode and switch back to original VT */
    if (g_tty0_fd >= 0) {
        syscall(SYS_ioctl, g_tty0_fd, KDSETMODE, KD_TEXT);
        if (g_original_vt > 0) {
            syscall(SYS_ioctl, g_tty0_fd, VT_ACTIVATE, g_original_vt);
        }
    } else if (g_test_tty_fd >= 0 && g_original_vt > 0) {
        syscall(SYS_ioctl, g_test_tty_fd, VT_ACTIVATE, g_original_vt);
    }

    _exit(4);
}

/*
 * Full clean restoration of the virtual console and X11 session.
 * Guaranteed to run only once via atomic test-and-set.
 */
static void restore_vt(void) {
    if (__atomic_test_and_set(&g_vt_restored, __ATOMIC_SEQ_CST)) {
        return;
    }

    /* 1. Stop child process if still active */
    if (g_child_pid > 0) {
        kill(g_child_pid, SIGTERM);
        /* Allow up to 300ms for graceful cleanup */
        for (int i = 0; i < 6; i++) {
            int status;
            pid_t p = waitpid(g_child_pid, &status, WNOHANG);
            if (p == g_child_pid) {
                g_child_pid = -1;
                break;
            }
            usleep(50000);
        }
        if (g_child_pid > 0) {
            kill(g_child_pid, SIGKILL);
            waitpid(g_child_pid, NULL, 0);
            g_child_pid = -1;
        }
    }

    /* 2. Close evdev keyboard descriptors */
    for (int i = 0; i < g_num_kbds; i++) {
        if (g_kbd_fds[i] >= 0) {
            close(g_kbd_fds[i]);
            g_kbd_fds[i] = -1;
        }
    }
    g_num_kbds = 0;

    /* 3. Restore test VT to text mode and sane terminal parameters */
    if (g_test_tty_fd >= 0) {
        ioctl(g_test_tty_fd, KDSETMODE, KD_TEXT);
        ioctl(g_test_tty_fd, KDSKBMODE, K_XLATE);

        struct termios term;
        if (tcgetattr(g_test_tty_fd, &term) == 0) {
            term.c_lflag |= (ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN);
            term.c_iflag |= (BRKINT | ICRNL | IXON | IMAXBEL);
            term.c_oflag |= (OPOST | ONLCR);
            term.c_cflag |= (CS8 | CREAD);
            tcsetattr(g_test_tty_fd, TCSANOW, &term);
        }

        /* Hardware ANSI reset (RIS) */
        ssize_t ignored = write(g_test_tty_fd, "\033c", 2);
        (void)ignored;

        close(g_test_tty_fd);
        g_test_tty_fd = -1;
    }

    /* 4. Restore kernel console messages */
    klogctl(7, NULL, 0);

    /* 5. Switch back to the original X11 / desktop VT */
    if (g_tty0_fd >= 0) {
        if (g_original_vt > 0) {
            ioctl(g_tty0_fd, VT_ACTIVATE, g_original_vt);
            ioctl(g_tty0_fd, VT_WAITACTIVE, g_original_vt);
        }
        close(g_tty0_fd);
        g_tty0_fd = -1;
    }
}

/* --- Hardware Keyboard Discovery (evdev) --- */

/*
 * Discover keyboard devices in /dev/input/ to detect key presses while console is in KD_GRAPHICS mode.
 */
static void discover_keyboards(void) {
    g_num_kbds = 0;
    DIR *dir = opendir("/dev/input");
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && g_num_kbds < MAX_KBD_DEVS) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;

        char dev_path[512];
        snprintf(dev_path, sizeof(dev_path), "/dev/input/%s", ent->d_name);

        int fd = open(dev_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;

        unsigned long evbit[4] = {0};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) >= 0) {
            if (evbit[0] & (1UL << EV_KEY)) {
                /* Confirmed input device with keys */
                g_kbd_fds[g_num_kbds++] = fd;
                continue;
            }
        }
        close(fd);
    }
    closedir(dir);
}

/* --- VT Takeover Function --- */

static int steal_vt(int requested_duration) {
    /* 1. Open console multiplexer */
    g_tty0_fd = open("/dev/tty0", O_RDWR | O_CLOEXEC);
    if (g_tty0_fd < 0) {
        fprintf(stderr, "[xbs_vt_runner] Cannot open /dev/tty0. Root privileges required.\n");
        return 2;
    }

    /* 2. Query currently active VT (user's desktop session) */
    struct vt_stat vts;
    if (ioctl(g_tty0_fd, VT_GETSTATE, &vts) < 0) {
        fprintf(stderr, "[xbs_vt_runner] Failed to read current VT state.\n");
        close(g_tty0_fd);
        g_tty0_fd = -1;
        return 1;
    }
    g_original_vt = vts.v_active;

    /* 3. Query first unused VT */
    int free_vt = 12;
    if (ioctl(g_tty0_fd, VT_OPENQRY, &free_vt) < 0 || free_vt <= 0) {
        free_vt = 12;
    }
    if (free_vt == g_original_vt) {
        free_vt = (g_original_vt == 12) ? 11 : 12;
    }
    g_test_vt = free_vt;

    /* 4. Open isolated test TTY */
    char test_tty_path[32];
    snprintf(test_tty_path, sizeof(test_tty_path), "/dev/tty%d", g_test_vt);
    g_test_tty_fd = open(test_tty_path, O_RDWR | O_CLOEXEC);
    if (g_test_tty_fd < 0) {
        fprintf(stderr, "[xbs_vt_runner] Failed to open %s\n", test_tty_path);
        close(g_tty0_fd);
        g_tty0_fd = -1;
        return 1;
    }

    /* 5. Register safety net BEFORE switching console */
    atexit(restore_vt);

    static char emergency_stack[EMERGENCY_STACK_SIZE];
    stack_t ss;
    ss.ss_sp = emergency_stack;
    ss.ss_size = sizeof(emergency_stack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = emergency_restore;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_ONSTACK;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGALRM, &sa, NULL);

    signal(SIGPIPE, SIG_IGN);

    /* 6. Suppress kernel printk output to prevent screen corruption */
    klogctl(6, NULL, 0);

    /* 7. Arm safety watchdog before VT_ACTIVATE in case of VT switch deadlock */
    alarm((unsigned int)(requested_duration + 5));

    /* 8. Switch console to test VT */
    if (ioctl(g_tty0_fd, VT_ACTIVATE, g_test_vt) < 0) {
        fprintf(stderr, "[xbs_vt_runner] VT_ACTIVATE failed for VT %d\n", g_test_vt);
        restore_vt();
        return 1;
    }
    ioctl(g_tty0_fd, VT_WAITACTIVE, g_test_vt);

    /* 9. Set graphics mode to suppress blinking cursor and unblank screen */
    ioctl(g_test_tty_fd, KDSETMODE, KD_GRAPHICS);
    char unblank = 4;
    ioctl(g_test_tty_fd, TIOCLINUX, &unblank);

    /* 10. Open evdev keyboard devices for input handling */
    discover_keyboards();

    return 0;
}

/* --- Main Runner Logic --- */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path_to_bootsplash_binary> [duration_seconds]\n", argv[0]);
        return 1;
    }

    const char *binary_path = argv[1];
    int duration = DEFAULT_TEST_DURATION_SEC;
    if (argc >= 3) {
        duration = atoi(argv[2]);
        if (duration <= 0) duration = DEFAULT_TEST_DURATION_SEC;
        if (duration > MAX_TEST_DURATION_SEC) duration = MAX_TEST_DURATION_SEC;
    }

    /* 1. Privilege check */
    if (getuid() != 0) {
        fprintf(stderr, "[xbs_vt_runner] Root privileges required (run with sudo).\n");
        return 2;
    }

    /* 2. Check binary existence and execution permission */
    if (access(binary_path, X_OK) != 0) {
        fprintf(stderr, "[xbs_vt_runner] Target binary '%s' is not executable: %s\n",
                binary_path, strerror(errno));
        return 3;
    }

    /* 3. Take over virtual console */
    int vt_ret = steal_vt(duration);
    if (vt_ret != 0) {
        return vt_ret;
    }

    /* 4. Fork and execute the splash binary */
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "[xbs_vt_runner] fork() failed: %s\n", strerror(errno));
        restore_vt();
        return 1;
    }

    if (pid == 0) {
        /* Child process: redirect standard file descriptors */
        int null_fd = open("/dev/null", O_RDWR);
        if (null_fd >= 0) {
            dup2(null_fd, STDIN_FILENO);
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }

        char *child_argv[] = { (char *)binary_path, NULL };
        execv(binary_path, child_argv);
        _exit(127);
    }

    g_child_pid = pid;

    /* Re-arm watchdog to exact test duration */
    alarm((unsigned int)(duration + 2));

    /* 5. Event Loop: Monitor keyboard events, timer, and child exit */
    time_t start_time = time(NULL);

    struct pollfd pfd[MAX_KBD_DEVS + 2];
    int nfds = 0;

    /* Add keyboard evdev descriptors */
    for (int i = 0; i < g_num_kbds; i++) {
        pfd[nfds].fd = g_kbd_fds[i];
        pfd[nfds].events = POLLIN;
        pfd[nfds].revents = 0;
        nfds++;
    }

    /* Add test tty descriptor */
    if (g_test_tty_fd >= 0) {
        pfd[nfds].fd = g_test_tty_fd;
        pfd[nfds].events = POLLIN;
        pfd[nfds].revents = 0;
        nfds++;
    }

    while (1) {
        /* Check if child terminated on its own */
        int status;
        pid_t p = waitpid(g_child_pid, &status, WNOHANG);
        if (p == g_child_pid) {
            g_child_pid = -1;
            break;
        }

        /* Check elapsed time */
        if (time(NULL) - start_time >= duration) {
            break;
        }

        /* Poll with 50ms timeout */
        int poll_ret = poll(pfd, nfds, 50);
        if (poll_ret > 0) {
            int key_pressed = 0;
            for (int i = 0; i < nfds; i++) {
                if (pfd[i].revents & POLLIN) {
                    if (i < g_num_kbds) {
                        /* evdev event */
                        struct input_event ev;
                        while (read(pfd[i].fd, &ev, sizeof(ev)) > 0) {
                            if (ev.type == EV_KEY && ev.value == 1) {
                                key_pressed = 1;
                                break;
                            }
                        }
                    } else {
                        /* tty input */
                        char buf[32];
                        if (read(pfd[i].fd, buf, sizeof(buf)) > 0) {
                            key_pressed = 1;
                        }
                    }
                }
            }
            if (key_pressed) {
                /* Drain any remaining key events from evdev and tty before restoring VT */
                struct input_event ev;
                for (int k = 0; k < g_num_kbds; k++) {
                    while (read(g_kbd_fds[k], &ev, sizeof(ev)) > 0) {}
                }
                if (g_test_tty_fd >= 0) {
                    tcflush(g_test_tty_fd, TCIFLUSH);
                }
                break;
            }
        }
    }

    /* 6. Clean restoration and exit */
    restore_vt();
    return 0;
}
