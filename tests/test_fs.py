"""Tests for core.fs file discovery and access checks."""
from core.fs import find_files_by_extensions, safe_file_operation


def _make_tree(root):
    (root / "a.mp4").write_bytes(b"x")
    (root / "b.MKV").write_bytes(b"x")
    (root / "notes.txt").write_bytes(b"x")
    sub = root / "season1"
    sub.mkdir()
    (sub / "ep1.mp4").write_bytes(b"x")
    excluded = root / "Subs"
    excluded.mkdir()
    (excluded / "sub.mp4").write_bytes(b"x")


class TestFindFilesByExtensions:
    def test_recursive_finds_nested(self, tmp_path):
        _make_tree(tmp_path)
        found = sorted(
            p.name for p in find_files_by_extensions(tmp_path, {'.mp4', '.mkv'})
        )
        assert found == ["a.mp4", "b.MKV", "ep1.mp4", "sub.mp4"]

    def test_exclude_dirs(self, tmp_path):
        _make_tree(tmp_path)
        found = sorted(
            p.name for p in find_files_by_extensions(
                tmp_path, {'.mp4', '.mkv'}, exclude_dirs={"Subs"}
            )
        )
        assert found == ["a.mp4", "b.MKV", "ep1.mp4"]

    def test_non_recursive(self, tmp_path):
        _make_tree(tmp_path)
        found = sorted(
            p.name for p in find_files_by_extensions(
                tmp_path, {'.mp4', '.mkv'}, recursive=False
            )
        )
        assert found == ["a.mp4", "b.MKV"]

    def test_case_insensitive_extension_match(self, tmp_path):
        (tmp_path / "UPPER.MP4").write_bytes(b"x")
        found = list(find_files_by_extensions(tmp_path, {'.mp4'}))
        assert len(found) == 1

    def test_missing_directory_yields_nothing(self, tmp_path):
        found = list(find_files_by_extensions(tmp_path / "nope", {'.mp4'}))
        assert found == []


class TestSafeFileOperation:
    def test_ok_file(self, tmp_path):
        f = tmp_path / "ok.bin"
        f.write_bytes(b"data")
        assert safe_file_operation(f) is True

    def test_missing_file(self, tmp_path):
        assert safe_file_operation(tmp_path / "missing.bin") is False

    def test_empty_file(self, tmp_path):
        f = tmp_path / "empty.bin"
        f.write_bytes(b"")
        assert safe_file_operation(f) is False

    def test_directory_is_not_file(self, tmp_path):
        assert safe_file_operation(tmp_path) is False
