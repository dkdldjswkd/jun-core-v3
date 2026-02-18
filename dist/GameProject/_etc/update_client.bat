@echo off
echo ================================================
echo  Unity 클라이언트 빌드 → dist 복사
echo ================================================

set SRC=C:\rpos\Unity\SimpleProject\Build
set DST=C:\rpos\jun-core-v3\dist\GameProject\client

echo.
echo [1/5] SimpleProject.exe 복사 중...
copy /Y "%SRC%\SimpleProject.exe" "%DST%\"

echo [2/5] UnityPlayer.dll 복사 중...
copy /Y "%SRC%\UnityPlayer.dll" "%DST%\"

echo [3/5] SimpleProject_Data 복사 중...
xcopy /E /I /Y "%SRC%\SimpleProject_Data" "%DST%\SimpleProject_Data"

echo [4/5] MonoBleedingEdge 복사 중...
xcopy /E /I /Y "%SRC%\MonoBleedingEdge" "%DST%\MonoBleedingEdge"

echo [5/5] D3D12 복사 중...
xcopy /E /I /Y "%SRC%\D3D12" "%DST%\D3D12"

echo.
echo ================================================
echo  완료!
echo ================================================
pause
