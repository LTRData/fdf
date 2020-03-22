#include "chkfile.h"

#ifndef MAX_PATH_MAX_LENGTH
#define MAX_PATH_MAX_LENGTH 32768
#endif

/// Note! This const must be power of 2
#ifndef FILE_RECORD_TABLE_SIZE
#define FILE_RECORD_TABLE_SIZE 256
#endif

struct FILE_RECORD
{
  long FileBlocks;
  ino_t FileIndex;
  ULARGE_INTEGER Checksum0;
  ULARGE_INTEGER Checksum1;
  bool bChecksumCalculated;
  dev_t dwVolumeSerial;

  LPSTR szFilePath;

  bool CalculateChecksum()
  {
    HANDLE h = open(szFilePath, O_RDONLY);
    if (h == INVALID_HANDLE_VALUE)
      return false;

    bChecksumCalculated = GetFileCheckSum(h, &Checksum0, &Checksum1);
    CloseHandle(h);
    return bChecksumCalculated;
  }
};

class FileRecordTableClass
{
  struct FILE_RECORD_CHAIN_ELEMENT
  {
    FILE_RECORD *record;
    FILE_RECORD_CHAIN_ELEMENT *next;
  } *Table[FILE_RECORD_TABLE_SIZE];
  
public:
  FileRecordTableClass()
  {
    ZeroMemory(Table, sizeof Table);
  }

  /**
   * FindEqualOrAdd() does not automatically check if there is already a
   * record with this index in the table, so caller must call Find() first to
   * make sure.
   */
  FILE_RECORD *FindEqualOrAdd(LPCSTR szFilePath)
  {
    struct stat info;
    if (lstat(szFilePath, &info))
      return NULL;

    ULARGE_INTEGER Checksum0;
    ULARGE_INTEGER Checksum1;
    Checksum0.QuadPart = 0;
    Checksum1.QuadPart = 0;
    bool bChecksumCalculated = false;

    for (FILE_RECORD_CHAIN_ELEMENT *fr_chelem = Table[(info.st_size >> 1)
						     & (FILE_RECORD_TABLE_SIZE-
							1)];
	 fr_chelem != NULL;
	 fr_chelem = fr_chelem->next)
      if (fr_chelem->record->FileBlocks == info.st_blocks)
	if ((fr_chelem->record->dwVolumeSerial == info.st_dev))
	  {
	    if (!bChecksumCalculated)
	      {
		HANDLE hFile = open(szFilePath, O_RDONLY);
		if (hFile == INVALID_HANDLE_VALUE)
		  return NULL;

		if (!GetFileCheckSum(hFile, &Checksum0, &Checksum1))
		  {
		    CloseHandle(hFile);
		    return NULL;
		  }
		
		CloseHandle(hFile);

		bChecksumCalculated = true;
	      }

	    if (!fr_chelem->record->bChecksumCalculated)
	      if (!fr_chelem->record->CalculateChecksum())
		{
		  Delete(fr_chelem);
		  continue;
		}

	    if ((fr_chelem->record->Checksum0.QuadPart == Checksum0.QuadPart) &
		(fr_chelem->record->Checksum1.QuadPart == Checksum1.QuadPart))
	      {
		switch (CompareFiles(fr_chelem->record->szFilePath,
				     szFilePath))
		  {
		  case FILE_COMPARE_EQUAL:
		    return fr_chelem->record;
		  case FILE_COMPARE_NOT_EQUAL:
		  case FILE_COMPARE_FILE1_LONGER:
		  case FILE_COMPARE_FILE2_LONGER:
		  case FILE_COMPARE_FILE2_FAILED:
		    continue;
		  case FILE_COMPARE_FILE1_FAILED:
		    Delete(fr_chelem);
		    continue;
		  }
	      }
	  }

    // Ok, no matching found so we add this one to the table.
    FILE_RECORD *fr = new FILE_RECORD;
    FILE_RECORD_CHAIN_ELEMENT *fr_chelem = new FILE_RECORD_CHAIN_ELEMENT;

    if ((fr == NULL) | (fr_chelem == NULL))
      return NULL;

    fr->FileBlocks = info.st_blocks;
    fr->FileIndex = info.st_ino;
    fr->Checksum0 = Checksum0;
    fr->Checksum1 = Checksum1;
    fr->bChecksumCalculated = bChecksumCalculated;
    fr->dwVolumeSerial = info.st_dev;
    fr->szFilePath = strdup(szFilePath);

    if (fr->szFilePath == NULL)
      {
	delete fr;
	delete fr_chelem;
	return NULL;
      }

    fr_chelem->record = fr;
    fr_chelem->next = Table[(info.st_blocks >> 1) &
			   (FILE_RECORD_TABLE_SIZE-1)];
    Table[(info.st_blocks >> 1) & (FILE_RECORD_TABLE_SIZE-1)] = fr_chelem;

    return NULL;
  }

  void Delete(FILE_RECORD_CHAIN_ELEMENT *fr_chelem)
  {
    FILE_RECORD_CHAIN_ELEMENT **priorptr = &Table[(fr_chelem->record->
						   FileBlocks >> 1) &
						 (FILE_RECORD_TABLE_SIZE-
						  1)];
    for ( ; *priorptr != fr_chelem; priorptr = &((*priorptr)->next));

    *priorptr = fr_chelem->next;

    free(fr_chelem->record->szFilePath);
    delete fr_chelem->record;
    delete fr_chelem;
  }

  FILE_RECORD *Find(LPCTSTR szFilePath)
  {
    struct stat info;
    if (lstat(szFilePath, &info))
      return NULL;

    for (FILE_RECORD_CHAIN_ELEMENT *fr_chelem = Table[(info.st_blocks >> 1)
						     & (FILE_RECORD_TABLE_SIZE-
							1)];
	 fr_chelem != NULL;
	 fr_chelem = fr_chelem->next)
      if (fr_chelem->record->FileIndex == info.st_ino)
	if ((fr_chelem->record->dwVolumeSerial == info.st_dev))
	  return fr_chelem->record;
    
    return NULL;
  }

};
