#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>

#include "w32types.h"
#include "fdftable.hpp"

BOOL CopyFile(const char *source, const char *target, BOOL b)
{
  errno = ENOSYS;
  return FALSE;
}

char cCurrentPath[MAX_PATH_MAX_LENGTH] = "";
char *szExcludeStrings = NULL;
DWORD dwExcludeStrings = 0;

QWORD qwDupSize = 0;
QWORD qwSavedSize = 0;
QWORD qwDupFiles = 0;
QWORD qwFiles = 0;
QWORD qwDeletedFiles = 0;

bool bDisplayLinks = true;
bool bDisplayEqual = true;
bool bVerbose = true;
bool bQuiet = false;
bool bDeleteEqual = false;
bool bForceDelete = false;
bool bLinkEqual = false;
bool bRecurse = false;
volatile bool bBreak = false;
bool bSplitLinks = false;

FileRecordTableClass *FileRecordTable = NULL;

/*
BOOL WINAPI ConsoleCtrlHandler(DWORD)
{
  bBreak = true;
  return true;
}
*/

bool ExcludedString()
{
  DWORD dwExcludeString = dwExcludeStrings;
  char *szExcludeString = szExcludeStrings;
  char *szPathEnd = cCurrentPath+strlen(cCurrentPath);

  do
    {
      DWORD dwExclStrLen = strlen(szExcludeString);

      for (char *szPathPtr = cCurrentPath;
	   szPathEnd-szPathPtr >= dwExclStrLen;
	   szPathPtr++)
	if (strnicmp(szPathPtr, szExcludeString, dwExclStrLen) == 0)
	  return true;

      szExcludeString += dwExclStrLen + 1;
    }
  while (--dwExcludeString);

  return false;
}			

void chkfile(LPCSTR szFilePath, const dirent *file)
{
  if (bVerbose)
    {
      printf("\r%.79s", szFilePath);
      clreol();
      fputc('\r', stdout);
    }

  FILE_RECORD *fr = FileRecordTable->Find(szFilePath);
  if (fr != NULL)
    {
      qwFiles++;

      if (bSplitLinks)
	{
	  if (unlink(szFilePath))
	    {
	      perror(szFilePath);
	      fprintf(stderr, "Cannot unlink '%s'\n", szFilePath);
	      clreol();
	      puts("");
	      return;
	    }

	  if (!CopyFile(fr->szFilePath, szFilePath, TRUE))
	    {
	      perror(szFilePath);
	      fprintf(stderr, "Cannot copy '%s' to '%s'\n", fr->szFilePath,
		      szFilePath);
	      clreol();
	      puts("");
	      return;
	    }

	  printf("'%s' copied to '%s'\n", fr->szFilePath, szFilePath);
	}
      else if (bDisplayLinks)
	{
	  printf("'%s' <=> '%s'", szFilePath, fr->szFilePath);
	  clreol();
	  puts("");
	}

      return;
    }

  fr = FileRecordTable->FindEqualOrAdd(szFilePath);
  if (fr == NULL)
    {
      qwFiles++;
      return;
    }

  if (strcmp(szFilePath, fr->szFilePath) == 0)
    return;

  qwFiles++;
  qwDupFiles++;

  if (bDisplayEqual)
    {
      printf("'%s' contains same as '%s'. ", szFilePath, fr->szFilePath);
      clreol();
    }

  if (bDeleteEqual)
    {
      DWORD dwFileAttrs;
      if (bForceDelete)
	{
	  //dwFileAttrs = GetFileAttributes(szFilePath);
	  //SetFileAttributes(szFilePath, FILE_ATTRIBUTE_NORMAL);
	}

      if (!unlink(szFilePath))
	{
	  perror(szFilePath);
	  //if (bForceDelete)
	  //  SetFileAttributes(szFilePath, dwFileAttrs);
	  fprintf(stderr, "Cannot delete '%s'\n", szFilePath);
	  qwDupSize += file->st_size;
	  return;
	}

      qwDeletedFiles++;

      if (bDisplayEqual)
	printf("Deleted ok.\n");

      qwSavedSize += file->nFileSizeLow;
      qwSavedSize += (QWORD)file->nFileSizeHigh << 32;
      return;
    }

  if (bLinkEqual)
    {
      DWORD dwFileAttrs;
      if (bForceDelete)
	{
	  dwFileAttrs = GetFileAttributes(szFilePath);
	  SetFileAttributes(szFilePath, FILE_ATTRIBUTE_NORMAL);
	}

      if (!HardLink(fr->szFilePath, szFilePath))
	{
	  perror(szFilePath);
	  if (bForceDelete)
	    SetFileAttributes(szFilePath, dwFileAttrs);
	  fprintf(stderr, "Cannot link '%s' to '%s'.\n",
		  szFilePath, fr->szFilePath, szFilePath);
	  qwDupSize += file->nFileSizeLow;
	  qwDupSize += (QWORD)file->nFileSizeHigh << 32;
	  return;
	}

      qwDeletedFiles++;

      if (bDisplayEqual)
	puts("Hard linked ok.\n");

      qwSavedSize += file->nFileSizeLow;
      qwSavedSize += (QWORD)file->nFileSizeHigh << 32;
      return;
    }

  qwDupSize += file->nFileSizeLow;
  qwDupSize += (QWORD)file->nFileSizeHigh << 32;
  puts("");
  return;
}

