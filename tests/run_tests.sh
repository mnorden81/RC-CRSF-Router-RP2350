#!/bin/sh
# Host-Tests (ohne RP2350): sh tests/run_tests.sh
set -e
T=$(cd "$(dirname "$0")" && pwd)
S="$T/../firmware/RC_CRSF_Router_RP2350"
B="${BUILD_DIR:-${TMPDIR:-/tmp}/router8_build}"
mkdir -p "$B"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Wno-unused-parameter -I"$S" \
  "$T/test_router.cpp" "$S/multi_router.cpp" "$S/router_config.cpp" "$S/src/RC_Common/lua_menu.cpp" \
  "$S/src/RC_Common/CRSF_ESP32/crsf_esp32.cpp" -o "$B/test_router"
"$B/test_router"
