"""Publish generated files atomically, preserving unchanged outputs and mtimes."""
from __future__ import annotations

from pathlib import Path
import tempfile


def publish_staged_if_changed(staged: Path, destination: Path) -> bool:
    """Remove an identical staged file; atomically replace destination otherwise."""
    try:
        same_size = staged.stat().st_size == destination.stat().st_size
    except FileNotFoundError:
        same_size = False
    identical = False
    if same_size:
        with staged.open("rb") as candidate, destination.open("rb") as current:
            while True:
                block = candidate.read(1024 * 1024)
                if block != current.read(1024 * 1024):
                    break
                if not block:
                    identical = True
                    break
    if identical:
        staged.unlink()
        return False
    staged.replace(destination)
    return True


def write_text_if_changed(destination: Path, text: str) -> bool:
    """Compare normalized UTF-8 text before staging; preserve existing newlines."""
    normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    try:
        if destination.read_text(encoding="utf-8") == normalized:
            return False
    except FileNotFoundError:
        pass
    staged = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="",
                                         dir=destination.parent, prefix=destination.name + ".",
                                         suffix=".tmp", delete=False) as output:
            staged = Path(output.name)
            output.write(normalized)
        staged.replace(destination)
        return True
    finally:
        if staged is not None and staged.exists():
            staged.unlink()
