#!/usr/bin/env python3
"""DOC to DOCX converter CLI entry point."""

from pathlib import Path
from typing import List, Optional

from converters.doc2docx import convert_folder
from core.cli import build_parser, resolve_directory
from core.log import setup_logging


def main(argv: Optional[List[str]] = None) -> None:
    parser = build_parser(
        "Convert .doc files to .docx using Word or LibreOffice",
        default_output="output",
        with_format=False,
    )
    parser.add_argument(
        "--no-skip-existing", action="store_true",
        help="Re-convert files whose .docx already exists in the output directory",
    )
    args = parser.parse_args(argv)
    logger = setup_logging(args.log_level)

    directory = resolve_directory(args.directory, logger)
    convert_folder(
        directory,
        output_dir=Path(args.output) if args.output != "output" else None,
        skip_existing=not args.no_skip_existing,
    )


if __name__ == "__main__":
    main()
