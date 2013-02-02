// Definitions for sfArk / WaveComp
// Copyright 1998-2000 Andy Inman
// Contact via: http://netgenius.co.uk or http://melodymachine.com

#include	"sfarklib.h"

// ----- typdefs -----
typedef unsigned short		USHORT;
typedef unsigned char			BYTE;
typedef unsigned long			ULONG;
//typedef int								bool;

typedef short							AWORD;				// Audio word (i.e., 16-bit audio)
typedef unsigned short		UAWORD;
typedef long		 					LAWORD;				// "long" audio word i.e. 32 bits
typedef unsigned long			ULAWORD;

// Types used by Bit I/O (BIO) routines...
typedef USHORT					BIOWORD;   
typedef ULONG						BIOWORD2;

// -------------------


// ----- Constants -----
#ifndef true
#define true    1
#define false		0
#endif

#define SHIFTWIN 64       // window size used for shift operations

// Just for readability...
#define SIGNBIT  1
#define FLAGBIT  1

// Some max sizes...
#define MAX_BUFSIZE		4096		// Largest buffer size
#define MAX_AWORD   ( (1 << (AWORD_BITS-1)) -1 )    // 32767 for 16-Bit AWORD
#define MAX_UAWORD  ( (1 << AWORD_BITS)-1 )         // 65535 for 16-Bit AWORD

// Typed NULLs ...
#define FL_NULL					((float *) 0)
#define AW_NULL					((AWORD *) 0)
#define LAW_NULL				((LAWORD *) 0)

// Compression methods...
#define COMPRESSION_v1            0       // sfArk V1 compression
#define COMPRESSION_v1Fast        1       // sfArk V1.1 "Fast" compression
#define COMPRESSION_v1Full        2       // sfArk V1.1 "Full" compression

#define	COMPRESSION_v2						3				// Lowest value for V2 compression

#define COMPRESSION_v2NonAudio    3       // sfArk V2 "NonAudio" (uses ZLib MemComp)
#define COMPRESSION_v2Turbo       4       // sfArk V2 "Turbo" compression (BD2 only, maxloops 3)
#define COMPRESSION_v2Fast        5       // sfArk V2 "Fast" compression (BD2,3,4, maxloops 15)
#define COMPRESSION_v2Standard    6       // sfArk V2 "Standard" compression (BD2 + LPC, 8 LPC parameters)
#define COMPRESSION_v2Max         7       // sfArk V2 "Max" compression (BD2, 3, 4 + LPC with 128(?) LPC parameters)
#define COMPRESSION_v2Custom      8       // sfArk V2 "Custom" compression (user choices)
#define COMPRESSION_NotV1         254     // Returned by ReadV1Header() for non-V1 files
#define COMPRESSION_Error         255     // Returned by ReadV1Header() for unrecognised compression type

// ----------------------



// ------ Macros -------
#define BIT_SIZEOF(x)   (sizeof(x) * 8)
#define AWORD_BITS      BIT_SIZEOF(AWORD)	// Number of bits in Audio Word

#define ABS(x)          ( ((x) > 0) ? (x) : -(x))

#define MIN(x, y)       ( ( (x) < (y) )?  (x) : (y) )
#define MAX(x, y)       ( ( (x) > (y) )?  (x) : (y) )
#define NZSIGN(w)       ((w) != 0)          // Used to add 1 for SignBit if value is non-zero
#define LOWBITS(w, n)   ( (w) & (1 << (n))-1 )


#if 0   // Safer methods (use in case of compile compatibility problems, etc)...
#define SIGN(x)      ( ((x) >= 0) ? 0 : 1)
#define QUICKABS(x)  ABS(x)
#define QUICKABS2(x) ( ABS(x) - SIGN(x) )

#else
// Quick "absolute value" for (much faster than using conditional) ...
// if x is positive QUICKABS2(x) = abs(x) ... but...
// if x is negative QUICKABS2(x) = abs(x)-1 -- wrong! but suits many of our purposes here!
#define SIGN(x)      ( (((x)) >> (BIT_SIZEOF(x)-1)) & 1 )				// Fast!  Returns 0 for positive, 1 for negative
#define QUICKABS2(x) ( (x) ^ ( (x) >> (BIT_SIZEOF(x)-1) ) )			// Fast version (inaccurate for negative value)
#define QUICKABS(x)  ( QUICKABS2(x) + SIGN(x) )									// Correct ABS value

#endif

// Fast division using Shift for Unsigned numbers (or where innacuracy isn't a problem)
// Innacurate on negative odd numbers, e.g. NSDIV(-100, 1) gives -51 instead of -50.  (Use SDIV instead).
#define NSDIV(x, y)  (  (x) >> (y)  )

// Fast division using Shift for Signed numbers
#define SDIV(x, y)      ( ((x) >= 0)? (x) >> (y) : -((-(x)) >> (y)) )


// ------- Prototypes -------

// sfArkLib_Diff...
extern long		BufSum(const AWORD *buf, USHORT bufsize);
extern void		UnBufDif2(AWORD *OutBuf, const AWORD *InBuf, USHORT bufsize, AWORD *prev);
extern void		UnBufDif4(AWORD *OutBuf, const AWORD *InBuf, USHORT bufsize, AWORD *prev);
extern void		UnBufDif3(AWORD *OutBuf, const AWORD *InBuf, USHORT bufsize, AWORD *prev);
extern void   UnBufShift(AWORD *Buf, USHORT SizeOfBuf, short *Shifts);

// sfArkLib_Crunch...
extern long		UnCrunchWin(AWORD *Buf, USHORT BufSize, USHORT WinSize);
extern void		BioDecompInit(void);
extern void		BioDecompEnd(void);
extern BIOWORD BioRead(int NumberOfBits);
extern bool		BioReadFlag(void);
extern long		BioReadBuf(BYTE *Buf, long BufSize);
extern AWORD	InputDiff(AWORD PrevValue);
extern short	GetNBits(short w);

// sfArkLib_File (or supplied by SDL.cpp)...
extern void		OpenOutputFile(const char *FileName);
extern void		OpenInputFile(const char *FileName);
extern int		ReadInputFile(BYTE *Buf, int NumberOfBytesToRead);
extern int		WriteOutputFile(const BYTE *Buf, int NumberOfBytesToWrite);
extern bool		SetInputFilePosition(int NewPos);
extern bool		SetOutputFilePosition(int NewPos);
extern void		CloseInputFile(void);
extern void		CloseOutputFile(void);

// sfArkLib_LPC...
extern void		LPCinit(void);
extern long		UnLPC(AWORD *OutBuf, AWORD *InBuf, short bufsize, short nc, ULONG *Flags);

// sfArkLib_Zip...
extern ULONG	UnMemcomp(const BYTE *InBuf, int InBytes, BYTE *OutBuf, int OutBufLen);
