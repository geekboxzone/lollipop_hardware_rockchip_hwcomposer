#include <machine/cpu-features.h>

        .text
        .fpu    neon

        .global intersect
        .type intersect, %function
        .align 4

    /*             r0    r1    r2    r3    r4
        intersect(dst, src0, src1, src_w, src_h) 
                    
    	ldr	r4, [sp, #16]
    	ldr	r5, [sp, #16+4]
    	ldr	r6, [sp, #16+8]
	*/

intersect:
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
	pld	[r1, #(64*2)]

	and	r5, r2, #3
	cmps	r5, #0
	bne	fail
	pld	[r2, #(64*2)]

	cmps	r3, #0
	beq	fail

	ldr	r4, [sp, #16]
	cmps	r4, #0
	beq	fail
	
	mov	r6, r0
	vpush	{d0-d3}

b_0:
	mov	r5, r3
b_A:
	subs 	r5, r5, #8
	bcc	b_B
	vld1.16 {d0,d1}, [r1]!
	pld	[r1, #(64*2)]
	vld1.16 {d2,d3}, [r2]!
	pld	[r2, #(64*2)]
	vst2.16 {d0,d1,d2,d3}, [r0]!
	b	b_A
b_B:
	adds	r5, r5, #8
	beq	next
b_C:
	vld1.16 {d0[0]}, [r1]!
	pld	[r1, #(64*2)]
	vld1.16 {d1[0]}, [r2]!
	pld	[r2, #(64*2)]
	vst2.16 {d0[0], d1[0]}, [r0]!
	subs 	r5, r5, #1
	bne	b_C
	b	next

next:
    mov r5, #2
	mul	r5, r5, r3
	add	r1, r1, r5
	add	r2, r2, r5
	subs 	r4, r4, #1
	bne	b_0
	vpop	{d0-d3}
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
