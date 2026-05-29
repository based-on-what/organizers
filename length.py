#!/usr/bin/env python3
"""Video duration analyzer CLI entry point."""

import argparse
import json
import logging
import sys
import time
from pathlib import Path

from analyzers.video import analyze_flat, DEFAULT_EXTENSIONS, DEFAULT_EXCLUDE
from core.fs import find_files_by_extensions
from core.formatters import format_duration, format_file_size
from core.log import setup_logging
from core.output import save_results_to_file


def save_results(results: dict, output_file: Path, format_type: str = "txt") -> None:
    try:
        if format_type.lower() == "json":
            with open(output_file, 'w', encoding='utf-8') as f:
                json.dump(results, f, indent=2, ensure_ascii=False)
        else:
            sorted_items = sorted(results.items(), key=lambda x: x[1]['duration'])
            results_text = []
            for file_path, info in sorted_items:
                results_text.extend([
                    f"File: {Path(file_path).name}",
                    f"Path: {file_path}",
                    f"Duration: {info['formatted_duration']}",
                    f"Size: {info['formatted_size']}",
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

            save_results_to_file(results_text, output_file, "VIDEO DURATION ANALYSIS REPORT")
    except Exception:
        logging.exception(f"Error saving results to {output_file}")


def print_summary(results: dict) -> None:
    log = logging.getLogger("organizers")
    if not results:
        log.info("No video files were successfully processed")
        return

    total_duration = sum(info['duration'] for info in results.values())
    total_size = sum(info['file_size'] for info in results.values())
    file_count = len(results)

    log.info("=" * 50)
    log.info("SUMMARY STATISTICS")
    log.info("=" * 50)
    log.info(f"Total files processed: {file_count}")
    log.info(f"Total duration: {format_duration(total_duration)}")
    log.info(f"Total size: {format_file_size(total_size)}")
    log.info(f"Average duration: {format_duration(total_duration / file_count)}")
    log.info(f"Average size: {format_file_size(total_size / file_count)}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Analyze video file durations in directories")
    parser.add_argument("directory", nargs="?", default=".", help="Directory to analyze")
    parser.add_argument("-o", "--output", default="video_duration_analysis.txt")
    parser.add_argument("-f", "--format", choices=["txt", "json"], default="txt")
    parser.add_argument("-e", "--extensions", nargs="+", default=list(DEFAULT_EXTENSIONS))
    parser.add_argument("-x", "--exclude", nargs="+", default=list(DEFAULT_EXCLUDE))
    parser.add_argument("-l", "--log-level", choices=["DEBUG", "INFO", "WARNING", "ERROR"], default="INFO")
    args = parser.parse_args()

    logger = setup_logging(args.log_level, "video_analyzer.log")

    base_path = Path(args.directory).resolve()
    output_file = Path(args.output)
    video_extensions = {ext.lower() for ext in args.extensions}
    excluded_dirs = set(args.exclude)

    if not base_path.exists():
        logger.error(f"Directory does not exist: {base_path}")
        sys.exit(1)
    if not base_path.is_dir():
        logger.error(f"Path is not a directory: {base_path}")
        sys.exit(1)

    logger.info(f"Starting video analysis in: {base_path}")
    logger.info(f"Video extensions: {', '.join(video_extensions)}")
    logger.info(f"Excluded directories: {', '.join(excluded_dirs)}")

    start_time = time.time()

    logger.info("Scanning for video files...")
    video_files = find_files_by_extensions(
        base_path, video_extensions, exclude_dirs=excluded_dirs, recursive=True
    )

    if not video_files:
        logger.warning("No video files found!")
        return

    logger.info(f"Found {len(video_files)} video files")
    results = analyze_flat(video_files)

    if results:
        logger.info(f"Saving results to: {output_file}")
        save_results(results, output_file, args.format)
    else:
        logger.warning("No results to save - all files failed to process")

    processing_time = time.time() - start_time
    logger.info(f"Processing completed in {processing_time:.2f} seconds")
    print_summary(results)


if __name__ == "__main__":
    main()
