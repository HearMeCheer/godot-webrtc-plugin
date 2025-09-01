#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status
set -o pipefail # Catch errors in pipelines

# Default settings
EXTENSION_PATH=
OUTPUT_PATH="./build"
EXPORT_PRESETS_FILE="./export_presets.cfg"

# Parse script arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --release)
            BUILD_MODE="release"
            shift # Remove the current argument
            ;;
        --debug)
            BUILD_MODE="debug"
            shift # Remove the current argument
            ;;
        --output)
            OUTPUT_PATH="$2"
            shift 2 # Remove both the flag and its value
            ;;
        --export-presets)
            EXPORT_PRESETS_FILE="$2"
            shift 2 # Remove both the flag and its value
            ;;            
        *)
            EXTENSION_PATH="$1"
            shift # Remove the current argument
            ;;
    esac
done

GODOT_BIN=
GODOT_PRESET="Windows Desktop"
EXPORT_EXT="exe"
DEMO_PATH=

function clone_demo() {
    echo "Cloning demo repository..."
    rm -rf ./GodotMultiplayerDemo
    git clone https://github.com/HearMeCheer/GodotMultiplayerDemo.git
    DEMO_PATH="$(pwd)/GodotMultiplayerDemo"
}

function download_godot() {
    local godot_version="${1:-4.3}"
    echo "Downloading Godot $godot_version ..."
    rm -rf ./godot
    wget "https://github.com/godotengine/godot/releases/download/$godot_version-stable/Godot_v$godot_version-stable_linux.x86_64.zip" -O godot.zip
    mkdir -p ./godot
    unzip -qo godot.zip -d ./godot
    if [[ ! -f godot_export_templates.tpz ]]; then
        wget "https://github.com/godotengine/godot/releases/download/$godot_version-stable/Godot_v$godot_version-stable_export_templates.tpz" -O godot_export_templates.tpz
        unzip -qo godot_export_templates.tpz -d ./godot
        mv ./godot/templates ~/.local/share/godot/export_templates/$godot_version.stable
    fi
    rm godot.zip
    GODOT_BIN="$(pwd)/godot/Godot_v${godot_version}-stable_linux.x86_64"
    if [[ ! -f "$GODOT_BIN" ]]; then
        echo "Error: Godot binary not found at $GODOT_BIN"
        exit 1
    fi
    chmod +x "$GODOT_BIN"
    echo "Godot binary is ready at $GODOT_BIN"
}

function build_demo() {
    echo "Building demo project $DEMO_PATH..."
    DIST_DIR="$(pwd)/dist"
    mkdir -p "$DIST_DIR"
	$GODOT_BIN -v --path $DEMO_PATH --headless --export-debug "$GODOT_PRESET" "$DIST_DIR/HMCDemo.exe"
}

mkdir -p "$OUTPUT_PATH"
cp "$EXPORT_PRESETS_FILE" "$OUTPUT_PATH/export_presets.cfg"
cd "$OUTPUT_PATH"

download_godot
clone_demo
cp "export_presets.cfg" "$DEMO_PATH/export_presets.cfg"
build_demo