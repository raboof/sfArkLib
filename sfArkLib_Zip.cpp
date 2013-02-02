// sfArkLib	ZLIB interface
// Copyright 1998-2000 Andy Inman, andyi@melodymachine.com

#include	"wcc.h"
#include	"zlib.h"
#include	"stdio.h"

ULONG	UnMemcomp(const BYTE *InBuf, int InBytes, BYTE *OutBuf, int OutBufLen)
{
	// Uncompress buffer using ZLIBs uncompress function...
	ULONG	OutBytes = OutBufLen;
	int Result = uncompress(OutBuf, &OutBytes, InBuf, InBytes);
  if (Result != Z_OK)				// uncompress failed?
	{
		sprintf(MsgTxt, "ZLIB uncompress failed: %d\n", Result);
    msg(MsgTxt, MSG_PopUp);
    Aborted = true;
		OutBytes = 0;
		if (Result == Z_MEM_ERROR)
			GlobalErrorFlag = SFARKLIB_ERR_MALLOC;
		else
			GlobalErrorFlag = SFARKLIB_ERR_CORRUPT;
	}

  return OutBytes;
}

