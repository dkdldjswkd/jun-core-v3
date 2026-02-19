@echo off
set PROTOC="%~dp0vcpkg_installed\x64-windows-static\tools\protobuf\protoc.exe"

for /r "%~dp0" %%f in (*.proto) do (
    echo %%f | findstr /i "vcpkg_installed" >nul || (
        %PROTOC% --proto_path="%%~dpf" --cpp_out="%%~dpf" "%%f"
    )
)
