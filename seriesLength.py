#!/usr/bin/env python3
"""TV series duration analyzer CLI entry point."""

import logging
from pathlib import Path

from analyzers.video import analyze_series
from core.formatters import format_duration
from core.log import setup_logging
from core.output import save_results_to_file


def main() -> None:
    setup_logging("INFO")
    log = logging.getLogger("organizers")

    base_directory = Path.cwd()
    log.info("Processing video files in series directories...")

    durations = analyze_series(base_directory)

    if not durations:
        log.warning("No series directories with video files found!")
        return

    sorted_durations = dict(sorted(durations.items(), key=lambda x: x[1]))

    log.info("\nSeries sorted by total duration:")
    total_duration = 0
    for series_name, duration in sorted_durations.items():
        log.info(f"{series_name}: {format_duration(duration)}")
        total_duration += duration

    log.info("\nOverall Statistics:")
    log.info(f"Total series: {len(durations)}")
    log.info(f"Total duration: {format_duration(total_duration)}")

    results_text = [f"{name}: {format_duration(dur)}" for name, dur in sorted_durations.items()]
    save_results_to_file(results_text, Path("series_durations.txt"), "TV SERIES DURATION ANALYSIS")
    log.info("Results saved to series_durations.txt")


if __name__ == "__main__":
    main()
