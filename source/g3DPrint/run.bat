@echo off
REM Wrapper script to run g3DPrint with correct working directory for data files

setlocal EnableDelayedExpansion

REM Get the directory where this script is located (bazel-bin/source/g3DPrint/)
set "SCRIPT_DIR=%~dp0"

REM The exe is in the same directory as this script
set "EXE_PATH=%SCRIPT_DIR%g3DPrint.exe"

REM Source directories (relative to the project root)
set "PROJECT_ROOT=%SCRIPT_DIR%..\..\..\"

REM Create temp directory for working files
set "WORK_DIR=%TEMP%\g3DPrint_%RANDOM%"
mkdir "%WORK_DIR%" 2>nul

REM Copy shared assets from source tree
xcopy /Y /Q "%PROJECT_ROOT%source\shared_assets\*" "%WORK_DIR%\" >nul 2>&1

REM Copy PTX files from bazel-bin
copy /Y "%SCRIPT_DIR%..\gvdb_library\*.ptx" "%WORK_DIR%\" >nul 2>&1

REM Copy shader files from source tree
xcopy /Y /Q "%PROJECT_ROOT%source\gvdb_library\shaders\*.glsl" "%WORK_DIR%\" >nul 2>&1

REM Run the application from the work directory
cd /d "%WORK_DIR%"
"%EXE_PATH%" %*

REM Store exit code
set "EXIT_CODE=%ERRORLEVEL%"

REM Cleanup
cd /d "%TEMP%"
rmdir /S /Q "%WORK_DIR%" 2>nul

exit /b %EXIT_CODE%
