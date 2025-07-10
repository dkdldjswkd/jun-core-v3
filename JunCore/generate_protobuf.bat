@echo off
setlocal enabledelayedexpansion
echo ===============================================
echo     Protobuf Auto-Generator for JunCore
echo ===============================================

:: protoc 경로 설정
set PROTOC_PATH=C:\vcpkg\installed\x64-windows\tools\protobuf\protoc.exe
if not exist "%PROTOC_PATH%" (
    echo [ERROR] protoc not found at %PROTOC_PATH%
    echo Please check your vcpkg installation
    pause
    exit /b 1
)

echo [INFO] Using protoc: %PROTOC_PATH%
echo.

:: 제외할 디렉터리 패턴 설정
set EXCLUDE_DIRS=Release Debug x64 x86 Win32 .vs .git bin obj packages node_modules

:: .proto 파일 검색 및 컴파일
set TOTAL_FILES=0
set SUCCESS_FILES=0
set ERROR_FILES=0

echo [INFO] Searching for .proto files...
echo.

:: 현재 디렉터리부터 재귀적으로 .proto 파일 검색
for /r %%f in (*.proto) do (
    set CURRENT_FILE=%%f
    set SKIP_FILE=0
    
    :: 제외할 디렉터리인지 확인
    for %%d in (%EXCLUDE_DIRS%) do (
        echo !CURRENT_FILE! | findstr /i "\\%%d\\" >nul
        if !ERRORLEVEL! == 0 (
            set SKIP_FILE=1
        )
    )
    
    :: 파일 처리
    if !SKIP_FILE! == 0 (
        set /a TOTAL_FILES+=1
        
        echo [PROCESSING] !CURRENT_FILE!
        
        :: 상대 경로로 변환
        set REL_PATH=!CURRENT_FILE:%CD%\=!
        
        :: protoc 실행
        "%PROTOC_PATH%" --cpp_out=. "!REL_PATH!"
        
        if !ERRORLEVEL! == 0 (
            set /a SUCCESS_FILES+=1
            echo [SUCCESS] Generated .pb.h and .pb.cc files
        ) else (
            set /a ERROR_FILES+=1
            echo [ERROR] Failed to compile !REL_PATH!
        )
        echo.
    ) else (
        echo [SKIPPED] !CURRENT_FILE! (in excluded directory)
    )
)

:: 결과 요약
echo ===============================================
echo               GENERATION SUMMARY
echo ===============================================
echo Total .proto files found: %TOTAL_FILES%
echo Successfully compiled: %SUCCESS_FILES%
echo Failed to compile: %ERROR_FILES%
echo ===============================================

if %ERROR_FILES% == 0 (
    echo [SUCCESS] All protobuf files generated successfully!
    echo.
    echo Generated .pb.h and .pb.cc files can be found next to their .proto sources.
    echo Remember to add new .pb.h and .pb.cc files to your project if needed.
) else (
    echo [WARNING] Some files failed to compile. Please check the errors above.
)

echo.
echo Press any key to exit...
pause >nul