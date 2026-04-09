# organizers_c — Windows-Native C Translations

This folder contains C (C99/C11) rewrites of every Python script in the
`organizers/` repository, targeting **native Windows execution only**.

All code uses Win32 / Windows-native APIs exclusively.  No POSIX calls,
no Cygwin, no WSL dependencies, no third-party runtime libraries (except
Windows SDK components that ship with every supported Windows install).

---

## OS Compatibility — TL;DR

| Platform | Can run the `.exe`? | Can compile the `.c`? | Notes |
| --- | --- | --- | --- |
| **Windows 10** | Yes | Yes (with Build Tools) | Full support |
| **Windows 11** | Yes | Yes (with Build Tools) | Full support |
| **Windows 7 / 8.1** | Partial | Yes | Media Foundation limited on Win7; COM works |
| **macOS** | No | No | Win32 API does not exist on macOS |
| **Linux** | No | No | Win32 API does not exist on Linux |
| **Linux + Wine** | Partial | No | See Wine note below |
| **WSL (Windows Subsystem for Linux)** | No | No | WSL is Linux; Win32 not available inside it |

---

## Why the `.exe` only runs on Windows

### Binary format: PE vs ELF vs Mach-O

The compiled `.exe` is a **PE (Portable Executable)** file — the binary
format native to Windows.  macOS uses **Mach-O** and Linux uses **ELF**.
These formats are fundamentally incompatible: the operating system loader
on macOS and Linux cannot parse or execute a PE file at all.

```
Windows  →  .exe / .dll   (PE format)
Linux    →  no extension  (ELF format)
macOS    →  no extension  (Mach-O format)
```

You cannot copy a `.exe` to a Mac or Linux machine and run it, regardless
of the hardware or CPU architecture.

### API dependencies: Win32 only

Even if the binary format were not an issue, the code calls functions that
simply do not exist outside Windows:

| API used in this code | Windows DLL | macOS / Linux equivalent |
| --- | --- | --- |
| `FindFirstFileA` / `FindNextFileA` | `kernel32.dll` | `opendir` / `readdir` (POSIX) |
| `CreateFileA`, `ReadFile`, `WriteFile` | `kernel32.dll` | `open`, `read`, `write` (POSIX) |
| `GetLocalTime` | `kernel32.dll` | `localtime` (POSIX) |
| `CoCreateInstance` / `IDispatch` (COM) | `ole32.dll` | No equivalent (macOS has Cocoa scripting; Linux has nothing) |
| `MFCreateSourceReaderFromURL` (Media Foundation) | `mfplat.dll` | `libavformat` / ffmpeg (POSIX) |
| `WinHttpOpen` / `WinHttpSendRequest` | `winhttp.dll` | `libcurl` (POSIX) |
| `GetFileAttributesExA` | `kernel32.dll` | `stat()` (POSIX) |

These Windows DLLs are not present on macOS or Linux.  The linker on
those platforms would refuse to compile this code even if you tried.

---

## Running on macOS or Linux — options

### Option 1: Use the original Python scripts (recommended)

The Python scripts in the parent `organizers/` folder are cross-platform.
They already run on macOS and Linux as long as the required Python packages
are installed.  That is the intended path for non-Windows users.

### Option 2: Wine (Linux only, partial)

