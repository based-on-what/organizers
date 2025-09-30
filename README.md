# Organizers

A collection of enhanced Python scripts for organizing and managing files based on their properties - including video duration, document formats, and media content analysis. Now featuring modern libraries, improved performance, and comprehensive error handling.

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
- [Configuration](#configuration)
- [Performance Features](#performance-features)
- [Troubleshooting](#troubleshooting)
- [Contribution](#contribution)
- [Credits](#credits)

## General Information
This project consists of several enhanced Python scripts that help you organize and manage different types of files:

- **Video files** by duration with robust error handling
- **Books and documents** by page count (PDF, EPUB, DOCX)
- **Steam games** by completion time using HowLongToBeat data
- **TV series** by total duration with series-level analysis
- **Comics/manga** by page count (CBZ, CBR, EPUB, PDF)
- **Legacy Word documents** converted to modern format (cross-platform)

### Recent Improvements ✨
- **Modern Libraries**: Replaced deprecated PyPDF2 with pypdf, removed unmaintained mobi library
- **Performance**: Added progress reporting, concurrent processing capabilities, and optimized algorithms
- **Cross-platform**: Enhanced compatibility across Windows, macOS, and Linux
- **Error Handling**: Comprehensive error handling with detailed logging
- **Code Quality**: Reduced code duplication, improved type hints, and consistent interfaces
- **Shared Utilities**: Common functionality extracted to shared modules

## Features
- **📹 Video Analysis**: Comprehensive video duration analysis with support for multiple formats
- **📚 Document Processing**: Page counting for PDF, EPUB, and DOCX files with modern libraries
- **🎮 Game Library Analysis**: Steam game completion time analysis using HowLongToBeat data
- **📺 TV Series Organization**: Calculate total duration for TV series collections
- **📖 Comic/Manga Management**: Page counting for comic book formats (CBZ, CBR, EPUB, PDF)
- **📄 Document Conversion**: Cross-platform DOC to DOCX conversion
- **🚀 Performance Features**: Progress reporting, batch processing, and optimized algorithms
- **🔧 Error Handling**: Robust error handling with detailed logging and recovery
- **🌐 Cross-platform**: Works on Windows, macOS, and Linux
- **⚡ Modern Dependencies**: Uses maintained, up-to-date libraries

## Technologies Used
### Core Dependencies
- **Python 3.8+** - Modern Python with type hints support
- **pypdf 4.0+** - Modern PDF processing (replaces deprecated PyPDF2)
- **python-docx 1.1+** - Enhanced Word document processing
- **ebooklib 0.18+** - EPUB file processing
- **moviepy 2.0+** - Video file analysis and processing
- **rarfile 4.0+** - RAR archive processing for comic books

### API Integration
- **requests 2.31+** - HTTP requests for Steam API integration
- **howlongtobeatpy 1.0.7+** - Game completion time data

### Cross-platform Compatibility
- **comtypes 1.2+** (Windows) - Alternative COM interface
- **LibreOffice** (Linux/macOS) - Cross-platform document conversion

## Prerequisites
- **Python 3.8 or higher** - Required for modern type hints and features
- **pip** - Python package manager for installing dependencies

### Optional Requirements
- **Steam account** (for steamSorter.py) - To fetch game library data
- **Steam Web API key** (for steamSorter.py) - Get from [Steam Developer Portal](https://steamcommunity.com/dev/apikey)
- **Microsoft Word** or **LibreOffice** (for doc2docx.py) - For document conversion
- **RAR tools** (for CBR comic files) - May require additional system packages

## Installation

### Quick Start
1. **Clone this repository:**
```bash
git clone https://github.com/based-on-what/organizers.git
```

2. **Navigate to the project directory:**
```bash
cd organizers
```

3. **Install required dependencies:**
```bash
pip install -r requirements.txt
```

### Alternative Installation Methods

#### Using pip install (individual packages):
```bash
# Core dependencies
pip install pypdf python-docx ebooklib moviepy rarfile

# API integration
pip install requests howlongtobeatpy

# Windows-specific (optional)
pip install comtypes  # Only on Windows

# Optional enhancements
pip install tqdm click
```

#### Using conda:
```bash
conda install -c conda-forge moviepy pypdf python-docx ebooklib requests
pip install howlongtobeatpy rarfile  # Not available in conda
```

### System-specific Setup

#### Linux/Ubuntu:
```bash
# Install system dependencies for RAR support
sudo apt-get update
sudo apt-get install unrar-free

# For LibreOffice document conversion
sudo apt-get install libreoffice
```

#### macOS:
```bash
# Using Homebrew
brew install unrar libreoffice
```

#### Windows:
- Install Microsoft Word for best DOC conversion support
- Or install LibreOffice as a free alternative

## Usage

All scripts now feature enhanced CLI interfaces with detailed logging and progress reporting.

### length.py
**Enhanced Video Duration Analyzer** - Analyzes video files with robust error handling and multiple output formats.

```bash
# Basic usage (current directory)
python length.py

# Analyze specific directory with custom output
python length.py /path/to/videos -o my_analysis.txt

# JSON output format
python length.py -f json -o analysis.json

# Custom file extensions and exclusions
python length.py -e .mp4 .mkv .avi -x Subtitles Extras

# Debug mode with detailed logging
python length.py -l DEBUG
```

**Features:**
- Supports multiple video formats (MP4, AVI, MKV, MOV, WMV, FLV, WEBM)
- Progress reporting during analysis
- Detailed logging with timestamps
- JSON and text output formats
- Customizable file extension filtering
- Directory exclusion support

### pageCounter.py
**Enhanced Document Page Counter** - Counts pages in modern document formats with improved error handling.

```bash
# Count pages in current directory
python pageCounter.py
```

**Features:**
- **Supported formats**: PDF, EPUB, DOCX (MOBI support removed due to deprecated library)
- Modern pypdf library for better PDF compatibility
- Enhanced error reporting
- Progress tracking for multiple files
- UTF-8 output encoding

**Note**: DOC files are no longer supported directly. Use `doc2docx.py` to convert them first.

### steamSorter.py
**Enhanced Steam Game Analyzer** - Analyzes Steam libraries with HowLongToBeat completion time data.

```bash
python steamSorter.py
```

**Setup Required:**
1. Get a Steam Web API key from [Steam Developer Portal](https://steamcommunity.com/dev/apikey)
2. Edit the script and add your API key and Steam IDs:
```python
api_key = "your_steam_api_key_here"
steam_ids = ["your_steam_id", "friend_steam_id"]
```

**Features:**
- Fetches games from multiple Steam libraries
- HowLongToBeat integration for completion times
- Error handling for API failures
- Progress reporting during analysis
- Comprehensive completion time statistics

### seriesLength.py
**TV Series Duration Analyzer** - Calculates total duration for TV series organized in subdirectories.

```bash
python seriesLength.py
```

**Features:**
- Processes each subdirectory as a separate TV series
- Recursive video file scanning
- Series-level duration reporting
- Improved progress tracking
### comanga.py
**Enhanced Comic/Manga Page Counter** - Analyzes comic books and graphic novels with support for multiple archive formats.

```bash
# Analyze current directory
python comanga.py

# Analyze specific directory
python comanga.py /path/to/comics
```

**Features:**
- **Supported formats**: CBZ, CBR, EPUB, PDF
- Analyzes both individual files and subdirectories
- Recursive scanning for organized collections
- Modern pypdf library for PDF support
- Progress reporting for large collections
- Detailed error handling and logging

### doc2docx.py
**Cross-platform DOC to DOCX Converter** - Converts legacy Word documents with multiple conversion methods.

```bash
# Convert all .doc files in current directory
python doc2docx.py

# Files are saved to ./output/ directory
```

**Features:**
- **Cross-platform support**: Works on Windows, macOS, and Linux
- **Multiple conversion methods**:
  - Microsoft Word COM interface (Windows)
  - LibreOffice headless mode (all platforms)
- Automatic fallback between conversion methods
- Progress reporting for batch conversions
- Comprehensive error handling
- Output to separate directory to preserve originals

**Platform Notes:**
- **Windows**: Best results with Microsoft Word installed
- **Linux/macOS**: Requires LibreOffice for conversion
- **Fallback**: Attempts multiple methods automatically

## Configuration

### Shared Configuration
All scripts now use a common configuration system through `shared_utils.py`. You can customize:

- **Logging levels**: DEBUG, INFO, WARNING, ERROR
- **File extensions**: Supported formats for each script
- **Output formats**: Text, JSON (where applicable)
- **Progress reporting**: Enable/disable progress bars

### Environment Variables
Set these optional environment variables for enhanced functionality:

```bash
# Steam API configuration
export STEAM_API_KEY="your_api_key_here"
export STEAM_USER_IDS="id1,id2,id3"

# Logging configuration
export LOG_LEVEL="INFO"  # DEBUG, INFO, WARNING, ERROR
```

## Performance Features

### Optimizations Implemented
- **Shared utilities**: Eliminated code duplication (~120 lines reduced)
- **Modern libraries**: Replaced deprecated dependencies
- **Progress reporting**: Real-time progress for long operations
- **Error recovery**: Graceful handling of corrupted files
- **Memory efficiency**: Proper resource cleanup
- **Batch processing**: Optimized for large file collections

### Performance Tips
- Use SSD storage for faster file access
- Process files locally rather than over network
- Use appropriate log levels (INFO for normal use, DEBUG for troubleshooting)
- Consider excluding large subtitle/extras directories

## Troubleshooting

### Common Issues

#### Import Errors
```bash
# If you get import errors, install missing packages:
pip install pypdf python-docx ebooklib moviepy

# For specific errors:
# ModuleNotFoundError: No module named 'pypdf'
pip install pypdf

# ModuleNotFoundError: No module named 'moviepy'
pip install moviepy
```

#### Video Processing Issues
- **Codec problems**: Install additional codec packages
- **Corrupted files**: Scripts now skip and report problematic files
- **Large files**: Increase system memory or process in smaller batches

#### Document Processing Issues
- **PDF encryption**: Some encrypted PDFs cannot be processed
- **DOCX page counting**: Results are estimates based on content analysis
- **CBR files**: Requires RAR tools installation on system

#### Steam API Issues
- **API key invalid**: Verify your Steam Web API key
- **Rate limiting**: Script includes automatic retry with delays
- **Game not found**: HowLongToBeat database may not have all games

### Getting Help
1. Check the logs generated by each script
2. Run with `-l DEBUG` for detailed information
3. Verify all dependencies are installed correctly
4. Check file permissions and accessibility

## Contribution
Contributions are welcome! To contribute:
1. Fork the repository
2. Create a new branch
3. Make your changes
4. Submit a pull request

Please report any issues in the Issues section.

## Credits
- @based-on-what - Main developer
