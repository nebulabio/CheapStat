
//*****************************************************************************
//
// File Name	: 'global.h'
// Title		: AVR project global include 
// Author		: Pascal Stang
// Created		: 7/12/2001
// Revised		: 9/30/2002
// Version		: 1.1
// Target MCU	: Atmel AVR series
// Editor Tabs	: 4
//
//	Description : This include file is designed to contain items useful to all
//					code files and projects.
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
//*****************************************************************************

#ifndef GLOBAL_H
#define GLOBAL_H

// global AVRLIB defines
//#include "avrlibdefs.h"
// global AVRLIB types definitions
//#include "avrlibtypes.h"

// project/system dependent defines

// CPU clock speed
//#define F_CPU        16000000               		// 16MHz processor
//#define F_CPU        14745000               		// 14.745MHz processor
//#define F_CPU        8000000               		// 8MHz processor
//#define F_CPU        7372800               		// 7.37MHz processor
//#define F_CPU        4000000               		// 4MHz processor
//#define F_CPU        3686400               		// 3.69MHz processor
#define CYCLES_PER_US ((F_CPU+500000)/1000000) 	// cpu cycles per microsecond

// Code compatibility to new AVR-libc
// outb(), inb(), inw(), outw(), BV(), sbi(), cbi(), sei(), cli()
#ifndef outb
	#define	outb(addr, data)	addr = (data)
#endif
#ifndef inb
	#define	inb(addr)			(addr)
#endif
#ifndef outw
	#define	outw(addr, data)	addr = (data)
#endif
#ifndef inw
	#define	inw(addr)			(addr)
#endif
#ifndef BV
	#define BV(bit)			(1<<(bit))
#endif
#ifndef cbi
	#define cbi(reg,bit)	reg &= ~(BV(bit))
#endif
#ifndef sbi
	#define sbi(reg,bit)	reg |= (BV(bit))
#endif
#ifndef cli
	#define cli()			__asm__ __volatile__ ("cli" ::)
#endif
#ifndef sei
	#define sei()			__asm__ __volatile__ ("sei" ::)
#endif

// port address helpers
#define DDR(x) ((x)-1)    // address of data direction register of port x
#define PIN(x) ((x)-2)    // address of input register of port x

// MIN/MAX/ABS macros
#define MIN(a,b)			((a<b)?(a):(b))
#define MAX(a,b)			((a>b)?(a):(b))
#define ABS(x)				((x>0)?(x):(-x))

// constants
#define PI		3.14159265359

#ifndef WIN32
	// true/false defines
	#define FALSE	0
	#define TRUE	-1
#endif

// datatype definitions macros
typedef unsigned char  u08;
typedef   signed char  s08;
typedef unsigned short u16;
typedef   signed short s16;
typedef unsigned long  u32;
typedef   signed long  s32;
typedef unsigned long long u64;
typedef   signed long long s64;

/* use inttypes.h instead
// C99 standard integer type definitions
typedef unsigned char	uint8_t;
typedef   signed char	int8_t;
typedef unsigned short	uint16_t;
typedef   signed short	int16_t;
typedef unsigned long	uint32_t;
typedef   signed long	int32_t;
typedef unsigned long	uint64_t;
typedef   signed long	int64_t;
*/
// maximum value that can be held
// by unsigned data types (8,16,32bits)
#define MAX_U08	255
#define MAX_U16	65535
#define MAX_U32	4294967295

// maximum values that can be held
// by signed data types (8,16,32bits)
#define MIN_S08	-128
#define MAX_S08	127
#define MIN_S16	-32768
#define MAX_S16	32767
#define MIN_S32	-2147483648
#define MAX_S32	2147483647

#ifndef WIN32
	// more type redefinitions
	typedef unsigned char   BOOL;
	typedef unsigned char	BYTE;
	typedef unsigned int	WORD;
	typedef unsigned long	DWORD;

	typedef unsigned char	UCHAR;
	typedef unsigned int	UINT;
	typedef unsigned short  USHORT;
	typedef unsigned long	ULONG;

	typedef char			CHAR;
	typedef int				INT;
	typedef long			LONG;
#endif


#endif