[Wine](https://www.winehq.org/) implements a subset of the Win32 API on
Linux and macOS, allowing some Windows programs to run.

- `comanga.exe`, `pageCounter.exe` — likely work under Wine (use only
  basic Win32 file I/O and ZIP parsing).
- `length.exe`, `seriesLength.exe` — unlikely to work; Wine's Media
  Foundation support is incomplete as of 2025.
- `doc2docx.exe` — requires Word COM automation, which does not work under
  Wine unless Word itself is installed inside the Wine prefix.
- `steamSorter.exe` — WinHTTP under Wine may work for plain HTTP; HTTPS
  (HLTB) is hit-or-miss depending on the Wine version.

Wine is **not** a supported platform for these tools; it is mentioned
only as a last resort.

### Option 3: Port the `.c` source to POSIX

The `.c` files can be ported to Linux/macOS by replacing each Win32 call
with its POSIX or platform-specific equivalent.  The table in
"Windows-Native API Substitutions" above shows what needs changing.
The effort involved is roughly equivalent to rewriting the scripts again.

---

## File Mapping

| Original Python file | C translation | Notes |
| --- | --- | --- |
| `shared_utils.py` | `shared_utils.h` + `shared_utils.c` | Shared utilities (logging, ZIP reader, file finder…) |
| `comanga.py` | `comanga.c` | Comic/manga page counter |
| `doc2docx.py` | `doc2docx.c` | DOC → DOCX via COM (Word) or LibreOffice |
| `length.py` | `length.c` | Video duration analyzer |
| `pageCounter.py` | `pageCounter.c` | PDF / EPUB / DOCX page counter |
| `seriesLength.py` | `seriesLength.c` | TV series duration analyzer |
| `steamSorter.py` | `steamSorter.c` | Steam + HowLongToBeat analyzer |

---

## Windows-Native API Substitutions

| Python library / call | Windows C equivalent |
| --- | --- |
| `pathlib.Path` | `char[]` arrays + `WIN32_FIND_DATAA` |
| `os.walk` / `Path.iterdir` | `FindFirstFileA` / `FindNextFileA` |
| `open(path, 'rb')` | `CreateFileA` + `ReadFile` |
| `os.stat().st_size` | `GetFileAttributesExA` → `LARGE_INTEGER` |
| `zipfile.ZipFile` | Manual ZIP central-directory parser |
| `logging` (with timestamps) | Custom `log_msg()` using `GetLocalTime` |
| `win32com.client` (Word COM) | `CoCreateInstance` + `IDispatch::Invoke` |
| `subprocess.run` (LibreOffice) | `CreateProcessA` + `WaitForSingleObject` |
| `moviepy.editor.VideoFileClip` | Windows Media Foundation (`MFCreateSourceReaderFromURL` + `MF_PD_DURATION`) |
| `requests.get` / `requests.post` | WinHTTP (`WinHttpOpen`, `WinHttpSendRequest`…) |
| `json.loads` | Minimal inline string scanner |
| `howlongtobeatpy.HowLongToBeat` | WinHTTP POST to `howlongtobeat.com/api/search` |

---

## Limitations and Untranslatable Functionality

| Feature | Limitation |
| --- | --- |
| **CBR (RAR) page counting** (`comanga.c`) | Requires `unrar.exe` to be present in `PATH`. Without it, CBR files are reported with 0 pages. No Windows-native RAR decompression API exists; the UnRAR DLL SDK (proprietary) would be needed for a fully embedded solution. |
| **PDF page counting** (both `comanga.c` and `pageCounter.c`) | Uses a binary scan for `/Type /Page` objects. Works for the vast majority of standard PDFs but may miscount cross-reference-stream PDFs (PDF 1.5+ XRef streams). A full PDF parser would be needed for 100% accuracy. |
| **DOCX page counting with deflate** (`pageCounter.c`) | When `word/document.xml` is deflate-compressed (ZIP method 8), the tool falls back to an estimate from the uncompressed size. Decompressing deflate without zlib requires either linking zlib or implementing RFC 1951 — beyond the scope of this translation. |
| **Video duration** (`length.c`, `seriesLength.c`) | Windows Media Foundation may not decode all codecs (e.g. older DivX/Xvid). Install the relevant codec pack if files report no duration. |
| **HLTB API stability** (`steamSorter.c`) | The HowLongToBeat search endpoint is unofficial and may change. If lookups fail, update the `path` and `post_body` in `hltb_get_main_story()`. |
| **JSON parsing** (`steamSorter.c`) | Uses a minimal string scanner — sufficient for well-formed Steam API responses but not a full JSON parser. Nested structures with the same key name could produce unexpected results. |
| **Large files / paths > MAX_PATH** | All tools use `char[MAX_PATH]` (260 chars). Very long paths require extended-path prefixes (`\\?\`) and wider buffers; not implemented to keep the code concise. |

---

## General Compilation Instructions (Windows)

### Prerequisites

- **MSVC (Visual Studio Build Tools)** — recommended, ships with all required SDK headers.
  Install the **"Desktop development with C++"** workload (includes `cl.exe` + Windows SDK).
- **MinGW-w64** — free alternative; install via MSYS2 (`pacman -S mingw-w64-ucrt-x86_64-gcc`).
- Windows 10 SDK or later (for Media Foundation headers).

---

### Target architecture — which Command Prompt to open

The compiled `.exe` architecture depends on **which Developer Command Prompt** you open:

| Shortcut to open | Target `.exe` | Runs on |
| --- | --- | --- |
| **Developer Command Prompt for VS 2022** | x86 (32-bit) | Any Windows 10/11 (x86, x64, ARM64 via emulation) |
| **x64 Native Tools Command Prompt for VS 2022** | x64 (64-bit) | Windows 10/11 x64 and ARM64 (via emulation) |
| **ARM64 Native Tools Command Prompt for VS 2022** | ARM64 (native) | Windows 10/11 ARM64 only, no emulation needed |

**Recommendation:** Use the **x64 Native Tools** prompt for best performance on modern machines.
The 32-bit (x86) binaries also work everywhere via WOW64, just with a 2 GB memory ceiling per process.

---

### Build all tools with MSVC

Open the appropriate **Native Tools Command Prompt for VS 2022** and run from this directory.

The flag `/D_CRT_SECURE_NO_WARNINGS` suppresses MSVC deprecation warnings about standard C
functions (`_snprintf`, `fopen`, etc.) — the code is correct, these are cosmetic warnings only.

#### Quick build — paste the whole block into the Developer Console

```bat
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS comanga.c      shared_utils.c /Fe:comanga.exe
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS doc2docx.c     shared_utils.c /Fe:doc2docx.exe     ole32.lib oleaut32.lib
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS length.c       shared_utils.c /Fe:length.exe        mfplat.lib mfreadwrite.lib mf.lib mfuuid.lib propsys.lib ole32.lib
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS pageCounter.c  shared_utils.c /Fe:pageCounter.exe
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS seriesLength.c shared_utils.c /Fe:seriesLength.exe  mfplat.lib mfreadwrite.lib mf.lib mfuuid.lib propsys.lib ole32.lib
cl /W3 /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS steamSorter.c  shared_utils.c /Fe:steamSorter.exe   winhttp.lib
```

> **Note:** `length.exe` and `seriesLength.exe` require `ole32.lib` for `PropVariantClear`
> (used by Windows Media Foundation). Omitting it causes `LNK2019` linker errors.

### Build all tools with MinGW (gcc)

Open an MSYS2 / MinGW-w64 shell:

```bash
# comanga
gcc -std=c11 -Wall -O2 comanga.c shared_utils.c -o comanga.exe

# doc2docx
gcc -std=c11 -Wall -O2 doc2docx.c shared_utils.c -o doc2docx.exe \
    -lole32 -loleaut32

# length
gcc -std=c11 -Wall -O2 length.c shared_utils.c -o length.exe \
    -lmfplat -lmfreadwrite -lmf -lmfuuid -lpropsys -lole32

# pageCounter
gcc -std=c11 -Wall -O2 pageCounter.c shared_utils.c -o pageCounter.exe

# seriesLength
gcc -std=c11 -Wall -O2 seriesLength.c shared_utils.c -o seriesLength.exe \
    -lmfplat -lmfreadwrite -lmf -lmfuuid -lpropsys -lole32

# steamSorter
gcc -std=c11 -Wall -O2 steamSorter.c shared_utils.c -o steamSorter.exe \
    -lwinhttp
```

---

## Usage

All tools default to the **current working directory** when run without arguments.

```
comanga.exe [directory]
    Count pages in CBZ/CBR/EPUB/PDF files.
    Output: page_count_results.txt

doc2docx.exe [directory]
    Convert all .doc files to .docx (requires Word or LibreOffice).
    Output: .\output\*.docx

length.exe [directory] [-o output.txt]
    Analyse video durations recursively.
    Output: video_duration_analysis.txt

pageCounter.exe [directory]
    Count pages in PDF/EPUB/DOCX files (non-recursive).
    Output: page_count_results.txt

seriesLength.exe [directory]
    Sum video durations per subdirectory (TV series mode).
    Output: series_durations.txt

steamSorter.exe <API_KEY> <STEAM_ID> [STEAM_ID2 ...]
    Fetch Steam library and look up HLTB completion times.
    Output: steam_games_completion_times.txt
```

---

## Static Linking Note

All tools link only against system-provided libraries that are part of
the Windows SDK (`ole32.lib`, `mfplat.lib`, `winhttp.lib`, etc.).
These are **statically linked** into the executable when using
`/MT` (MSVC) or `-static` (MinGW), producing a self-contained `.exe`.

To enable fully static linking with MSVC, add `/MT` to the `cl` command.
With MinGW, add `-static` to the `gcc` command.
