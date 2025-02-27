#!/bin/bash

set -e

echo ""
echo ""
echo "--------------------------------------------------------------------"
echo "| RUNNING CMAKE IN DEBUG MODE (NINJA)                              |"
echo "--------------------------------------------------------------------"
echo ""

cmake .. -G"Ninja" \
         -DFORCE_COLORED_OUTPUT=1 \
         -DCMAKE_BUILD_TYPE=DEBUG \
         -DCMAKE_C_COMPILER="clang" \
         -DCMAKE_C_FLAGS="-fuse-ld=lld" \
         -DCMAKE_CXX_COMPILER="clang++" \
         -DCMAKE_CXX_FLAGS="\
            -fuse-ld=lld \
            -ftime-trace \
            -Og -g3 -fno-omit-frame-pointer \
            -Wall -Wextra -Wpedantic -Wno-braced-scalar-init \
            -Wno-pragmas -Wno-missing-field-initializers -Wno-array-bounds \
            -D_GLIBCXX_ASSERTIONS=1 -D_FORTIFY_SOURCE=3 \
            -fstack-protector -Wno-pragmas \
            -frounding-math -ffp-contract=off \
            -Wno-unknown-warning-option \
            -Wno-deprecated-non-prototype -Wno-unknown-attributes -Wno-maybe-uninitialized \
            -Wno-unused-command-line-argument \
            -DSFML_ENABLE_LIFETIME_TRACKING=1" \
         -DCMAKE_C_FLAGS="-Wno-deprecated-non-prototype -Wno-unknown-attributes"
