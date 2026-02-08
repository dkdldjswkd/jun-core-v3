@echo off
:: Protobuf Generator Launcher - calls PowerShell script
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0generate_protobuf.ps1"
