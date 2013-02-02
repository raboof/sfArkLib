//sfArkLib Coding
//Copyright 1998-2000 Andy Inman, andyi@melodymachine.com

// V2.11	Initial release (with SDL 0.07)
// V2.12	Bug fix in UnMemcomp (OutBytes was uninitialised causing crash for some files)
//				Used for SDL 0.08.
// V2.13	Bug fix, previously file written was corrupt due to wrong buffer pointers
// V2.14	Now using UpdateProgress function (supplied by application)
// V2.15	Now using GetLicenseAgreement & DisplayNotes functions (app-supplied)

// CLIB headers...
#include	<string.h>
#include	<stdio.h>
#include	<stdlib.h>

// sfArk specific headers...
#define		SFARKLIB_GLOBAL
#include	<wcc.h>
#include	<zlib.h>

// Set to 1 for special debug mode (needs enabled version of compressor)
#define		DB_BLOCKCHECK 0

// FileSection equates...
#define		RIFF_HEADER		0
#define		RIFF_INFO     1
#define		INFO_LIST     2
#define		SDTA_LIST     3
#define		AUDIO_START   4
#define		AUDIO         5
#define		PRE_AUDIO     6
#define		POST_AUDIO    7
#define		NON_AUDIO     8
#define		FINISHED      9

// Misc equates...
#define		OPTWINSIZE		32									// Default window size used by CrunchWin()
#define		ZBUF_SIZE     (256 * 1024)				// Size of buffer used for MemComp (do not change!)
#define		NSHIFTS	(MAX_BUFSIZE / SHIFTWIN)	// Max number of shift values per block (MaxBuf/SHIFTWIN = 4096/64)
#define		MAX_DIFF_LOOPS	20								// Max number of BufDif loops

// --- Version 2 File Header Structure ---

#define	HDR_NAME_LEN	5
#define	HDR_VERS_LEN	5

typedef struct
/*
 NB: For compatibilty with sfArk V1, we must store "sfArk" at offset 27 (base 1) in
 5 characters and the compression method as one byte immediately afterwards at offset 32.
 This will allow sfArk V1 to recoginse this as a .sfArk file (though not decompress it!)
*/
	{
	ULONG			Flags;										// 0-3		Bits 0 & 1 used to indicate presence of Notes and License files
	ULONG			OriginalSize;							// 4-7		Uncompressed file size
	ULONG			CompressedSize;						// 8-11		Compressed file size (including header)
	ULONG			FileCheck;								// 12-15	File Checksum
	ULONG			HdrCheck;									// 16-19	Header Checksum
	BYTE			ProgVersionNeeded;				// 20			SfArk version needed to unpack this file (20 = Version 2.0x, etc.)
	char			ProgVersion[HDR_NAME_LEN];// 21-25	Version string (nn.nn) that created this file (NOT terminated)
	char			ProgName[HDR_VERS_LEN];		// 26-30	Program name 'sfArk' (not terminated)
	BYTE			CompMethod;								// 31			Compression Method
	USHORT		FileType;									// 32-33	Currently always 0 (for SF2)
	ULONG			AudioStart;								// 34-37	Position in original file of start of audio data
	ULONG			PostAudioStart;						// 38-41	Position in original file of start any data after audio data (e.g. SF2 parameters)
	char			FileName[MAX_FILENAME];		// 42-297	Original filename, no path (stored variable length, null terminated)
} V2_FILEHEADER;

// Offsets used to correct structure aligmnet problem with MSVCC...
#define V2_FILEHEADER_SIZE	298			// *Actual* size of header in file, not same as sizeof(V2_FILEHEADER)
#define V2_FILEHEADER_PART1	34					// Length of first part
#define V2_FILEHEADER_ALIGN	2						// Length of padding needed to align second part
#define V2_FILEHEADER_HDRCHECK	16			// Offset of HeaderCheck

// FileHeader Flags...
#define	FLAGS_Notes			(1 << 0)			// Notes file included
#define	FLAGS_License		(1 << 1)			// License file included

const char	LicenseExt[] = ".license.txt";		// File extension for license file
const char	NotesExt[] = ".txt";							// File extension for notes file

