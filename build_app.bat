@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

cd /d "C:\Users\xenon\Desktop\neko\nekoray-src"
if not exist build mkdir build
cd build

cmake .. -GNinja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DQT_VERSION_MAJOR=6 ^
    -DCMAKE_PREFIX_PATH="C:/Users/xenon/Desktop/neko/nekoray-src/qtsdk/Qt;C:/Users/xenon/Desktop/neko/nekoray-src/libs/deps/built"

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    exit /b %ERRORLEVEL%
)

ninja nekobox
if %ERRORLEVEL% NEQ 0 (
    echo Ninja build failed!
    exit /b %ERRORLEVEL%
)

echo nekobox built successfully!

if exist "..\..\release\nekoray" (
    echo Copying nekobox.exe to release\nekoray...
    copy /y "nekobox.exe" "..\..\release\nekoray\nekobox.exe"
)
