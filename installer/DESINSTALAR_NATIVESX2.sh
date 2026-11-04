#!/usr/bin/env bash
# Desinstalador Limpo do NativeSX2 para Linux
# NewPortGame - Cristiano Mariano

echo "=========================================================="
echo "  Desinstalador do NativeSX2 (by NewPortGame)"
echo "=========================================================="

echo "1. Removendo atalhos da Area de Trabalho..."
rm -f "$HOME/Desktop/NativeSX2.desktop" "$HOME/Área de trabalho/NativeSX2.desktop" 2>/dev/null || true

echo "2. Removendo instalacao do usuario (~/.local/share/NativeSX2)..."
rm -rf "$HOME/.local/share/NativeSX2"
rm -f "$HOME/.local/share/applications/nativesx2.desktop"
rm -f "$HOME/.local/share/icons/hicolor/256x256/apps/nativesx2.png"

echo "3. Verificando instalacao de sistema (/opt/nativesx2)..."
if dpkg -l nativesx2 >/dev/null 2>&1; then
    echo "Pacote .deb detectado. Solicitando permissao para remover do sistema:"
    sudo dpkg -r nativesx2 2>/dev/null || true
    sudo rm -rf /opt/nativesx2 /usr/share/pixmaps/nativesx2.png /usr/share/applications/nativesx2.desktop /usr/local/bin/nativesx2 2>/dev/null || true
fi

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true

echo ""
echo "=========================================================="
echo "  SUCESSO! O NativeSX2 foi totalmente desinstalado."
echo "  Seu sistema esta 100% limpo para testar uma nova instalacao."
echo "=========================================================="
