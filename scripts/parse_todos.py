# SPDX-FileCopyrightText: 2025 PicoKernel Contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#!/usr/bin/env python3
import re
import sys
import json
import hashlib
from pathlib import Path

VALID_TAGS = {
    "Bug",
    "Cleanup",
    "Docs",
    "Driver",
    "Enhancement",
    "Interface",
    "Kernel",
    "Milestone",
    "Module",
    "Perf",
    "WIP",
}

TODO_RE = re.compile(
    r'@todo\s+'
    r'((?:\[[\w.-]+\]\s*)+)'
    r'(.+)$',
    re.MULTILINE
)
TAG_RE = re.compile(r'\[([\w.-]+)\]')

def fingerprint(file: str, tags: list, description: str) -> str:
    return hashlib.sha256(
        f"{file}:{','.join(tags)}:{description}".encode()
    ).hexdigest()[:8]

def parse_file(path: Path) -> list:
    results = []
    text = path.read_text(errors="replace")
    for lineno, line in enumerate(text.splitlines(), start=1):
        m = TODO_RE.search(line)
        if not m:
            continue
        tags = TAG_RE.findall(m.group(1))
        for tag in tags:
            if tag not in VALID_TAGS:
                raise ValueError(
                    f"{path}:{lineno}: Invalid TODO tag '{tag}'"
                )
        description = m.group(2).strip()
        results.append({
            "file": str(path),
            "line": lineno,
            "tags": tags,
            "description": description,
            "fingerprint": fingerprint(str(path), tags, description),
        })
    return results

def main():
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
    todos = []
    for ext in {".c", ".h", ".cpp", ".hpp"}:
        for f in root.rglob(f"*{ext}"):
            todos.extend(parse_file(f))
    print(json.dumps(todos, indent=2))

if __name__ == "__main__":
    main()
