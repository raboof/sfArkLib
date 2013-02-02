// sfArkLib file i/o
// copyright 1998-2000, Andy Inman
// Contact via: http://netgenius.co.uk or http://melodymachine.com

// NB: When sfArkLib is used with SDL, these functions are replaced
// by equvalents in sdl.cpp because the linker finds those first.

#include "stdio.h"
#include "wcc.h"
#include	"zlib.h"

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include "windows.h"
#undef	WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

// Some equates just for readability...
#define	FILE_SHARE_NONE		0
#define NO_SECURITY				((LPSECURITY_ATTRIBUTES) 0)
#define NOT_OVERLAPPED		((LPOVERLAPPED) 0)
#define	NO_TEMPLATE				((HANDLE) 0)

// Static data to track current input and output file...
HANDLE	InputFileHandle = INVALID_HANDLE_VALUE;		// current input file handle
HANDLE	OutputFileHandle = INVALID_HANDLE_VALUE;	// ... output file handle
char		InFileName[256];				// current input file name
char		OutFileName[256];				// ... and output file name


// Local prototypes...
int	ChkErr(const char *message, const char *filename);

// =================================================================================

void OpenOutputFile(const char *FileName)
{
  HANDLE	fh;
  LPCTSTR	fn = FileName;
	strncpy(OutFileName, FileName, sizeof(OutFileName));

  fh = CreateFile(fn, GENERIC_WRITE, FILE_SHARE_NONE, NO_SECURITY, CREATE_ALWAYS, 
									FILE_ATTRIBUTE_NORMAL + FILE_FLAG_SEQUENTIAL_SCAN, NO_TEMPLATE);
  OutputFileHandle = fh;
  if (fh < 0)  ChkErr("create", OutFileName);
  return;
}
// =================================================================================

void OpenInputFile(const char *FileName)
{
	//printf("OpenInputFile: %s\n", FileName);
  HANDLE	fh;
  LPCTSTR	fn = FileName;

	strncpy(InFileName, FileName, sizeof(InFileName));

  fh = CreateFile(fn, GENERIC_READ, FILE_SHARE_READ, NO_SECURITY, OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL + FILE_FLAG_SEQUENTIAL_SCAN, NO_TEMPLATE);

  InputFileHandle = fh;
  if (fh == INVALID_HANDLE_VALUE)  ChkErr("open", InFileName);
  return;
}
// =================================================================================

int ReadInputFile(BYTE *Buf, int NumberOfBytesToRead)
{
	DWORD	NumberOfBytesRead;
  if (ReadFile(InputFileHandle, Buf, NumberOfBytesToRead, &NumberOfBytesRead, NOT_OVERLAPPED) == 0)
	{
    ChkErr("read from", InFileName);
    NumberOfBytesRead = 0;
	}
  return NumberOfBytesRead;
}
// =================================================================================

int WriteOutputFile(const BYTE *Buf, int NumberOfBytesToWrite)
{
	DWORD	NumberOfBytesWritten;
	//printf("WriteOutputFile: Length=%d CRC=%ld\n", NumberOfBytesToWrite, adler32(0, Buf, NumberOfBytesToWrite) & 0xffff);
  if (WriteFile(OutputFileHandle, Buf, NumberOfBytesToWrite, &NumberOfBytesWritten, NOT_OVERLAPPED) == 0)
	{
    ChkErr("write to", OutFileName);
    NumberOfBytesWritten = 0;
	}
  return NumberOfBytesWritten;
}

// =================================================================================

bool SetInputFilePosition(int NewPos)
{
  if (SetFilePointer(InputFileHandle, NewPos, 0, FILE_BEGIN) < 0)
	{
    ChkErr("SetInputFilePosition", InFileName);
		return false;
	}
  return true;
}
// =================================================================================

bool SetOutputFilePosition(int NewPos)
{
  if (SetFilePointer(OutputFileHandle, NewPos, 0, FILE_BEGIN) < 0)
	{
    ChkErr("SetOutputFilePosition", OutFileName);
		return false;
	}
  return true;
}
// =================================================================================

void CloseInputFile(void)
{
	if (InputFileHandle != INVALID_HANDLE_VALUE && CloseHandle(InputFileHandle) == 0)
		ChkErr("Close input file: ", InFileName);
	InputFileHandle = INVALID_HANDLE_VALUE;
  return;
}
// =================================================================================

void CloseOutputFile(void)
{
	if (OutputFileHandle != INVALID_HANDLE_VALUE && CloseHandle(OutputFileHandle) == 0)
		ChkErr("Close output file", OutFileName);
	OutputFileHandle = INVALID_HANDLE_VALUE;
  return;
}
// =================================================================================

int ChkErr(const char *ErrorMsg, const char *FileName)
{
	int		ErrCode;
	char	ErrDesc[MAX_MSGTEXT];

  if (~GlobalErrorFlag)										// Prevent multiple error messages
	{
    ErrCode = GetLastError();
		sprintf(ErrDesc, "ERROR %d - Failed to %s: %s", ErrCode, ErrorMsg, FileName);
    msg(ErrDesc, MSG_PopUp);
    GlobalErrorFlag = true;
    Aborted = true;
	}
  return(GlobalErrorFlag);
}


/**********************************************************************************/

