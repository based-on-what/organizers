# Organizers

A collection of Python scripts for organizing and managing files based on their properties - including video duration, document formats, and media content length.

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
- [Contribution](#contribution)
- [Credits](#credits)

## General Information
This project consists of several Python scripts that help you organize and manage different types of files:
- Video files by duration
- Books and documents by page count
- Steam games by completion time
- TV series by total duration
- Comics/manga by page count
- Convert legacy Word documents to modern format

## Features
- `length.py`: Organizes video files based on their duration
- `pageCounter.py`: Organizes book files based on their page count
- `steamSorter.py`: Organizes Steam games by completion time using HowLongToBeat data
- `seriesLength.py`: Calculates total duration of TV series by scanning subdirectories
- `comanga.py`: Analyzes and organizes comics/manga files by page count
- `doc2docx.py`: Converts legacy .doc files to modern .docx format

## Technologies Used
- Python 3.x
- Libraries:
  - moviepy (video processing)
  - PyPDF2 (PDF handling)
  - Steam API
  - HowLongToBeat API
  - python-docx (Word document processing)

## Prerequisites
- Python 3.6 or higher
- pip (Python package manager)
- Steam account (for steamSorter.py)
- Microsoft Word or compatible software (for doc2docx.py)

## Installation
1. Clone this repository:
```bash
git clone https://github.com/based-on-what/organizers.git
```

2. Navigate to the project directory:
```bash
cd organizers
```

3. Install required dependencies:
```bash
pip install -r requirements.txt
```

## Usage
### length.py
Organizes video files by duration:
```bash
python length.py
```

### pageCounter.py
Organizes PDF files by page count:
```bash
python pageCounter.py
```

### steamSorter.py
Organizes Steam games by completion time:
```bash
python steamSorter.py
```

### seriesLength.py
Calculates total duration of TV series in subdirectories:
```bash
python seriesLength.py
```

### comanga.py
Analyzes and organizes comics/manga by page count:
```bash
python comanga.py
```

### doc2docx.py
Converts old .doc files to .docx format:
```bash
python doc2docx.py
```

## Contribution
Contributions are welcome! To contribute:
1. Fork the repository
2. Create a new branch
3. Make your changes
4. Submit a pull request

Please report any issues in the Issues section.

## Credits
- @based-on-what - Main developer
