#!/usr/bin/env python3
"""Document page counter CLI entry point."""

from pathlib import Path
from typing import List, Optional, Tuple

from analyzers.documents import analyze_directory
from core.cli import build_parser, resolve_directory
from core.log import setup_logging
from core.output import save_results


def display_and_save_results(
    files_with_pages: List[Tuple[str, int]],
    output_file: str = "document_page_counts.txt",
    fmt: str = "txt",
) -> None:
    if not files_with_pages:
        print("No files were successfully processed.")
        return

    sorted_files = sorted(files_with_pages, key=lambda x: x[1])

    print("\nFiles sorted by page count:")
    total_pages = 0
    for filename, pages in sorted_files:
        print(f"{filename}: {pages} pages")
        total_pages += pages

    print("\nSummary:")
    print(f"Total files: {len(sorted_files)}")
    print(f"Total pages: {total_pages}")

    results_text = [f"{filename}: {pages} pages" for filename, pages in sorted_files]
    results_text.extend(["", f"Total files: {len(sorted_files)}", f"Total pages: {total_pages}"])

    json_payload = {
        "files": [{"name": n, "pages": p} for n, p in sorted_files],
        "total_files": len(sorted_files),
        "total_pages": total_pages,
    }
    save_results(
        Path(output_file), results_text, json_payload,
        "FILES SORTED BY PAGE COUNT", fmt,
    )


def main(argv: Optional[List[str]] = None) -> None:
    parser = build_parser(
        "Count pages in PDF, EPUB and DOCX documents",
        default_output="document_page_counts.txt",
    )
    args = parser.parse_args(argv)
    logger = setup_logging(args.log_level)

    target = resolve_directory(args.directory, logger)
    logger.info(f"Scanning directory: {target}")

    files_with_pages = analyze_directory(target)
    display_and_save_results(files_with_pages, args.output, args.format)


if __name__ == "__main__":
    main()