// Data per block, passed to ProcessNextBlock()
	typedef	struct
	{
	V2_FILEHEADER	FileHeader;								// FileHeader structre
	int				FileSection;									// Track current "file section"

	int				ReadSize;											// Number of words to read per block
	int				MaxLoops;											// Max loops for reduction with BufDiff2/3
	int				MaxBD4Loops;									// Max loops for reduction with BufDiff4
	int				nc;														// Number of LPC parameters
	int				WinSize;											// Window size for CrunchWin

	AWORD			*SrcBuf;											// Address of source buffer
	AWORD			*DstBuf;											// Address of destination buffer

	ULONG			TotBytesWritten;							// Total bytes written in file
	ULONG			FileCheck;										// File checksum (accumulated)
	AWORD			PrevIn[MAX_DIFF_LOOPS];				// Previous values (per loop)

	USHORT		PrevEncodeCount;							// Previous number of loops used
	USHORT		BD4PrevEncodeCount;						// Previous number of loops used for BD4
	short			PrevShift;										// Previous Shift value
	short			PrevUsedShift;								// Previously used (non zero) Shift value
	} BLOCK_DATA;

// Messages...
const char	CorruptedMsg[]	= "- This file appears to be corrupted.";
const char	UpgradeMsg[]		= "Please see Help/About for information on how to obtain an update.";

// ==============================================================
USHORT	GetsfArkLibVersion(void)
{
	return (ProgVersionMaj * 10) + ProgVersionMin/10;
}
// ==============================================================

char *ChangeFileExt(char *OutFileName, const char *NewExt, int OutFileNameSize)
{
	int n = strlen(OutFileName);
	char *p;

	for (p = OutFileName+n; *p != '.'; p--)
	{
		if (*p == '\\'  ||  p <= OutFileName)	// No extension found?
		{
			p = OutFileName + n;
			break;	
		}
	}

	n = p - OutFileName;	// Length of filename without extension
	strncpy(p, NewExt, OutFileNameSize-1 - n);
	return OutFileName;
}
// ==============================================================

// Read the File Header....
bool	ReadHeader(V2_FILEHEADER *FileHeader)
{
  USHORT	HeaderLen;
	int			i;

	BYTE	HdrBuf[V2_FILEHEADER_SIZE];
	BYTE	*FileHeaderp = (BYTE *) FileHeader;

  // Read maximum size Header from file....
  SetInputFilePosition(SourceFileOffset);
  ReadInputFile(HdrBuf, V2_FILEHEADER_SIZE);
  if (Aborted) return false;

	//Copy from HdrBuf to FileHeader, handling alignment issues (thanks to MSVC)...
	for (i = 0; i < V2_FILEHEADER_PART1; i++)	FileHeaderp[i] = HdrBuf[i];
	for ( ; i < V2_FILEHEADER_SIZE; i++)	FileHeaderp[i+V2_FILEHEADER_ALIGN] = HdrBuf[i];

	// Find actual length of header, and reposition file pointer...
  HeaderLen = V2_FILEHEADER_SIZE - sizeof(FileHeader->FileName) + strlen(FileHeader->FileName) +1;
  SetInputFilePosition(SourceFileOffset + HeaderLen);
  if (Aborted) return false;

	// Get CreatedBy program name and version number...
	char	CreatedByProg[HDR_NAME_LEN +1];
	char	CreatedByVersion[HDR_VERS_LEN +1];

	strncpy(CreatedByProg, FileHeader->ProgName, HDR_NAME_LEN);					// Copy program name
	CreatedByProg[HDR_NAME_LEN] = 0;																		// Terminate string
	strncpy(CreatedByVersion, FileHeader->ProgVersion, HDR_VERS_LEN);		// Copy version string
	CreatedByVersion[HDR_VERS_LEN] = 0;																	// Terminate string

	// Check that it looks like a valid sfArk file
	if (strcmp(CreatedByProg, "sfArk") != 0)														// Not "sfArk"?
	{
		sprintf(MsgTxt, "This does not appear to be a sfArk file!%s", CorruptedMsg);
    msg(MsgTxt, MSG_PopUp);
    Aborted = true;	GlobalErrorFlag = SFARKLIB_ERR_SIGNATURE;
    return false;
	}

	// Check for sfArk V1 file (not currently supported by sfArkLib)
  if (FileHeader->CompMethod < COMPRESSION_v2)
	{
		sprintf(MsgTxt, "This file was created with %s V1.x so cannot be uncompressed with %s.  Use %s instead.", 
							CreatedByProg, ProgName, CreatedByProg);
    msg(MsgTxt, MSG_PopUp);
    Aborted = true;	GlobalErrorFlag = SFARKLIB_ERR_INCOMPATIBLE;
    return false;
	}

	// Check the Header CheckSum...
	memset(HdrBuf+V2_FILEHEADER_HDRCHECK, 0, sizeof(ULONG));						// Clear original check from buffer
  ULONG CalcHdrCheck = adler32(0, HdrBuf, HeaderLen);									// Recalculate checksum

  if (CalcHdrCheck != FileHeader->HdrCheck) 													// Compare to given value
	{
		sprintf(MsgTxt, "File Header fails checksum!%s", CorruptedMsg);
    msg(MsgTxt, MSG_PopUp);
    Aborted = true;	GlobalErrorFlag = SFARKLIB_ERR_HEADERCHECK;
    return false;
	}

	// Check for compatible version...
  if (FileHeader->ProgVersionNeeded > ProgVersionMaj)
	{
		sprintf(MsgTxt, "You need %s version %2.1f (or higher) to decompress this file (your version is %s) %s", 
							ProgName, (float)FileHeader->ProgVersionNeeded/10, ProgVersion, UpgradeMsg);
    msg(MsgTxt, MSG_PopUp);
    Aborted = true;	GlobalErrorFlag = SFARKLIB_ERR_INCOMPATIBLE;
    return false;
	}

	// Warn if file was created by a newer version than this version...
	float fProgVersion = (float) atof(ProgVersion);
	float fCreatedByVersion = (float) atof(CreatedByVersion);
	if (fCreatedByVersion > fProgVersion)
	{
		sprintf(MsgTxt, "This file was created with %s %s.  Your version of %s (%s) can uncompress this file, "
						"but you might like to obtain the latest version.  %s",
						CreatedByProg, CreatedByVersion, ProgName, ProgVersion, UpgradeMsg);
		msg(MsgTxt, MSG_PopUp);
	}
	return true;
}

