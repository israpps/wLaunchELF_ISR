/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# $Id: as_reg_compat.h 594 2004-09-21 11:35:19Z pixel $
# Header used for compatibility from binutils 2.9 and 2.14
*/


#ifndef __REGNAMES_H__
#define __REGNAMES_H__

/* register defines for the GNU assembler */

// MIPS CPU Registsers
#define zero	$0		// Always 0
#define at		$1		// Assembler temporary
#define v0		$2		// Function return
#define v1		$3		//
#define a0		$4		// Function arguments
#define a1		$5
#define a2		$6
#define a3		$7
#define t0		$8		// Temporaries. No need
#define t1		$9		// to preserve in your
#define t2		$10		// functions.
#define t3		$11
#define t4		$12
#define t5		$13
#define t6		$14
#define t7		$15
#define s0		$16		// Saved Temporaries.
#define s1		$17		// Make sure to restore
#define s2		$18		// to original value
#define s3		$19		// if your function
#define s4		$20		// changes their value.
#define s5		$21
#define s6		$22
#define s7		$23
#define t8		$24		// More Temporaries.
#define t9		$25
#define k0		$26		// Reserved for Kernel
#define k1		$27
#define gp		$28		// Global Pointer
#define sp		$29		// Stack Pointer
#define fp		$30		// Frame Pointer
#define ra		$31		// Function Return Address

// Playstation2 GS Privileged Registers
#define pmode		0x12000000	// Setup CRT Controller
#define smode2		0x12000020	// CRTC Video Settings: PAL/NTCS, Interlace, etc.
#define dispfb1		0x12000070	// Setup the CRTC's Read Circuit 1 data source settings
#define display1	0x12000080	// RC1 display output settings
#define dispfb2		0x12000090	// Setup the CRTC's Read Circuit 2 data source settings
#define display2	0x120000a0	// RC2 display output settings
#define extbuf		0x120000b0	// ...
#define extdata		0x120000c0	// ...
#define extwrite	0x120000d0	// ...
#define bgcolor		0x120000e0	// Set CRTC background color
#define csr			0x12001000	// System status and reset
#define imr			0x12001010	// Interrupt Mask Register
#define busdir		0x12001040	// ...
#define siglblid	0x12001080	// ...


#endif
