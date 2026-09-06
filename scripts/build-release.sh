#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
DIST_DIR="${ROOT}/dist"
ARTIFACT_NAME="${1:-yumebot-linux-x64}"

mkdir -p "${DIST_DIR}"

detect_clang() {
    for candidate in clang++ clang++-23 clang++-22; do
        if command -v "${candidate}" >/dev/null 2>&1; then
            echo "${candidate}"
            return 0
        fi
    done
    return 1
}

CXX="$(detect_clang)" || {
    echo "No suitable clang++ found. Install LLVM 22+ (clang++-23 recommended)." >&2
    exit 1
}
case "${CXX}" in
    */clang++-*) CC="${CXX%/*}/clang-${CXX##*/clang++-}" ;;
    clang++-*)   CC="clang-${CXX#clang++-}" ;;
    *clang++)    CC="${CXX%++}" ;;
    *)           CC="${CXX}" ;;
esac

if [ -z "${OPENSSL_ROOT_DIR:-}" ] && command -v brew >/dev/null 2>&1; then
    if brew --prefix openssl@3 >/dev/null 2>&1; then
        export OPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"
    fi
fi

echo "Using C compiler: ${CC}"
echo "Using C++ compiler: ${CXX}"

CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_CXX_COMPILER="${CXX}"
    -DCMAKE_C_COMPILER="${CC}"
)

if [[ "$(uname -s)" == Linux ]]; then
    LLVM_MAJOR="$("${CXX}" -dumpversion | cut -d. -f1)"
    MODULES_JSON="/usr/lib/llvm-${LLVM_MAJOR}/lib/libc++.modules.json"
    if [[ ! -f "${MODULES_JSON}" ]]; then
        echo "Missing ${MODULES_JSON}; install libc++-${LLVM_MAJOR}-dev" >&2
        exit 1
    fi
    CMAKE_ARGS+=(
        "-DCMAKE_CXX_FLAGS=-stdlib=libc++"
        "-DCMAKE_EXE_LINKER_FLAGS=-stdlib=libc++ -lc++abi"
        "-DCMAKE_CXX_STDLIB_MODULES_JSON=${MODULES_JSON}"
    )
    echo "Using libc++ modules metadata: ${MODULES_JSON}"
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja "${CMAKE_ARGS[@]}"

if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
else
    JOBS="$(sysctl -n hw.ncpu)"
fi

cmake --build "${BUILD_DIR}" --target YumeBot -j "${JOBS}"

cp "${BUILD_DIR}/bin/YumeBot" "${DIST_DIR}/${ARTIFACT_NAME}"
chmod +x "${DIST_DIR}/${ARTIFACT_NAME}"

echo "Built ${DIST_DIR}/${ARTIFACT_NAME}"
