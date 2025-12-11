param(
    [int]$Port = 8080,
    [string]$WSL_IP = ""
)

if (-not $WSL_IP) {
    Write-Host "Detecting WSL IP address..."
    $WSL_IP = wsl hostname -I | Out-String
    $WSL_IP = $WSL_IP.Trim().Split(" ")[0]
}

Write-Host "Setting up port forwarding for TCP port $Port to $WSL_IP"

# Remove any existing portproxy rule
netsh interface portproxy delete v4tov4 listenport=$Port listenaddress=0.0.0.0 2>$null

# Add new portproxy rule
netsh interface portproxy add v4tov4 listenport=$Port listenaddress=0.0.0.0 connectport=$Port connectaddress=$WSL_IP

Write-Host "Adding Windows Firewall rule for TCP port $Port"

# Add firewall rule
New-NetFirewallRule -DisplayName "Polaris TCP $Port" -Direction Inbound -LocalPort $Port -Protocol TCP -Action Allow

Write-Host "Port forwarding and firewall setup complete."