"""
Backward-compatibility shim.

All symbols now live in core/ and readers/. This file re-exports them so
any external scripts or notebooks importing from shared_utils continue to work.
"""
from core.formatters import format_duration, format_file_size
from core.fs import find_files_by_extensions, safe_file_operation
from core.loaders import (
    get_docx_document as _get_docx_document,
    get_epub_modules,
    get_pdf_reader,
    get_rar_file,
    get_video_file_clip,
)
from core.log import setup_logging
from core.output import ProgressReporter, save_results_to_file
from readers.pages import count_epub_pages, count_pdf_pages

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
