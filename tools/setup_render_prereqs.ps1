# ThisIs395 VJ optional FFmpeg/Python setup. Run only after user clicks Install.
$ErrorActionPreference = "Stop"
Write-Host "ThisIs395 VJ: Installing render dependencies with Windows Package Manager."
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Error "winget is missing. Install App Installer from Microsoft Store, then retry."
    exit 1
}
foreach ($p in @("Gyan.FFmpeg", "Python.Python.3.12")) {
    Write-Host "Checking $p..."
    winget install --id $p --exact --accept-package-agreements --accept-source-agreements --disable-interactivity
    if ($LASTEXITCODE -ne 0) {
        Write-Error "winget could not install $p. Inspect the package-manager output."
        exit 1
    }
}
Write-Host "Installation commands completed. Restart ThisIs395 VJ to refresh PATH."
Read-Host "Press Enter to close"
