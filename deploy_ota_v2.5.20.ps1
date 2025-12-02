# Deploy Firmware v2.5.20 to OTA Repository
# This script automates the deployment process

param(
    [string]$OTARepoPath = "C:\Users\Administrator\Music\GatewaySuriotaOTA"
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Deploying Firmware v2.5.20 to OTA Repo" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if OTA repo exists
if (-not (Test-Path $OTARepoPath)) {
    Write-Host "ERROR: OTA repository not found at: $OTARepoPath" -ForegroundColor Red
    Write-Host "Please clone the repository first or specify correct path with -OTARepoPath" -ForegroundColor Yellow
    exit 1
}

# Source binary path
$SourceBinary = "Main\build\esp32.esp32.esp32s3\Main.ino.bin"
$SourceManifest = "Documentation\firmware_manifest_v2.5.20.json"

# Check if source files exist
if (-not (Test-Path $SourceBinary)) {
    Write-Host "ERROR: Source binary not found: $SourceBinary" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $SourceManifest)) {
    Write-Host "ERROR: Source manifest not found: $SourceManifest" -ForegroundColor Red
    exit 1
}

# Get binary size
$BinarySize = (Get-Item $SourceBinary).Length
Write-Host "Source binary: $SourceBinary" -ForegroundColor Green
Write-Host "Binary size: $BinarySize bytes ($('{0:N2}' -f ($BinarySize / 1MB)) MB)" -ForegroundColor Green
Write-Host ""

# Verify size matches manifest
$ExpectedSize = 2010336
if ($BinarySize -ne $ExpectedSize) {
    Write-Host "WARNING: Binary size mismatch!" -ForegroundColor Yellow
    Write-Host "  Expected: $ExpectedSize bytes" -ForegroundColor Yellow
    Write-Host "  Actual:   $BinarySize bytes" -ForegroundColor Yellow
    $continue = Read-Host "Continue anyway? (y/n)"
    if ($continue -ne "y") {
        exit 1
    }
}

# Create release directory in OTA repo
$ReleaseDir = Join-Path $OTARepoPath "releases\v2.5.20"
Write-Host "Creating release directory: $ReleaseDir" -ForegroundColor Cyan
New-Item -ItemType Directory -Path $ReleaseDir -Force | Out-Null

# Copy binary
$DestBinary = Join-Path $ReleaseDir "firmware.bin"
Write-Host "Copying binary to: $DestBinary" -ForegroundColor Cyan
Copy-Item $SourceBinary $DestBinary -Force

# Verify copy
$DestSize = (Get-Item $DestBinary).Length
if ($DestSize -ne $BinarySize) {
    Write-Host "ERROR: Copy verification failed!" -ForegroundColor Red
    exit 1
}
Write-Host "Binary copied successfully" -ForegroundColor Green
Write-Host ""

# Copy manifest to OTA repo root
$DestManifest = Join-Path $OTARepoPath "firmware_manifest.json"
Write-Host "Updating manifest: $DestManifest" -ForegroundColor Cyan
Copy-Item $SourceManifest $DestManifest -Force
Write-Host "Manifest updated successfully" -ForegroundColor Green
Write-Host ""

# Git operations
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Git Operations" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

Push-Location $OTARepoPath

try {
    # Check git status
    Write-Host "Git status:" -ForegroundColor Cyan
    git status --short
    Write-Host ""

    # Add files
    Write-Host "Adding files to git..." -ForegroundColor Cyan
    git add releases/v2.5.20/firmware.bin
    git add firmware_manifest.json

    # Commit binary
    Write-Host "Committing binary..." -ForegroundColor Cyan
    git commit -m "feat: Add firmware binary v2.5.20 - OTA stability fixes"

    # Show commit
    Write-Host ""
    Write-Host "Commit created:" -ForegroundColor Green
    git log -1 --oneline
    Write-Host ""

    # Ask before push
    Write-Host "Ready to push to GitHub" -ForegroundColor Yellow
    $push = Read-Host "Push now? (y/n)"
    
    if ($push -eq "y") {
        Write-Host "Pushing to origin main..." -ForegroundColor Cyan
        git push origin main
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "Deployment Complete!" -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Green
        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Cyan
        Write-Host "1. Verify binary on GitHub: https://github.com/GifariKemal/GatewaySuriotaOTA/tree/main/releases/v2.5.20" -ForegroundColor White
        Write-Host "2. Test OTA update with BLE command: {`"op`":`"ota`",`"type`":`"check_update`"}" -ForegroundColor White
        Write-Host "3. Start OTA update: {`"op`":`"ota`",`"type`":`"start_update`"}" -ForegroundColor White
    } else {
        Write-Host "Push cancelled. You can push manually later with:" -ForegroundColor Yellow
        Write-Host "  cd $OTARepoPath" -ForegroundColor White
        Write-Host "  git push origin main" -ForegroundColor White
    }
} finally {
    Pop-Location
}
