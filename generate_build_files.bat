@echo off
setlocal enabledelayedexpansion

for %%a in (%*) do set "%%~a=1"

if "%release%"=="1" (
    echo [release mode]
    cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
) else (
    echo [debug mode]
    cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Debug
)
pause