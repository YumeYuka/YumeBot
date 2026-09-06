#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
DIST_DIR="${ROOT}/dist"
ARTIFACT_NAME="${1:-yumebot-linux-x64}"

mkdir -p "${DIST_DIR}"

detect_clang() {
    for candidate in clang++-21 clang++-20 clang++-19 clang++; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            echo "${candidate}"
            return 0
        fi
    done
    return 1
}

CXX="$(detect_clang)" || {
    echo "No suitable clang++ found. Install LLVM 19+ (clang++-21 recommended)." >&2
    exit 1
}
CC="${CXX%++}"

if [ -z "${OPENSSL_ROOT_DIR:-}" ] && command -v brew >/dev/null 2>&1; then
    if brew --prefix openssl@3 >/dev/null 2>&1; then
        export OPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
    fi
fi

echo "Using compiler: ${CXX}"

cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="${CXX}" \
    -DCMAKE_C_COMPILER="${CC}"

if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
else
    JOBS="$(sysctl -n hw.ncpu)"
fi

cmake --build "${BUILD_DIR}" --target YumeBot -j "${JOBS}"

cp "${BUILD_DIR}/bin/YumeBot" "${DIST_DIR}/${ARTIFACT_NAME}"
chmod +x "${DIST_DIR}/${ARTIFACT_NAME}"

echo "Built ${DIST_DIR}/${ARTIFACT_NAME}"
