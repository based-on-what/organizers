"""
Video duration analyzers.

Two modes:
  analyze_flat()   — processes a list of files (used by length.py)
  analyze_series() — groups by subdirectory (used by seriesLength.py)

No display, no file writing — that belongs in the CLI entry points.

Duration probing is subprocess/I-O bound, so files are processed by a thread
pool. Progress and result collection happen on the main thread (as_completed),
so ProgressReporter needs no locking. Callers sort results before writing,
so completion order does not matter.
"""
import logging
import os
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Dict, List, Optional, Set

from core.formatters import format_duration, format_file_size
from core.fs import find_files_by_extensions
from core.output import ProgressReporter
from readers.video import read_duration

_log = logging.getLogger("organizers")

DEFAULT_EXTENSIONS: Set[str] = {'.mp4', '.avi', '.mkv', '.mov', '.wmv', '.flv', '.webm'}
DEFAULT_EXCLUDE: Set[str] = {"Sub", "Subs", "Subtitles", "Featurettes", "Extras"}

# I/O- and subprocess-bound: threads win; cap to avoid thrashing slow disks
_MAX_WORKERS = min(8, (os.cpu_count() or 4))


def _read_durations(
    video_files: List[Path], description: str
) -> Dict[Path, tuple]:
    """Probe all files in a thread pool. Returns {path: (duration, size)}."""
    durations: Dict[Path, tuple] = {}
    progress = ProgressReporter(len(video_files), description)

    with ThreadPoolExecutor(max_workers=_MAX_WORKERS) as pool:
        futures = {pool.submit(read_duration, p): p for p in video_files}
        try:
            for fut in as_completed(futures):
                path = futures[fut]
                try:
                    result = fut.result()
                    if result is not None:
                        durations[path] = result
                    else:
                        _log.warning(f"✗ Skipped: {path.name} (unable to process)")
                except Exception:
                    _log.exception(f"✗ Error processing {path.name}")
                progress.update()
        except KeyboardInterrupt:
            _log.info("Process interrupted by user")
            pool.shutdown(wait=False, cancel_futures=True)

    progress.finish()
    return durations


def analyze_flat(video_files: List[Path]) -> Dict[str, Dict]:
    """
    Analyze a pre-collected list of video files.
    Returns {path_str: {duration, file_size}} — raw values only; formatting
    happens at display time in the CLI layer.
    """
    if not video_files:
        return {}

    _log.info(f"Processing {len(video_files)} files with {_MAX_WORKERS} workers")
    durations = _read_durations(video_files, "Analyzing videos")

    results: Dict[str, Dict] = {}
    for path, (duration, size) in durations.items():
        results[str(path)] = {'duration': duration, 'file_size': size}
        _log.info(f"✓ {path.name}: {format_duration(duration)} | {format_file_size(size)}")
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

        video_files = list(find_files_by_extensions(
            item, extensions, exclude_dirs=excluded_dirs, recursive=True
        ))

        if not video_files:
            _log.info(f"📁 {item.name}: No video files found")
            series[item.name] = 0
            continue

        durations = _read_durations(video_files, f"Processing {item.name}")
        total = sum(d for d, _ in durations.values())

        series[item.name] = total
        _log.info(f"📺 {item.name}: {len(video_files)} files, {format_duration(total)}")

    return series
