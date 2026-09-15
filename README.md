# Find Duplicate Files (fdf)

A Windows command-line utility that finds duplicate file contents and existing hard links. By default it reports matches; optional modes delete duplicates, replace them with hard links, or split existing hard links into separate copies.

The Windows implementation supports Unicode filenames and files larger than 4 GiB, with comparison improvements added in 2024–2025. The repository also retains an incomplete POSIX port.

## Contents

| Component | Purpose |
| --- | --- |
| [fdf.cpp](fdf.cpp), [fdftable.hpp](fdftable.hpp) | Windows directory traversal, duplicate detection and actions |
| [chkfile.cpp](chkfile.cpp), [lnk.c](lnk.c) | Checksum/comparison routines and native Windows hard-link operations |
| [xorsum.cpp](xorsum.cpp) | Companion command-line utility that prints the internal checksum for matching files |
| [posix/](posix/) | Unfinished port with separate sources and a makefile; not equivalent to the Windows implementation |

## Windows usage

```text
fdf [options] [filepattern ...]
```

Run from the directory to scan. Put options before filename patterns. With no patterns, fdf uses `*`; subdirectories are included only with `-r`. Use `fdf -?` for built-in help.

For example, from Windows Command Prompt (`cmd.exe`):

```bat
cd /d C:\Data\ToCompare
fdf -q
fdf -r -j -q
fdf -r -j -q "*.jpg" "*.png"
```

These commands only report matches. The recursive examples use `-j` to avoid descending into junctions and other directory reparse points. Apply patterns to filenames within the current directory tree rather than treating positional arguments as independent scan roots.

| Option | Effect |
| --- | --- |
| `-r` | Recurse into subdirectories. |
| `-j` | With `-r`, skip directory reparse points during traversal. |
| `-o` | Skip files marked offline, virtual, or requiring recall on open/data access. |
| `-q` | Hide the current-file/current-directory progress display. |
| `-qq` | Also suppress file-operation error messages; usage and argument errors remain visible. |
| `-x:string[,string2...]` | Exclude paths and filenames containing any listed substring, matched case-insensitively. |
| `-h` | Hide reports of existing hard links. |
| `-s` | Hide duplicate-content reports. |
| `-d` | Delete files found to duplicate an earlier candidate. |
| `-l` | Replace duplicate files with hard links. |
| `-d -l` | Delete duplicate contents and also remove encountered existing hard links. |
| `-c` | Split encountered existing hard links by deleting a link and copying from another name for the file. |
| `-f` | With `-d` or `-l`, attempt the operation on read-only files by changing attributes. |
| `-n:LEN` | Ignore the first LEN bytes when comparing same-size files. Files no larger than LEN are not compared for duplicate contents. |

For `-n`, uppercase `K`/`M` mean multiples of 1024 and lowercase `k`/`m` mean multiples of 1000. Follow the built-in help's limit of less than 2 GiB for LEN. **Using `-n` with deletion or linking can discard different header bytes**, because only the remaining contents are compared.

Pass exclusions in one `-x:` option, for example `fdf -r -j -q -x:cache,backup`. Entries are literal substrings rather than wildcard patterns. Empty comma-separated entries are ignored, but at least one nonempty entry is required. Quote the complete option if an entry contains spaces.

## How matching and actions work

Candidates must have the same full file size and volume serial number. Small files go directly to byte comparison; larger files first use a checksum based on selected file regions. A matching checksum is followed by a byte comparison through the end of the files, starting at the optional skip offset. Existing hard links are recognized using volume serial number and file index. Empty files are skipped.

Comparison covers the ordinary file data stream. It does not compare alternate data streams, security descriptors, timestamps or other metadata. The internal checksum printed by `xorsum` is not a cryptographic or full-file integrity hash.

Deletion, linking and splitting happen during traversal, without per-file confirmation or rollback. Review a report-only scan first and keep backups before using these modes. Files should remain unchanged during a scan. The retained candidate depends on traversal order; there is no “keep newest” or preferred-directory selection.

Hard links require filesystem support and refer to the same underlying file: subsequent writes through one name affect the others. When linking fails in one direction, fdf tries the reverse direction, so do not assume one particular path's metadata will be retained. Splitting a link removes that name before copying; a failed copy can leave that path missing. Ctrl+C requests cancellation but does not undo completed operations.

## Building

### Windows

[fdf.sln](fdf.sln) builds the main utility for Win32 and x64. The project selects the **v120** Visual C++ toolset and Windows SDK version `10.0`, and imports external `..\winstrct.props` plus maintainer-specific absolute paths to WDK 7 and Visual Studio 2013 property sheets. These imports need to be available or adapted for a local build.

The root [Makefile](Makefile) is a separate NMAKE build for both `fdf.exe` and `xorsum.exe`, with `i386`, `AMD64`, `ARM` and `ARM64` cases. It takes `CPU` from `_BUILDARCH` or defaults to `i386`. Prepare the matching compiler environment and CPU output directory before running `nmake`.

Both routes depend on shared LTR Data headers such as `winstrct.h`, `wfind.h`, `wconsole.h` and `ntdll.h` from [LTRData/include](https://github.com/LTRData/include), and associated libraries available through the include/library search paths. The NMAKE build explicitly references `..\lib\winstrct.lib`; the x86 DLL-runtime build also uses `minwcrt.lib`. The repository is not a self-contained Windows build.

### POSIX

The [posix](posix/) directory is an incomplete port, not a ready-to-use Linux/macOS/FreeBSD build. Its sources retain Windows-only traversal/types and an unimplemented copy function that returns `ENOSYS`. Its makefile and comparison/action code also need work before use. The Windows commands and behavior documented above should not be assumed to apply to this port.

## Credits

By Olof Lagerkvist, LTR Data. The executable resources carry a 2004–2024 copyright notice; the command-line help describes the program as open source freeware.
