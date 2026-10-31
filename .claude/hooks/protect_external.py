"""PreToolUse hook: refuse edits under `external/`.

The submodules are never patched; a vendored library's quirk is worked around in the root
`CMakeLists.txt`, which says so at length. An edit there would also be lost on the next
`git submodule update`, or worse, committed into a submodule nobody meant to move.
"""

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def main() -> None:
    data = json.load(sys.stdin)
    tool_input = data.get("tool_input", {})
    file_path = tool_input.get("file_path") or tool_input.get("notebook_path")
    if not file_path:
        return
    try:
        relative = Path(file_path).resolve().relative_to(REPO)
    except ValueError:
        return
    if relative.parts and relative.parts[0] == "external":
        print(f"{relative.as_posix()} is in a submodule, and external/ is never patched. Work around the library's behaviour in the root CMakeLists.txt (documenting why), or in the engine's own code.", file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
