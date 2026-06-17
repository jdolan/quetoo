@echo off
rem Dev helper: build and run the standalone qglib unit tests with MSVC.
rem Uses cl if already on PATH, else imports VS via vswhere + vcvars64.bat.
rem On Linux/macOS use the Makefile instead: `make test`.
rem Usage: run_tests.bat [source files...]   (defaults to the full test+impl set)
setlocal enabledelayedexpansion
cd /d "%~dp0"
set "SRCS=%*"
if "%SRCS%"=="" set "SRCS=test_qglib.c qglib_array.c qglib_list.c"

where cl >nul 2>nul
if not errorlevel 1 goto :build

rem Capture via a temp file, not for /f in(...): the ) in (x86) would close the for parens.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!VSWHERE!" "!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\qg_vspath.txt" 2>nul
if exist "%TEMP%\qg_vspath.txt" set /p VSPATH=<"%TEMP%\qg_vspath.txt"
del "%TEMP%\qg_vspath.txt" >nul 2>nul
if defined VSPATH if exist "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul

where cl >nul 2>nul
if errorlevel 1 (echo ERROR: cl.exe not found. Run from a VS Developer Command Prompt. & exit /b 1)

:build
cl /nologo /std:c11 /W3 /Fe:"%~dp0qgtest.exe" %SRCS%
if errorlevel 1 exit /b 1
"%~dp0qgtest.exe"
