#include <machine/cpu-features.h>

        .text
        .fpu    neon

        .global blend
        .type blend, %function
        .align 4

    /*         r0   r1    r2    r3    r4
        blend(dst, src, dst_w, src_w, src_h) 
                    
    	ldr	r4, [sp, #16]
    	ldr	r5, [sp, #16+4]
    	ldr	r6, [sp, #16+8]
	*/

blend:
    .fnstart
    .save   {r4-r6, lr}
    stmfd   sp!, {r4-r6, lr}

	/* Paramater Check */
	and	r5, r0, #3
	cmps	r5, #0
	bne	fail

	and	r5, r1, #3
	cmps	r5, #0
	bne	fail

	cmps	r2, #0
	beq	fail

	cmps	r3, #0
	beq	fail

	ldr	r4, [sp, #16]
	cmps	r4, #0
	beq	fail
	
	mov	r6, r0
	vpush	{d0-d11}

	mov r5, #256
	vdup.8 d8, r5

b_0:
	mov	r5, r3
b_A:
	subs 	r5, r5, #8
	bcc	b_B

    //      R  G  B  A
	vld4.8 {d0,d1,d2,d3}, [r0]      //load dst
	pld	[r0, #(64*2)]
	vld4.8 {d4,d5,d6,d7}, [r1]!     //load src
    pld	[r1, #(64*2)]

    vqsub.u8 d9, d8, d7           //d9 = 256 - d7

    vmull.u8 q5, d9, d0             //q5 = d0 * (256 - d7)
    vmull.u8 q6, d9, d1             //q5 = d1 * (256 - d7)
    vmull.u8 q7, d9, d2             //q5 = d2 * (256 - d7)    
    vmull.u8 q8, d3, d7             //q5 = d3 * d7    
    vqshrn.u16 d0, q5, #8           //d0 = q5 >> 8
    vqadd.u8 d0, d0, d4           //d0 += d4    
  
    vqshrn.u16 d1, q6, #8           //d1 = q5 >> 8
    vqadd.u8 d1, d1, d5           //d1 += d5

  
    vqshrn.u16 d2, q7, #8           //d2 = q5 >> 8
    vqadd.u8 d2, d2, d6           //d2 += d6

    
    vqshrn.u16 d9, q8, #8           //d9 = q5 >> 8
    vqsub.u8 d3, d3, d9             //d3 -= d9
    vqadd.u8 d3, d3, d7             //d3 += d7

	vst4.8 {d0,d1,d2,d3}, [r0]!     //store dst
	b	b_A
b_B:
	adds	r5, r5, #8
	beq	next
b_C:
	//vld1.16 {d0[0]}, [r1]!
	//vld1.16 {d1[0]}, [r2]!
	//vst2.16 {d0[0], d1[0]}, [r0]!
	subs 	r5, r5, #1
	bne	b_C
	b	next

next:
    mov r5, #4
	mul	r5, r5, r2                  //r5 = r2 * 4
	add	r6, r6, r5                  //r6 += r5
	mov r0, r6
	subs r4, r4, #1
	bne	b_0

	vpop	{d0-d11}
	b	ok
	

	/********************** return **********************/
fail:	
	mov	r0, #0
	b	return
ok:
	mov	r0, r6
	
return:
	ldmfd	sp!, {r4-r6, lr}
	mov	pc, lr
	.fnend	
