#!/bin/bash
# Wrapper script to run gBoolean with correct working directory for data files

# Get the directory where this script is located (runfiles root)
RUNFILES_DIR="${BASH_SOURCE[0]}.runfiles/_main"

# Copy/link necessary files to a temp directory for runtime
WORK_DIR=$(mktemp -d)
trap "rm -rf $WORK_DIR" EXIT

# Link shared assets
ln -sf "$RUNFILES_DIR/source/shared_assets"/* "$WORK_DIR/" 2>/dev/null || true

# Link PTX files
ln -sf "$RUNFILES_DIR/source/gvdb_library"/*.ptx "$WORK_DIR/" 2>/dev/null || true

# Link shader files
ln -sf "$RUNFILES_DIR/source/gvdb_library/shaders"/*.glsl "$WORK_DIR/" 2>/dev/null || true

# Run the application from the work directory
cd "$WORK_DIR"
exec "$RUNFILES_DIR/source/gBoolean/gBoolean" "$@"
