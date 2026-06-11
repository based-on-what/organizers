"""Output helpers: console progress reporting and file writing."""
import json
import logging
import sys
from pathlib import Path
from typing import Dict, List, Union

_log = logging.getLogger("organizers")


class ProgressReporter:
    """Writes progress to stderr, leaving stdout clean for piping."""

    _FLUSH_EVERY = 50  # syscall budget: flush at most once per N updates

    def __init__(self, total: int, description: str = "Processing"):
        self.total = total
        self.current = 0
        self.description = description

    def update(self, increment: int = 1) -> None:
        self.current += increment
        if self.current % self._FLUSH_EVERY == 0 or self.current == self.total:
            pct = (self.current / self.total * 100) if self.total > 0 else 0
            print(
                f"\r{self.description}: {self.current}/{self.total} ({pct:.1f}%)",
                end="",
                flush=True,
                file=sys.stderr,
            )

    def finish(self) -> None:
        print(
            f"\r{self.description}: done ({self.current}/{self.total})        ",
            file=sys.stderr,
        )


def save_results_to_file(
    results: Union[Dict, List],
    output_file: Path,
    title: str = "RESULTS",
    encoding: str = "utf-8",
) -> bool:
    try:
        with open(output_file, "w", encoding=encoding) as f:
            f.write(f"{title}\n")
            f.write("=" * len(title) + "\n\n")

            if isinstance(results, dict):
                for key, value in results.items():
                    f.write(f"{key}: {value}\n")
            elif isinstance(results, list):
                for item in results:
                    if isinstance(item, tuple) and len(item) == 2:
                        f.write(f"{item[0]}: {item[1]}\n")
                    else:
                        f.write(f"{item}\n")
            else:
                f.write(str(results))

        _log.info(f"Results saved to: {output_file}")
        return True

    except OSError as e:
        _log.error(f"Error saving results to {output_file}: {e}")
        return False


def save_results(
    output_file: Path,
    txt_lines: List,
    json_payload,
    title: str,
    fmt: str = "txt",
) -> bool:
    """
    Single serializer for all CLIs.
    txt: legacy line-based layout (byte-identical to the old per-CLI writers).
    json: structured dump of json_payload.
    """
    if fmt.lower() == "json":
        try:
            with open(output_file, "w", encoding="utf-8") as f:
                json.dump(json_payload, f, indent=2, ensure_ascii=False)
            _log.info(f"Results saved to: {output_file}")
            return True
        except OSError as e:
            _log.error(f"Error saving results to {output_file}: {e}")
            return False
    return save_results_to_file(txt_lines, output_file, title)
