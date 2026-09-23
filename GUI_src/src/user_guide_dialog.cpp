/*
 * user_guide_dialog.cpp - Interactive Quick Start & User Guide dialog
 * by seb3773 - https://github.com/seb3773
 */

#include "user_guide_dialog.h"
#include "app_icons.h"

#include <ntqlayout.h>
#include <ntqlabel.h>
#include <ntqpushbutton.h>
#include <ntqlistbox.h>
#include <ntqtextbrowser.h>
#include <ntqsplitter.h>
#include <ntqframe.h>

UserGuideDialog::UserGuideDialog(TQWidget *parent, const char *name)
    : TQDialog(parent, name, true),
      m_topicList(0),
      m_browser(0),
      m_prevBtn(0),
      m_nextBtn(0),
      m_closeBtn(0),
      m_topicCounterLabel(0)
{
    setCaption("XBootsplash Studio — Quick Start & User Guide");
    setIcon(appWindowIcon());
    resize(880, 620);
    setupUI();
    selectTopic(0);
}

UserGuideDialog::~UserGuideDialog() {
}

void UserGuideDialog::setupUI() {
    TQVBoxLayout *mainLayout = new TQVBoxLayout(this, 12, 10);

    // Header Banner
    TQHBoxLayout *headerLayout = new TQHBoxLayout(mainLayout, 10);
    TQLabel *headerIcon = new TQLabel(this);
    headerIcon->setFixedSize(48, 48);
    headerIcon->setScaledContents(true);
    headerIcon->setPixmap(aboutAnimationFrame(11));
    headerLayout->addWidget(headerIcon, 0, TQt::AlignVCenter);

    TQVBoxLayout *headerTextLayout = new TQVBoxLayout(headerLayout, 2);
    TQLabel *titleLabel = new TQLabel(TQString::fromUtf8("<b><font size=\"+1\">XBootsplash Studio — User Guide & Technical Manual</font></b>"), this);
    titleLabel->setTextFormat(TQt::RichText);
    TQLabel *subLabel = new TQLabel("<font color=\"#666666\">Essential concepts, step-by-step workflow, compression trade-offs, and deployment.</font>", this);
    subLabel->setTextFormat(TQt::RichText);
    headerTextLayout->addWidget(titleLabel);
    headerTextLayout->addWidget(subLabel);
    headerLayout->addStretch(1);

    // Splitter between Sidebar List and Rich Text Browser
    TQSplitter *splitter = new TQSplitter(TQt::Horizontal, this);
    splitter->setOpaqueResize(true);

    // Left Sidebar Pane
    TQWidget *leftPane = new TQWidget(splitter);
    TQVBoxLayout *leftLayout = new TQVBoxLayout(leftPane, 0, 4);
    TQLabel *tocLabel = new TQLabel("<b>Guide Topics:</b>", leftPane);
    leftLayout->addWidget(tocLabel);

    m_topicList = new TQListBox(leftPane);
    m_topicList->insertItem("1. Overview & Architecture");
    m_topicList->insertItem("2. Quick Start Tutorial");
    m_topicList->insertItem("3. Display Modes & Layout");
    m_topicList->insertItem("4. Compression: UPKR vs ZX0");
    m_topicList->insertItem("5. Safe Hardware Testing (VT)");
    m_topicList->insertItem("6. Packaging & Installation");
    m_topicList->insertItem("7. Silent Boot & GRUB Setup");
    m_topicList->setFixedWidth(240);
    leftLayout->addWidget(m_topicList, 1);

    // Right Content Pane
    TQWidget *rightPane = new TQWidget(splitter);
    TQVBoxLayout *rightLayout = new TQVBoxLayout(rightPane, 0, 4);

    m_browser = new TQTextBrowser(rightPane);
    m_browser->setReadOnly(true);
    rightLayout->addWidget(m_browser, 1);

    splitter->setResizeMode(leftPane, TQSplitter::KeepSize);
    splitter->setResizeMode(rightPane, TQSplitter::Stretch);
    mainLayout->addWidget(splitter, 1);

    // Bottom Action Bar
    TQHBoxLayout *bottomLayout = new TQHBoxLayout(mainLayout, 8);

    m_prevBtn = new TQPushButton("< &Previous", this);
    m_nextBtn = new TQPushButton("&Next >", this);
    m_topicCounterLabel = new TQLabel(this);
    m_topicCounterLabel->setAlignment(TQt::AlignVCenter | TQt::AlignLeft);
    m_topicCounterLabel->setMinimumWidth(110);

    bottomLayout->addWidget(m_prevBtn);
    bottomLayout->addWidget(m_nextBtn);
    bottomLayout->addWidget(m_topicCounterLabel);
    bottomLayout->addStretch(1);

    m_closeBtn = new TQPushButton("&Close", this);
    m_closeBtn->setDefault(true);
    bottomLayout->addWidget(m_closeBtn);

    // Connections
    connect(m_topicList, SIGNAL(highlighted(int)), this, SLOT(onTopicSelected(int)));
    connect(m_topicList, SIGNAL(selected(int)), this, SLOT(onTopicSelected(int)));
    connect(m_prevBtn, SIGNAL(clicked()), this, SLOT(onPrevClicked()));
    connect(m_nextBtn, SIGNAL(clicked()), this, SLOT(onNextClicked()));
    connect(m_closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
    connect(m_browser, SIGNAL(anchorClicked(const TQString&, const TQString&)),
            this, SLOT(onAnchorClicked(const TQString&, const TQString&)));
}

void UserGuideDialog::selectTopic(int index) {
    if (index < 0 || index >= (int)m_topicList->count()) return;
    m_topicList->setCurrentItem(index);
    onTopicSelected(index);
}

void UserGuideDialog::scrollContents(int x, int y) {
    if (m_browser) {
        m_browser->setContentsPos(x, y);
    }
}

void UserGuideDialog::onTopicSelected(int index) {
    if (index < 0 || index >= (int)m_topicList->count()) return;

    m_browser->setText(getChapterHtml(index));
    m_browser->setContentsPos(0, 0); // Scroll to top

    if (m_topicCounterLabel) {
        m_topicCounterLabel->setText(TQString("<font color=\"#666666\">Topic %1 of %2</font>")
                                     .arg(index + 1)
                                     .arg(m_topicList->count()));
    }
    updateNavigationButtons();
}

void UserGuideDialog::onPrevClicked() {
    int cur = m_topicList->currentItem();
    if (cur > 0) {
        selectTopic(cur - 1);
    }
}

void UserGuideDialog::onNextClicked() {
    int cur = m_topicList->currentItem();
    if (cur < (int)m_topicList->count() - 1) {
        selectTopic(cur + 1);
    }
}

void UserGuideDialog::onAnchorClicked(const TQString &href, const TQString &target) {
    (void)target;
    if (href.startsWith("topic:")) {
        int topicIdx = href.mid(6).toInt();
        selectTopic(topicIdx);
    }
}

void UserGuideDialog::updateNavigationButtons() {
    int cur = m_topicList->currentItem();
    int total = (int)m_topicList->count();
    m_prevBtn->setEnabled(cur > 0);
    m_nextBtn->setEnabled(cur < total - 1);
}

TQString UserGuideDialog::getChapterHtml(int index) {
    switch (index) {
        case 0: // Overview & Architecture
            return TQString(
                "<h2>1. Overview & Architecture</h2>"
                "<p><b>XBootsplash</b> is a high-performance, minimalist Linux boot splash system designed "
                "to replace heavy boot splash daemons with an ultra-compact standalone binary.</p>"

                "<h3>Why XBootsplash?</h3>"
                "<ul>"
                "<li><b>Freestanding (Zero libc)</b>: Compiled with <code>-nostdlib</code> using a custom <code>nolibc.h</code> "
                "Linux syscall wrapper and assembly entry point (<code>start.S</code>). It never links against <code>libc.so</code>, "
                "<code>ld-linux.so</code>, or dynamic libraries.</li>"
                "<li><b>Microsecond Execution</b>: Initializes and renders the first frame within microseconds of kernel startup, "
                "before systemd user-space initialization begins.</li>"
                "<li><b>Universal Standalone Binary</b>: Typically only <b>15 KB to 40 KB</b> total size, containing "
                "the decompression engine, frame data, and graphics driver code in a single executable file.</li>"
                "<li><b>Crash-Proof Shutdown</b>: Because the binary has zero shared library dependencies, late-stage shutdown "
                "unmounting of root filesystems will never cause crashes or broken animations.</li>"
                "</ul>"

                "<h3>Dual Graphics Backends</h3>"
                "<table border='1' cellspacing='0' cellpadding='6' width='100%'>"
                "<tr bgcolor='#e2e8f0'><th align='left'><font color='#0f172a'>Backend</font></th><th align='left'><font color='#0f172a'>Advantages</font></th><th align='left'><font color='#0f172a'>Best Used For</font></th></tr>"
                "<tr><td><b>Framebuffer (/dev/fb0)</b></td>"
                "<td>Universal compatibility across all Linux kernels, UEFI efifb, VESA, SimpleFB, and DRM emulation. 100% static freestanding.</td>"
                "<td>Standard boots, early init, late shutdown. Recommended default.</td></tr>"
                "<tr><td><b>DRM / KMS</b></td>"
                "<td>Direct kernel mode setting with hardware VSync and PageFlip support for completely tear-free rendering.</td>"
                "<td>High-refresh displays and modern Intel, AMD, and NVIDIA KMS drivers.</td></tr>"
                "</table>"

                "<h3>Why Plymouth is Incompatible</h3>"
                "<p><font color='#eab308'><b>Important Note:</b></font> Plymouth and XBootsplash cannot run simultaneously. "
                "Both systems attempt to acquire exclusive ownership of the framebuffer and DRM master channels. "
                "If Plymouth is installed, it should be uninstalled or disabled before enabling XBootsplash.</p>"
            );

        case 1: // Quick Start Tutorial
            return TQString(
                "<h2>2. Quick Start Tutorial</h2>"
                "<p>Creating a complete boot splash takes only a few simple steps in XBootsplash Studio:</p>"

                "<h3>Step 1: Prepare Your Animation Assets</h3>"
                "<ul>"
                "<li><b>Image Sequence</b>: Prepare a folder containing sequential PNG frames (e.g. <code>frame_00.png</code> through <code>frame_24.png</code>).</li>"
                "<li><b>Dimensions</b>: Keep animations reasonably sized (e.g. 200x200 to 480x270 for spinners or logos). Full-screen dimensions (1920x1080) for every frame are unnecessary and generate large files.</li>"
                "<li>Click <b>Browse...</b> in section 1 of the configuration panel to select your frames directory.</li>"
                "</ul>"

                "<h3>Step 2: Position & Alignment</h3>"
                "<ul>"
                "<li>Use section 2 to adjust horizontal and vertical offsets, or click and drag the animation directly in the preview pane.</li>"
                "<li>Center alignments are automatic; offsets shift the animation relative to the screen center.</li>"
                "</ul>"

                "<h3>Step 3: Speed, Loop & Background Color</h3>"
                "<ul>"
                "<li>Set frame delay in milliseconds (e.g. <b>33 ms</b> = ~30 FPS, <b>16 ms</b> = ~60 FPS).</li>"
                "<li>Choose <b>Loop Behavior</b>:"
                "  <ul>"
                "    <li><i>Infinite Loop (Full cycle)</i>: Continuously loops all frames from start to finish.</li>"
                "    <li><i>No Loop (Stop at final frame)</i>: Plays the sequence once and holds the final frame until boot finishes.</li>"
                "    <li><i>Partial Loop (Loop from start frame)</i>: Plays an intro sequence once (frames 0..K), then continuously loops from frame K onward.</li>"
                "    <li><i>Ping-Pong (Forward &harr; Backward)</i>: Smoothly plays forward then mirrors back to the start.</li>"
                "  </ul>"
                "</li>"
                "<li>Optional: Set <b>Min Complete Loops</b> if you want the splash to guarantee showing at least N full cycles even on ultra-fast NVMe boot systems.</li>"
                "<li>Use the <b>Color Pipette</b> to sample the background color directly from your animation edge.</li>"
                "</ul>"

                "<h3>Step 4: Live Preview & Simulation</h3>"
                "<ul>"
                "<li>Press <b>Spacebar</b> to play/pause the animation in the preview window.</li>"
                "<li>Press <b>F11</b> to enter <b>Full-Screen Simulation</b>. This previews the exact visual output without rebooting your computer.</li>"
                "</ul>"

                "<h3>Step 5: Build Standalone Binary</h3>"
                "<ul>"
                "<li>Press <b>F5</b> or click <b>Build</b> in the toolbar.</li>"
                "<li>The build engine compiles the generator, super-packs the delta stream, and outputs the final standalone binary in seconds.</li>"
                "</ul>"
            );

        case 2: // Display Modes & Layout
            return TQString(
                "<h2>3. Display Modes & Layout</h2>"
                "<p>XBootsplash supports 5 distinct display modes tailored to different visual needs:</p>"

                "<h3>Mode 0: Animation on Solid Background (Default)</h3>"
                "<ul>"
                "<li><b>How it works</b>: Clears the display to your selected background color, then renders only the changing pixels of your animation frame-by-frame.</li>"
                "<li><b>Best for</b>: Circular spinners, pulsing logos, progress indicators, or minimalist graphics.</li>"
                "<li><b>Efficiency</b>: Extremely compact and lightweight.</li>"
                "</ul>"

                "<h3>Mode 1: Animation on Background Image</h3>"
                "<ul>"
                "<li><b>How it works</b>: Decompresses a full-screen high-resolution background image once into memory, then overlays and loops the animation on top.</li>"
                "<li><b>Best for</b>: Branded wallpapers with an animated spinner or glowing emblem in the center.</li>"
                "<li><b>Controls</b>: Both the animation and the background image can be positioned and offset independently.</li>"
                "</ul>"

                "<h3>Mode 2: Static Image on Solid Background</h3>"
                "<ul>"
                "<li><b>How it works</b>: Displays a single high-quality logo or badge centered on a solid color backdrop.</li>"
                "<li>Compressed with an 8-bit adaptive palette for instant startup.</li>"
                "</ul>"

                "<h3>Mode 3 & 4: Static Wallpaper (Fit / Stretch)</h3>"
                "<ul>"
                "<li><b>Mode 3 (Fit to screen)</b>: Scales the static image maintaining aspect ratio with black letterboxing.</li>"
                "<li><b>Mode 4 (Fill / Stretch)</b>: Scales the static image to completely fill the target display resolution.</li>"
                "</ul>"

                "<h3>Helpful Alignment Tips</h3>"
                "<ul>"
                "<li>Toggle <b>Guides (G)</b> to inspect crosshairs and boundary margins.</li>"
                "<li>In Mode 1, select either <i>[Anim]</i> or <i>[Bg Image]</i> buttons above the preview to drag that specific layer.</li>"
                "</ul>"
            );

        case 3: // Compression: UPKR vs ZX0
            return TQString(
                "<h2>4. Compression: UPKR vs ZX0</h2>"
                "<p>One of XBootsplash's greatest technological strengths is its multi-tiered compression pipeline.</p>"

                "<h3>Differential XOR Delta Pipeline</h3>"
                "<p>Rather than compressing full standalone frames, XBootsplash computes the bitwise difference "
                "between consecutive frames: <code>delta[n] = frame[n] ^ frame[n-1]</code>. "
                "Static pixels become zeros, reducing animation data by over <b>90%</b> before compression even starts.</p>"

                "<h3>Super-Compression Options</h3>"
                "<table border='1' cellspacing='0' cellpadding='5' width='100%'>"
                "<tr bgcolor='#e2e8f0'><th align='left'><font color='#0f172a'>Algorithm</font></th><th align='left'><font color='#0f172a'>Ratio</font></th><th align='left'><font color='#0f172a'>Build Time</font></th><th align='left'><font color='#0f172a'>Decode</font></th><th align='left'><font color='#0f172a'>Best Used For</font></th></tr>"
                "<tr><td><font color='#059669'><b>UPKR</b></font><br><font size='-1'>Dennis Ranke</font></td>"
                "<td><b>Best</b><br><font size='-1'>~15&ndash;25% &lt; ZX0</font></td>"
                "<td><b>Fast</b><br><font size='-1'>~1&ndash;2s</font></td>"
                "<td>Lightweight rANS<br><font size='-1'>&lt; 1 ms</font></td>"
                "<td><b>Recommended default</b> for all splashes.</td></tr>"
                "<tr><td><font color='#d97706'><b>ZX0</b></font><br><font size='-1'>Einar Saukas</font></td>"
                "<td><b>Very Good</b><br><font size='-1'>Standard</font></td>"
                "<td><b>Very Slow</b><br><font size='-1'>quadratic</font></td>"
                "<td>Microsecond<br><font size='-1'>0B dynamic RAM</font></td>"
                "<td>Extreme micro-footprint environments.</td></tr>"
                "<tr><td><b>None</b><br><font size='-1'>Standard RLE</font></td>"
                "<td>Moderate<br><font size='-1'>Baseline</font></td>"
                "<td><b>Instant</b><br><font size='-1'>&lt; 0.2s</font></td>"
                "<td>Fastest</td>"
                "<td>Fast iterative testing and preview builds.</td></tr>"
                "</table>"

                "<h3>How to Enable Super-Compression</h3>"
                "<p>In <b>Section 5 (Binary & Build Settings)</b>, toggle the <b>Enable Super-Compression</b> switch. "
                "Select <b>UPKR</b> for the best compression ratio or <b>ZX0</b> for microsecond decoding.</p>"
            );

        case 4: // Safe Hardware Testing (VT)
            return TQString(
                "<h2>5. Safe Hardware Testing (VT)</h2>"
                "<p>Testing a bootsplash on real hardware can be hazardous if executed directly inside a desktop session.</p>"

                "<h3>The Danger of Direct Desktop Testing</h3>"
                "<p>Graphic display servers (Xorg, Wayland, X11) maintain exclusive control over GPU scanout and VT modes. "
                "Running a raw framebuffer or KMS splash directly on top of an active graphical session can corrupt the video driver, "
                "blank the display, or leave the keyboard completely unresponsive, requiring a hard power reset.</p>"

                "<h3>The Integrated VT Runner</h3>"
                "<p>XBootsplash Studio includes a safe hardware execution bridge (<b>VT Runner</b>):</p>"
                "<ul>"
                "<li>Click <b>Test Live</b> in the top toolbar to launch a safe test.</li>"
                "<li>The runner safely switches the virtual console to an inactive VT (<b>VT8</b>).</li>"
                "<li>Keyboard raw mode is intercepted: pressing <b>ESC</b>, <b>Enter</b>, or <b>Spacebar</b> instantly terminates the splash.</li>"
                "<li>The original VT graphics state, keyboard mode, and active desktop session are cleanly restored upon exit.</li>"
                "</ul>"

                "<p><i>Note: Hardware VT switching requires root/sudo privileges to access <code>/dev/tty0</code> and graphics devices.</i></p>"
            );

        case 5: // Packaging & Installation
            return TQString(
                "<h2>6. Packaging & Installation</h2>"
                "<p>Once your bootsplash binary is compiled, XBootsplash Studio provides integrated tools to package, "
                "distribute, and install it as an early boot system service.</p>"

                "<h3>The <code>.xbs</code> Package Format</h3>"
                "<ul>"
                "<li>An <b>.xbs</b> file is a self-contained archive containing:</li>"
                "  <ul>"
                "    <li>The compiled standalone bootsplash executable.</li>"
                "    <li>Theme metadata (resolution, backend, frame rate, loop mode, compression format).</li>"
                "    <li>A representative preview thumbnail for package managers.</li>"
                "  </ul>"
                "<li>Click <b>Package...</b> in the toolbar to export the current project or inspect existing packages.</li>"
                "</ul>"

                "<h3>Installing as a System Service</h3>"
                "<ul>"
                "<li>Click <b>Install...</b> to register the bootsplash with your system init.</li>"
                "<li>XBootsplash deploys two lightweight systemd units:</li>"
                "  <ul>"
                "    <li><code>xbootsplash.service</code>: Launches at earliest boot (before <code>sysinit.target</code>) and exits when display manager begins.</li>"
                "    <li><code>xbootsplash-shutdown.service</code>: Launches during shutdown/reboot to display a graceful exit animation.</li>"
                "  </ul>"
                "</ul>"
            );

        case 6: // Silent Boot & GRUB Setup
            return TQString(
                "<h2>7. Silent Boot & GRUB Setup</h2>"
                "<p>To ensure your bootsplash looks smooth and professional, kernel text logs (dmesg) "
                "must be prevented from writing over the graphics buffer.</p>"

                "<h3>Recommended Kernel Parameters</h3>"
                "<table border='1' cellspacing='0' cellpadding='6' width='100%'>"
                "<tr bgcolor='#e2e8f0'><th align='left'><font color='#0f172a'>Parameter</font></th><th align='left'><font color='#0f172a'>Description</font></th></tr>"
                "<tr><td><code>quiet</code></td><td>Suppresses standard informational kernel boot messages.</td></tr>"
                "<tr><td><code>loglevel=3</code></td><td>Limits console output strictly to critical system errors.</td></tr>"
                "<tr><td><code>vt.global_cursor_default=0</code></td><td>Disables the blinking white text console cursor on framebuffer consoles.</td></tr>"
                "<tr><td><code>fbcon=nodefer</code></td><td>Ensures the framebuffer console initializes immediately at early boot.</td></tr>"
                "</table>"

                "<h3>Silent Boot Configuration Helper</h3>"
                "<p>Open <b>Tools -> Silent Boot Configuration (GRUB)...</b> from the menu bar:</p>"
                "<ul>"
                "<li>The helper inspects your current <code>/etc/default/grub</code> and active <code>/proc/cmdline</code>.</li>"
                "<li>Missing parameters are highlighted in red.</li>"
                "<li>Click <b>Apply & Update GRUB</b> to safely merge missing silent boot parameters into your configuration while preserving all custom hardware options.</li>"
                "</ul>"
            );

        default:
            return "<p>Select a topic from the list on the left to read the guide.</p>";
    }
}

#include "user_guide_dialog.moc"
