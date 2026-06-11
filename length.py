#!/usr/bin/env python3
"""Video duration analyzer CLI entry point."""

import time
from pathlib import Path
from typing import List, Optional

from analyzers.video import DEFAULT_EXCLUDE, DEFAULT_EXTENSIONS, analyze_flat
from core.cli import build_parser, resolve_directory
from core.formatters import format_duration, format_file_size
from core.fs import find_files_by_extensions
from core.log import setup_logging
from core.output import save_results


def _enriched(results: dict) -> dict:
    """Add display fields to raw analyzer output (JSON layout compatibility)."""
    return {
        path: {
            **info,
            'formatted_duration': format_duration(info['duration']),
            'formatted_size': format_file_size(info['file_size']),
        }
        for path, info in results.items()
    }


def save_video_results(results: dict, output_file: Path, format_type: str = "txt") -> None:
    sorted_items = sorted(results.items(), key=lambda x: x[1]['duration'])
    results_text = []
    for file_path, info in sorted_items:
        results_text.extend([
            f"File: {Path(file_path).name}",
            f"Path: {file_path}",
            f"Duration: {format_duration(info['duration'])}",
            f"Size: {format_file_size(info['file_size'])}",
            "-" * 50,
        ])

    total_duration = sum(info['duration'] for info in results.values())
    total_size = sum(info['file_size'] for info in results.values())
    file_count = len(results)

    results_text.extend([
        "", "SUMMARY:",
        f"Total files: {file_count}",
        f"Total duration: {format_duration(total_duration)}",
        f"Total size: {format_file_size(total_size)}",
    ])
    if file_count > 0:
        results_text.extend([
            f"Average duration: {format_duration(total_duration / file_count)}",
            f"Average size: {format_file_size(total_size / file_count)}",
        ])

    save_results(
        output_file, results_text, _enriched(results),
        "VIDEO DURATION ANALYSIS REPORT", format_type,
    )


def print_summary(results: dict) -> None:
    if not results:
        print("No video files were successfully processed")
        return

    total_duration = sum(info['duration'] for info in results.values())
    total_size = sum(info['file_size'] for info in results.values())
    file_count = len(results)

    print("=" * 50)
    print("SUMMARY STATISTICS")
    print("=" * 50)
    print(f"Total files processed: {file_count}")
    print(f"Total duration: {format_duration(total_duration)}")
    print(f"Total size: {format_file_size(total_size)}")
    print(f"Average duration: {format_duration(total_duration / file_count)}")
    print(f"Average size: {format_file_size(total_size / file_count)}")


def main(argv: Optional[List[str]] = None) -> None:
    parser = build_parser(
        "Analyze video file durations in directories",
        default_output="video_duration_analysis.txt",
    )
    parser.add_argument("-e", "--extensions", nargs="+", default=list(DEFAULT_EXTENSIONS))
    parser.add_argument("-x", "--exclude", nargs="+", default=list(DEFAULT_EXCLUDE))
    args = parser.parse_args(argv)

    logger = setup_logging(args.log_level, "video_analyzer.log")

    base_path = resolve_directory(args.directory, logger)
    output_file = Path(args.output)
    video_extensions = {ext.lower() for ext in args.extensions}
    excluded_dirs = set(args.exclude)

    logger.info(f"Starting video analysis in: {base_path}")
    logger.info(f"Video extensions: {', '.join(video_extensions)}")
    logger.info(f"Excluded directories: {', '.join(excluded_dirs)}")

    start_time = time.time()

    logger.info("Scanning for video files...")
    video_files = list(find_files_by_extensions(
        base_path, video_extensions, exclude_dirs=excluded_dirs, recursive=True
    ))

    if not video_files:
        logger.warning("No video files found!")
        return

    logger.info(f"Found {len(video_files)} video files")
    results = analyze_flat(video_files)

    if results:
        logger.info(f"Saving results to: {output_file}")
        save_video_results(results, output_file, args.format)
    else:
        logger.warning("No results to save - all files failed to process")

    processing_time = time.time() - start_time
    logger.info(f"Processing completed in {processing_time:.2f} seconds")
    print_summary(results)


if __name__ == "__main__":
    main()
