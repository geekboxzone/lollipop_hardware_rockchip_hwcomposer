#include <machine/cpu-features.h>

    .text
    .fpu    neon

    .global blend
    .type blend, %function
    .align 4

/*         r0   r1    r2    r3     r4      r8     r9
    blend(dst, src, dst_w, src_w, src_h, bak_wr, bak_rd) 
                
     0  .. 20  24     28      32      36
    [r4 .. r9, lr,   src_h, bak_wr, bak_rd]
*/

/*
    stmfd  sp!, {r0-r1}
    mov r1, xx
    adr r0, format
    bl printf
    ldmfd  sp!, {r0-r1}
*/

blend:
    .fnstart
    .save   {r4-r9, lr}
    stmfd   sp!, {r4-r9, lr}

    /* Paramater Check */
    and	r5, r0, #3
    cmps	r5, #0
    bne	fail

    and	r5, r1, #3
    cmps	r5, #0
    bne	fail

    cmps	r2, #8
    blt	fail

    cmps	r3, #8
    blt	fail

    ldr	r4, [sp, #28]
    cmps	r4, #0
    beq	fail

    ldr	r8, [sp, #32]
    ldr	r9, [sp, #36]

    cmps r9, #0
    movne r8, #0        //if(bak_rd)    r8 = 0;

    mov	r6, r0
    mov r7, r9
    vpush	{d0-d12}
    mov r5, #256
    vdup.8 d8, r5

b_0:
    cmps r7, #0
    moveq r9, r0        //if(bak_rd==0) r9 = r0;    
    mov	r5, r3
b_A:
    subs r5, r5, #8
    bcc	b_B

b_A1:
    //      R  G  B  A
    vld4.8 {d0,d1,d2,d3}, [r9]!     //load dst
    pld	[r0, #(64*2)]
    vld4.8 {d4,d5,d6,d7}, [r1]!     //load src
    pld	[r1, #(64*2)]

    vqsub.u8 d9, d8, d7             //d9 = 256 - d7

    vmull.u8 q5, d0, d9             //q5 = d0 * d9
    vqshrn.u16 d12, q5, #8          //d12 = q5 >> 8
    vqadd.u8 d4, d4, d12            //d4 += d12

    vmull.u8 q5, d1, d9             //q5 = d1 * d9
    vqshrn.u16 d12, q5, #8          //d12 = q5 >> 8
    vqadd.u8 d5, d5, d12            //d5 += d12

    vmull.u8 q5, d2, d9             //q5 = d2 * d9
    vqshrn.u16 d12, q5, #8          //d12 = q5 >> 8
    vqadd.u8 d6, d6, d12            //d6 += d12

    vmull.u8 q5, d3, d7             //q5 = d3 * d7
    vqshrn.u16 d12, q5, #8          //d12 = q5 >> 8
    vqsub.u8 d7, d7, d12            //d7 -= d12
    vqadd.u8 d7, d7, d3             //d7 += d3

    cmps r8, #0
    beq b_A2
    vst4.8 {d0,d1,d2,d3}, [r8]!
b_A2:
    vst4.8 {d4,d5,d6,d7}, [r0]!     //store dst
    b b_A
b_B:
    adds r5, r5, #8
    beq	next
    
    rsb r5, r5, #8
b_C:
    sub r0, r0, #4
    sub r1, r1, #4
    sub r9, r9, #4
    cmps r8, #0
    subne r8, r8, #4
    
#if 1
    vst4.8 {d0[7],d1[7],d2[7],d3[7]}, [r0]     //store dst
    vshl.u64 d0, d0, #8
    vshl.u64 d1, d1, #8
    vshl.u64 d2, d2, #8
    vshl.u64 d3, d3, #8
#else
    vst4.8 {d0[7],d1[7],d2[7],d3[7]}, [r0]     //store dst
    vshr.u64 d0, d0, #8
    vshr.u64 d1, d1, #8
    vshr.u64 d2, d2, #8
    vshr.u64 d3, d3, #8
#endif
    subs r5, r5, #1
    bne b_C
    b b_A1

next:
    mov r5, #4
    mul r5, r2, r5                  //r5 = r2 * 4
    add r6, r6, r5                  //r6 += r5
    mov r0, r6
    subs r4, r4, #1
    bne b_0

    vpop {d0-d12}
    b ok

    /********************** return **********************/
fail:	
    mov r0, #0
    b return
ok:
    mov r0, #1
	
return:
    ldmfd sp!, {r4-r9, lr}
    mov pc, lr
    .fnend

