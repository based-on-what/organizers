"""
Comic/manga directory analyzer.

Responsibility: scan a directory, count pages per item, return results dict.
No display, no file writing — that belongs in the CLI entry point.

The thread pool consumes individual files, not top-level items, so one giant
series directory cannot starve the other workers. Results are aggregated per
top-level item on the main thread (as_completed), so no locking is needed.
"""
import logging
import os
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Dict

from core.fs import find_files_by_extensions, safe_file_operation
from core.output import ProgressReporter
from readers.pages import count_pages, supported_extensions

_log = logging.getLogger("organizers")

# Comics use all page formats except DOCX
_EXTENSIONS = supported_extensions() - frozenset({'.docx'})

# I/O-bound work: threads win; cap to avoid thrashing slow disks
_MAX_WORKERS = min(8, (os.cpu_count() or 4))


def _count_file(file_path: Path) -> int:
    """Page count for one file; 0 when inaccessible or unreadable."""
    try:
        if not safe_file_operation(file_path):
            _log.warning(f"Skipping inaccessible file: {file_path}")
            return 0
        return count_pages(file_path)
    except Exception as e:
        _log.error(f"Error processing {file_path.name}: {e}")
        return 0


def analyze_directory(directory: Path) -> Dict[str, int]:
    """
    Return {name: page_count} for each item directly under directory.
    Subdirectories are treated as series and scanned recursively.
    """
    results: Dict[str, int] = {}

    if not directory.exists():
        _log.error(f"Directory does not exist: {directory}")
        return results

    _log.info(f"Analyzing: {directory}")

    # (item_name, file) pairs — the pool works on files so all workers stay busy
    work = []
    for item in directory.iterdir():
        if item.is_dir():
            results[item.name] = 0
            for f in find_files_by_extensions(item, _EXTENSIONS, recursive=True):
                work.append((item.name, f))
        elif item.suffix.lower() in _EXTENSIONS:
            results[item.name] = 0
            work.append((item.name, item))

    if not work:
        return results

    progress = ProgressReporter(len(work), "Counting pages")
    with ThreadPoolExecutor(max_workers=_MAX_WORKERS) as pool:
        futures = {pool.submit(_count_file, f): name for name, f in work}
        for fut in as_completed(futures):
            try:
                results[futures[fut]] += fut.result()
            except Exception as e:
                _log.error(f"Unexpected error processing {futures[fut]}: {e}")
            progress.update()
    progress.finish()

    for name, pages in results.items():
        _log.info(f"📄 {name}: {pages} pages")

    return results
