#!/bin/bash

source "scripts/install-functions.sh"

echo "INFO: copying files"
copy_files_into_install_folder "release" "${HOME}/local"
copy_files_into_install_folder "debug" "${HOME}/local-gcc-memcheck"

echo "INFO: install success"
echo ""

exit 0

# eof
