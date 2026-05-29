"""
Document page-count analyzer.

Responsibility: scan a flat directory for PDF/EPUB/DOCX, count pages,
return results list. No display, no file writing.
"""
import logging
from pathlib import Path
from typing import List, Tuple

from core.fs import find_files_by_extensions, safe_file_operation
from core.output import ProgressReporter
from readers.pages import count_pages

_log = logging.getLogger("organizers")

_EXTENSIONS = frozenset({'.pdf', '.epub', '.docx'})


def analyze_directory(directory: Path) -> List[Tuple[str, int]]:
    """
    Return [(filename, page_count)] for each supported file in directory.
    Non-recursive: documents are expected flat, not in subdirs.
    """
    files = find_files_by_extensions(directory, _EXTENSIONS, recursive=False)
    if not files:
        _log.info("No supported files found in the directory.")
        return []

    results: List[Tuple[str, int]] = []
    errors: List[Tuple[str, str]] = []
    progress = ProgressReporter(len(files), "Processing files")
    _log.info(f"Found {len(files)} supported files to process")

    for file_path in files:
        try:
            if not safe_file_operation(file_path):
                errors.append((file_path.name, "File access error"))
                progress.update()
                continue

            pages = count_pages(file_path)
            results.append((file_path.name, pages))
            _log.info(f"✓ {file_path.name}: {pages} pages")

        except Exception as e:
            _log.error(f"✗ Error processing {file_path.name}: {e}")
            errors.append((file_path.name, str(e)))

        progress.update()

    progress.finish()

    if errors:
        _log.warning(f"\nSummary of {len(errors)} files with errors:")
        for filename, error in errors:
            _log.warning(f"  {filename}: {error}")

    return results
