#!/usr/bin/env python3
"""Document page counter CLI entry point."""

import logging
from pathlib import Path
from typing import List, Tuple

from analyzers.documents import analyze_directory
from core.log import setup_logging
from core.output import save_results_to_file


def display_and_save_results(
    files_with_pages: List[Tuple[str, int]],
    output_file: str = "document_page_counts.txt",
) -> None:
    log = logging.getLogger("organizers")

    if not files_with_pages:
        log.info("No files were successfully processed.")
        return

    sorted_files = sorted(files_with_pages, key=lambda x: x[1])

    log.info("\nFiles sorted by page count:")
    total_pages = 0
    for filename, pages in sorted_files:
        log.info(f"{filename}: {pages} pages")
        total_pages += pages

    log.info("\nSummary:")
    log.info(f"Total files: {len(sorted_files)}")
    log.info(f"Total pages: {total_pages}")

    results_text = [f"{filename}: {pages} pages" for filename, pages in sorted_files]
    results_text.extend(["", f"Total files: {len(sorted_files)}", f"Total pages: {total_pages}"])

    if save_results_to_file(results_text, Path(output_file), "FILES SORTED BY PAGE COUNT"):
        log.info(f"Results saved to '{output_file}'")


def main() -> None:
    logger = setup_logging("INFO")
    current_directory = Path.cwd()
    logger.info(f"Scanning directory: {current_directory}")

    files_with_pages = analyze_directory(current_directory)
    display_and_save_results(files_with_pages)


if __name__ == "__main__":
    main()
