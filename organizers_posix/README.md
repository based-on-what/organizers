# organizers_posix — Linux & macOS Native C Translations

This folder contains POSIX-compatible C (C11) rewrites of every Python
script in the `organizers/` repository.

They target **Linux and macOS** natively.  They do **not** use any
Windows-specific API and produce standard ELF/Mach-O binaries.

---

## OS Compatibility

| Platform | Compiles | Runs | Notes |
| --- | --- | --- | --- |
| **Linux** (x86-64, ARM64) | Yes — gcc / clang | Yes | Full support |
| **macOS** (Intel, Apple Silicon) | Yes — clang / gcc | Yes | Full support |
| **Windows** | No | No | Use `organizers_c/` instead |
| **BSD / other POSIX** | Likely | Likely | Not tested |

---

## Why a separate folder from `organizers_c/`?

The Windows version uses APIs that do not exist on Linux/macOS:

| Feature | Windows (`organizers_c/`) | POSIX (`organizers_posix/`) |
| --- | --- | --- |
| Directory traversal | `FindFirstFileA` / `FindNextFileA` | `opendir` / `readdir` — `<dirent.h>` |
| File I/O | `CreateFileA` + `ReadFile` | `fopen` + `fread` — standard C |
| File metadata | `GetFileAttributesExA` | `stat()` — `<sys/stat.h>` |
| Timestamps | `GetLocalTime` | `time()` + `localtime()` — `<time.h>` |
| Video duration | Windows Media Foundation | **libavformat** (FFmpeg) |
| HTTP / HTTPS | WinHTTP | **libcurl** |
| DOC conversion | Word COM (`IDispatch`) | LibreOffice CLI (`soffice --headless`) |
| Path separator | `\` | `/` |
| Max path macro | `MAX_PATH` (260) | `PATH_MAX` (4096) — `<limits.h>` |
| String utilities | `_stricmp`, `_strdup` | `strcasecmp`, `strdup` — POSIX standard |

The ZIP binary parser, PDF byte-scanner, and all output/sorting logic
are **identical** in both versions — those operate on raw byte buffers
and have no OS dependency.

---

## External Dependencies

| Tool / Library | Used by | Install |
| --- | --- | --- |
| **libavformat** (FFmpeg) | `length`, `seriesLength` | See below |
| **libcurl** | `steamSorter` | See below |
| **LibreOffice** (runtime) | `doc2docx` | See below |
| `unrar` (runtime, optional) | `comanga` — CBR files only | See below |

### Install on Linux (Debian / Ubuntu)

```bash
sudo apt update
sudo apt install libavformat-dev libavutil-dev   # FFmpeg
sudo apt install libcurl4-openssl-dev            # cURL
sudo apt install libreoffice                     # doc2docx
sudo apt install unrar                           # comanga CBR (optional)
```

### Install on Linux (Fedora / RHEL)

```bash
sudo dnf install ffmpeg-devel                   # FFmpeg
sudo dnf install libcurl-devel                  # cURL
sudo dnf install libreoffice                    # doc2docx
sudo dnf install unrar                          # comanga CBR (optional)
```

### Install on macOS (Homebrew)

```bash
brew install ffmpeg                             # FFmpeg
brew install curl                               # cURL (usually pre-installed)
brew install --cask libreoffice                 # doc2docx
brew install unar                               # comanga CBR (optional, unar reads RAR)
```

> On macOS, `clang` is the default C compiler and ships with Xcode Command
> Line Tools (`xcode-select --install`). You can also use `gcc` via Homebrew.

---

## Compilation

### All tools (Linux and macOS)

```bash
# comanga — no external libs
gcc -std=c11 -Wall -O2 comanga.c shared_utils.c -o comanga

# doc2docx — no external libs (LibreOffice is a runtime dependency only)
gcc -std=c11 -Wall -O2 doc2docx.c shared_utils.c -o doc2docx

