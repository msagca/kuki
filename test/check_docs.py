"""Fails when a CLAUDE.md or a skill names something the repository no longer has.

The CLAUDE.md files and the skills in `.claude/skills/` are read before nearly every change, and
what they say is taken on trust: a function they name is assumed to exist, a file they point at is
opened. When a rename leaves one behind, nothing breaks -- the build does not read prose -- and the
next change is planned against a name that is gone. So this is a test, run by ctest with the rest
and so by CI on every push, and a rename that strands a name fails until the text is brought along.

What is checked is everything between backticks outside fenced code blocks:

- A path (it has a `/` or a file extension) must be a file or directory in git. With a `/`, it is
  resolved against the document's own directory, the repository's root and `engine/` (where
  `include/...` and `internal/...` are written from) -- not matched against the end of any path,
  which would let `tools/x.py` in the root pass for `game/duel/tools/x.py`. Without one it is a bare
  name, as headers are included, and may be anywhere. A `<placeholder>` matches one path segment
  and `*` a glob. A path the build writes (under `build/`) is not checked; a file name the code
  writes or reads at run time (`kuki.config.json`) passes if the code spells it.
- Anything else is split into identifiers, and each one of three letters or more must appear as a
  whole word in a tracked source file (C++, HLSL, Python, CMake, JSON, YAML, `.clang-format`).
  A span that is another program's command (`gh issue list`) or flag (`-spirv`) is skipped. Prose is not a source
  file, so a name surviving only in another CLAUDE.md does not count.

That is deliberately loose -- `Camera::transform` passes if `Camera` and `transform` both exist
somewhere, not only if one is a member of the other -- because what goes stale in practice is a name
that has gone altogether, and a checker that needed the C++ parsed would be a project of its own.

Run by hand: `python test/check_docs.py` from anywhere in the repository.
"""

import fnmatch
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath

REPO = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".hpp", ".cpp", ".h", ".hlsl", ".py", ".cmake", ".json", ".yml", ".yaml", ".in", ".mat", ".txt"}
# Configuration with no suffix to go by, whose keys the conventions name (`ColumnLimit`).
SOURCE_NAMES = {"CMakeLists.txt", ".clang-format", ".clang-tidy", ".gitattributes", ".gitmodules"}
# Spans that are a command of another program, whose words are that program's and not the repo's.
COMMANDS = ("git ", "gh ", "cmake ", "ctest ", "python ", "dxc ")
PATH_SUFFIX = re.compile(r"\.(hpp|cpp|h|hlsl|py|md|json|yml|yaml|txt|cmake|blend|mat|glb|png|ttf)$")
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
SPAN = re.compile(r"`([^`\n]+)`")
# Bases a path with a slash is resolved from, besides the document's own directory.
PATH_BASES = [PurePosixPath("."), PurePosixPath("engine")]
# Produced by the build, so never in git.
BUILD_OUTPUT = ("build/",)


def tracked_files() -> list[PurePosixPath]:
    # Untracked files too, unless ignored, so a file added and not yet staged counts.
    result = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], cwd=REPO, capture_output=True, check=True)
    return [PurePosixPath(name) for name in result.stdout.decode("utf-8").split("\0") if name]


def documents(files: list[PurePosixPath]) -> list[PurePosixPath]:
    return [f for f in files if f.name == "CLAUDE.md" or (f.name == "SKILL.md" and f.parts[:2] == (".claude", "skills"))]


def spans(text: str):
    """(line number, span) for each inline code span outside fenced blocks."""
    fenced = False
    for number, line in enumerate(text.splitlines(), 1):
        if line.lstrip().startswith("```"):
            fenced = not fenced
            continue
        if not fenced:
            for match in SPAN.finditer(line):
                yield number, match.group(1)


def is_path(span: str) -> bool:
    return " " not in span and "(" not in span and ("/" in span or PATH_SUFFIX.search(span) is not None)


def to_glob(path: str) -> str:
    return re.sub(r"<[^>]+>", "[!/]*", path.replace("[", "[[]"))


def path_exists(span: str, document: PurePosixPath, files: set[str], directories: set[str], names: set[str], corpus: str) -> bool:
    path = span.rstrip("/")
    if span.startswith(BUILD_OUTPUT):
        return True
    if span.startswith(".") and "/" not in span:
        return True  # an extension on its own, such as `.mat`
    pattern = "<" in path or "*" in path
    if "/" not in path:
        if pattern:
            return any(fnmatch.fnmatchcase(name, to_glob(path)) for name in names)
        return path in names or path in corpus
    for base in [document.parent, *PATH_BASES]:
        candidate = str(base / path).removeprefix("./")
        if pattern:
            glob = to_glob(candidate)
            if any(fnmatch.fnmatchcase(f, glob) for f in files) or any(fnmatch.fnmatchcase(d, glob) for d in directories):
                return True
        elif candidate in files or candidate in directories:
            return True
    return False


def main() -> int:
    files = tracked_files()
    file_set = {str(f) for f in files}
    directories = {str(parent) for f in files for parent in f.parents if str(parent) != "."}
    names = {f.name for f in files}
    texts = []
    for f in files:
        if f.suffix in SOURCE_SUFFIXES or f.name in SOURCE_NAMES:
            try:
                texts.append((REPO / f).read_text(encoding="utf-8", errors="replace"))
            except OSError:
                pass  # a submodule's gitlink, or a file deleted but not yet staged
    corpus = "\n".join(texts)
    words = set(IDENTIFIER.findall(corpus))
    stale = []
    for document in documents(files):
        text = (REPO / document).read_text(encoding="utf-8")
        for number, span in spans(text):
            if is_path(span):
                if not path_exists(span, document, file_set, directories, names, corpus):
                    stale.append(f"{document}:{number}: `{span}` is not a path in the repository")
                continue
            if span.startswith(COMMANDS):
                continue
            # A CMake cache entry is its name: `-DKUKI_ENABLE_LINTING=ON` names `KUKI_ENABLE_LINTING`.
            # Any other single-dash flag is another program's (`-spirv` is dxc's); ours are `--name`.
            if span.startswith("-D"):
                span = span[2:]
            elif span.startswith("-") and not span.startswith("--"):
                continue
            # A placeholder is a slot, not a name: `<Target>Runtime` names `Runtime` only.
            for word in IDENTIFIER.findall(re.sub(r"<[a-z][^>]*>", "", span)):
                if len(word) >= 3 and word not in words:
                    stale.append(f"{document}:{number}: `{span}` names `{word}`, which no source file has")
    for line in stale:
        print(line)
    if stale:
        print(f"\n{len(stale)} stale name(s). Bring the text along with the rename, or put the name back.")
        return 1
    print(f"Every name in {len(documents(files))} documents was found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