bool dosubdir(LPSTR pCurrentPathPtr)
{
  if (bRecurse)
    {
      if (bVerbose)
	{
	  printf("\r%.79s", cCurrentPath);
	  clreol();
	  fputc('\r', stdout);
	}

      pCurrentPathPtr[0] = '*';
      pCurrentPathPtr[1] = 0;

      WFilteredFileFinder dirfound(cCurrentPath, 0, FILE_ATTRIBUTE_DIRECTORY);
      if (dirfound)
	do
	  {
#ifndef NO_LOOP_SLEEPS
	    Sleep(0);
#endif
	    if (bBreak)
	      return false;

	    if (sizeof(cCurrentPath) - (pCurrentPathPtr - cCurrentPath) <
		strlen(dirfound.cFileName) + 2)
	      {
		fprintf(stderr, "Skipping too long name: '%s'\n",
			dirfound.cFileName);
		continue;
	      }

	    strcpy(pCurrentPathPtr, dirfound.cFileName);

	    if ((strcmp(dirfound.cFileName, ".") == 0) |
		(strcmp(dirfound.cFileName, "..") == 0))
	      continue;

	    if (sizeof(cCurrentPath)-strlen(pCurrentPathPtr)-
		(pCurrentPathPtr-cCurrentPath) < 4)
	      {
		fprintf(stderr, "Skipping too long path: '%s'\n",
			cCurrentPath);
		continue;
	      }

	    strcat(pCurrentPathPtr, "\\");

	    if (dwExcludeStrings)
	      if (ExcludedString())
		continue;

	    if (!dosubdir(pCurrentPathPtr+strlen(pCurrentPathPtr)))
	      return false;

	    continue;
	  }
	while (dirfound.Next());
    }

  char **argv = __argv;
  while ((++argv)[0])
    {
#ifndef NO_LOOP_SLEEPS
      Sleep(0);
#endif

      if (bBreak)
	return false;

      if (sizeof(cCurrentPath) - (pCurrentPathPtr - cCurrentPath) -
	  strlen(argv[0]) < 1)
	{
	  fprintf(stderr, "Skipping too long path: '%s'\n", cCurrentPath);
	  continue;
	}

      strcpy(pCurrentPathPtr, argv[0]);

      WFilteredFileFinder found(cCurrentPath, FILE_ATTRIBUTE_DIRECTORY);
      if (!found)
	continue;

      do
	{
#ifndef NO_LOOP_SLEEPS
	  Sleep(0);
#endif

	  if (bBreak)
	    return false;

	  if (sizeof(cCurrentPath) - (pCurrentPathPtr - cCurrentPath) <
	      strlen(found.cFileName) + 2)
	    {
	      fprintf(stderr, "Skipping too long name: '%s'\n",
		      found.cFileName);
	      continue;
	    }

	  strcpy(pCurrentPathPtr, found.cFileName);

	  if (dwExcludeStrings)
	    if (ExcludedString())
	      continue;

	  if (found.nFileSizeHigh | found.nFileSizeLow)
	    chkfile(cCurrentPath, &found);
	}
      while (found.Next());
    }
  
  return true;
}

