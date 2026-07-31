#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build"
cmake -S "${ROOT}" -B "${BUILD}" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build "${BUILD}" -j"$(nproc)"
ctest --test-dir "${BUILD}" --output-on-failure