// ==============================================================
bool InvalidEncodeCount(int EncodeCount, int MaxLoops)
{
	if (EncodeCount < 0  ||  EncodeCount > MaxLoops)	// EncodeCount out of range?
	{
		sprintf(MsgTxt, "ERROR - Invalid EncodeCount (apparently %d) %s", EncodeCount, CorruptedMsg);
		msg(MsgTxt, MSG_PopUp);
		Aborted = true; GlobalErrorFlag = SFARKLIB_ERR_CORRUPT;
		return true;
	}
	else
		return false;
}

// ==============================================================
void	DecompressTurbo(BLOCK_DATA *Blk, USHORT NumWords)
{
  int EncodeCount = InputDiff(Blk->PrevEncodeCount);
  if (InvalidEncodeCount(EncodeCount, Blk->MaxLoops))  return;
  Blk->PrevEncodeCount = EncodeCount;

  int UnCrunchResult = UnCrunchWin(Blk->SrcBuf, NumWords, 8*OPTWINSIZE);
  if (UnCrunchResult < 0)
	{
		sprintf(MsgTxt, "ERROR - UnCrunchWin returned: %d %s", UnCrunchResult, CorruptedMsg);
		msg(MsgTxt, MSG_PopUp);
    Aborted = true; GlobalErrorFlag = SFARKLIB_ERR_CORRUPT;
    return;
  }

	for (int j = EncodeCount-1; j >= 0; j--)
	{
    if (j == 0)  Blk->FileCheck = (Blk->FileCheck << 1) + BufSum(Blk->SrcBuf, NumWords);
    UnBufDif2(Blk->DstBuf, Blk->SrcBuf, NumWords, &(Blk->PrevIn[j]));
    AWORD *SwapBuf = Blk->SrcBuf;  Blk->SrcBuf = Blk->DstBuf; Blk->DstBuf = SwapBuf;
  }

}

// ==============================================================
bool CheckShift(short *ShiftVal, USHORT NumWords, short *PrevShift, short *PrevUsedShift)
// Here we look to see if the current buffer has been rightshifted
// There is a flag for the whole buffer to show if any shifted data exists,
// and if so there further flags to show if the Shift value changes within each sub block 
// of ShiftWin words.
// For any changes, then follows the sub-block number where change occurs, then the new
// ShiftValue.  The latter is given either as absolute value (if shift was previously
// non-zero) or relative change from previously used value (if shift was previously zero)
// For non-zero shift value, we then need to leftshift the data by that number of bits.
{
	#define		ShiftWin	64		// Size of window for Shift

  bool UsingShift = BioReadFlag();		// Read flag to see if using any shift
  if (UsingShift)											// and if so...
	{
		int MaxShifts = (NumWords+ShiftWin-1) / ShiftWin;		// Number of ShiftWin sized sub-blocks in this block
    int ChangePos = 0;																	// Init. position of last change

		int p = 0;
    while (BioReadFlag())		// Read flag to see if there is a (further) change of shift value
		{
      // Read position of new shift value...
      int nb = GetNBits(MaxShifts - ChangePos -1);	// number of possible bits for ChangePos
      ChangePos = BioRead(nb) + ChangePos;          // Get position of next change of shift value

      // Read value of new shift...
     	short	NewShift;
      if (*PrevShift == 0)													// If previous shift was 0
			{
        NewShift = InputDiff(*PrevUsedShift);				// Get new shift as diff from last used shift
        *PrevUsedShift = NewShift;									// Update PrevUsedShift
			}
      else                                          // Else
        NewShift = InputDiff(0);                    // Get new shift as difference from 0

      // Update all ShiftVal[] data prior to change...
			if (ChangePos > MaxShifts)										// Corrupt data?
			{
				sprintf(MsgTxt, "ERROR - Invalid Shift ChangePos (apparently %d) %s", ChangePos, CorruptedMsg);
				msg(MsgTxt, MSG_PopUp);
				Aborted = true; GlobalErrorFlag = SFARKLIB_ERR_CORRUPT;
				return false;
			}
			
      for ( ; p < ChangePos; p++) ShiftVal[p] = *PrevShift;
      *PrevShift = NewShift;                        // Update prev shift
		}

		for ( ; p < MaxShifts; p++)	ShiftVal[p] = *PrevShift;		// Fill array to end with final shift value

	}
	return UsingShift;
}

