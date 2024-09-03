@echo off

Echo uninstalling Gaming Services appxbundle

FOR /f "tokens=1,2*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\PowerShell\1\ShellIds\Microsoft.PowerShell" /v "Path" /reg:32 2^>NUL') DO SET POWERSHELLEXE=%%c

IF "%POWERSHELLEXE%"=="" (
    ECHO Unable to find powershell exe path.
    EXIT /B 1
)

%POWERSHELLEXE% -Version 3.0 -NoProfile -NonInteractive -InputFormat None -ExecutionPolicy Bypass -Command "Remove-AppxPackage -AllUsers Microsoft.GamingServices_16.82.27002.0_x64__8wekyb3d8bbwe;";
