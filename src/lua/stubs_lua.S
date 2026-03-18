/*
 * From what I can tell, My Singing Monster's Lua functions use a non-standard calling convention.
 * This looks like a result of MSVC link-time codegen:
 *   - Param 1 in ECX, Param 2 in EDX
 *   - Remaining params on stack (right-to-left)
 *   - Callee does plain RET (caller cleans stack)
 *
 * GCC cannot express this natively, so we use asm wrappers.
 * BBB may stop using LTCG for building which would cause our current solution to break. Keep an eye on this!
 *
 * Stubs:
 *   call_lua1(func, p1)               -> func(ECX=p1)
 *   call_lua2(func, p1, p2)           -> func(ECX=p1, EDX=p2)
 *   call_lua3(func, p1, p2, p3)       -> func(ECX=p1, EDX=p2, stack: p3)
 *   call_lua4(func, p1, p2, p3, p4)   -> func(ECX=p1, EDX=p2, stack: p3, p4)
 */

.text

/* call_lua1(func, p1) — func(ECX=p1) */
.global _call_lua1
_call_lua1:
    movl  4(%esp), %eax
    movl  8(%esp), %ecx
    jmp   *%eax

/* call_lua2(func, p1, p2) — func(ECX=p1, EDX=p2) */
.global _call_lua2
_call_lua2:
    movl  4(%esp), %eax
    movl  8(%esp), %ecx
    movl  12(%esp), %edx
    jmp   *%eax

/* call_lua3(func, p1, p2, p3) — func(ECX=p1, EDX=p2, stack: p3) */
.global _call_lua3
_call_lua3:
    pushl %ebp
    movl  %esp, %ebp
    movl  12(%ebp), %ecx
    movl  16(%ebp), %edx
    pushl 20(%ebp)
    call  *8(%ebp)
    addl  $4, %esp
    popl  %ebp
    ret

/* call_lua4(func, p1, p2, p3, p4) — func(ECX=p1, EDX=p2, stack: p3, p4) */
.global _call_lua4
_call_lua4:
    pushl %ebp
    movl  %esp, %ebp
    movl  12(%ebp), %ecx
    movl  16(%ebp), %edx
    pushl 24(%ebp)
    pushl 20(%ebp)
    call  *8(%ebp)
    addl  $8, %esp
    popl  %ebp
    ret
