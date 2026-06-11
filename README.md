# Organizers

A collection of Python scripts for organizing and managing files based on their properties — video duration, document formats, and media content analysis. The project follows a layered architecture: thin CLI entry points delegate to analyzer modules backed by pure reader and core utility packages.

## Table of Contents
- [General Information](#general-information)
- [Features](#features)
- [Technologies Used](#technologies-used)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Usage](#usage)
  - [length.py](#lengthpy)
  - [pageCounter.py](#pagecounterpy)
  - [steamSorter.py](#steamsorterpy)
  - [seriesLength.py](#serieslengthpy)
  - [comanga.py](#comangapy)
  - [doc2docx.py](#doc2docxpy)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)
- [Contribution](#contribution)
- [License](#license)
- [Credits](#credits)

## General Information

Six CLI scripts that help you organize different types of files:

- **Video files** by duration
- **Books and documents** by page count (PDF, EPUB, DOCX)
- **Steam games** by HowLongToBeat completion time
- **TV series** by total duration, grouped by subdirectory
- **Comics and manga** by page count (CBZ, CBR, EPUB, PDF)
- **Legacy Word documents** converted to modern DOCX format

## Features

- **Video Analysis**: duration analysis for MP4, AVI, MKV, MOV, WMV, FLV, WEBM
- **Document Processing**: page counting for PDF, EPUB, DOCX
- **Game Library Analysis**: Steam library + HowLongToBeat integration
- **TV Series Organization**: total duration per series folder
- **Comic/Manga Management**: page counting for CBZ, CBR, EPUB, PDF
- **Document Conversion**: DOC to DOCX with Word COM (Windows) or LibreOffice fallback
- **Progress Reporting**: stderr progress bars keep stdout clean for piping
- **Layered Architecture**: CLI, analyzer, reader, and core layers with clear responsibilities
- **Lazy Dependencies**: heavy libraries loaded on first use; missing ones fail with a clear install hint
- **Cross-platform**: Windows, macOS, Linux

## Technologies Used

### Core Dependencies
- **Python 3.8+**
- **pypdf 3.0+** — PDF reading (falls back to PyPDF2 if pypdf is absent)
- **python-docx 0.8.11+** — DOCX reading and page estimation
- **ebooklib 0.18+** — EPUB processing
- **ffprobe** (part of ffmpeg) — fast video duration reading; **moviepy 1.0.3+** is used as fallback when ffprobe is not on PATH
- **rarfile 4.0+** — CBR archive reading

### API Integration
- **requests 2.28+** — Steam Web API calls
- **howlongtobeatpy 1.0+** — HowLongToBeat game data

### Platform-specific
- **pywin32 305+** (Windows only) — Microsoft Word COM interface for DOC conversion
- **LibreOffice** (Linux/macOS) — DOC conversion fallback

## Prerequisites

- **Python 3.8 or higher**
- **pip**

### Optional
- **ffmpeg** (provides `ffprobe`) for fast video duration analysis — strongly recommended; without it moviepy is used and is roughly 10x slower
- Steam Web API key for `steamSorter.py` — get one at [Steam Developer Portal](https://steamcommunity.com/dev/apikey)
- Microsoft Word or LibreOffice for `doc2docx.py`
- RAR tools (`unrar`) for CBR comic files

## Installation

```bash
git clone https://github.com/based-on-what/organizers.git
cd organizers
pip install -r requirements.txt
```

### System packages

#### Linux/Ubuntu
```bash
sudo apt-get install unrar-free libreoffice
```

#### macOS
```bash
brew install unrar libreoffice
```

#### Windows
Install Microsoft Word or LibreOffice. `pywin32` is included in `requirements.txt` and installed automatically on Windows.

## Usage

All tools share the same base flags: an optional positional `directory`
(default: current directory), `-o/--output`, `-f/--format txt|json`, and
`-l/--log-level`. Results are printed to stdout (pipeable); diagnostics and
progress go to stderr.

Installing the package (`pip install -e .`) provides a single `organizers`
command with one subcommand per tool:

```bash
organizers videos /path/to/movies -o report.txt
organizers series          # seriesLength.py
organizers pages           # pageCounter.py
organizers comics          # comanga.py
organizers steam           # steamSorter.py
organizers doc2docx        # doc2docx.py
```

The standalone `python <script>.py` invocations below keep working unchanged.

### length.py

Analyzes video file durations in a directory tree.

```bash
# Current directory
python length.py

# Specific directory, custom output file
python length.py /path/to/videos -o my_analysis.txt

# JSON output
python length.py -f json -o analysis.json

# Custom extensions and excluded subdirectory names
python length.py -e .mp4 .mkv .avi -x Subtitles Extras

# Verbose logging
python length.py -l DEBUG
```

**Arguments:**

| Flag | Default | Description |
|------|---------|-------------|
| `directory` | `.` | Directory to analyze |
| `-o` | `video_duration_analysis.txt` | Output file path |
| `-f` | `txt` | Output format: `txt` or `json` |
| `-e` | `.mp4 .avi .mkv .mov .wmv .flv .webm` | File extensions to include |
| `-x` | `Sub Subs Subtitles Featurettes Extras` | Subdirectory names to skip |
| `-l` | `INFO` | Log level: `DEBUG INFO WARNING ERROR` |

Output file: `video_duration_analysis.txt` (or path given with `-o`).
Log file: `video_analyzer.log`.

### pageCounter.py

Counts pages in documents in a directory (non-recursive).

```bash
python pageCounter.py [directory] [-o output.txt] [-f json]
```

**Supported formats:** PDF, EPUB, DOCX.

DOCX page count is estimated: explicit page breaks are counted, and if none are found, total characters divided by 2000 is used as a fallback.

Output file: `document_page_counts.txt`.

### steamSorter.py

Fetches your Steam library and looks up main-story completion times from HowLongToBeat.

**Setup — set environment variables before running:**

```bash
# Linux/macOS
export STEAM_API_KEY="your_api_key_here"
export STEAM_IDS="76561197960287930,76561197960287931"

# Windows (PowerShell)
$env:STEAM_API_KEY = "your_api_key_here"
$env:STEAM_IDS    = "76561197960287930,76561197960287931"
```

```bash
python steamSorter.py
```

`STEAM_IDS` accepts one or more comma-separated Steam 64-bit user IDs. Duplicate games across libraries are deduplicated. HLTB requests are rate-limited to one per second.

HLTB results are cached on disk for 90 days (`%LOCALAPPDATA%\organizers\hltb_cache.json` on Windows, `~/.cache/organizers/hltb_cache.json` elsewhere), so reruns complete in seconds and an interrupted run resumes where it left off.

Output file: `steam_games_completion_times.txt`.

### seriesLength.py

Calculates total video duration for each subdirectory, treating each subdirectory as a separate TV series.

```bash
python seriesLength.py [directory] [-o output.txt] [-f json]
```

Output file: `series_durations.txt`.

### comanga.py

Counts pages in comic/manga files and directories.

```bash
# Current directory
python comanga.py

# Specific directory
python comanga.py /path/to/comics
```

**Supported formats:** CBZ, CBR, EPUB, PDF.

Each immediate child of the target directory is analyzed: files are counted directly, subdirectories are scanned recursively (treated as series). Processing is parallelized with a thread pool (up to 8 workers).

Output file: `comanga_page_counts.txt`.

### doc2docx.py

Converts all `.doc` files in a directory to `.docx`.

```bash
python doc2docx.py [directory] [-o output_dir] [--no-skip-existing]
```

Converted files are written to `<directory>/output/` (or the directory given with `-o`). Original `.doc` files are not modified. Files whose `.docx` already exists in the output directory are skipped; pass `--no-skip-existing` to re-convert them.

**Conversion backends (tried in order):**
1. Microsoft Word via COM (Windows, requires pywin32 + Word installed)
2. LibreOffice headless (all platforms)

## Project Structure

```
organizers/
├── pyproject.toml           # packaging, ruff and pytest config; `organizers` entry point
├── requirements.txt
├── shared_utils.py          # DEPRECATED re-export shim — emits DeprecationWarning, removed next minor version
├── organizers_cli.py        # `organizers` command: one subcommand per tool
├── length.py                # CLI: video duration analyzer
├── seriesLength.py          # CLI: TV series duration analyzer
├── pageCounter.py           # CLI: document page counter
├── comanga.py               # CLI: comic/manga page counter
├── doc2docx.py              # CLI: DOC to DOCX converter
├── steamSorter.py           # CLI: Steam game completion analyzer
├── core/
│   ├── cli.py               # shared argparse contract (directory, -o, -f, -l)
│   ├── formatters.py        # pure formatting helpers (duration, file size)
│   ├── fs.py                # streaming file discovery and access checks
│   ├── loaders.py           # lazy-import registry for optional dependencies
│   ├── log.py               # logging setup (diagnostics to stderr)
│   └── output.py            # ProgressReporter and txt/json result serializer
├── readers/
│   ├── pages.py             # pure page-count readers (PDF, EPUB, CBZ, CBR, DOCX)
│   └── video.py             # pure video duration reader (ffprobe, moviepy fallback)
├── analyzers/
│   ├── comics.py            # comic/manga directory scanner with thread pool
│   ├── documents.py         # document directory scanner
│   ├── steam.py             # SteamClient, HltbClient, HltbCache, analyze_libraries()
│   └── video.py             # analyze_flat() and analyze_series(), thread pool
├── converters/
│   └── doc2docx.py          # DOC-to-DOCX conversion backends and orchestration
├── tests/                   # pytest suite (no network, no codecs required)
├── organizers_c/            # FROZEN Windows-native C ports (MSVC) — reference only
└── organizers_posix/        # FROZEN Linux/macOS C ports (gcc/clang) — reference only
```

### Layer responsibilities

| Layer | Responsibility |
|-------|---------------|
| CLI entry points | Argument parsing, logging setup, display, output file writing |
| `analyzers/` | Directory scanning, orchestration, progress reporting |
| `readers/` | Reading a single file and returning raw data |
| `core/` | Formatting, filesystem utilities, lazy loaders, logging, output helpers |

`shared_utils.py` is a deprecated backward-compatibility shim that re-exports symbols from `core/` and `readers/`. It emits a `DeprecationWarning` on import and will be removed in the next minor version — import directly from those packages.

### C ports (frozen)

`organizers_c/` (Windows, built with Visual Studio Build Tools / MSVC `cl`) and `organizers_posix/` (Linux/macOS, gcc/clang) contain C rewrites of all six tools. **Both trees are frozen and unmaintained**: they are kept as reference implementations, receive no feature updates, and may drift from the Python behavior. CI compiles both trees on every push so they cannot silently rot. See each folder's README for build instructions; compiled binaries are not committed.

## Configuration

### Environment Variables

| Variable | Used by | Description |
|----------|---------|-------------|
| `STEAM_API_KEY` | `steamSorter.py` | Steam Web API key (required) |
| `STEAM_IDS` | `steamSorter.py` | Comma-separated Steam 64-bit user IDs (required) |

### Logging

All scripts use the `organizers` logger and accept `-l/--log-level` (`DEBUG INFO WARNING ERROR`, default `INFO`). Diagnostics go to stderr; results are printed to stdout so output stays pipeable at any log level. `length.py` additionally writes to `video_analyzer.log`.

## Troubleshooting

### Import errors

```bash
# Core dependencies
pip install pypdf python-docx ebooklib moviepy rarfile

# API tools
pip install requests howlongtobeatpy

# Windows DOC conversion
pip install pywin32
```

### Video processing

- Files smaller than 100 KB are skipped (trailers, thumbnails).
- Corrupted or codec-unsupported files are logged and skipped.

### Document processing

- Encrypted PDFs cannot be read and are skipped.
- DOCX page counts are estimates; results may differ from Word's page count.
- CBR files require `unrar` or `unrar-free` to be installed on the system.

### Steam

- `STEAM_API_KEY` and `STEAM_IDS` must be set before running.
- HLTB data may be missing for niche or very new games.
- The script is rate-limited to 1 HLTB request per second to avoid being blocked.

### Getting help

1. Check the log output for per-file error messages.
2. Run `length.py` with `-l DEBUG` for detailed video processing logs.
3. Verify all dependencies are installed with `pip list`.

## Contribution

1. Fork the repository
2. Create a new branch
3. Make your changes
4. Submit a pull request

Please report issues in the Issues section.

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE).

## Credits

- @based-on-what — Main developer
