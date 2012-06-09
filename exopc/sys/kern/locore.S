
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */


#include <xok/asm.h>
#include <xok/mmu.h>
#include <xok/trap.h>
#include <xok/env.h>
#include <xok/sys_syms.h>

/* The kernel is linked to be run up around KERNBASE + 1 Meg,
 * but when boot jumps to start, we are executing aroud 1 Meg.
 * RELOC(x) gives the address of symbol x before physical memory
 * is remapped at KERNBASE
 */
#define	RELOC(x) ((x) - KERNBASE)


#define SEG_NULL           \
	.word 0, 0;        \
	.byte 0, 0, 0, 0
#define SEG(type,base,lim)                                    \
	.word ((lim)&0xffff), ((base)&0xffff);                \
	.byte (((base)>>16)&0xff), (0x90|(type)),             \
		(0xc0|(((lim)>>16)&0xf)), (((base)>>24)&0xff)

	
.data
	
/* See mmu.h for a complete description of these two.*/
	.globl	_vpt
	.set	_vpt, VPT
	.globl	_vpd
	.set	_vpd, (VPT + SRL(VPT, 10))

###################################################################
# boot stack
###################################################################
	.p2align	PGSHIFT		# force page alignment
	.globl		bootstack
bootstack:
	.space		KSTKSIZE
	.globl		_bootstacktop   
_bootstacktop:

	
###################################################################
# setup the GDT	
###################################################################
.set CODE_SEL,0x8				# index of code seg within mygdt
.set DATA_SEL,0x10				# index of data seg within mygdt
.p2align	2				# force 4 byte alignment
mygdt:
	SEG_NULL				# null seg
	SEG(STA_X|STA_R, -KERNBASE, 0xffffffff)	# code seg
	SEG(STA_W, -KERNBASE, 0xffffffff)	# data seg
mygdtdesc:
	.word	0x17			# sizeof (mygdt) - 1
	.long	RELOC(mygdt)		# address mygdt
	
.text
	
###################################################################
# entry point
###################################################################

.globl		_text_start
_text_start:
.globl		start
start:
	movw	$0x1234,0x472			# warm boot
	lgdt	RELOC(mygdtdesc)		# load descriptor
	movl	$DATA_SEL, %eax			# reload seg regs
	movl	%ax,%es				#
	movl	%ax,%ds				#
	movl	%ax,%ss				#
	ljmp	$CODE_SEL,$relocated		# reload CS
relocated:
	movl	$0x0,%ebp			# And nuke frame pointer
	# XXX use sizeof(.)
        # Leave a few words on the stack for a user utf trap frame 
	# first 4 bytes on each stack is reserved for cpu context ptr
	movl	$(_bootstacktop-0x40),%esp	# set the stack pointer 

	/* Zero the BSS */
	xorl	%eax,%eax			# quickest way to get zero
	cld					# d=0 string op is ascending
	movl	$_edata,%edi			# destination of string op
	movl	$(3 + _end),%ecx		# repeat count for "rep"
	subl	$_edata,%ecx			#  = ceil ((_end-_edata)/4)
	shr	$2, %ecx
	rep
	 stosl					# store it!
	
	# now to C code
	call	_i386_init
	



/* 
 * Test pure trap speed.
 */
ENTRY(fasttrap)
#if 0
	pushl	%ds
	pushl	%es
#if 0
	pushl 	$GD_KD
	popl	%ds
	pushl	$GD_KD
	popl	%es
#endif
	popl	%es
	popl	%ds
#endif
	iret


/*
 * System call entry point.  This routine was a bit slow in C.  This
 * should help things.
 *
 * On entry:
 *       %eax - # of system call
 *       %edx - 1st argument (if any)
 *       %ecx - 2nd argument (if any)
 *       %ebx - 3rd argument (if any)
 * (12(%esp)) - rest of arguments (if any) (user %esp is in trapframe)
 */

DBLC2:
 	.ascii "syscall path 0x%x\n"
.text
ENTRY(syscall)
#ifdef __HOST__
	cmpl $GD_NULLS*8, -4(%esp)
	jl _fault_trampoline
#endif
	pushl	%ds
	pushl	%es
	andl	$(MAX_SYSCALL-1),%eax	# Save one cycle with lazy decoding
	pushal				# Save all registers in case of fault
	pushl	$GD_KD			# Load up kernel segment registers
	popl	%ds
	pushl	$GD_KD
	popl	%es
	movl	_sctab+4(,%eax,8),%esi	# %esi is the number of args to syscall
	subl	$3,%esi			# Take the slow path for > 3 arguments
	ja	1f			#  (leaving # stackargs in %esi)
	pushl	%ebx			# Push third argument on stack
#if 0
#ifdef __SMP__
	lock
#endif
	incl	(_nsyscalls)
#endif	
	movl	_sctab(,%eax,8),%esi	# Address of function for syscall
	movl	$0, _syscall_pfcleanup	# Disable any old cleanup function
	call	%esi
	movl	8(%esp),%esi		# Restore %esi from pushal
	addl	$36,%esp
	popl	%es
	popl	%ds
	iret				
1:					
	movl	%esp,%ebp		# Remember stack pointer
	movl	52(%ebp),%edi		# User stack pointer


/* Allow page faults getting user args */
#if 1
	movl    $PFM_PROP,__page_fault_mode_H_ADDR
#else
	xorl	%eax,%eax
	str     %ax
	addl 	$-GD_TSS,%eax
	shrl 	$3,%eax
	movl 	___cpucxts(,%eax,4),%eax
	movl	$PFM_PROP, PFMODE_OFFSET(%eax)
#endif
	cmpl	$ULIM,%edi		# Make sure %edi is not in kern space
	ja	2f
1:
	movl	-4(%edi,%esi,4),%eax	# Take arguments on user stack
	pushl	%eax			# And push them on kernel stack
	decl	%esi			#  while (--%esi);
	jg	1b


/* Arguments over, no more faults */
#if 1
	movl    $PFM_NONE,__page_fault_mode_H_ADDR
#else
 	xorl	%eax,%eax
	str     %ax
	addl 	$-GD_TSS,%eax
	shrl 	$3,%eax
	movl 	___cpucxts(,%eax,4),%eax
	movl	$PFM_NONE, PFMODE_OFFSET(%eax)
#endif
	pushl	%ebx			# 3th argument still in %ebx
	movl	28(%ebp),%eax		# Restore %eax from pushal
	movl	_sctab(,%eax,8),%esi	# Address of fucntion for syscall
	movl	$0, _syscall_pfcleanup	# Disable any old cleanup function
	call	%esi

	movl	(%ebp),%edi		# Restore some registers
	movl	4(%ebp),%esi
	leal	32(%ebp),%esp		# Move %esp above pushal & seg regs
	movl	8(%ebp),%ebp
	popl	%ds
	popl	%es
	iret				
2:					# bogus user %esp and # args > 3
	movl	$ULIM,%edi		# Guarantee a fault
	jmp	1b