__declspec(noreturn) void usage()
{
  fputs("Utility to check for duplicate files.\r\n"
	"\n"
	"Version 1.0.0. Copyright (C) Olof Lagerkvist 2004.\r\n"
	"This program is open source freeware.\r\n"
	"http://here.is/olof\r\n"
	"\n"
	"Usage:\r\n"
	"fdf [-cdfhlqrs] [-x:string[,string2 ...]] [filepattern ...]\r\n"
	"\n"
	"If no filepatterns are given, all files are processed.\r\n"
	"\n"
	"-c      Split existing hard links by copying them into separate\r\n"
	"        files.\r\n"
	"-d      Delete files found when a file with same contents have\r\n"
	"        been found elsewhere.\r\n"
	"-f      Used with -d to force deletion of read-only files or with\r\n"
	"        -l to overwrite read-only files with hard links to files\r\n"
	"        with same contents.\r\n"
	"-h      Do not display information about hard links.\r\n"
	"-l      Hard link duplicate files. This saves disk space but\r\n"
	"        means that updates to one of these files will affect all\r\n"
	"        of them.\r\n"
	"-q      Do not display which file or directory is currently\r\n"
	"        scanned.\r\n"
	"-qq     Same as -q but also skips error messages.\r\n"
	"-r      Recurse into subdirectories.\r\n"
	"-s      Do not display information about files with same\r\n"
	"        contents as other scanned file(s).\r\n"
	"-x      Exclude paths and filenames containing given strings.\r\n",
	stderr);

  exit(1);
}

int main(int argc, char **argv)
{
  // Nice argument parse loop :)
  while (argv[1] ? argv[1][0] ? (argv[1][0]|0x02) == '/' : false : false)
    {
      while ((++argv[1])[0])
	switch (argv[1][0]|0x20)
	  {
	  case 'h':
	    bDisplayLinks = false;
	    break;
	  case 's':
	    bDisplayEqual = false;
	    break;
	  case 'd':
	    bDeleteEqual = true;
	    if (bLinkEqual)
	      usage();
	    break;
	  case 'f':
	    bForceDelete = true;
	    break;
	  case 'l':
	    bLinkEqual = true;
	    if (bDeleteEqual)
	      usage();
	    break;
	  case 'q':
	    if (!bVerbose)
	      bQuiet = true;
	    bVerbose = false;
	    break;
	  case 'r':
	    bRecurse = true;
	    break;
	  case 'c':
	    bSplitLinks = true;
	    break;
	  case 'x':
	    if (dwExcludeStrings)
	      usage();
	    if (argv[1][1] != ':')
	      usage();
	    if (argv[1][2] == 0)
	      usage();
	    szExcludeStrings = strtok(argv[1]+2, ",");
	    dwExcludeStrings = 1;
	    while (strtok(NULL, ","))
	      dwExcludeStrings++;
	    argv[1] += strlen(argv[1])-1;
	    break;
	  default:
	    usage();
	  }

      --argc;
      ++argv;
    }

  if (bForceDelete & !(bDeleteEqual | bLinkEqual))
    usage();

  char *fakeargv[3];
  if (argv[1] == NULL)
    {
      fakeargv[1] = "*";
      fakeargv[2] = NULL;
      __argv = fakeargv;
    }
  else
    {
      __argv = argv;
      while ((++argv)[0])
	CharToOem(argv[0], argv[0]);
    }

  SetFileApisToOEM();

  FileRecordTable = new FileRecordTableClass;
  if (FileRecordTable == NULL)
    {
      perror("Memory allocation error");
      return -1;
    }

  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
  SetConsoleCtrlHandler(ConsoleCtrlHandler, true);

  EnableBackupPrivileges();

  if (!dosubdir(cCurrentPath))
    printf("\r* Break *");

  clreol();
  puts("");

  fflush(stdout);
  fflush(stderr);

  if (qwFiles)
    printf("%.f file%s scanned.\n", (double)qwFiles, qwFiles==1?"":"s");
  else
    puts("No files found.");
  
  if (qwSavedSize)
    {
      printf("%.f file%s %s saved %.4g %s\n",
	     (double)qwDeletedFiles, qwDeletedFiles==1?"":"s",
	     bDeleteEqual?"removed":"linked with duplicate", _h(qwSavedSize),
	     _p(qwSavedSize));

      if (qwDupSize)
	printf("%.f duplicate file%s left occupying %.4g %s\n",
	       (double)qwDupFiles, qwDupFiles==1?"":"s",_h(qwDupSize),
	       _p(qwDupSize));
    }
  else
    {
      if (qwDupSize)
	printf("%.f duplicate file%s occupying %.4g %s\n", (double)qwDupFiles,
	       qwDupFiles==1?"":"s", _h(qwDupSize), _p(qwDupSize));
      else
	puts("No duplicate files found.");
    }

  return 0;
}
