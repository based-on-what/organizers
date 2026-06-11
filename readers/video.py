"""
Pure video duration reader.

Returns raw (duration_seconds, file_size_bytes) or None — no formatting,
no logging beyond warnings, no orchestration.

Primary backend is ffprobe (reads container metadata only, ~50ms per file).
moviepy is a fallback used only when ffprobe is not on PATH.
"""
import json
import logging
import shutil
import subprocess
from pathlib import Path
from typing import Optional, Tuple

_log = logging.getLogger("organizers")

MIN_FILE_SIZE = 100 * 1024  # 100 KB — skip trailers / thumbnails

_FFPROBE_PATH: Optional[str] = None
_FFPROBE_CHECKED = False


def _get_ffprobe() -> Optional[str]:
    """Locate ffprobe once per process. Returns its path or None."""
    global _FFPROBE_PATH, _FFPROBE_CHECKED
    if not _FFPROBE_CHECKED:
        _FFPROBE_PATH = shutil.which("ffprobe")
        _FFPROBE_CHECKED = True
        if _FFPROBE_PATH is None:
            _log.warning("ffprobe not found on PATH; falling back to moviepy (much slower)")
    return _FFPROBE_PATH


def _read_duration_ffprobe(ffprobe: str, video_path: Path) -> Optional[float]:
    try:
        result = subprocess.run(
            [ffprobe, "-v", "error",
             "-show_entries", "format=duration",
             "-of", "json", str(video_path)],
            capture_output=True, text=True, timeout=60,
        )
        if result.returncode != 0:
            _log.warning(f"ffprobe failed for {video_path}: {result.stderr.strip()}")
            return None
        duration = float(json.loads(result.stdout)["format"]["duration"])
        return duration
    except (subprocess.TimeoutExpired, KeyError, ValueError, json.JSONDecodeError):
        _log.warning(f"ffprobe returned no usable duration for {video_path}")
        return None


def _read_duration_moviepy(video_path: Path) -> Optional[float]:
    from core.loaders import get_video_file_clip
    try:
        with get_video_file_clip()(str(video_path)) as clip:
            return clip.duration
    except Exception:
        _log.exception(f"Error opening video: {video_path}")
        return None


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

    ffprobe = _get_ffprobe()
    if ffprobe:
        duration = _read_duration_ffprobe(ffprobe, video_path)
    else:
        duration = _read_duration_moviepy(video_path)

    if not duration or duration <= 0:
        _log.warning(f"Invalid duration: {video_path}")
        return None
    return duration, file_size
