#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status
set -o pipefail # Catch errors in pipelines

# Default settings
BUILD_MODE="debug"

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
        *)
            TARGET="$1"
            shift # Remove the current argument
            ;;
    esac
done

# Functions corresponding to Makefile targets
function go() {
    echo "Building pion wrapper..."
    if [[ $BUILD_MODE == "release" ]]; then
        cd ./thirdparty/pionc/ && make build-release
    else
        cd ./thirdparty/pionc/ && make build
    fi
}

function scons() {
    echo "Building Godot extension in $BUILD_MODE mode..."
    if [[ $BUILD_MODE == "release" ]]; then
        scons platform=macos target=release
    else
        scons platform=macos target=debug
    fi
}

function copy_ext() {
    echo "Copying extension output to demo projects..."
    cp -R -f ./build/extension-4.1/* ./demo/BomberMultiplayerDemo/
    cp -R -f ./build/extension-4.1/* ./demo/MultiplayerTemplate/
}

function gd() {
    echo "Building Godot extension and copying output to demo projects in $BUILD_MODE mode..."
    scons
    copy_ext
}

function dev() {
    echo "Building Godot extension in dev mode..."
    scons platform=macos dev_build=1
}

function tests() {
    echo "Running unit tests..."
    cd ./tests && make
}

function clean() {
    echo "Cleaning build artifacts..."
    scons -c
    rm -rf ./demo/BomberMultiplayerDemo/webrtc/*
    rm -rf ./demo/MultiplayerTemplate/webrtc/*
    rm -f build/Godot3DMultiplayer.linux.pck
}

function godot_export_debug() {
    echo "Exporting Godot project in debug mode..."
    mkdir -p ./dist/macOS
    /Applications/Godot.app/Contents/MacOS/Godot --path ./demo/MultiplayerTemplate --headless --export-debug "macOS" ../../dist/macOS/Godot3DMultiplayer.debug.macOS.dmg
}

function godot_server_export() {
    echo "Exporting Godot package for a dedicated game server..."
    mkdir -p ./dist/linux-server
    /Applications/Godot.app/Contents/MacOS/Godot --path ./demo/MultiplayerTemplate --headless --export-pack "Linux/DedicatedServer" ../../dist/linux-server/Godot3DMultiplayer.linux.pck
}

function docker() {
    echo "Building Docker image..."
    [ ! -f build/Godot3DMultiplayer.linux.pck ] && godot_server_export
    docker build --platform linux/x86_64 -t game-server:latest -f docker/Dockerfile ./build
}

function docker_push() {
    echo "Pushing Docker image to Docker Hub..."
    docker
    docker login
    docker tag game-server:latest eses2020/projects
    docker push eses2020/projects
}

function docker_run() {
    echo "Running Docker container..."
    docker
    docker run -d -p 1370:1370/udp game-server:latest
}

function all() {
    echo "Running all: go and gd in $BUILD_MODE mode..."
    go
    gd
}

# Check if a target was specified
if [[ -z $TARGET ]]; then
    echo "Usage: $0 [--release|--debug] <target>"
    echo "Available targets: go, scons, copy_ext, gd, dev, tests, clean, godot_export_debug, godot_server_export, docker, docker_push, docker_run, all"
    exit 1
fi

# Call the specified function
if declare -f "$TARGET" > /dev/null; then
    "$TARGET"
else
    echo "Error: Unknown target '$TARGET'"
    exit 1
fi