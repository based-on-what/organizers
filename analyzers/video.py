"""
Video duration analyzers.

Two modes:
  analyze_flat()   — processes a list of files (used by length.py)
  analyze_series() — groups by subdirectory (used by seriesLength.py)

No display, no file writing — that belongs in the CLI entry points.
"""
import logging
from pathlib import Path
from typing import Dict, List, Optional, Set

from core.fs import find_files_by_extensions
from core.formatters import format_duration, format_file_size
from core.output import ProgressReporter
from readers.video import read_duration

_log = logging.getLogger("organizers")

DEFAULT_EXTENSIONS: Set[str] = {'.mp4', '.avi', '.mkv', '.mov', '.wmv', '.flv', '.webm'}
DEFAULT_EXCLUDE: Set[str] = {"Sub", "Subs", "Subtitles", "Featurettes", "Extras"}


def analyze_flat(video_files: List[Path]) -> Dict[str, Dict]:
    """
    Analyze a pre-collected list of video files.
    Returns {path_str: {duration, file_size, formatted_duration, formatted_size}}.
    """
    if not video_files:
        return {}

    results: Dict[str, Dict] = {}
    progress = ProgressReporter(len(video_files), "Analyzing videos")
    _log.info(f"Starting sequential processing of {len(video_files)} files")

    for path in video_files:
        try:
            result = read_duration(path)
            if result is not None:
                duration, size = result
                results[str(path)] = {
                    'duration': duration,
                    'file_size': size,
                    'formatted_duration': format_duration(duration),
                    'formatted_size': format_file_size(size),
                }
                _log.info(
                    f"✓ {path.name}: {format_duration(duration)} | {format_file_size(size)}"
                )
            else:
                _log.warning(f"✗ Skipped: {path.name} (unable to process)")
        except KeyboardInterrupt:
            _log.info("Process interrupted by user")
            break
        except Exception:
            _log.exception(f"✗ Error processing {path.name}")

        progress.update()

    progress.finish()
    return results


def analyze_series(
    base_directory: Path,
    extensions: Optional[Set[str]] = None,
    excluded_dirs: Optional[Set[str]] = None,
) -> Dict[str, float]:
    """
    Group video files by subdirectory and sum durations.
    Returns {series_name: total_seconds}.
    """
    extensions = extensions or DEFAULT_EXTENSIONS
    excluded_dirs = excluded_dirs or DEFAULT_EXCLUDE
    series: Dict[str, float] = {}

    for item in base_directory.iterdir():
        if not item.is_dir() or item.name in excluded_dirs:
            continue

        video_files = find_files_by_extensions(
            item, extensions, exclude_dirs=excluded_dirs, recursive=True
        )

        if not video_files:
            _log.info(f"📁 {item.name}: No video files found")
            series[item.name] = 0
            continue

        total = 0.0
        progress = ProgressReporter(len(video_files), f"Processing {item.name}")
        for vf in video_files:
            result = read_duration(vf)
            if result:
                total += result[0]
            progress.update()
        progress.finish()

        series[item.name] = total
        _log.info(f"📺 {item.name}: {len(video_files)} files, {format_duration(total)}")

    return series
