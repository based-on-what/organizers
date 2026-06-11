"""
Backward-compatibility shim. DEPRECATED — will be removed in the next minor
version. Import directly from core/ and readers/ instead.
"""
import warnings

from core.formatters import format_duration, format_file_size
from core.fs import find_files_by_extensions as _find_files_by_extensions
from core.fs import safe_file_operation
from core.loaders import (
    get_docx_document as _get_docx_document,  # noqa: F401 — legacy private export
)
from core.loaders import (
    get_epub_modules,
    get_pdf_reader,
    get_rar_file,
    get_video_file_clip,
)
from core.log import setup_logging
from core.output import ProgressReporter, save_results_to_file
from readers.pages import count_epub_pages, count_pdf_pages

warnings.warn(
    "shared_utils is deprecated and will be removed in the next minor version; "
    "import from core/ and readers/ instead",
    DeprecationWarning,
    stacklevel=2,
)


def find_files_by_extensions(*args, **kwargs):
    """Legacy signature returned a list; core.fs now yields a generator."""
    return list(_find_files_by_extensions(*args, **kwargs))


__all__ = [
    "format_duration",
    "format_file_size",
    "find_files_by_extensions",
    "safe_file_operation",
    "get_epub_modules",
    "get_pdf_reader",
    "get_rar_file",
    "get_video_file_clip",
    "setup_logging",
    "ProgressReporter",
    "save_results_to_file",
    "count_epub_pages",
    "count_pdf_pages",
]