// ==============================================================

void	DecompressFast(BLOCK_DATA *Blk, USHORT NumWords)
{
	int			i, EncodeCount;
	short		ShiftVal[NSHIFTS];						// Shift values (one per SHIFTWIN words)
	USHORT	Method[MAX_DIFF_LOOPS];				// Block processing methods used per iteration

	#if	DB_BLOCKCHECK											// If debug mode block check enabled
		ULONG BlockCheck = BioRead(16);				// Read block check bits
	#endif

	bool UsingShift = CheckShift(ShiftVal, NumWords, &Blk->PrevShift, &Blk->PrevUsedShift);
	bool UsingBD4 = BioReadFlag();			// See if using BD4

  if (UsingBD4)
	{
    EncodeCount = InputDiff(Blk->BD4PrevEncodeCount);
    if (InvalidEncodeCount(EncodeCount, Blk->MaxBD4Loops))  return;
    Blk->BD4PrevEncodeCount = EncodeCount;
	}
  else	// Using BD2/3
	{
    EncodeCount = InputDiff(Blk->PrevEncodeCount);
    if (InvalidEncodeCount(EncodeCount, Blk->MaxLoops))  return;
    Blk->PrevEncodeCount = EncodeCount;

    for(i = 0; i < EncodeCount; i++)
      Method[i] = BioReadFlag();			// Read flags for BD2/3
  }

	// If using LPC, check for and read flags...
	ULONG LPCflags;
	bool UsingLPC = (Blk->FileHeader.CompMethod != COMPRESSION_v2Fast);
	if (UsingLPC)
	{
		if (BioReadFlag())															// Any flags?
		  LPCflags = BioRead(16) | (BioRead(16) << 16);	// Then read them (32 bits)
		else																						// else
			LPCflags = 0;																	// Clear flags
	}

  // Read the file and unpack the bitstream into buffer at Buf1p...
  if (int UnCrunchResult = UnCrunchWin(Blk->SrcBuf, NumWords, OPTWINSIZE) < 0)		// failed?
	{
		sprintf(MsgTxt, "ERROR - UnCrunchWin returned: %d %s", UnCrunchResult, CorruptedMsg);
		msg(MsgTxt, MSG_PopUp);
    Aborted = true; GlobalErrorFlag = SFARKLIB_ERR_CORRUPT;
    return;
  }

  if (UsingLPC)
	{
    UnLPC(Blk->DstBuf, Blk->SrcBuf, NumWords, Blk->nc, &LPCflags);
		AWORD *SwapBuf = Blk->SrcBuf;  Blk->SrcBuf = Blk->DstBuf; Blk->DstBuf = SwapBuf;
	}

	if (UsingBD4)
	{
  	for (i = EncodeCount-1; i >= 0; i--)
		{
			UnBufDif4(Blk->DstBuf, Blk->SrcBuf, NumWords, &(Blk->PrevIn[i]));
			AWORD *SwapBuf = Blk->SrcBuf;  Blk->SrcBuf = Blk->DstBuf; Blk->DstBuf = SwapBuf;
		}
	}
	else
	{
  	for (i = EncodeCount-1; i >= 0; i--)
		{
			switch (Method[i])
			{
				case 0:
				UnBufDif2(Blk->DstBuf, Blk->SrcBuf, NumWords, &(Blk->PrevIn[i]));
				break;

				case 1:
				UnBufDif3(Blk->DstBuf, Blk->SrcBuf, NumWords, &(Blk->PrevIn[i]));
				break;
			}
			AWORD *SwapBuf = Blk->SrcBuf;  Blk->SrcBuf = Blk->DstBuf; Blk->DstBuf = SwapBuf;
		}
	}

  if (UsingShift)  UnBufShift(Blk->SrcBuf, NumWords, ShiftVal);

	#if	DB_BLOCKCHECK											// If debug mode block check enabled
		ULONG CalcBlockCheck = adler32(0, (const BYTE *) Blk->SrcBuf, 2*NumWords) & 0xffff;
		//ULONG CalcBlockCheck = Blk->FileCheck & 0xffff;
		printf("Audio Block Checks Read: %ld, Calc %ld  Length=%d\n", BlockCheck, CalcBlockCheck, 2*NumWords);
		//getc(stdin);
		if (BlockCheck != CalcBlockCheck)			// Compare to calculated cheksum
		{
      printf("*** Audio Block check FAIL\n");
			getc(stdin);
		}
		//else
    //  printf("Audio Block check Ok\n");
	#endif

  Blk->FileCheck = 2 * Blk->FileCheck + BufSum(Blk->SrcBuf, NumWords);
}
// ==============================================================
void	ProcessNextBlock(BLOCK_DATA *Blk)
{
	int				TotBytesRead = 0;							// Total bytes read in file
	int				NumWords;											//

	ULONG			n, m;													// NB: Must be 32-bit integer

	#define		AWBYTES	(sizeof(AWORD))
	BYTE *zSrcBuf = (BYTE *) Blk->SrcBuf;
	BYTE *zDstBuf = (BYTE *) Blk->DstBuf;

  switch	(Blk->FileSection)
	{
		case AUDIO:
		{
			NumWords = Blk->ReadSize;						// Number of words we will read in this block
			n = NumWords * AWBYTES;							// ... and number of bytes

			if (Blk->TotBytesWritten + n >= Blk->FileHeader.PostAudioStart)			// Short block? (near end of file)
			{
				n = Blk->FileHeader.PostAudioStart - Blk->TotBytesWritten;				// Get exact length in bytes
				NumWords = n / AWBYTES;																						// ... and words
				Blk->FileSection = POST_AUDIO;		// End of Audio -- PostAudio section is next
			}
    
			//printf("AUDIO, read %d bytes\n", n);

      if (Blk->FileHeader.CompMethod == COMPRESSION_v2Turbo)						// If using Turbo compression
        DecompressTurbo(Blk, NumWords);									// Decompress
      else																															// For all other methods
        DecompressFast(Blk, NumWords);										// Decompress

			//printf("B4 WriteOutputFile: %ld\n", adler32(0, (const BYTE *) Blk->SrcBuf, n) & 0xffff);
			WriteOutputFile((const BYTE *)Blk->SrcBuf, n);										// Write to output file
      Blk->TotBytesWritten += n;																				// Accumulate total bytes written
			break;
		}
	
		case PRE_AUDIO: case POST_AUDIO: case NON_AUDIO:
		{

	    BioReadBuf((BYTE *)&n, sizeof(n));																// Read length of block from file

			if (n < 0  ||  n > ZBUF_SIZE)																			// Check for valid block length
			{
				sprintf(MsgTxt, "ERROR - Invalid length for Non-audio Block (apparently %d bytes) %s", n, CorruptedMsg);
				msg(MsgTxt, MSG_PopUp);
				Aborted = true; GlobalErrorFlag = SFARKLIB_ERR_CORRUPT;
				return;
			}

	    BioReadBuf(zSrcBuf, n);																							// Read the block
		  m = UnMemcomp(zSrcBuf, n, zDstBuf, ZBUF_SIZE);											// Uncompress
			if (Aborted == false  &&  m <= ZBUF_SIZE)														// Uncompressed ok & size is valid?
			{
				Blk->FileCheck = adler32(Blk->FileCheck, zDstBuf, m);	   					// Accumulate checksum
				WriteOutputFile(zDstBuf, m);																			// and write to output file
				Blk->TotBytesWritten += m;																				// Accumulate byte count
			}

			#if	DB_BLOCKCHECK											// If debug mode block check enabled
			ULONG BlockCheck = BioRead(16);				// Read block check bits
			ULONG CalcBlockCheck = adler32(0, zDstBuf, m) & 0xFFFF;
			printf("NonAudio Block Checks Read: %ld, Calc %ld Length=%d\n", BlockCheck, CalcBlockCheck, m);
			if (BlockCheck != CalcBlockCheck)			// Compare to calculated cheksum
			{
				printf("*** NonAudio Block check FAIL\n");
				getc(stdin);
			}
			else
				printf("NonAudio Block check Ok\n");
			#endif

			//printf("PRE/POST AUDIO, read %d bytes, writing %d bytes...", n, m); getc(stdin);

      if (Blk->TotBytesWritten >= Blk->FileHeader.OriginalSize)						// Have we finished the file?
				Blk->FileSection = FINISHED;
			else if (Blk->TotBytesWritten >= Blk->FileHeader.AudioStart)				// Else, have we finished Pre-Audio?
	  		Blk->FileSection = AUDIO;																					// .. then next section is Audio
			else
			{
				// For SF2, we don't expect anything after post-audio section so...
				sprintf(MsgTxt, "ERROR - Unexpected file section %s", CorruptedMsg);
				msg(MsgTxt, MSG_PopUp);
				Aborted = true; GlobalErrorFlag = SFARKLIB_ERR_CORRUPT;
				return;
			}

			break;
		}
	}


	// sprintf(MsgTxt, "BytesWritten: %d of %d", Blk->TotBytesWritten, Blk->FileHeader.OriginalSize);
	//  msg('Bytes Written: ' & TotBytesWritten & ' of ' & TotOutBytes);

}

