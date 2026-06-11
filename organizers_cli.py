#!/usr/bin/env python3
"""
Single `organizers` command with one subcommand per tool.

Each subcommand forwards remaining argv to the existing entry-point main(),
so `organizers videos -o out.txt` behaves exactly like `python length.py -o out.txt`.
The standalone scripts keep working unchanged.
"""
import argparse
import sys
from typing import List, Optional

import comanga
import doc2docx
import length
import pageCounter
import seriesLength
import steamSorter

_SUBCOMMANDS = {
    "videos": (length.main, "Analyze video file durations"),
    "series": (seriesLength.main, "Sum video durations per series subdirectory"),
    "pages": (pageCounter.main, "Count pages in PDF/EPUB/DOCX documents"),
    "comics": (comanga.main, "Count pages in comic/manga files"),
    "steam": (steamSorter.main, "Sort Steam library by completion time"),
    "doc2docx": (doc2docx.main, "Convert .doc files to .docx"),
}


def main(argv: Optional[List[str]] = None) -> None:
    if argv is None:
        argv = sys.argv[1:]

    parser = argparse.ArgumentParser(
        prog="organizers",
        description="File organization tools: videos, documents, comics, Steam games.",
    )
    sub = parser.add_subparsers(dest="command", metavar="command")
    for name, (_, help_text) in _SUBCOMMANDS.items():
        # add_help=False: the subcommand's own parser handles -h on forwarded argv
        sub.add_parser(name, help=help_text, add_help=False)

    args, rest = parser.parse_known_args(argv)
    if args.command is None:
        parser.print_help()
        sys.exit(1)

    _SUBCOMMANDS[args.command][0](rest)


if __name__ == "__main__":
    main()
