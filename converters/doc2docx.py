"""
DOC-to-DOCX conversion logic.

Two conversion backends:
  convert_with_word()        — Microsoft Word via COM (Windows only)
  convert_with_libreoffice() — LibreOffice headless (cross-platform fallback)

convert_folder() orchestrates: find files, try Word, fall back to LibreOffice.
No CLI argument parsing — that belongs in the entry point.
"""
import logging
import platform
import subprocess
from pathlib import Path
from typing import List, Optional

from core.fs import find_files_by_extensions
from core.output import ProgressReporter

_log = logging.getLogger("organizers")

_PLATFORM_WINDOWS = platform.system() == "Windows"

_WIN32_AVAILABLE = False
if _PLATFORM_WINDOWS:
    try:
        import win32com.client as win32  # noqa: F401
        import pywintypes  # noqa: F401
        _WIN32_AVAILABLE = True
    except ImportError:
        pass


def find_doc_files(input_path: Path) -> List[Path]:
    """Find .doc files (not .docx) in input_path, non-recursively."""
    return [
        p for p in find_files_by_extensions(input_path, {'.doc'}, recursive=False)
        if not p.name.lower().endswith('.docx')
    ]


def convert_with_libreoffice(input_file: Path, output_dir: Path) -> bool:
    """Convert DOC to DOCX using LibreOffice headless mode. Returns True on success."""
    try:
        check = subprocess.run(
            ['libreoffice', '--version'],
            capture_output=True, text=True, timeout=10,
        )
        if check.returncode != 0:
            return False
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False

    try:
        result = subprocess.run(
            ['libreoffice', '--headless', '--convert-to', 'docx',
             '--outdir', str(output_dir), str(input_file)],
            capture_output=True, text=True, timeout=60,
        )
        if result.returncode == 0:
            _log.info(f"✓ LibreOffice: {input_file.name}")
            return True
        _log.error(f"✗ LibreOffice failed: {result.stderr}")
        return False
    except Exception as e:
        _log.error(f"✗ LibreOffice error: {e}")
        return False


def convert_with_word(input_file: Path, output_dir: Path, word_app) -> bool:
    """Convert DOC to DOCX using Microsoft Word COM. Returns True on success."""
    if not _WIN32_AVAILABLE:
        return False
    try:
        output_file = output_dir / f"{input_file.stem}.docx"
        doc = word_app.Documents.Open(str(input_file))
        doc.SaveAs(str(output_file), FileFormat=16)  # 16 = wdFormatXMLDocument
        doc.Close()
        _log.info(f"✓ Word: {input_file.name}")
        return True
    except Exception as e:
        _log.error(f"✗ Word failed for {input_file.name}: {e}")
        return False


def convert_folder(input_folder: Optional[Path] = None) -> None:
    """
    Convert all .doc files in input_folder to .docx, writing to input_folder/output/.
    Tries Word first on Windows, falls back to LibreOffice.
    """
    if input_folder is None:
        input_folder = Path.cwd()

    output_path = input_folder / "output"
    output_path.mkdir(exist_ok=True)
    _log.info(f"Output directory: {output_path}")

    doc_files = find_doc_files(input_folder)
    if not doc_files:
        _log.info("No .doc files found in the input folder.")
        return

    _log.info(f"Found {len(doc_files)} .doc file(s) to convert.")

    converted_count = 0
    word_app = None
    progress = ProgressReporter(len(doc_files), "Converting files")

    if _PLATFORM_WINDOWS and _WIN32_AVAILABLE:
        try:
            import win32com.client as win32
            word_app = win32.Dispatch("Word.Application")
            word_app.Visible = False
            _log.info("Using Microsoft Word for conversion")
        except Exception as e:
            _log.warning(f"Could not initialize Microsoft Word: {e}")
            _log.info("Will try LibreOffice as fallback")

    try:
        for doc_file in doc_files:
            success = False

            if word_app:
                success = convert_with_word(doc_file, output_path, word_app)

            if not success:
                success = convert_with_libreoffice(doc_file, output_path)

            if success:
                converted_count += 1
            else:
                _log.error(f"✗ Failed to convert: {doc_file.name}")

            progress.update()

    finally:
        if word_app:
            try:
                word_app.Quit()
            except Exception:
                pass
        progress.finish()

    _log.info(f"\nConversion complete!")
    _log.info(f"Successfully converted: {converted_count}/{len(doc_files)} files")
    _log.info(f"Converted files saved in: {output_path}")

    if converted_count < len(doc_files):
        _log.warning(f"Failed conversions: {len(doc_files) - converted_count}")
        _log.info("Note: Install Microsoft Word or LibreOffice for best results")
