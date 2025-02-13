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
    ULARGE_INTEGER FileSize;
    ULARGE_INTEGER FileIndex;
    ULARGE_INTEGER Checksum;
    bool bChecksumCalculated;
    DWORD dwVolumeSerial;

    LPWSTR szFilePath;
};

class FileRecordTableClass
{
    struct FILE_RECORD_CHAIN_ELEMENT
    {
        FILE_RECORD* record;
        FILE_RECORD_CHAIN_ELEMENT* next;
    } *Table[FILE_RECORD_TABLE_SIZE];

    static bool CalculateChecksum(LPCWSTR szFilePath, DWORD dwSkipSize, PULARGE_INTEGER Checksum, volatile BOOL* bBreak)
    {
        HANDLE h = CreateFile(szFilePath, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

        if (h == INVALID_HANDLE_VALUE)
            return false;

        bool bChecksumCalculated = GetFileCheckSum(h, dwSkipSize, Checksum, bBreak) == TRUE;
        CloseHandle(h);
        return bChecksumCalculated;
    }

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
    FILE_RECORD* FindEqualOrAdd(LPCWSTR szFilePath, DWORD dwSkipSize, volatile BOOL* bBreak)
    {
        HANDLE hFile = CreateFile(szFilePath, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            return NULL;
        }

        BY_HANDLE_FILE_INFORMATION info;
        if (!GetFileInformationByHandle(hFile, &info))
        {
            CloseHandle(hFile);
            return NULL;
        }

        CloseHandle(hFile);

        ULARGE_INTEGER FileSize = { 0 };
        FileSize.HighPart = info.nFileSizeHigh;
        FileSize.LowPart = info.nFileSizeLow;

        ULARGE_INTEGER Checksum = { 0 };

        bool bChecksumCalculated = false;
        FILE_RECORD_CHAIN_ELEMENT* fr_chelem;

        for (fr_chelem =
            Table[(info.nFileSizeLow >> 10) & (FILE_RECORD_TABLE_SIZE - 1)];
            fr_chelem != NULL;
            fr_chelem = fr_chelem->next)
        {
            if (fr_chelem->record->FileSize.QuadPart == FileSize.QuadPart &&
                fr_chelem->record->dwVolumeSerial == info.dwVolumeSerialNumber)
            {
                if (!bChecksumCalculated)
                {
                    if (FileSize.QuadPart <= 32768)
                    {
                        Checksum.QuadPart = 1;
                    }
                    else if (!CalculateChecksum(szFilePath, dwSkipSize, &Checksum, bBreak))
                    {
                        return NULL;
                    }

                    bChecksumCalculated = true;
                }

                if (!fr_chelem->record->bChecksumCalculated)
                {
                    if (fr_chelem->record->FileSize.QuadPart <= 32768)
                    {
                        fr_chelem->record->Checksum.QuadPart = 1;
                    }
                    else if (!CalculateChecksum(fr_chelem->record->szFilePath, dwSkipSize, &fr_chelem->record->Checksum, bBreak))
                    {
                        Delete(fr_chelem);
                        continue;
                    }

                    fr_chelem->record->bChecksumCalculated = true;
                }

                if (fr_chelem->record->Checksum.QuadPart == Checksum.QuadPart)
                {
                    switch (CompareFiles(fr_chelem->record->szFilePath,
                        szFilePath, dwSkipSize, bBreak))
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
        }

        // Ok, no matching found so we add this one to the table.
        FILE_RECORD* fr = new FILE_RECORD;
        fr_chelem = new FILE_RECORD_CHAIN_ELEMENT;

        if ((fr == NULL) || (fr_chelem == NULL))
        {
            return NULL;
        }

        fr->FileSize = FileSize;
        fr->FileIndex.HighPart = info.nFileIndexHigh;
        fr->FileIndex.LowPart = info.nFileIndexLow;
        fr->Checksum = Checksum;
        fr->bChecksumCalculated = bChecksumCalculated;
        fr->dwVolumeSerial = info.dwVolumeSerialNumber;
        fr->szFilePath = _wcsdup(szFilePath);

        if (fr->szFilePath == NULL)
        {
            delete fr;
            delete fr_chelem;
            return NULL;
        }

        fr_chelem->record = fr;
        fr_chelem->next =
            Table[(FileSize.LowPart >> 10) & (FILE_RECORD_TABLE_SIZE - 1)];
        Table[(FileSize.LowPart >> 10) & (FILE_RECORD_TABLE_SIZE - 1)] = fr_chelem;

        return NULL;
    }

    void Delete(FILE_RECORD_CHAIN_ELEMENT* fr_chelem)
    {
        FILE_RECORD_CHAIN_ELEMENT** priorptr;
        for (priorptr = &Table[(fr_chelem->record->FileSize.LowPart >> 10) &
            (FILE_RECORD_TABLE_SIZE - 1)];
            *priorptr != fr_chelem;
            priorptr = &((*priorptr)->next));

        *priorptr = fr_chelem->next;

        free(fr_chelem->record->szFilePath);
        delete fr_chelem->record;
        delete fr_chelem;
    }

    FILE_RECORD* Find(LPCTSTR szFilePath) const
    {
        HANDLE hFile = CreateFile(szFilePath, GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            return NULL;

        BY_HANDLE_FILE_INFORMATION info;
        if (!GetFileInformationByHandle(hFile, &info))
        {
            CloseHandle(hFile);
            return NULL;
        }

        CloseHandle(hFile);

        // If there is only one link we may find the same link through different
        // junction paths and attempt to unlink this last link to make two
        // different copies that would delete the file instead... No good, so just
        // skip finding files with only one link.

        if (info.nNumberOfLinks <= 1)
        {
            return NULL;
        }

        ULARGE_INTEGER FileIndex = { 0 };
        FileIndex.HighPart = info.nFileIndexHigh;
        FileIndex.LowPart = info.nFileIndexLow;

        for (FILE_RECORD_CHAIN_ELEMENT* fr_chelem =
            Table[(info.nFileSizeLow >> 10) & (FILE_RECORD_TABLE_SIZE - 1)];
            fr_chelem != NULL;
            fr_chelem = fr_chelem->next)
        {
            if (fr_chelem->record->FileIndex.QuadPart == FileIndex.QuadPart &&
                fr_chelem->record->dwVolumeSerial == info.dwVolumeSerialNumber)
            {
                return fr_chelem->record;
            }
        }

        return NULL;
    }

};

