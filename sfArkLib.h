// Header for sfArkLib

// copyright 1998-2000 Andy Inman
// Contact via: http://netgenius.co.uk or http://melodymachine.com

// This file is part of sfArkLib.
//
// sfArkLib is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// sfArkLib is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with sfArkLib.  If not, see <http://www.gnu.org/licenses/>.


// Some max sizes...
#define	MAX_FILENAME	256			// Longest filename handled (or directory name)
#define	MAX_FILEPATH	1024		// Longest full path handled
#define	MAX_MSGTEXT		(MAX_FILEPATH + 1024)	// Longest message we might produce with msg()

// Flags used with msg() function...
#define		MSG_SameLine		(1 << 0)
#define		MSG_AppendLine  (1 << 1)
#define		MSG_PopUp				(1 << 2)

// Error codes...
#define SFARKLIB_SUCCESS						 0	// No error
#define	SFARKLIB_ERR_INIT						-1	// Failed to initialise
#define	SFARKLIB_ERR_MALLOC					-2	// Failed to allocate memory
#define	SFARKLIB_ERR_SIGNATURE			-3	// header does not contain "sfArk" signature
#define	SFARKLIB_ERR_HEADERCHECK		-4	// sfArk file has a corrupt header
#define	SFARKLIB_ERR_INCOMPATIBLE		-5	// sfArk file is incompatible (i.e. not sfArk V2.x)
#define	SFARKLIB_ERR_UNSUPPORTED		-6	// sfArk file uses unsupported feature
#define	SFARKLIB_ERR_CORRUPT				-7	// got invalid compressed data (file is corrupted)
#define	SFARKLIB_ERR_FILECHECK			-8	// file checksum failed (file is corrupted)
#define	SFARKLIB_ERR_FILEIO					-9	// File i/o error
#define	SFARKLIB_ERR_LICENSE				-10 // License included not agreed by user
#define	SFARKLIB_ERR_OTHER					-11 // Other error (currently unused)
																	
// Future SfArkLib versions may include additional further error codes,
// so, any other negative value is "unknown error"

// -------- Global flags and data ----------
#ifdef	SFARKLIB_GLOBAL													// Compiling sfArkLib?
	bool	Aborted;
	int		GlobalErrorFlag;
	const char *ProgName			= "sfArkLib";
	const char *ProgVersion		=	" 2.15";					// 5 characters xx.xx
	const unsigned char ProgVersionMaj = 21;			// 0-255 = V0 to V25.5xx, etc.
	const unsigned char 	ProgVersionMin = 50;		// 0-99  = Vx.x99, etc.
	char	MsgTxt[MAX_MSGTEXT];										// Text buffer for msg()
	unsigned SourceFileOffset = 0;								// Set non-zero by app for self-extraction
  
	// Application-supplied functions...
	extern void msg(const char *MessageText, int Flags);				// Message display function
  extern void UpdateProgress(int ProgressPercent);						// Progress indication
	extern bool GetLicenseAgreement(const char *LicenseText);	// Display/confirm license
	extern void DisplayNotes(const char *NotesFileName);				// Display notes text file

#else																						// Compiling application?
	extern	bool	Aborted;
	extern	int		GlobalErrorFlag;
	extern	char	*MsgTxt;

	extern	const char *ProgName;									// e.g. "sfArkLib"
	extern	const char *ProgVersion;							// e.g."2.10 "
	extern	const unsigned char 	ProgVersionMaj;	// 00-255 = V25.5x, etc.
	extern	const unsigned char 	ProgVersionMin;	// 00-99 = Vx.x99, etc.
	extern	unsigned SourceFileOffset;						// Set non-zero by app for self-extraction

	extern	unsigned short	GetsfArkLibVersion(void);
	extern	bool						Decode(const char *InFileName, const char *ReqOutFileName);

	// Application-supplied functions...
	void msg(const char *MessageText, int Flags);				// Message display function
  void UpdateProgress(int ProgressPercent);						// Progress indication
	bool GetLicenseAgreement(const char *LicenseText);	// Display/confirm license
	void DisplayNotes(const char *NotesFileName);				// Display notes text file
#endif

