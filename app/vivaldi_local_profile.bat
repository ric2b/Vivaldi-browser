@echo off
rem This scripts starts vivaldi with non-default profile directory.
rem It automatically finds vivaldi.exe (checking first the local folder).
rem 
rem (c)2014 Vivaldi Technologies AS
rem
setlocal

set REG_QUERY="reg query HKCU\Software\Vivaldi /v InstallerSuccessLaunchCmdLine"
for /F "skip=2 tokens=3,*" %%A in ('%REG_QUERY%')  do set "VIVALDI_EXE=%%A"
rem reg query HKEY_CURRENT_USER\Software\Vivaldi /v UninstallString

if exist "vivaldi.exe" set VIVALDI_EXE="vivaldi.exe"

if "%VIVALDI_EXE%"=="" goto :FAIL

start "" %VIVALDI_EXE% --user-data-dir="%~dp0User Data" %*
goto :EOF

:FAIL
echo Failed to launch - Either install Vivaldi or move this script to 
echo the same folder as vivaldi.exe
pause
