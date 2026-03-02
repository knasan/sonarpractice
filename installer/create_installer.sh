#!/bin/bash
set -e

# --- 1. Pfade & Verzeichnisse definieren ---
# Wir nutzen die Variablen aus der cli.yml oder leiten sie dynamisch ab
INSTALLERDIR=$(dirname "$0")

# Falls GITHUB_WORKSPACE leer ist (lokal), nutzen wir den Pfad relativ zum Skript
REPO_ROOT="${GITHUB_WORKSPACE:-$(cd "$INSTALLERDIR/.." && pwd)}"

# Dynamische Suche nach den Tools auf dem Runner
# Wir suchen im Qt-Ordner (D:/a/sonarpractice/Qt)
QT_TOOLS_ROOT="D:/a/sonarpractice/Qt/Tools"

# Finde IFW Pfad (repogen/binarycreator)
IFW_BIN_DIR=$(find "$QT_TOOLS_ROOT/QtInstallerFramework" -name "repogen.exe" 2>/dev/null | head -n 1 | xargs dirname)

# Finde Qt-Bin Pfad (windeployqt6.exe)
# Da du LLVM nutzt, suchen wir im llvm-mingw Ordner
QT_BIN_DIR=$(find "D:/a/sonarpractice/Qt/6.10.2" -name "windeployqt6.exe" 2>/dev/null | head -n 1 | xargs dirname)

# Validierung der Tools
if [ -z "$IFW_BIN_DIR" ] || [ -z "$QT_BIN_DIR" ]; then
    echo "FEHLER: Tools konnten nicht gefunden werden!"
    echo "IFW: $IFW_BIN_DIR | Qt: $QT_BIN_DIR"
    exit 1
fi

# --- 2. Vorbereitung der Ordner ---
tmpDir="tmp"
dataDir="$INSTALLERDIR/packages/io.github.knasan.sonarpractice/data"
mkdir -p "$dataDir"
mkdir -p "$tmpDir"

# --- 3. Release-Datei kopieren & Deployment ---
# Nutze den Pfad aus der cli.yml
ACTUAL_RELEASE_PATH="${RELEASE_PATH:-$REPO_ROOT/build/SonarPractice.exe}"

echo "Kopiere Release von: $ACTUAL_RELEASE_PATH"
if [ -f "$ACTUAL_RELEASE_PATH" ]; then
    cp -v "$ACTUAL_RELEASE_PATH" "$tmpDir/SonarPractice.exe"
else
    echo "Fehler: SonarPractice.exe nicht gefunden!"
    exit 1
fi

# windeployqt6 ausführen
cd "$tmpDir" || exit 1
"$QT_BIN_DIR/windeployqt6.exe" SonarPractice.exe
cd ..

# --- 4. Installer Struktur befüllen ---
echo "Befülle $dataDir"
rm -rf "$dataDir"/*
cp -r "$tmpDir"/* "$dataDir/"
rm -rf "$tmpDir"

# --- 5. Versionierung via Git ---
VERSION=$(git -C "$REPO_ROOT" describe --tags --always --abbrev=0 2>/dev/null || date +%Y.%m.%d)
BUILD_DATE=$(date +%Y-%m-%d)
BRANCH=$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD)

# --- 6. Templates anpassen & Bauen ---
# 6a. Templates in die Zielordner kopieren
cp -v "$INSTALLERDIR/config_template.xml" "$INSTALLERDIR/config/config.xml"
cp -v "$INSTALLERDIR/package_template.xml" \
   "$INSTALLERDIR/packages/io.github.knasan.sonarpractice/meta/package.xml"

# 6b. Platzhalter mit sed ersetzen
sed -i "s|@VERSION@|$VERSION|g" "$INSTALLERDIR/config/config.xml"
sed -i "s|@BUILD_DATE@|$BUILD_DATE|g" "$INSTALLERDIR/config/config.xml"

sed -i "s|@VERSION@|$VERSION|g" "$INSTALLERDIR/packages/io.github.knasan.sonarpractice/meta/package.xml"
sed -i "s|@BUILD_DATE@|$BUILD_DATE|g" "$INSTALLERDIR/packages/io.github.knasan.sonarpractice/meta/package.xml"

echo "Baue Installer für Version: $VERSION"

"$IFW_BIN_DIR/repogen.exe" -p "$INSTALLERDIR/packages" "$INSTALLERDIR/repo" || exit 1
"$IFW_BIN_DIR/binarycreator.exe" --online-only -c "$INSTALLERDIR/config/config.xml" -p "$INSTALLERDIR/packages" SonarPractice_Web_Setup.exe || exit 1
