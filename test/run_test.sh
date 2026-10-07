#!/bin/bash
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [ ! -f build_dummy/pitrix ]; then
    echo "=== Building pitrix (dummy mode) ==="
    mkdir -p build_dummy && cd build_dummy
    cmake .. -DPTX_SCREEN=dummy -DPTX_PLATFORM=dummy && make -j$(nproc)
    cd ..
fi

PASS=0; FAIL=0

# ---- Test 1: all apps load + display ----
echo "=== Test 1: all apps load and display ==="
OUTPUT=$(mktemp -p .)
{
    echo 'dofile("test/test.lua")'
    sleep 3
} | timeout -s SIGINT 5 ./build_dummy/pitrix > "$OUTPUT" 2>&1 || true

cat "$OUTPUT"
echo ""

grep -q '#'            "$OUTPUT" && { echo "PASS: display has lit pixels";  PASS=$((PASS+1)); } || { echo "FAIL: no lit pixels";  FAIL=$((FAIL+1)); }
grep -q '\[test\] loading'    "$OUTPUT" && { echo "PASS: test script loaded apps"; PASS=$((PASS+1)); } || { echo "FAIL: script failed"; FAIL=$((FAIL+1)); }
grep -q '\[test\] calling sys.ls' "$OUTPUT" && { echo "PASS: sys.ls called"; PASS=$((PASS+1)); } || { echo "FAIL: sys.ls failed"; FAIL=$((FAIL+1)); }
rm -f "$OUTPUT"

# ---- Test 2: time app continuous loop ----
echo ""
echo "=== Test 2: loop() continuous refresh (5s) ==="
mkdir -p test/tmp_debug/minitest
cat > test/tmp_debug/minitest/entry.lua << 'LUA'
function setup()
    label = lvgl.Label(nil, { x = 0, y = 1, text = "HELO", text_color = "#FFF" })
    return { loop = 1000 }
end
count = 0
function loop()
    count = count + 1
    print("LOOP#" .. count .. " at " .. os.date("%S"))
    label:set({ text = "T" .. count })
end
LUA

OUTPUT2=$(mktemp -p .)
{
    echo 'sys.load("test/tmp_debug/minitest")'
    sleep 5
} | timeout -s SIGINT 7 ./build_dummy/pitrix > "$OUTPUT2" 2>&1 || true

cat "$OUTPUT2"
echo ""

LOOPCNT=$(grep -c "LOOP#" "$OUTPUT2" 2>/dev/null || true)
ESCCNT=$(tr -dc "\033" < "$OUTPUT2" | wc -c)
echo "loop() fires: $LOOPCNT  |  ESC bytes: $ESCCNT"

if [ "$LOOPCNT" -ge 3 ]; then
    echo "PASS: loop() fired $LOOPCNT times (timer mechanism ok)"
    PASS=$((PASS+1))
else
    echo "FAIL: loop() only $LOOPCNT fires"
    FAIL=$((FAIL+1))
fi

if [ "$ESCCNT" -ge 3 ]; then
    echo "PASS: screen flushed (init + disp_flush ok)"
    PASS=$((PASS+1))
else
    echo "FAIL: screen not flushing"
    FAIL=$((FAIL+1))
fi

rm -f "$OUTPUT2"
rm -rf test/tmp_debug

echo ""
echo "Results: $PASS passed, $FAIL failed"
exit $FAIL
