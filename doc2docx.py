#!/usr/bin/env python3
"""DOC to DOCX converter CLI entry point."""

from converters.doc2docx import convert_folder
from core.log import setup_logging


def main() -> None:
    setup_logging("INFO")
    convert_folder()


if __name__ == "__main__":
    main()
