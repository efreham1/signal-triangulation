param(
    [int]$Port = 8080
)

Write-Host "Removing portproxy and firewall rule for TCP port $Port..."

# Remove portproxy rule
netsh interface portproxy delete v4tov4 listenport=$Port listenaddress=0.0.0.0 2>$null

# Remove firewall rule
Remove-NetFirewallRule -DisplayName "Polaris TCP $Port" 2>$null

Write-Host "Cleanup complete."