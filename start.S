/* start.S - Entry point for freestanding x86_64 binary
 * by seb3773 - https://github.com/seb3773
 */
.section .data
.global environ
environ:
    .quad 0   /* Will be set to envp pointer at startup */

.section .text
.global _start
.type _start, @function

_start:
    /* Kernel already provides a stack - don't change rsp */
    
    /* Clear frame pointer */
    xorq %rbp, %rbp
    
    /* Extract environ pointer from stack
     * Stack layout at entry:
     *   [rsp]     = argc
     *   [rsp+8]   = argv[0]
     *   [rsp+16]  = argv[1]
     *   ...
     *   [rsp+8*(argc+1)] = NULL (argv terminator)
     *   [rsp+8*(argc+2)] = envp[0] (start of environment)
     */
    movq (%rsp), %rax           /* argc */
    leaq 8(%rsp), %rcx          /* argv = rsp+8 */
    leaq 8(%rsp, %rax, 8), %rax /* envp = rsp+8 + 8*(argc+1) = rsp+8*(argc+2) */
    
    /* Store envp in environ global */
    movq %rax, environ(%rip)
    
    /* Call main function */
    call main
    
    /* Exit with return value from main */
    movl %eax, %edi
    movl $60, %eax   /* SYS_exit */
    syscall

.size _start, .-_start
