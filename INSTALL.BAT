@echo off
:StartExec
net session >nul 2>&1
if '%errorlevel%' NEQ '0' (
  echo Requesting administrative privileges...
  goto UACPrompt
) else ( goto gotAdmin )
:UACPrompt
echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\getadmin.vbs"
"%temp%\getadmin.vbs"
exit /B
:gotAdmin
if exist "%temp%\getadmin.vbs" ( del "%temp%\getadmin.vbs" )
pushd "%CD%"
CD /D "%~dp0"

if not exist %windir%\system32\drivers\vidprt0.sys (
copy %windir%\system32\drivers\videoprt.sys %windir%\system32\drivers\vidprt0.sys
)
takeown /f %windir%\system32\drivers\videoprt.sys
icacls %windir%\system32\drivers\videoprt.sys  /grant *S-1-1-0:F /T
copy videoprt.sys %windir%\system32\drivers\videoprt.sys
