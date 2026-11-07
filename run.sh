#!/bin/sh
# Builds and starts the app. On macOS it is a bundle: build/raven_app.app
cd "$(dirname "$0")" || exit 1
[ -f build/CMakeCache.txt ] || cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug || exit 1
cmake --build build || exit 1
if [ -x build/raven_app.app/Contents/MacOS/raven_app ]; then
	exec build/raven_app.app/Contents/MacOS/raven_app "$@"
fi
exec build/raven_app "$@"
