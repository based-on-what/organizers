"""Filesystem utilities: file discovery and access checks."""
import logging
import os
import stat as _stat
from pathlib import Path
from typing import List, Optional, Set

_log = logging.getLogger("organizers")


def safe_file_operation(file_path: Path, operation_name: str = "process") -> bool:
    """Check that file exists, is non-empty, and is readable."""
    try:
        st = file_path.stat()
    except FileNotFoundError:
        _log.warning(f"File does not exist: {file_path}")
        return False
    except PermissionError:
        _log.error(f"Permission denied accessing {file_path}")
        return False
    except OSError as e:
        _log.error(f"OS error accessing {file_path}: {e}")
        return False

    # Use mode from the stat we already have — avoids a second stat() call
    if not _stat.S_ISREG(st.st_mode):
        _log.warning(f"Path is not a file: {file_path}")
        return False

    if st.st_size == 0:
        _log.warning(f"File is empty: {file_path}")
        return False

    if not os.access(file_path, os.R_OK):
        _log.error(f"Permission denied reading {file_path}")
        return False

    return True


def find_files_by_extensions(
    directory: Path,
    extensions: Set[str],
    exclude_dirs: Optional[Set[str]] = None,
    recursive: bool = True,
) -> List[Path]:
    """Find files with given extensions. Uses os.walk (Python 3.6+ compatible)."""
    if exclude_dirs is None:
        exclude_dirs = set()

    files: List[Path] = []
    try:
        if recursive:
            for root, dirs, filenames in os.walk(directory):
                dirs[:] = [d for d in dirs if d not in exclude_dirs]
                for filename in filenames:
                    p = Path(root) / filename
                    if p.suffix.lower() in extensions:
                        files.append(p)
        else:
            for entry in os.scandir(directory):
                if entry.is_file() and Path(entry.name).suffix.lower() in extensions:
                    files.append(Path(entry.path))
    except OSError:
        _log.exception(f"Error searching for files in {directory}")

    return files
