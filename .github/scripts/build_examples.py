"""Discover and build every example/board combination used by CI."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "components/wt_bsp/tools"))

from wt_bsp_set_board import available_project_boards, discover_boards


# This standalone C61 application uses set-target, as documented in its README.
TARGET_ONLY_EXAMPLES = {
    "examples/get-started/c61-hello-through-p4": "esp32c61",
}


def build_matrix():
    boards = discover_boards(ROOT)
    cases = []
    for directory, children, files in os.walk(ROOT / "examples"):
        children[:] = sorted(name for name in children
                             if not name.startswith("build") and name != "managed_components")
        if "CMakeLists.txt" not in files:
            continue
        project = Path(directory)
        if not re.search(r"^\s*project\s*\(", (project / "CMakeLists.txt").read_text(), re.MULTILINE):
            continue
        example = project.relative_to(ROOT).as_posix()
        available = available_project_boards(project, boards)
        if available:
            cases.extend({"example": example, "board": board.name, "target": board.target}
                         for board in available.values())
        elif example in TARGET_ONLY_EXAMPLES:
            cases.append({"example": example, "board": "", "target": TARGET_ONLY_EXAMPLES[example]})
        else:
            raise ValueError("No board configurations or explicit target for {}".format(example))
    if not cases:
        raise ValueError("No example projects found")
    return {"include": cases}


def build_example(case):
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path or not (Path(idf_path) / "tools/idf.py").is_file():
        raise ValueError("Activate the ESP-IDF environment before building")
    project = ROOT / case["example"]
    command = [sys.executable, str(Path(idf_path) / "tools/idf.py")]
    env = os.environ.copy()
    env.pop("WT_BSP_BOARD", None)
    env.pop("IDF_TARGET", None)
    if case["board"]:
        # Feed the board name to the existing menu, including on a clean project.
        subprocess.run(command + ["set-board"], input=case["board"] + "\n",
                       text=True, cwd=project, env=env, check=True)
    else:
        subprocess.run(command + ["set-target", case["target"]],
                       cwd=project, env=env, check=True)
    subprocess.run(command + ["build"], cwd=project, env=env, check=True)

    config = (project / "build/sdkconfig").read_text()
    if 'CONFIG_IDF_TARGET="{}"'.format(case["target"]) not in config.splitlines():
        raise ValueError("Built target does not match {}".format(case["target"]))
    if case["board"]:
        selected = re.findall(r"^CONFIG_WT_BSP_BOARD_(WT\w+)=y$", config, re.MULTILINE)
        if selected != [case["board"].replace("-", "_")]:
            raise ValueError("Built board does not match {}".format(case["board"]))
    if not any(path.stat().st_size for path in (project / "build").glob("*.bin")):
        raise ValueError("No application binary generated")
    print("PASS: {} / {}".format(case["example"], case["board"] or case["target"]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", action="store_true", help="Print the GitHub Actions matrix as JSON")
    parser.add_argument("--example")
    parser.add_argument("--board", default="")
    parser.add_argument("--target")
    args = parser.parse_args()
    try:
        matrix = build_matrix()
        if args.matrix:
            print(json.dumps(matrix, separators=(",", ":")))
            return 0
        case = {"example": args.example, "board": args.board, "target": args.target}
        if case not in matrix["include"]:
            parser.error("Specify an example, board and target from --matrix")
        build_example(case)
    except subprocess.CalledProcessError as error:
        print("Build command failed with exit code {}".format(error.returncode), file=sys.stderr)
        return error.returncode if error.returncode > 0 else 1
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
