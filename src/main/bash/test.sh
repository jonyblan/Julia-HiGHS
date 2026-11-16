#!/usr/bin/env bash
set -u
BASE_PATH="$(dirname "$0")/../../.."
cd "$BASE_PATH" || exit 1
status=0
accepted=0
rejected=0
if [ ! -x .build/Flex-Bison-Compiler ]; then
    echo "ERROR: Build the compiler before running tests." >&2
    exit 1
fi
for test in src/test/c/accept/*; do
    [ -f "$test" ] || continue
    if .build/Flex-Bison-Compiler < "$test" >/dev/null 2>&1; then
        echo "PASS accept: $(basename "$test")"
        accepted=$((accepted+1))
        if [ ! -s output/program.jl ] || [ ! -s output/program.txt ] || [ ! -s output/program.ipynb ]; then
            echo "FAIL artifacts: $(basename "$test")"
            status=1
        fi
        if ! cmp -s output/program.txt output/program.jl; then
            echo "FAIL generated text/jl disagree: $(basename "$test")"
            status=1
        fi
        expected="src/test/expected/$(basename "$test" .txt).jl"
        if [ -f "$expected" ] && ! cmp -s "$expected" output/program.jl; then
            echo "FAIL golden Julia comparison: $(basename "$test")"
            status=1
        fi
        if command -v python3 >/dev/null 2>&1; then
            if ! python3 - <<'PY'
import json
from pathlib import Path
p = json.loads(Path('output/program.ipynb').read_text())
assert p['nbformat'] == 4
assert len(p['cells']) == 1
assert ''.join(p['cells'][0]['source']) == Path('output/program.jl').read_text()
PY
            then
                echo "FAIL notebook JSON: $(basename "$test")"
                status=1
            fi
        fi
    else
        echo "FAIL accept: $(basename "$test")"
        status=1
    fi
done
for test in src/test/c/reject/*; do
    [ -f "$test" ] || continue
    if .build/Flex-Bison-Compiler < "$test" >/dev/null 2>&1; then
        echo "FAIL reject: $(basename "$test")"
        status=1
    else
        echo "PASS reject: $(basename "$test")"
        rejected=$((rejected+1))
        if [ -e output/program.jl ] || [ -e output/program.ipynb ]; then
            echo "FAIL stale artifacts after rejection: $(basename "$test")"
            status=1
        fi
    fi
done
printf '\nAccepted %d; rejected %d; status %d\n' "$accepted" "$rejected" "$status"
exit "$status"
