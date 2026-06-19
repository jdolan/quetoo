@echo off
REM Build QuemapGui.exe using the in-box .NET Framework C# compiler.
REM No Visual Studio or .NET SDK required -- csc.exe ships with Windows.

setlocal
set "CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%CSC%" set "CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"

if not exist "%CSC%" (
  echo Could not find the in-box C# compiler ^(csc.exe^).
  echo Expected under %%WINDIR%%\Microsoft.NET\Framework[64]\v4.0.30319\.
  echo Build in Visual Studio instead, or install the .NET Framework 4.x developer pack.
  exit /b 1
)

"%CSC%" /nologo /target:winexe /out:QuemapGui.exe ^
  /reference:System.dll ^
  /reference:System.Drawing.dll ^
  /reference:System.Windows.Forms.dll ^
  QuemapGui.cs

if errorlevel 1 (
  echo.
  echo Build FAILED.
  exit /b 1
)

echo.
echo Built QuemapGui.exe
endlocal
