"""
Pure video duration reader.

Returns raw (duration_seconds, file_size_bytes) or None — no formatting,
no logging beyond warnings, no orchestration.
"""
import logging
from pathlib import Path
from typing import Optional, Tuple

from core.loaders import get_video_file_clip

_log = logging.getLogger("organizers")

MIN_FILE_SIZE = 100 * 1024  # 100 KB — skip trailers / thumbnails


def read_duration(video_path: Path) -> Optional[Tuple[float, int]]:
    """Return (duration_seconds, file_size_bytes) or None on any error."""
    try:
        file_size = video_path.stat().st_size
    except OSError:
        _log.warning(f"Cannot stat: {video_path}")
        return None

    if file_size < MIN_FILE_SIZE:
        _log.warning(f"File too small ({file_size} bytes): {video_path}")
        return None

    try:
        with get_video_file_clip()(str(video_path)) as clip:
            duration = clip.duration
            if not duration or duration <= 0:
                _log.warning(f"Invalid duration: {video_path}")
                return None
            return duration, file_size
    except Exception:
        _log.exception(f"Error opening video: {video_path}")
        return None
