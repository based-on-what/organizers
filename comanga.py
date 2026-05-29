#!/usr/bin/env python3
"""Comic/manga page counter CLI entry point."""

import logging
from pathlib import Path

from analyzers.comics import analyze_directory
from core.log import setup_logging
from core.output import save_results_to_file

_log = logging.getLogger("organizers")


def display_results(results: dict) -> None:
    if not results:
        _log.info("No supported files found.")
        return

    sorted_results = sorted(results.items(), key=lambda x: x[1])

    _log.info("\n" + "=" * 50)
    _log.info("FINAL RESULTS (sorted by page count)")
    _log.info("=" * 50)

    lines = []
    total_pages = 0
    for name, pages in sorted_results:
        msg = f"{name}: {pages} pages"
        _log.info(msg)
        lines.append(msg)
        total_pages += pages

    _log.info("-" * 50)
    _log.info(f"Total items: {len(results)}")
    _log.info(f"Total pages: {total_pages}")

    lines.extend(["", f"Total items: {len(results)}", f"Total pages: {total_pages}"])
    save_results_to_file(lines, Path("comanga_page_counts.txt"), "COMIC/MANGA PAGE COUNT RESULTS")


def main(directory: Path = None) -> None:
    logger = setup_logging("INFO")

    if directory is None:
        directory = Path.cwd()

    logger.info(f"Starting comic/manga analysis in: {directory}")

    results = analyze_directory(directory)
    display_results(results)


if __name__ == "__main__":
    main()
