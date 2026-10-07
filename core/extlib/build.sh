#!/bin/bash
CUR_DIR=$1
BUILD_TYPE=Release
CROSS_C_COMPILER=$2
CROSS_CXX_COMPILER=$3
CMAKE_COMPILE_FLAGS="-DCMAKE_C_COMPILER=$CROSS_C_COMPILER -DCMAKE_CXX_COMPILER=$CROSS_CXX_COMPILER -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_INSTALL_PREFIX=$CUR_DIR/output -DCMAKE_BUILD_TYPE=$BUILD_TYPE"

pushd $CUR_DIR

# 目标切换时清理旧产物 (build.sh 的跳过逻辑只看文件是否存在,
# 不清理的话 x86 的 .a 会被当成"已编译"跳过, 导致交叉链接失败)
# 用 -dumpmachine 归一化: gcc 与 /usr/bin/cc 都得到 x86_64-linux-gnu
TARGET=$("$CROSS_C_COMPILER" -dumpmachine 2>/dev/null || basename "$CROSS_C_COMPILER")
if [ "$(cat output/.target 2>/dev/null)" != "$TARGET" ]; then
    echo "=== target changed to $TARGET, cleaning old artifacts ==="
    rm -rf output lvgl/build luavgl/build rpi-ws281x/build lua-cjson/build
    (cd lua && make clean) || true
fi

mkdir -p output
mkdir -p output/include
mkdir -p output/lib
echo "$TARGET" > output/.target

# lua
if [ ! -f output/lib/liblua.a ]; then
    echo "=== building lua ==="
    pushd lua
    make a CC=$CROSS_C_COMPILER CFLAGS="-Wall -O2 -std=c99 -fno-stack-protector -fno-common" -j
    cp *.h ../output/include
    cp liblua.a ../output/lib
    popd
else
    echo "=== skip lua (already built) ==="
fi

# lvgl
if [ ! -f output/lib/liblvgl.a ]; then
    echo "=== building lvgl ==="
    pushd lvgl
    cp ../patch/lv_conf.h .
    mkdir build
    cd build
    cmake .. $CMAKE_COMPILE_FLAGS
    make -j
    make install
    popd
else
    echo "=== skip lvgl (already built) ==="
fi

# luavgl
# 注意: 必须用专用模板 CMakeLists.extlib.luavgl (只编译 luavgl.c),
# 不能用共用的 CMakeLists.extlib.tostatic (它会把所有 .c 单独编译,
# 而 luavgl 源码结构是 luavgl.c 通过 #include 聚合所有文件)
if [ ! -f output/lib/libluavgl.a ]; then
    echo "=== building luavgl ==="
    cp CMakeLists.extlib.luavgl luavgl/CMakeLists.txt
    pushd luavgl
    # v0.1.0编译时少包含了头文件；暂时不打算尝试luavgl上最新的提交了，就这么对付一下
    git apply ../patch/luavgl-v0.1.0.patch
    mkdir -p build
    cd build
    cmake .. -DSRC_PATH=$CUR_DIR/luavgl/src -DEXTERN_INCLUDE_PATH="$CUR_DIR/output/include/lvgl;$CUR_DIR/luavgl/src;$CUR_DIR/lua" $CMAKE_COMPILE_FLAGS -DLIBRARY_NAME="luavgl"
    make -j
    cp libluavgl.a $CUR_DIR/output/lib
    cp ../src/*.h $CUR_DIR/output/include
    popd
else
    echo "=== skip luavgl (already built) ==="
fi

# rpi-ws281x
if [ ! -f output/lib/libws2811.a ]; then
    echo "=== building rpi-ws281x ==="
    pushd rpi-ws281x
    mkdir build
    cd build
    cmake .. $CMAKE_COMPILE_FLAGS
    make -j
    make install
    popd
else
    echo "=== skip rpi-ws281x (already built) ==="
fi

# lua-cjson
if [ ! -f output/lib/liblua-cjson.a ]; then
    echo "=== building lua-cjson ==="
    cp CMakeLists.extlib.tostatic lua-cjson/CMakeLists.txt
    pushd lua-cjson
    # 用一个比较傻的办法让它使用fpconv.c...
    mv dtoa.c dtoa.c.disable
    mkdir build
    cd build
    cmake .. -DSRC_PATH=$CUR_DIR/lua-cjson -DEXTERN_INCLUDE_PATH="$CUR_DIR/lua" $CMAKE_COMPILE_FLAGS -DLIBRARY_NAME="lua-cjson"
    make -j
    cp liblua-cjson.a $CUR_DIR/output/lib
    cp ../*.h $CUR_DIR/output/include
    cd ..
    mv dtoa.c.disable dtoa.c
    popd
else
    echo "=== skip lua-cjson (already built) ==="
fi

popd
