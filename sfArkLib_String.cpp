//sfArkLib String functions
// copyright 1998-2000 Andy Inman
// Contact via: http://netgenius.co.uk or http://melodymachine.com

//
const char *GetFileExt(const char *FileName)
{
// return pointer to (final) file extension including the dot e.g. '.txt'
	const char *p;

	for(p = FileName + strlen(FileName); p > FileName &&  *p != '.'; p--);
  if (*p == '.')  p = FileName + strlen(FileName);
	return p;
}

/****
! ==============================================================
GetFileName   FUNCTION(FileName)
! return file name without path
p   USHORT(0)
q   USHORT, AUTO
l   USHORT, AUTO
Result  STRING(FILE:MaxFileName), AUTO
  CODE
  l = size(FileName)
  loop
    q = instring('\', FileName, 1, p+1)
    if q
      p = q
    else
      break
    .
  .
  if p = 0
    Result = FileName
  elsif p = l   ! Avoid an array index error in this case...
    Result = ''
  else
    Result = FileName[p+1 : l]
  .
  return(clip(Result))

! ==============================================================
GetFileNameNoExt    FUNCTION(FileName)
p   USHORT(0)
q   USHORT, AUTO
l   USHORT, AUTO
Result  STRING(File:MaxFileName), AUTO
  CODE
  Result = GetFileName(FileName)
  loop
    q = instring('.', Result, 1, p+1)
    if q
      p = q
    else
      break
    .
  .
  case p
  of 0
    !
  of 1
    Result = ''     ! Handle an invalid filename of just '.ext'
  else
    Result = Result[1 : p-1]
  .
  return(clip(Result))
! ==============================================================
GetPathName   FUNCTION(FileName)
! return file path *without* trailing '\' without filename
p   USHORT(0)
q   USHORT, AUTO
  CODE
  loop
    q = instring('\', FileName, 1, p+1)
    if q
      p = q
    else
      break
    .
  .
  if p = 0 or p = 1
    return('')
  else
    return(FileName[1 : p-1] )
  .

! ==============================================================

BuildFileName    FUNCTION(PathName, FileName, FileExt)
l       USHORT
  CODE
  l = len(clip(PathName))
  if l = 0
    return(clip(FileName) & FileExt)
  elsif PathName[l] = '\'
    return(clip(PathName) & clip(FileName) & FileExt)
  else
    return(clip(PathName) & '\' & clip(FileName) & FileExt)
  .
*****/
