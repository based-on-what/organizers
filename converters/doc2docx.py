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
import shutil
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
        import pywintypes  # noqa: F401
        import win32com.client as win32  # noqa: F401
        _WIN32_AVAILABLE = True
    except ImportError:
        pass

# Consecutive Word COM failures before the instance is restarted —
# a corrupt document can leave Word in a state where every later call fails
_WORD_RESTART_AFTER = 2

_SOFFICE_PATH: Optional[str] = None
_SOFFICE_CHECKED = False


def _get_soffice() -> Optional[str]:
    """Locate the LibreOffice binary once per process. Returns path or None."""
    global _SOFFICE_PATH, _SOFFICE_CHECKED
    if not _SOFFICE_CHECKED:
        for name in ("libreoffice", "soffice"):
            _SOFFICE_PATH = shutil.which(name)
            if _SOFFICE_PATH:
                break
        _SOFFICE_CHECKED = True
    return _SOFFICE_PATH


def find_doc_files(input_path: Path) -> List[Path]:
    """Find .doc files in input_path, non-recursively."""
    return list(find_files_by_extensions(input_path, {'.doc'}, recursive=False))


def convert_with_libreoffice(input_file: Path, output_dir: Path) -> bool:
    """Convert DOC to DOCX using LibreOffice headless mode. Returns True on success."""
    soffice = _get_soffice()
    if soffice is None:
        return False

    try:
        result = subprocess.run(
            [soffice, '--headless', '--convert-to', 'docx',
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


def _open_word():
    """Start a hidden Word COM instance, or None if unavailable."""
    if not (_PLATFORM_WINDOWS and _WIN32_AVAILABLE):
        return None
    try:
        import win32com.client as win32
        app = win32.Dispatch("Word.Application")
        app.Visible = False
        return app
    except Exception as e:
        _log.warning(f"Could not initialize Microsoft Word: {e}")
        return None


def _quit_word(word_app) -> None:
    try:
        word_app.Quit()
    except Exception:
        pass


def convert_folder(
    input_folder: Optional[Path] = None,
    output_dir: Optional[Path] = None,
    skip_existing: bool = True,
) -> None:
    """
    Convert all .doc files in input_folder to .docx, writing to output_dir
    (default: input_folder/output). Tries Word first on Windows, falls back
    to LibreOffice. Existing .docx targets are skipped unless skip_existing
    is False.
    """
    if input_folder is None:
        input_folder = Path.cwd()

    output_path = output_dir if output_dir is not None else input_folder / "output"
    output_path.mkdir(parents=True, exist_ok=True)
    _log.info(f"Output directory: {output_path}")

    doc_files = find_doc_files(input_folder)
    if not doc_files:
        _log.info("No .doc files found in the input folder.")
        return

    _log.info(f"Found {len(doc_files)} .doc file(s) to convert.")

    converted_count = 0
    skipped_count = 0
    word_failures = 0
    progress = ProgressReporter(len(doc_files), "Converting files")

    word_app = _open_word()
    if word_app:
        _log.info("Using Microsoft Word for conversion")
    elif _PLATFORM_WINDOWS:
        _log.info("Will try LibreOffice as fallback")

    try:
        for doc_file in doc_files:
            target = output_path / f"{doc_file.stem}.docx"
            if skip_existing and target.exists():
                _log.info(f"→ Skipping {doc_file.name}: {target.name} already exists")
                skipped_count += 1
                progress.update()
                continue

            success = False

            if word_app:
                success = convert_with_word(doc_file, output_path, word_app)
                if success:
                    word_failures = 0
                else:
                    word_failures += 1
                    if word_failures >= _WORD_RESTART_AFTER:
                        _log.warning("Restarting Word after repeated failures")
                        _quit_word(word_app)
                        word_app = _open_word()
                        word_failures = 0

            if not success:
                success = convert_with_libreoffice(doc_file, output_path)

            if success:
                converted_count += 1
            else:
                _log.error(f"✗ Failed to convert: {doc_file.name}")

            progress.update()

    finally:
        if word_app:
            _quit_word(word_app)
        progress.finish()

    _log.info("Conversion complete!")
    _log.info(f"Successfully converted: {converted_count}/{len(doc_files)} files")
    if skipped_count:
        _log.info(f"Skipped (already converted): {skipped_count}")
    _log.info(f"Converted files saved in: {output_path}")

    failed = len(doc_files) - converted_count - skipped_count
    if failed > 0:
        _log.warning(f"Failed conversions: {failed}")
        _log.info("Note: Install Microsoft Word or LibreOffice for best results")
