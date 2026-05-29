"""
Pure page-count readers.

No I/O side effects beyond reading the target file. No logging calls.
All dispatch goes through count_pages() — single entry point replaces the
duplicated dict-based dispatchers that existed in comanga.py and pageCounter.py.
"""
import os.path
import zipfile
from pathlib import Path

from core.loaders import get_pdf_reader, get_epub_modules, get_rar_file, get_docx_document

# Without leading dot — matched against os.path.splitext()[1][1:] to avoid
# allocating a Path object for every entry in large archives.
_IMAGE_EXTENSIONS = frozenset({'jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp'})


def count_pdf_pages(file_path: Path) -> int:
    try:
        PdfReader = get_pdf_reader()
        with open(file_path, 'rb') as f:
            return len(PdfReader(f).pages)
    except Exception as e:
        raise RuntimeError(f"Error reading PDF: {file_path}") from e


def count_epub_pages(file_path: Path) -> int:
    try:
        ebooklib, epub = get_epub_modules()
        book = epub.read_epub(str(file_path))
        return len(list(book.get_items_of_type(ebooklib.ITEM_DOCUMENT)))
    except Exception as e:
        raise RuntimeError(f"Error reading EPUB: {file_path}") from e


def _is_image(name: str) -> bool:
    _, ext = os.path.splitext(name)
    return ext[1:].lower() in _IMAGE_EXTENSIONS


def count_cbz_pages(file_path: Path) -> int:
    try:
        with zipfile.ZipFile(file_path, 'r') as archive:
            return sum(1 for name in archive.namelist() if _is_image(name))
    except Exception as e:
        raise RuntimeError(f"Error reading CBZ: {file_path}") from e


def count_cbr_pages(file_path: Path) -> int:
    try:
        RarFile = get_rar_file()
        with RarFile(file_path, 'r') as archive:
            return sum(1 for name in archive.namelist() if _is_image(name))
    except Exception as e:
        raise RuntimeError(f"Error reading CBR: {file_path}") from e


def count_docx_pages(file_path: Path) -> int:
    try:
        doc = get_docx_document()(str(file_path))
        # Page breaks are <w:br w:type="page"/> XML elements, NOT text characters.
        # run.text only yields <w:t> content — checking \f there always returns 0.
        _W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
        page_breaks = 0
        total_chars = 0
        for para in doc.paragraphs:
            for br in para._p.findall(f'.//{{{_W}}}br'):
                if br.get(f'{{{_W}}}type') == 'page':
                    page_breaks += 1
            for run in para.runs:
                total_chars += len(run.text)
        if page_breaks == 0:
            return max(1, total_chars // 2000)
        return page_breaks + 1
    except Exception as e:
        raise RuntimeError(f"Error processing DOCX: {file_path}") from e


_COUNTERS = {
    '.pdf': count_pdf_pages,
    '.epub': count_epub_pages,
    '.cbz': count_cbz_pages,
    '.cbr': count_cbr_pages,
    '.docx': count_docx_pages,
}


def count_pages(file_path: Path) -> int:
    """Dispatch to the correct counter by extension. Raises RuntimeError if unsupported."""
    counter = _COUNTERS.get(file_path.suffix.lower())
    if counter is None:
        raise RuntimeError(f"Unsupported format: {file_path.suffix}")
    return counter(file_path)


def supported_extensions() -> frozenset:
    return frozenset(_COUNTERS)
