"""Tests for readers.video — ffprobe path mocked, no codecs required."""
import json

import pytest

import readers.video as video_mod
from readers.video import read_duration


@pytest.fixture
def big_file(tmp_path):
    f = tmp_path / "movie.mp4"
    f.write_bytes(b"\0" * (video_mod.MIN_FILE_SIZE + 1))
    return f


@pytest.fixture(autouse=True)
def fake_ffprobe(monkeypatch):
    monkeypatch.setattr(video_mod, "_FFPROBE_PATH", "ffprobe")
    monkeypatch.setattr(video_mod, "_FFPROBE_CHECKED", True)


def _fake_run(stdout, returncode=0):
    class Result:
        pass
    r = Result()
    r.stdout = stdout
    r.stderr = ""
    r.returncode = returncode
    return lambda *a, **kw: r


class TestReadDuration:
    def test_small_file_skipped(self, tmp_path):
        f = tmp_path / "tiny.mp4"
        f.write_bytes(b"x" * 100)
        assert read_duration(f) is None

    def test_missing_file(self, tmp_path):
        assert read_duration(tmp_path / "nope.mp4") is None

    def test_ffprobe_success(self, big_file, monkeypatch):
        payload = json.dumps({"format": {"duration": "123.45"}})
        monkeypatch.setattr(video_mod.subprocess, "run", _fake_run(payload))
        result = read_duration(big_file)
        assert result is not None
        duration, size = result
        assert duration == pytest.approx(123.45)
        assert size == big_file.stat().st_size

    def test_ffprobe_failure_returns_none(self, big_file, monkeypatch):
        monkeypatch.setattr(video_mod.subprocess, "run", _fake_run("", returncode=1))
        assert read_duration(big_file) is None

    def test_zero_duration_rejected(self, big_file, monkeypatch):
        payload = json.dumps({"format": {"duration": "0.0"}})
        monkeypatch.setattr(video_mod.subprocess, "run", _fake_run(payload))
        assert read_duration(big_file) is None

    def test_garbage_json_rejected(self, big_file, monkeypatch):
        monkeypatch.setattr(video_mod.subprocess, "run", _fake_run("not json"))
        assert read_duration(big_file) is None
