# 1. Update Binary in OTA Repo
$sourceBin = "Main\build\esp32.esp32.esp32s3\Main.ino.bin"
$destBin = "..\GatewaySuriotaOTA\releases\v2.5.19\firmware.bin"

Write-Host "Updating binary in OTA Repo..."
Copy-Item $sourceBin -Destination $destBin -Force

# 2. Create Updated Manifest (v2.5.20 pointing to v2.5.19 binary)
$manifestContent = @{
    version = "2.5.20"
    build_number = 2520
    release_date = "2025-12-01"
    min_version = "2.3.0"
    firmware = @{
        url = "https://api.github.com/repos/GifariKemal/GatewaySuriotaOTA/contents/releases/v2.5.19/firmware.bin?ref=main"
        filename = "firmware.bin"
        size = 2010064
        sha256 = "53affee120d2fda5feb03058d3f3dd27e4ced42fb5c4ef71b53325b05a35b248"
        signature = "304502202ee91856492441344cb0defe477fd72b98b811748c62e54de603bcd905bba6ec022100d79a8642e70c03f24fa081169439bd91466c5e6fdf8ed3e4c8ef23944d23514b"
    }
    changelog = @("v2.5.20: Critical Fixes & MQTT Optimization (Testing Release)")
    mandatory = $false
}

$manifestJson = ConvertTo-Json $manifestContent -Depth 10
$manifestPath = "..\GatewaySuriotaOTA\firmware_manifest.json"

Write-Host "Updating manifest in OTA Repo..."
Set-Content -Path $manifestPath -Value $manifestJson

# 3. Commit & Push
Set-Location "..\GatewaySuriotaOTA"
Write-Host "Git Status:"
git status
Write-Host "Git Add..."
git add .
Write-Host "Git Commit..."
git commit -m "fix: Update binary and manifest to match WDT-fixed build"
Write-Host "Git Push..."
git push origin main
Write-Host "Done!"
