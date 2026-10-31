"""PostToolUse hook: format what an edit changed, and nothing else.

C++ goes through clang-format restricted to the lines that differ from HEAD -- what
`git clang-format` does. A whole-file format is what this is not, on purpose: a good number of
sources have drifted from `.clang-format` (mostly include order), and formatting a whole file on the
first touch would bury a three-line change in a hundred lines of reordering. A file git does not
know yet is new, and is formatted whole.

CMake goes through gersemi whole, since the CMake files are all formatted already and gersemi has no
notion of a range.

A missing formatter is not an error: the hook is shared through the repository and not everyone has
both installed. When the file changes, Claude is told, so it reads the file again before the next
edit instead of matching against text that is no longer there.
"""

import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CPP_SUFFIXES = {".cpp", ".hpp", ".h", ".cc"}
SKIPPED_ROOTS = {"external", "build", ".git"}


def run(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(args, cwd=REPO, capture_output=True, text=True, encoding="utf-8", errors="replace")


def changed_lines(path: Path) -> list[tuple[int, int]] | None:
    """The line ranges of `path` that differ from HEAD, or None when git does not track it."""
    if run("git", "ls-files", "--error-unmatch", "--", str(path)).returncode != 0:
        return None
    diff = run("git", "diff", "-U0", "--no-color", "--no-ext-diff", "HEAD", "--", str(path)).stdout
    ranges = []
    for match in re.finditer(r"^@@ -\S+ \+(\d+)(?:,(\d+))? @@", diff, re.MULTILINE):
        start = int(match.group(1))
        count = 1 if match.group(2) is None else int(match.group(2))
        if count:
            ranges.append((start, start + count - 1))
    return ranges


def format_cpp(path: Path) -> bool:
    clang_format = shutil.which("clang-format")
    if not clang_format:
        return False
    ranges = changed_lines(path)
    if ranges == []:
        return False
    arguments = [clang_format, "-i"]
    arguments += [f"--lines={first}:{last}" for first, last in ranges or []]
    return run(*arguments, str(path)).returncode == 0


def format_cmake(path: Path) -> bool:
    gersemi = shutil.which("gersemi")
    if not gersemi:
        return False
    return run(gersemi, "-i", str(path)).returncode == 0


def main() -> None:
    data = json.load(sys.stdin)
    file_path = data.get("tool_input", {}).get("file_path")
    if not file_path:
        return
    path = Path(file_path).resolve()
    try:
        relative = path.relative_to(REPO)
    except ValueError:
        return
    if not path.is_file() or relative.parts[0] in SKIPPED_ROOTS:
        return
    if path.suffix in CPP_SUFFIXES:
        formatter = format_cpp
    elif path.name == "CMakeLists.txt" or path.suffix == ".cmake":
        formatter = format_cmake
    else:
        return
    before = path.read_bytes()
    if formatter(path) and path.read_bytes() != before:
        print(json.dumps({"hookSpecificOutput": {"hookEventName": "PostToolUse", "additionalContext": f"The format hook reformatted {relative.as_posix()}; read it again before editing it."}}))


if __name__ == "__main__":
    main()
