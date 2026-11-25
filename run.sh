#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v gcc >/dev/null 2>&1 || ! command -v make >/dev/null 2>&1; then
    echo "Faltan gcc/make. Instalando dependencias..."
    if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update
        sudo apt-get install -y build-essential
    elif command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y gcc make
    elif command -v pacman >/dev/null 2>&1; then
        sudo pacman -S --needed --noconfirm gcc make
    else
        echo "Instala gcc y make manualmente y vuelve a ejecutar." >&2
        exit 1
    fi
fi

make

if [ "$#" -eq 0 ]; then
    exec bash ./test_blackout.sh
fi

exec ./tienda_node "$@"