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
- **moviepy 1.0.3+** — video duration reading
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

Counts pages in documents in the current directory (non-recursive).

```bash
python pageCounter.py
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

Output file: `steam_games_completion_times.txt`.

### seriesLength.py

Calculates total video duration for each subdirectory of the current directory, treating each subdirectory as a separate TV series.

```bash
python seriesLength.py
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

Converts all `.doc` files in the current directory to `.docx`.

```bash
python doc2docx.py
```

Converted files are written to `./output/`. Original `.doc` files are not modified.

**Conversion backends (tried in order):**
1. Microsoft Word via COM (Windows, requires pywin32 + Word installed)
2. LibreOffice headless (all platforms)

## Project Structure

```
organizers/
├── requirements.txt
├── shared_utils.py          # backward-compat re-export shim
├── length.py                # CLI: video duration analyzer
├── seriesLength.py          # CLI: TV series duration analyzer
├── pageCounter.py           # CLI: document page counter
├── comanga.py               # CLI: comic/manga page counter
├── doc2docx.py              # CLI: DOC to DOCX converter
├── steamSorter.py           # CLI: Steam game completion analyzer
├── core/
│   ├── formatters.py        # pure formatting helpers (duration, file size)
│   ├── fs.py                # file discovery and access checks
│   ├── loaders.py           # lazy-import registry for optional dependencies
│   ├── log.py               # logging setup
│   └── output.py            # ProgressReporter and file-writing helpers
├── readers/
│   ├── pages.py             # pure page-count readers (PDF, EPUB, CBZ, CBR, DOCX)
│   └── video.py             # pure video duration reader
├── analyzers/
│   ├── comics.py            # comic/manga directory scanner with thread pool
│   ├── documents.py         # document directory scanner
│   ├── steam.py             # SteamClient, HltbClient, analyze_libraries()
│   └── video.py             # analyze_flat() and analyze_series()
└── converters/
    └── doc2docx.py          # DOC-to-DOCX conversion backends and orchestration
```

### Layer responsibilities

| Layer | Responsibility |
|-------|---------------|
| CLI entry points | Argument parsing, logging setup, display, output file writing |
| `analyzers/` | Directory scanning, orchestration, progress reporting |
| `readers/` | Reading a single file and returning raw data |
| `core/` | Formatting, filesystem utilities, lazy loaders, logging, output helpers |

`shared_utils.py` is a backward-compatibility shim that re-exports symbols from `core/` and `readers/`. New code should import directly from those packages.

## Configuration

### Environment Variables

| Variable | Used by | Description |
|----------|---------|-------------|
| `STEAM_API_KEY` | `steamSorter.py` | Steam Web API key (required) |
| `STEAM_IDS` | `steamSorter.py` | Comma-separated Steam 64-bit user IDs (required) |

### Logging

All scripts use the `organizers` logger. Log level defaults to `INFO`. `length.py` additionally writes to `video_analyzer.log`. Pass `-l DEBUG` to `length.py` for verbose output; other scripts are INFO-only.

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

No license file is currently included. Add a `LICENSE` file to define usage and distribution terms.

## Credits

- @based-on-what — Main developer
