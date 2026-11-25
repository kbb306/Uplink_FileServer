#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
CC=${CC:-clang}
LINK=${LINK:-lld-link}
mkdir -p bin .obj
# Build an import library with the public KERNEL32 exports, using no Windows SDK.
$LINK /dll /noentry /machine:x86 /def:src/kernel32.def /out:.obj/kernel32-stub.dll /implib:.obj/kernel32.lib >/dev/null
$CC -target i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -mstack-probe-size=65536 -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -Werror -Wall -Wextra -c src/fileserver_fix.c -o .obj/fileserver_fix.obj
args=()
for spec in 'GetModuleHandleA:4' 'GetModuleFileNameA:12' 'GetCurrentProcess:0' 'GetCurrentThreadId:0' 'GetCurrentProcessId:0' 'GetTickCount:0' 'FindWindowA:8' 'GetWindowThreadProcessId:8' 'VirtualProtect:16' 'FlushInstructionCache:12' 'CreateFileA:28' 'ReadFile:20' 'WriteFile:20' 'CloseHandle:4' 'SetFilePointer:16' 'AddVectoredExceptionHandler:8' 'RemoveVectoredExceptionHandler:4' 'TlsAlloc:0' 'TlsGetValue:4' 'TlsSetValue:8' 'DisableThreadLibraryCalls:4' 'CreateThread:24' 'Sleep:4' 'CreateRemoteThread:28' 'CreateToolhelp32Snapshot:8' 'Process32FirstA:8' 'Process32NextA:8' 'OpenProcess:12' 'VirtualAllocEx:20' 'WriteProcessMemory:20' 'VirtualFreeEx:16' 'GetProcAddress:8' 'WaitForSingleObject:8' 'GetExitCodeThread:8' 'GetStdHandle:4' 'ExitProcess:4' 'Module32FirstA:8' 'Module32NextA:8' 'CreateProcessA:40' 'GetCommandLineA:0' 'GetExitCodeProcess:8' 'GetSystemDirectoryA:8' 'LoadLibraryA:4' 'GetTempPathA:8' 'GetLastError:0'; do
  name=${spec%:*}; bytes=${spec#*:}
  args+=("/alternatename:__imp__${name}@${bytes}=__imp__${name}")
done
$LINK /dll /machine:x86 /subsystem:windows /entry:DllMain@12 /nodefaultlib /opt:ref /implib:.obj/UplinkFileServer.lib /out:bin/UplinkFileServer.dll .obj/fileserver_fix.obj .obj/kernel32.lib "${args[@]}"
# All components are 32-bit PE files; the proxy also imports USER32 for
# positive detection of Uplink's game window (not just a timer).
$LINK /dll /noentry /machine:x86 /def:src/user32.def /out:.obj/user32-stub.dll /implib:.obj/user32.lib >/dev/null
file bin/UplinkFileServer.dll

$CC -target i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -mstack-probe-size=65536 -fno-stack-protector -Werror -Wall -Wextra -c src/inject_core.c -o .obj/inject_core.obj
$CC -target i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -mstack-probe-size=65536 -fno-stack-protector -Werror -Wall -Wextra -c src/injector.c -o .obj/injector.obj
$LINK /machine:x86 /subsystem:console /entry:mainCRTStartup /nodefaultlib /opt:ref /out:bin/InjectUplinkFileServer.exe .obj/injector.obj .obj/inject_core.obj .obj/kernel32.lib "${args[@]}"
file bin/InjectUplinkFileServer.exe

$CC -target i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -mstack-probe-size=65536 -fno-stack-protector -Werror -Wall -Wextra -c src/auto_launcher.c -o .obj/auto_launcher.obj
$LINK /machine:x86 /subsystem:windows /entry:mainCRTStartup /nodefaultlib /opt:ref /out:bin/AutoUplinkFileServer.exe .obj/auto_launcher.obj .obj/inject_core.obj .obj/kernel32.lib "${args[@]}"
file bin/AutoUplinkFileServer.exe

# Drop-in, automatic VERSION.dll proxy.  No injector or Steam launch wrapper.
$CC -target i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -mstack-probe-size=65536 -fno-stack-protector -Werror -Wall -Wextra -c src/version_proxy.c -o .obj/version_proxy.obj
$LINK /dll /machine:x86 /subsystem:windows /entry:DllMain@12 /nodefaultlib /opt:ref /def:src/version_proxy.def /out:bin/VERSION.dll .obj/version_proxy.obj .obj/kernel32.lib .obj/user32.lib "${args[@]}"
file bin/VERSION.dll

# Diagnostic pure forwarder: all VERSION exports, but no CreateThread and no patch.
$CC -target i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -mstack-probe-size=65536 -fno-stack-protector -Werror -Wall -Wextra -DUFS_PASSTHROUGH_ONLY -c src/version_proxy.c -o .obj/version_proxy_passthrough.obj
$LINK /dll /machine:x86 /subsystem:windows /entry:DllMain@12 /nodefaultlib /opt:ref /def:src/version_proxy.def /out:bin/VERSION_PassthroughOnly.dll .obj/version_proxy_passthrough.obj .obj/kernel32.lib .obj/user32.lib "${args[@]}"
file bin/VERSION_PassthroughOnly.dll
