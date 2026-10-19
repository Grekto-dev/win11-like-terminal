#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SCRIPT_DIR"

echo "=== Win11-Like Terminal: Instalando dependências ==="
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  qt6-base-dev \
  qt6-declarative-dev \
  qt6-shadertools-dev \
  libqt6svg6-dev \
  libvterm-dev \
  libgl1-mesa-dev \
  libxcb1-dev \
  zenity

echo "=== Compilando o programa ==="
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build

echo "=== Instalando no sistema (/usr/bin/win11terminal) ==="
sudo cmake --install build

echo "=== Atualizando atalhos e ícones ==="
sudo update-desktop-database 2>/dev/null || true
sudo gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true

echo ""
echo "=========================================================="
echo " Instalação concluída com sucesso! (Versão 0.9.0)"
echo " Você já pode abrir digitando: win11terminal"
echo " Ou procurando por 'Terminal' no menu de aplicativos."
echo "=========================================================="
