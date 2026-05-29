"""
Comic/manga directory analyzer.

Responsibility: scan a directory, count pages per item, return results dict.
No display, no file writing — that belongs in the CLI entry point.
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
    _log.info("-" * 50)

    work = []
    for item in directory.iterdir():
        if item.is_dir():
            work.append((item, True))
        elif item.suffix.lower() in _EXTENSIONS:
            work.append((item, False))

    if not work:
        return results

    def _process(item: Path, is_dir: bool):
        return item.name, (_count_series(item) if is_dir else _count_single(item))

    with ThreadPoolExecutor(max_workers=_MAX_WORKERS) as pool:
        futures = {pool.submit(_process, item, is_dir): item for item, is_dir in work}
        for fut in as_completed(futures):
            try:
                name, count = fut.result()
                results[name] = count
            except Exception as e:
                _log.error(f"Unexpected error processing {futures[fut].name}: {e}")

    return results


def _count_series(directory: Path) -> int:
    files = find_files_by_extensions(directory, _EXTENSIONS, recursive=True)
    if not files:
        return 0

    total = 0
    file_count = 0
    progress = ProgressReporter(len(files), f"Processing {directory.name}")

    for f in files:
        try:
            if safe_file_operation(f):
                total += count_pages(f)
                file_count += 1
            else:
                _log.warning(f"Skipping inaccessible file: {f}")
        except Exception as e:
            _log.error(f"Error processing {f.name}: {e}")
        progress.update()

    progress.finish()
    _log.info(f"📁 {directory.name}: {file_count} files, {total} pages")
    return total


def _count_single(file_path: Path) -> int:
    try:
        if not safe_file_operation(file_path):
            _log.warning(f"Skipping inaccessible file: {file_path}")
            return 0
        pages = count_pages(file_path)
        _log.info(f"📄 {file_path.name}: {pages} pages")
        return pages
    except Exception as e:
        _log.error(f"Error processing {file_path.name}: {e}")
        return 0
