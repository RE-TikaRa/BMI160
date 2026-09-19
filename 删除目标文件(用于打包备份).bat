@echo off
setlocal
pushd "%~dp0" || exit /b 1
set "exit_code=0"

if exist "project\MDK(V5)\Listings" (
    rd /Q /S "project\MDK(V5)\Listings" || set "exit_code=1"
)
if exist "project\MDK(V5)\Objects" (
    rd /Q /S "project\MDK(V5)\Objects" || set "exit_code=1"
)
if exist "project\MDK(V5)\Project.uvgui.*" (
    del /Q "project\MDK(V5)\Project.uvgui.*" || set "exit_code=1"
)

popd
endlocal & exit /b %exit_code%
