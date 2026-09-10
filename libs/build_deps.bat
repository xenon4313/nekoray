@echo off
setlocal enabledelayedexpansion

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set "ROOT=%~dp0"
set "DEPS_DIR=%ROOT%deps"
set "PREFIX=%DEPS_DIR%\built"

if not exist "%DEPS_DIR%" mkdir "%DEPS_DIR%"
if not exist "%PREFIX%" mkdir "%PREFIX%"

cd /d "%DEPS_DIR%"

echo [1/3] Building ZXing...
if not exist "%PREFIX%\lib\ZXing.lib" (
    if not exist "zxing-cpp-2.0.0" (
        curl.exe -L "https://github.com/nu-book/zxing-cpp/archive/refs/tags/v2.0.0.zip" -o zxing.zip
        tar -xf zxing.zip
        del zxing.zip
    )
    cd zxing-cpp-2.0.0
    if not exist build mkdir build
    cd build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_BLACKBOX_TESTS=OFF -DCMAKE_INSTALL_PREFIX="%PREFIX%"
    ninja install
    cd "%DEPS_DIR%"
) else (
    echo ZXing already built.
)

echo [2/3] Building yaml-cpp...
if not exist "%PREFIX%\lib\yaml-cpp.lib" (
    if not exist "yaml-cpp-yaml-cpp-0.7.0" (
        curl.exe -L "https://github.com/jbeder/yaml-cpp/archive/refs/tags/yaml-cpp-0.7.0.zip" -o yaml.zip
        tar -xf yaml.zip
        del yaml.zip
    )
    cd yaml-cpp-yaml-cpp-0.7.0
    if not exist build mkdir build
    cd build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="%PREFIX%"
    ninja install
    cd "%DEPS_DIR%"
) else (
    echo yaml-cpp already built.
)

echo [3/3] Building protobuf...
if not exist "%PREFIX%\lib\libprotobuf.lib" (
    if not exist "protobuf" (
        git clone --recurse-submodules -b v21.4 --depth 1 --shallow-submodules https://github.com/protocolbuffers/protobuf
    )
    cd protobuf
    if not exist build mkdir build
    cd build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -Dprotobuf_MSVC_STATIC_RUNTIME=OFF -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX="%PREFIX%"
    ninja install
    cd "%DEPS_DIR%"
) else (
    echo protobuf already built.
)

echo Dependencies built successfully!
