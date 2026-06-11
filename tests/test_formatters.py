"""Tests for core.formatters — pure functions, no fixtures needed."""

from core.formatters import format_duration, format_file_size


class TestFormatFileSize:
    def test_zero_bytes(self):
        assert format_file_size(0) == "0.0 B"

    def test_bytes(self):
        assert format_file_size(512) == "512.0 B"

    def test_kilobytes(self):
        assert format_file_size(2048) == "2.0 KB"

    def test_megabytes(self):
        assert format_file_size(5 * 1024 * 1024) == "5.0 MB"

    def test_gigabytes(self):
        assert format_file_size(3 * 1024 ** 3) == "3.0 GB"

    def test_terabytes(self):
        assert format_file_size(2 * 1024 ** 4) == "2.0 TB"

    def test_above_one_tb_stays_tb(self):
        # No PB unit: values past TB keep the TB suffix
        assert format_file_size(5000 * 1024 ** 4) == "5000.0 TB"

    def test_boundary_just_below_kb(self):
        assert format_file_size(1023) == "1023.0 B"


class TestFormatDuration:
    def test_zero(self):
        assert format_duration(0) == "00:00:00"

    def test_seconds_only(self):
        assert format_duration(59) == "00:00:59"

    def test_minutes(self):
        assert format_duration(125) == "00:02:05"

    def test_hours(self):
        assert format_duration(3 * 3600 + 4 * 60 + 5) == "03:04:05"

    def test_over_24_hours(self):
        assert format_duration(25 * 3600) == "25:00:00"

    def test_fractional_seconds_truncated(self):
        assert format_duration(61.9) == "00:01:01"