// ==============================================================
bool	EndProcess(bool SuccessFlag)
{
	CloseInputFile();
	CloseOutputFile();
  Aborted = (SuccessFlag == false);
	return SuccessFlag;
}

// ==============================================================
// Extract License & Notes files
// These are stored as 4-bytes length, followed by length-bytes of compressed data
bool	ExtractTextFile(BLOCK_DATA *Blk, ULONG FileType)
{
		ULONG n, m;
		BYTE *zSrcBuf = (BYTE *) Blk->SrcBuf;
		BYTE *zDstBuf = (BYTE *) Blk->DstBuf;

		const char *FileExt;
		if (FileType == FLAGS_License)
			FileExt = LicenseExt;
		else if (FileType == FLAGS_Notes)
			FileExt = NotesExt;
		else
			return false;

		// NB:Can't use BioReadBuf... aligment problems? Yet it works for ProcessNextBlock() !!??
		// Ok, can use ReadInputFile here cause everythjing is whole no. of bytes...

		//BioReadBuf((BYTE *)&n, sizeof(n));					// Read length of block from file
	  ReadInputFile((BYTE *)&n, sizeof(n));					// Read length of block from file

		if (n <= 0  ||  n > ZBUF_SIZE)								// Check for valid block length
		{
			sprintf(MsgTxt, "ERROR - Invalid length for %s file (apparently %d bytes) %s", FileExt, n, CorruptedMsg);
			msg(MsgTxt, MSG_PopUp);
			GlobalErrorFlag = SFARKLIB_ERR_CORRUPT;
			return false;
		}

		//BioReadBuf(zSrcBuf, n);																					// Read the block
	  ReadInputFile((BYTE *)zSrcBuf, n);																// Read the block
		m = UnMemcomp(zSrcBuf, n, zDstBuf, ZBUF_SIZE);										// Uncompress
		Blk->FileCheck = adler32(Blk->FileCheck, zDstBuf, m);	   					// Accumulate checksum
		if (Aborted == true  ||  m > ZBUF_SIZE)														// Uncompressed ok & size is valid?
			return false;

		// Write file - Use original file name plus specified extension for OutFileName...
		char OutFileName[MAX_FILENAME];
		strncpy(OutFileName, Blk->FileHeader.FileName, sizeof(OutFileName));	// copy output filename
		ChangeFileExt(OutFileName, FileExt, sizeof(OutFileName));
		OpenOutputFile(OutFileName);	// Create license file
		WriteOutputFile(zDstBuf, m);																			// and write to output file
		CloseOutputFile();

		if (FileType == FLAGS_License)
		{
			if (GetLicenseAgreement((const char *)zDstBuf) == false)
			{
				GlobalErrorFlag = SFARKLIB_ERR_LICENSE;
				return EndProcess(false);
			}
		}
		else if (FileType == FLAGS_Notes)
			DisplayNotes(OutFileName);
		else
			;

		return true;
}

