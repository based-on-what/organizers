# Organizers

Python scripts that sort your files based on how long they are, using their length (videos) or page numbers (books).

## Table of Contents
- [General Information](#general-information)
- [Features](#features)
- [Technologies Used](#technologies-used)
- [Installation](#installation)
- [Usage](#usage)
  - [length.py](#lengthpy)
  - [pageCounter.py](#pagecounterpy)
- [Contribution](#contribution)
- [Credits](#credits)

## General Information
This project consists of two Python scripts, `length.py` and `pageCounter.py`, that help you organize your files based on their duration (videos) or number of pages (books).

## Features
- `length.py`: Checks the video files in the directory where it is running and organizes the directories and/or files based on how long they are.
- `pageCounter.py`: Organizes book files based on the number of pages they contain. However, this script still has problems organizing directories within directories.

## Technologies Used
- Python

## Installation
1. Clone this repository:
```bash
git clone https://github.com/based-on-what/organizers.git
```
2. Navigate to the project directory:
```bash
cd organizers
```

## Usage
### length.py
`length.py` is responsible for checking the video files in the directory where it is running and organizing the directories and/or files based on how long they are. You can run it as follows:

```bash
python length.py
```

This script will create new directories based on the duration of the videos and move them to those directories.

### pageCounter.py
`pageCounter.py` organizes book files based on the number of pages they contain. You can run it as follows:

```bash
python pageCounter.py
```

This script will create new directories based on the number of pages of the files and move them to those directories. However, it still has problems organizing directories within directories.

## Contribution
If you would like to contribute to this project, you can do so by submitting a pull request or reporting issues in the Issues section.

## Credits
- @based-on-what - Main developer
