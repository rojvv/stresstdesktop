#!/bin/bash
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_root="$repo_root/.stress-build"
archive="$build_root/onnxruntime-apple-1.22.0.zip"
checksum=90d9de5a139087a6b05a18125d01d01d198820e1731e6f0f11b38749b2ab181f
architecture="${STRESS_ARCH:-$(uname -m)}"
mkdir -p "$build_root"
if [[ ! -f "$archive" ]]; then
    curl --fail --location --retry 3 \
        https://download.onnxruntime.ai/pod-archive-onnxruntime-c-1.22.0.zip \
        -o "$archive.partial"
    mv "$archive.partial" "$archive"
fi
printf '%s  %s\n' "$checksum" "$archive" | shasum -a 256 -c -
unzip -oq "$archive" 'onnxruntime.xcframework/macos-arm64_x86_64/*' LICENSE \
    -d "$build_root/ort-apple"
framework="$build_root/ort-apple/onnxruntime.xcframework/macos-arm64_x86_64/onnxruntime.framework/Versions/A"
install_root="$build_root/onnxruntime-static"
mkdir -p "$install_root/lib" "$install_root/include"
lipo "$framework/onnxruntime" -thin "$architecture" \
    -output "$install_root/lib/libonnxruntime.a"
cp "$framework/Headers/"*.h "$install_root/include/"
cp "$build_root/ort-apple/LICENSE" "$install_root/LICENSE"
file "$install_root/lib/libonnxruntime.a"
