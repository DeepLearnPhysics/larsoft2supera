"""Gate coverage for the native converter, excluding dependency implementation."""
import gzip
import json
from pathlib import Path
import sys


def main(directory):
    reports = list(directory.glob("*.gcov.json.gz"))
    if len(reports) != 1:
        raise RuntimeError("Expected one gcov translation-unit report")
    with gzip.open(reports[0], "rt") as stream:
        data = json.load(stream)
    files = [entry for entry in data["files"]
             if entry["file"].endswith("/src/Generator.cxx")]
    if len(files) != 1:
        raise RuntimeError("Native converter missing from coverage report")
    entry = files[0]
    lines = entry["lines"]
    functions = entry["functions"]
    branches = [branch for line in lines for branch in line.get("branches", [])]
    result = {
        "line_covered": sum(line["count"] > 0 for line in lines),
        "line_total": len(lines),
        "function_covered": sum(function["execution_count"] > 0 for function in functions),
        "function_total": len(functions),
        "branch_covered": sum(branch["count"] > 0 for branch in branches),
        "branch_total": len(branches),
    }
    (directory / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    if not lines or result["line_covered"] / len(lines) < 0.9:
        raise RuntimeError("Generator converter line coverage below 90%")
    if not functions or result["function_covered"] != len(functions):
        raise RuntimeError("Generator converter function coverage below 100%")


if __name__ == "__main__":
    main(Path(sys.argv[1]))
