#!/usr/bin/env python3
"""TV series duration analyzer CLI entry point."""

from pathlib import Path
from typing import List, Optional

from analyzers.video import analyze_series
from core.cli import build_parser, resolve_directory
from core.formatters import format_duration
from core.log import setup_logging
from core.output import save_results


def main(argv: Optional[List[str]] = None) -> None:
    parser = build_parser(
        "Sum video durations per series subdirectory",
        default_output="series_durations.txt",
    )
    args = parser.parse_args(argv)
    logger = setup_logging(args.log_level)

    base_directory = resolve_directory(args.directory, logger)
    logger.info("Processing video files in series directories...")

    durations = analyze_series(base_directory)

    if not durations:
        logger.warning("No series directories with video files found!")
        return

    sorted_durations = dict(sorted(durations.items(), key=lambda x: x[1]))

    print("\nSeries sorted by total duration:")
    total_duration = 0
    for series_name, duration in sorted_durations.items():
        print(f"{series_name}: {format_duration(duration)}")
        total_duration += duration

    print("\nOverall Statistics:")
    print(f"Total series: {len(durations)}")
    print(f"Total duration: {format_duration(total_duration)}")

    results_text = [f"{name}: {format_duration(dur)}" for name, dur in sorted_durations.items()]
    json_payload = {
        "series": [
            {"name": n, "seconds": s, "duration": format_duration(s)}
            for n, s in sorted_durations.items()
        ],
        "total_series": len(durations),
        "total_seconds": total_duration,
    }
    save_results(
        Path(args.output), results_text, json_payload,
        "TV SERIES DURATION ANALYSIS", args.format,
    )
    logger.info(f"Results saved to {args.output}")


if __name__ == "__main__":
    main()
