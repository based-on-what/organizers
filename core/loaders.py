"""
Lazy-import registry for optional heavy dependencies.

Each loader runs its try/except once at first call and caches the result.
All scripts import from here — no duplicated try/except blocks elsewhere.
"""
import logging

_log = logging.getLogger("organizers")

_PDF_READER = None
_EPUB_MODULES = None
_RAR_FILE = None
_VIDEO_FILE_CLIP = None
_DOCX_DOCUMENT = None
_REQUESTS_MOD = None
_HLTB_CLS = None


def get_pdf_reader():
    """Return PdfReader class (pypdf preferred, PyPDF2 fallback)."""
    global _PDF_READER
    if _PDF_READER is None:
        try:
            from pypdf import PdfReader
            _PDF_READER = PdfReader
        except ImportError:
            try:
                from PyPDF2 import PdfReader  # noqa: F401 — legacy fallback
                _log.warning("Using deprecated PyPDF2. Install pypdf instead.")
                _PDF_READER = PdfReader
            except ImportError:
                raise ImportError(
                    "Neither pypdf nor PyPDF2 available. Install with: pip install pypdf"
                ) from None
    return _PDF_READER


def get_epub_modules():
    """Return (ebooklib, epub) tuple."""
    global _EPUB_MODULES
    if _EPUB_MODULES is None:
        try:
            import ebooklib
            from ebooklib import epub
            _EPUB_MODULES = (ebooklib, epub)
        except ImportError:
            raise ImportError("ebooklib required. Install with: pip install ebooklib") from None
    return _EPUB_MODULES


def get_rar_file():
    """Return RarFile class."""
    global _RAR_FILE
    if _RAR_FILE is None:
        try:
            from rarfile import RarFile
            _RAR_FILE = RarFile
        except ImportError:
            raise ImportError("rarfile required. Install with: pip install rarfile") from None
    return _RAR_FILE


def get_video_file_clip():
    """Return VideoFileClip class from moviepy."""
    global _VIDEO_FILE_CLIP
    if _VIDEO_FILE_CLIP is None:
        try:
            from moviepy.editor import VideoFileClip
            _VIDEO_FILE_CLIP = VideoFileClip
        except ImportError:
            raise ImportError("moviepy required. Install with: pip install moviepy") from None
    return _VIDEO_FILE_CLIP


def get_docx_document():
    """Return Document class from python-docx."""
    global _DOCX_DOCUMENT
    if _DOCX_DOCUMENT is None:
        try:
            from docx import Document
            _DOCX_DOCUMENT = Document
        except ImportError:
            raise ImportError(
                "python-docx required. Install with: pip install python-docx"
            ) from None
    return _DOCX_DOCUMENT


def get_requests():
    """Return the requests module."""
    global _REQUESTS_MOD
    if _REQUESTS_MOD is None:
        try:
            import requests
            _REQUESTS_MOD = requests
        except ImportError:
            raise ImportError("requests required. Install with: pip install requests") from None
    return _REQUESTS_MOD


def get_howlongtobeat():
    """Return HowLongToBeat class."""
    global _HLTB_CLS
    if _HLTB_CLS is None:
        try:
            from howlongtobeatpy import HowLongToBeat
            _HLTB_CLS = HowLongToBeat
        except ImportError:
            raise ImportError(
                "howlongtobeatpy required. Install with: pip install howlongtobeatpy"
            ) from None
    return _HLTB_CLS
