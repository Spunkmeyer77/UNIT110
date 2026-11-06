#!/bin/bash
# Packs a Release build into dist/UNIT110-<version>-Linux-x86_64.tar.gz (with an install script)
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -nE 's/^project\(UNIT110 VERSION ([0-9.]+)\).*/\1/p' CMakeLists.txt)
ART="build/UNIT110_artefacts/Release"
NAME="UNIT110-$VERSION-Linux-$(uname -m)"
STAGE="dist/$NAME"

rm -rf "$STAGE" && mkdir -p "$STAGE"
cp -R "$ART/VST3/UNIT 110.vst3"   "$STAGE/"
cp -R "$ART/LV2/UNIT 110.lv2"     "$STAGE/"
cp    "$ART/Standalone/UNIT 110"  "$STAGE/"

cat > "$STAGE/install.sh" <<'EOS'
#!/bin/bash
# Installs UNIT 110 for the current user (no root needed).
set -e
cd "$(dirname "$0")"
mkdir -p ~/.vst3 ~/.lv2 ~/.local/bin
rm -rf ~/.vst3/"UNIT 110.vst3" ~/.lv2/"UNIT 110.lv2"
cp -R "UNIT 110.vst3" ~/.vst3/
cp -R "UNIT 110.lv2"  ~/.lv2/
cp "UNIT 110" ~/.local/bin/UNIT110 && chmod +x ~/.local/bin/UNIT110
echo "Installed: ~/.vst3/UNIT 110.vst3, ~/.lv2/UNIT 110.lv2, ~/.local/bin/UNIT110"
echo "Rescan plug-ins in your DAW (Bitwig, Reaper, Ardour...)."
EOS

cat > "$STAGE/uninstall.sh" <<'EOS'
#!/bin/bash
rm -rf ~/.vst3/"UNIT 110.vst3" ~/.lv2/"UNIT 110.lv2" ~/.local/bin/UNIT110
echo "UNIT 110 removed."
EOS
chmod +x "$STAGE/install.sh" "$STAGE/uninstall.sh"

cat > "$STAGE/README.txt" <<EOS
UNIT 110 $VERSION - hyperrealistic 110-style rhythm composer - Transistor Labs

Install:    ./install.sh      (VST3 -> ~/.vst3, LV2 -> ~/.lv2, app -> ~/.local/bin/UNIT110)
Uninstall:  ./uninstall.sh
Needs: ALSA or JACK, X11, FreeType (standard on desktop distributions).
EOS

tar -C dist -czf "dist/$NAME.tar.gz" "$NAME"
rm -rf "$STAGE"
echo "==> Done: dist/$NAME.tar.gz"
