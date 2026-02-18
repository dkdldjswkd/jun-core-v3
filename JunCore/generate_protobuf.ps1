# Protobuf Auto-Generator for JunCore (Incremental Build)
# Usage: Right-click -> Run with PowerShell, or: powershell -ExecutionPolicy Bypass -File generate_protobuf.ps1

$ErrorActionPreference = "Stop"
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

Write-Host "===============================================" -ForegroundColor Cyan
Write-Host "    Protobuf Auto-Generator for JunCore" -ForegroundColor Cyan
Write-Host "         (Incremental Build Mode)" -ForegroundColor Cyan
Write-Host "===============================================" -ForegroundColor Cyan

# protoc 경로
$protocPath = "C:\vcpkg\packages\protobuf_x64-windows-static\tools\protobuf\protoc.exe"
if (-not (Test-Path $protocPath)) {
    Write-Host "[ERROR] protoc not found at $protocPath" -ForegroundColor Red
    exit 1
}

# 검색할 폴더 목록 (proto 파일이 있는 폴더만!)
$searchFolders = @(
    "Test",
    "EchoServer",
    "GameServer"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$totalFiles = 0
$buildFiles = 0
$skipFiles = 0
$errorFiles = 0

Write-Host ""
Write-Host "[INFO] Searching for .proto files..." -ForegroundColor Gray

foreach ($folder in $searchFolders) {
    $folderPath = Join-Path $scriptDir $folder
    if (-not (Test-Path $folderPath)) { continue }

    $protoFiles = Get-ChildItem -Path $folderPath -Filter "*.proto" -Recurse -ErrorAction SilentlyContinue

    foreach ($proto in $protoFiles) {
        $totalFiles++
        $pbcc = $proto.FullName -replace '\.proto$', '.pb.cc'
        $pbh = $proto.FullName -replace '\.proto$', '.pb.h'

        $needBuild = $false
        $reason = ""

        if (-not (Test-Path $pbcc)) {
            $needBuild = $true
            $reason = ".pb.cc not found"
        }
        elseif ($proto.LastWriteTime -gt (Get-Item $pbcc).LastWriteTime) {
            $needBuild = $true
            $reason = "newer than .pb.cc"
        }

        if ($needBuild) {
            $buildFiles++
            Write-Host "[BUILD] $($proto.Name) - $reason" -ForegroundColor Yellow

            Push-Location $proto.DirectoryName
            try {
                & $protocPath --proto_path=. --cpp_out=. $proto.Name 2>&1
                if ($LASTEXITCODE -eq 0) {
                    Write-Host "[OK] Generated $($proto.BaseName).pb.h/.cc" -ForegroundColor Green
                } else {
                    $errorFiles++
                    Write-Host "[FAIL] $($proto.Name)" -ForegroundColor Red
                }
            }
            finally {
                Pop-Location
            }
        }
        else {
            $skipFiles++
        }
    }
}

$elapsed = $stopwatch.ElapsedMilliseconds

Write-Host ""
Write-Host "===============================================" -ForegroundColor Cyan
Write-Host "            GENERATION SUMMARY" -ForegroundColor Cyan
Write-Host "===============================================" -ForegroundColor Cyan
Write-Host "Total .proto files: $totalFiles"
Write-Host "Built:              $buildFiles"
Write-Host "Skipped:            $skipFiles"
Write-Host "Failed:             $errorFiles"
Write-Host "Time:               $elapsed ms" -ForegroundColor Magenta
Write-Host "===============================================" -ForegroundColor Cyan

if ($buildFiles -eq 0 -and $errorFiles -eq 0) {
    Write-Host "[INFO] All files are up-to-date!" -ForegroundColor Green
}

Write-Host ""