# pageCounter — no external libs
gcc -std=c11 -Wall -O2 pageCounter.c shared_utils.c -o pageCounter

# length — requires FFmpeg headers
gcc -std=c11 -Wall -O2 length.c shared_utils.c -o length \
    $(pkg-config --cflags --libs libavformat libavutil)

# seriesLength — requires FFmpeg headers
gcc -std=c11 -Wall -O2 seriesLength.c shared_utils.c -o seriesLength \
    $(pkg-config --cflags --libs libavformat libavutil)

# steamSorter — requires libcurl headers
gcc -std=c11 -Wall -O2 steamSorter.c shared_utils.c -o steamSorter \
    $(pkg-config --cflags --libs libcurl)
```

Replace `gcc` with `clang` on macOS if preferred — both accept the same flags.

> **`pkg-config` not found?**
> On macOS you may need: `brew install pkg-config`
> Or replace the `$(pkg-config ...)` with explicit flags, for example:
> ```bash
> # length on macOS without pkg-config
> clang -std=c11 -O2 length.c shared_utils.c -o length \
>     -I/opt/homebrew/include -L/opt/homebrew/lib -lavformat -lavutil
> ```

### Static linking (optional, fully self-contained binary)

```bash
# Linux static example (links all libs into the binary)
gcc -std=c11 -O2 length.c shared_utils.c -o length \
    -static $(pkg-config --cflags --libs libavformat libavutil) \
    -lpthread -lm -lz -lbz2 -llzma
```

> Static linking on macOS is restricted for system libraries — dynamic
> linking is the recommended approach there.

---

## Usage

All tools default to the current working directory.

```
./comanga [directory]
    Count pages in CBZ/CBR/EPUB/PDF files.
    Output: page_count_results.txt

./doc2docx [directory]
    Convert .doc files to .docx using LibreOffice.
    Output: ./output/*.docx

./length [directory] [-o output.txt]
    Analyze video durations recursively.
    Output: video_duration_analysis.txt

./pageCounter [directory]
    Count pages in PDF/EPUB/DOCX files (non-recursive).
    Output: page_count_results.txt

./seriesLength [directory]
    Sum video durations per subdirectory (TV series mode).
    Output: series_durations.txt

./steamSorter <API_KEY> <STEAM_ID> [STEAM_ID2 ...]
    Fetch Steam library and look up HLTB completion times.
    Output: steam_games_completion_times.txt
```

---

## Limitations

| Feature | Limitation |
| --- | --- |
| **DOC conversion** | Only LibreOffice is available; Microsoft Word does not run natively on Linux/macOS. Conversion quality depends on LibreOffice's DOC support. |
| **CBR (RAR) in comanga** | Requires `unrar` (Linux) or `unar` (macOS) in `PATH`. Without it, CBR files are skipped. |
| **PDF page counting** | Byte-scan heuristic (`/Type /Page`). Works for standard PDFs; may miscount PDF 1.5+ cross-reference-stream files. |
| **DOCX deflate** | If `word/document.xml` is deflate-compressed (ZIP method 8), falls back to an estimated page count. Exact decompression requires zlib. |
| **HLTB API** | The HowLongToBeat search endpoint is unofficial and may change without notice. |
| **Codec support** | `length` and `seriesLength` rely on FFmpeg for decoding metadata. All common formats (MP4, MKV, AVI, MOV…) are supported out of the box. |

---

## File Mapping (Python → POSIX C)

| Original Python file | POSIX C translation |
| --- | --- |
| `shared_utils.py` | `shared_utils.h` + `shared_utils.c` |
| `comanga.py` | `comanga.c` |
| `doc2docx.py` | `doc2docx.c` |
| `length.py` | `length.c` |
| `pageCounter.py` | `pageCounter.c` |
| `seriesLength.py` | `seriesLength.c` |
| `steamSorter.py` | `steamSorter.c` |
