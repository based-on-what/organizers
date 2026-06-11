#!/usr/bin/env python3
"""Comic/manga page counter CLI entry point."""

from pathlib import Path
from typing import List, Optional

from analyzers.comics import analyze_directory
from core.cli import build_parser, resolve_directory
from core.log import setup_logging
from core.output import save_results


def display_results(results: dict, output_file: str, fmt: str = "txt") -> None:
    if not results:
        print("No supported files found.")
        return

    sorted_results = sorted(results.items(), key=lambda x: x[1])

    print("\n" + "=" * 50)
    print("FINAL RESULTS (sorted by page count)")
    print("=" * 50)

    lines = []
    total_pages = 0
    for name, pages in sorted_results:
        msg = f"{name}: {pages} pages"
        print(msg)
        lines.append(msg)
        total_pages += pages

    print("-" * 50)
    print(f"Total items: {len(results)}")
    print(f"Total pages: {total_pages}")

    lines.extend(["", f"Total items: {len(results)}", f"Total pages: {total_pages}"])
    json_payload = {
        "items": [{"name": n, "pages": p} for n, p in sorted_results],
        "total_items": len(results),
        "total_pages": total_pages,
    }
    save_results(
        Path(output_file), lines, json_payload,
        "COMIC/MANGA PAGE COUNT RESULTS", fmt,
    )


def main(argv: Optional[List[str]] = None) -> None:
    parser = build_parser(
        "Count pages in comic/manga files (PDF, EPUB, CBZ, CBR)",
        default_output="comanga_page_counts.txt",
    )
    args = parser.parse_args(argv)
    logger = setup_logging(args.log_level)

    directory = resolve_directory(args.directory, logger)
    logger.info(f"Starting comic/manga analysis in: {directory}")

    results = analyze_directory(directory)
    display_results(results, args.output, args.format)


if __name__ == "__main__":
    main()
