/* start.S - Entry point for freestanding x86_64 binary
 * by seb3773 - https://github.com/seb3773
 */
.section .text
.global _start
.type _start, @function

_start:
    /* Kernel already provides a stack - don't change rsp */
    
    /* Clear frame pointer */
    xorq %rbp, %rbp
    
    /* Call main function */
    call main
    
    /* Exit with return value from main */
    movl %eax, %edi
    movl $60, %eax   /* SYS_exit */
    syscall

.size _start, .-_start
