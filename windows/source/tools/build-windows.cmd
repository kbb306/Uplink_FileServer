@echo off
REM Run from a 32-bit x86 Visual Studio Developer Command Prompt.
cd /d "%~dp0\.."
if not exist bin mkdir bin
if not exist .obj mkdir .obj
cl /nologo /TC /O2 /GS- /W4 /c src\fileserver_fix.c /Fo.obj\fileserver_fix.obj
if errorlevel 1 exit /b 1
link /nologo /DLL /MACHINE:X86 /SUBSYSTEM:WINDOWS /ENTRY:_DllMain@12 /NODEFAULTLIB /OUT:bin\UplinkFileServer.dll .obj\fileserver_fix.obj kernel32.lib
if errorlevel 1 exit /b 1
cl /nologo /TC /O2 /GS- /W4 /c src\injector.c /Fo.obj\injector.obj
if errorlevel 1 exit /b 1
cl /nologo /TC /O2 /GS- /W4 /c src\inject_core.c /Fo.obj\inject_core.obj
if errorlevel 1 exit /b 1
link /nologo /MACHINE:X86 /SUBSYSTEM:CONSOLE /ENTRY:mainCRTStartup /NODEFAULTLIB /OUT:bin\InjectUplinkFileServer.exe .obj\injector.obj .obj\inject_core.obj kernel32.lib
if errorlevel 1 exit /b 1
cl /nologo /TC /O2 /GS- /W4 /c src\auto_launcher.c /Fo.obj\auto_launcher.obj
if errorlevel 1 exit /b 1
link /nologo /MACHINE:X86 /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /NODEFAULTLIB /OUT:bin\AutoUplinkFileServer.exe .obj\auto_launcher.obj .obj\inject_core.obj kernel32.lib
if errorlevel 1 exit /b 1
cl /nologo /TC /O2 /GS- /W4 /c src\version_proxy.c /Fo.obj\version_proxy.obj
if errorlevel 1 exit /b 1
link /nologo /DLL /MACHINE:X86 /SUBSYSTEM:WINDOWS /ENTRY:_DllMain@12 /NODEFAULTLIB /DEF:src\version_proxy.def /OUT:bin\VERSION.dll .obj\version_proxy.obj kernel32.lib user32.lib
if errorlevel 1 exit /b 1
echo Build succeeded.
REM Diagnostic pure forwarder, no worker thread and no FILESERVER patch.
cl /nologo /TC /O2 /GS- /W4 /DUFS_PASSTHROUGH_ONLY /c src\version_proxy.c /Fo.obj\version_proxy_passthrough.obj
if errorlevel 1 exit /b 1
link /nologo /DLL /MACHINE:X86 /SUBSYSTEM:WINDOWS /ENTRY:_DllMain@12 /NODEFAULTLIB /DEF:src\version_proxy.def /OUT:bin\VERSION_PassthroughOnly.dll .obj\version_proxy_passthrough.obj kernel32.lib user32.lib
if errorlevel 1 exit /b 1
