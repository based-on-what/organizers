"""Shared CLI plumbing: every entry point gets the same minimal contract."""
import argparse
import logging
import sys
from pathlib import Path

LOG_LEVELS = ["DEBUG", "INFO", "WARNING", "ERROR"]


def build_parser(
    description: str,
    default_output: str,
    with_directory: bool = True,
    with_format: bool = True,
) -> argparse.ArgumentParser:
    """Standard parser: optional positional directory, -o, -f, -l."""
    parser = argparse.ArgumentParser(description=description)
    if with_directory:
        parser.add_argument(
            "directory", nargs="?", default=".",
            help="Directory to analyze (default: current directory)",
        )
    parser.add_argument(
        "-o", "--output", default=default_output,
        help=f"Output file (default: {default_output})",
    )
    if with_format:
        parser.add_argument(
            "-f", "--format", choices=["txt", "json"], default="txt",
            help="Output file format (default: txt)",
        )
    parser.add_argument(
        "-l", "--log-level", choices=LOG_LEVELS, default="INFO",
        help="Logging verbosity (default: INFO)",
    )
    return parser


def resolve_directory(raw: str, logger: logging.Logger) -> Path:
    """Resolve and validate the directory argument. Exits on bad input."""
    path = Path(raw).resolve()
    if not path.exists():
        logger.error(f"Directory does not exist: {path}")
        sys.exit(1)
    if not path.is_dir():
        logger.error(f"Path is not a directory: {path}")
        sys.exit(1)
    return path
