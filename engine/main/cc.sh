#!/bin/sh
# Compile files of engine/main one at a time with the flags of the CMake build.
# Usage: sh engine/main/cc.sh BUILD_DIR file.c [file.c ...]
# Run it from the root of the repo. BUILD_DIR is a configured CMake build
# directory. The script prints the errors and the warnings of each file.
build="$1"
shift
status=0
for f in "$@"; do
	name=$(basename "$f")
	cmake --build "$build" --target "engine/main/CMakeFiles/d_main.dir/$name.obj" 2>&1 |
		grep -E "error|warning|FAILED" | grep -v "^ninja: build stopped"
	if [ -f "$build/engine/main/CMakeFiles/d_main.dir/$name.obj" ]; then
		echo "$name: object file exists"
	else
		echo "$name: NO object file"
		status=1
	fi
done
exit $status
