@echo off
setlocal
cd /d "%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto missingvs
set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL goto missingvs
call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 goto failed
set "CMAKEEXE=%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NINJAEXE=%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not exist "%CMAKEEXE%" goto missingcmake
if not exist "%NINJAEXE%" goto missingcmake
"%CMAKEEXE%" -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM:FILEPATH="%NINJAEXE%"
if errorlevel 1 goto failed
"%CMAKEEXE%" --build "%~dp0build" --target Afterglow_VST3 --parallel 2
if errorlevel 1 goto failed
set "PLUGINFOLDER=%~dp0build\Afterglow_artefacts\Release\VST3"
if not exist "%PLUGINFOLDER%\Afterglow.vst3" goto failed
echo.
echo SUCCESS: Your VST3 is ready in the folder opening now.
echo Copy the entire Afterglow.vst3 folder into C:\Program Files\Common Files\VST3
echo Windows may ask for administrator permission when you copy it.
echo Then rescan VST3 plug-ins in Ableton.
start "" explorer.exe "%PLUGINFOLDER%"
pause
exit /b 0
:missingvs
echo C++ build tools were not found. In Visual Studio Installer, select Desktop development with C++.
pause
exit /b 1
:missingcmake
echo CMake or Ninja was not found. In Visual Studio Installer, add C++ CMake tools for Windows.
pause
exit /b 1
:failed
echo.
echo BUILD STOPPED. Please share the first error above with Codex.
pause
exit /b 1
