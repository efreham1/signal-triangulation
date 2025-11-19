param([switch]$Force)

# PowerShell installer for Android platform-tools (adb) on Windows.
# - Downloads platform-tools zip from Google
# - Extracts to %USERPROFILE%\platform-tools
# - Adds to User PATH (persists across sessions)

$platformTools = Join-Path $env:USERPROFILE 'platform-tools'
$zipUrl = 'https://dl.google.com/android/repository/platform-tools-latest-windows.zip'

if ((Get-Command adb -ErrorAction SilentlyContinue) -and -not $Force) {
  Write-Host "adb already available at: $(Get-Command adb).Path"
  exit 0
}

Write-Host "Downloading Android platform-tools..."
$tmp = Join-Path $env:TEMP 'platform-tools-latest-windows.zip'
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
Invoke-WebRequest -Uri $zipUrl -OutFile $tmp -UseBasicParsing

if (Test-Path $platformTools) {
  Write-Host "Removing existing $platformTools"
  Remove-Item -Recurse -Force $platformTools
}

Write-Host "Extracting to $env:USERPROFILE"
Expand-Archive -LiteralPath $tmp -DestinationPath $env:USERPROFILE -Force
Remove-Item $tmp -Force

# Add to User PATH if not already present
$userPath = [Environment]::GetEnvironmentVariable('Path','User')
if (-not ($userPath.Split(';') -contains $platformTools)) {
  [Environment]::SetEnvironmentVariable('Path', "$userPath;$platformTools", 'User')
  Write-Host "Added $platformTools to User PATH. Close and reopen shells to pick up the change."
} else {
  Write-Host "$platformTools already present in User PATH."
}

Write-Host "Installation complete. Verify with: adb version"
& "$platformTools\adb.exe" version
