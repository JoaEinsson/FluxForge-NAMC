"""Validate repository-level policy files without third-party dependencies."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[2]

REQUIRED_FILES = (
    "AGENTS.md",
    "CHANGELOG.md",
    "CODE_OF_CONDUCT.md",
    "CONTRIBUTING.md",
    "DCO",
    "GOVERNANCE.md",
    "LICENSE",
    "NOTICE",
    "README.md",
    "ROADMAP.md",
    "SECURITY.md",
    "SUPPORT.md",
    "docs/project-charter.md",
    "docs/safety-scope.md",
    "docs/development/ai-assisted-development.md",
    "docs/development/project-management.md",
    "docs/development/repository-setup.md",
    "docs/development/workflow.md",
    "docs/adr/README.md",
)

TEXT_SUFFIXES = {
    ".c",
    ".cmake",
    ".h",
    ".json",
    ".md",
    ".py",
    ".toml",
    ".txt",
    ".yaml",
    ".yml",
}

TEXT_NAMES = {
    ".editorconfig",
    ".gitattributes",
    ".gitignore",
    "DCO",
    "LICENSE",
    "NOTICE",
}

MARKDOWN_LINK = re.compile(r"(?<!!)\[[^\]]*\]\(([^)]+)\)")


def repository_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    return [ROOT / item.decode("utf-8") for item in result.stdout.split(b"\0") if item]


def validate_required_files(errors: list[str]) -> None:
    for relative_path in REQUIRED_FILES:
        if not (ROOT / relative_path).is_file():
            errors.append(f"missing required file: {relative_path}")


def validate_text_file(path: Path, errors: list[str]) -> str | None:
    relative = path.relative_to(ROOT).as_posix()
    try:
        raw = path.read_bytes()
        text = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        errors.append(f"{relative}: not valid UTF-8 ({error})")
        return None

    if b"\r" in raw:
        errors.append(f"{relative}: contains CR or CRLF line endings")
    if raw and not raw.endswith(b"\n"):
        errors.append(f"{relative}: missing final newline")

    for line_number, line in enumerate(text.splitlines(), start=1):
        if line.endswith((" ", "\t")):
            errors.append(f"{relative}:{line_number}: trailing whitespace")

    return text


def validate_markdown_links(path: Path, text: str, errors: list[str]) -> None:
    relative = path.relative_to(ROOT).as_posix()
    for match in MARKDOWN_LINK.finditer(text):
        target = match.group(1).strip().split(maxsplit=1)[0].strip("<>")
        if not target or target.startswith(("#", "http://", "https://", "mailto:")):
            continue

        file_part = unquote(target.split("#", maxsplit=1)[0])
        if not file_part:
            continue

        resolved = (path.parent / file_part).resolve()
        try:
            resolved.relative_to(ROOT)
        except ValueError:
            errors.append(f"{relative}: link escapes repository: {target}")
            continue

        if not resolved.exists():
            errors.append(f"{relative}: broken relative link: {target}")


def main() -> int:
    errors: list[str] = []
    validate_required_files(errors)

    for path in repository_files():
        if not path.is_file():
            continue
        if path.suffix.lower() not in TEXT_SUFFIXES and path.name not in TEXT_NAMES:
            continue

        text = validate_text_file(path, errors)
        if text is not None and path.suffix.lower() == ".md":
            validate_markdown_links(path, text, errors)

    if errors:
        print("Repository policy validation failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("Repository policy validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