// ==============================================================

bool	Decode(const char *InFileName, const char *ReqOutFileName)
{
	char			OutFileName[MAX_FILEPATH];		// File name for current output file
	int				NumLoops;											// Number of loops before screen update etc.

	BLOCK_DATA	Blk;
	memset(&Blk, 0, sizeof(Blk));

	V2_FILEHEADER	*FileHeader = &Blk.FileHeader;

	// NB: We keep 2 buffers with pointers in Blk->SrcBuf and Blk->DstBuf
  // Generally we process from SrcBuf to DstBuf then swap the pointers,
	// so that current data is always at SrcBuf

	BYTE			Zbuf1[ZBUF_SIZE];							// Buffer1
	BYTE			Zbuf2[ZBUF_SIZE];							// Buffer2

	Blk.SrcBuf = (AWORD *) Zbuf1;					// Point to Zbuf1
	Blk.DstBuf = (AWORD *) Zbuf2;					// Point to Zbuf2

	// Initialisation...
  BioDecompInit();						// Initialise bit i/o
  LPCinit();									// Init LPC
	Aborted = false;
	GlobalErrorFlag = SFARKLIB_SUCCESS;

	// Open input (.sfArk) file and read the header...
	OpenInputFile(InFileName);
	ReadHeader(FileHeader);
	if (Aborted)  return EndProcess(false);	// Something went wrong?

	if (ReqOutFileName == NULL)							// If no output filename requested
		ReqOutFileName = FileHeader->FileName;

	if ((FileHeader->Flags & FLAGS_License) != 0)		// License file exists?
	{
		if (ExtractTextFile(&Blk, FLAGS_License) == false)
			return EndProcess(false);
	}

	if ((FileHeader->Flags & FLAGS_Notes) != 0)		// Notes file exists?
	{
		if (ExtractTextFile(&Blk, FLAGS_Notes) == false)
			return EndProcess(false);
	}


/*
	{
		ULONG n, m;
		BYTE *zSrcBuf = (BYTE *) Blk.SrcBuf;
		BYTE *zDstBuf = (BYTE *) Blk.DstBuf;

		// NB:Can't use BioReadBuf... aligment problems?????
		//BioReadBuf((BYTE *)&n, sizeof(n));					// Read length of block from file
	  ReadInputFile((BYTE *)&n, sizeof(n));					// Read length of block from file

		if (n <= 0  ||  n > ZBUF_SIZE)							// Check for valid block length
		{
			sprintf(MsgTxt, "ERROR - Invalid length for License File (apparently %d bytes) %s", n, CorruptedMsg);
			msg(MsgTxt, MSG_PopUp);
			GlobalErrorFlag = SFARKLIB_ERR_CORRUPT;
			return EndProcess(false);
		}

		//BioReadBuf(zSrcBuf, n);																						// Read the block
	  ReadInputFile((BYTE *)zSrcBuf, n);																	// Read the block
		m = UnMemcomp(zSrcBuf, n, zDstBuf, ZBUF_SIZE);									// Uncompress

		if (Aborted == false  &&  m <= ZBUF_SIZE)														// Uncompressed ok & size is valid?
		{
			// Use original file extension for OutFileName...
			strncpy(OutFileName, ReqOutFileName, sizeof(OutFileName));	// copy output filename
			ChangeFileExt(OutFileName, LicenseExt, sizeof(OutFileName));
			OpenOutputFile(OutFileName);	// Create license file

			Blk.FileCheck = adler32(Blk.FileCheck, zDstBuf, m);	   					// Accumulate checksum
			WriteOutputFile(zDstBuf, m);																			// and write to output file
			CloseOutputFile();
			if (GetLicenseAgreement((const char *)zDstBuf) == false)
			{
				GlobalErrorFlag = SFARKLIB_ERR_CORRUPT;
				return EndProcess(false);
			}
		}
	}
*/

	// Old Clarion source, yet to be converted to C...
/*
  OutFileName = BuildFileName(GetPathName(OutFileName), GetFileNameNoExt(OutFileName), GetFileExt(Hdr:FileName))

  Buf1P = address(Buf1)
  Buf2P = address(Buf2)

  ! Do License file if required...
  if band(Hdr:Flags, FLAGS:License)
    clear(Zbuf2)
    ReadInputFile(address(n#), size(n#))            ! Read compressed size
    ReadInputFile(Buf1P, n#)                        ! Read compressed data
    m# = UnMemComp(Buf1P, n#, Buf2P, ZBUF_SIZE)     ! Uncompress

    FileCheck = Adler32(FileCheck, Buf2P, m#)
    OpenOutputFile(BuildFileName(GetPathName(OutFileName), GetFileNameNoExt(OutFileName), LicenseExt), false)
    WriteOutputFile(Buf2P, m#)
!!!    if ~GetAgreementToLicenseB(clip(Zbuf2)) then goto CloseAndExit.
    CloseOutputFile()
  .
  if band(Hdr:Flags, FLAGS:Notes)
    clear(Zbuf2)
    ReadInputFile(address(n#), size(n#))            ! Read compressed size
    ReadInputFile(Buf1P, n#)                        ! Read compressed data
    m# = UnMemComp(Buf1P, n#, Buf2P, ZBUF_SIZE)     ! Uncompress

    FileCheck = Adler32(FileCheck, Buf2P, m#)
    OpenOutputFile(BuildFileName(GetPathName(OutFileName), GetFileNameNoExt(OutFileName), NotesExt), false)
    WriteOutputFile(Buf2P, m#)
    CloseOutputFile()

    ! Open notes window on a new thread, or update it if already open...
!!!    start(ShowNotes, , GetFileName(OutFileName) & '|' & clip(ZBuf2), 0{PROP:Xpos}+60, 0{PROP:Ypos}-60)
!!!    post(EVENT:GainFocus,,NotesThread)
!!!    yield()                                         ! Allow notes window to display
  .
********/

  // Use original file extension for OutFileName...
  strncpy(OutFileName, ReqOutFileName, sizeof(OutFileName));			// Copy output filename
  OpenOutputFile(OutFileName);																		// Create the main output file...

	// Set the decompression parameters...
	switch (FileHeader->CompMethod)								// Depending on compression method that was used...
	{
		case COMPRESSION_v2Max:
		{
			//printf("Compression: Max\n");
			Blk.ReadSize = 4096;
			Blk.MaxLoops = 3;
			Blk.MaxBD4Loops = 5;
			Blk.nc = 128;
			Blk.WinSize = OPTWINSIZE;
			NumLoops = 2 * 4096 / Blk.ReadSize;
			break;
		}
		case COMPRESSION_v2Standard:
		{
			//printf("Compression: Standard\n");
			Blk.MaxLoops = 3;
			Blk.MaxBD4Loops = 3;
			Blk.ReadSize = 4096;
			Blk.nc = 8;
			Blk.WinSize = OPTWINSIZE;
			NumLoops = 50 * 4096 / Blk.ReadSize;
			break;
		}
		case COMPRESSION_v2Fast:
		{
			//printf("Compression: Fast\n");
			Blk.MaxLoops = 20;
			Blk.MaxBD4Loops = 20;
			Blk.ReadSize = 1024;
			Blk.WinSize = OPTWINSIZE;
			NumLoops = 300 * 4096 / Blk.ReadSize;
			break;
		}
		case COMPRESSION_v2Turbo:
		{
			//printf("Compression: Turbo\n");
			Blk.MaxLoops = 3;
			Blk.MaxBD4Loops = 0;
			Blk.ReadSize = 4096;
			Blk.WinSize = OPTWINSIZE << 3;
			NumLoops = 400 * 4096 / Blk.ReadSize;
			break;
		}
		default:
		{
			sprintf(MsgTxt, "Unknown Compression Method: %d%s", FileHeader->CompMethod, CorruptedMsg);
			msg(MsgTxt, MSG_PopUp);
			return EndProcess(false);
		}
	}

	// Process the main file...
  Blk.FileSection = PRE_AUDIO;		// We start with pre-audio data

  ULONG ProgressUpdateInterval = Blk.FileHeader.OriginalSize / 100; // Calculate progress update
	ULONG NextProgressUpdate = ProgressUpdateInterval;
	UpdateProgress(0);

	int BlockNum = 0;
	for (Blk.FileSection = PRE_AUDIO; Blk.FileSection != FINISHED; )
	{
		for (int BlockCount = 0; BlockCount < NumLoops  &&  Blk.FileSection != FINISHED; BlockCount++)
		{
			//printf("Block: %d\n", ++BlockNum);

			ProcessNextBlock(&Blk);
			if (Blk.TotBytesWritten >= NextProgressUpdate)  
			{
				UpdateProgress(100 * Blk.TotBytesWritten / Blk.FileHeader.OriginalSize);
				NextProgressUpdate += ProgressUpdateInterval;
			}
  		if (Aborted)  return EndProcess(false);
		}
		// CheckForCancel();
		if (Aborted)  return EndProcess(false);
	}

	UpdateProgress(100);

  // Check the CheckSum...
  if (Blk.FileCheck != FileHeader->FileCheck)
	{
    sprintf(MsgTxt, "CheckSum Fail!%s",CorruptedMsg);
		msg(MsgTxt, MSG_PopUp);
		GlobalErrorFlag = SFARKLIB_ERR_FILECHECK;
		return EndProcess(false);
	}

  sprintf(MsgTxt, "Created %s, %d kb.", ReqOutFileName, Blk.TotBytesWritten/1024);
	msg(MsgTxt, 0);

	return EndProcess(true);
}

