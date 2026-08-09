# Tear down leftover Windows→WSL MQTT portproxy (no podman required).
# Run elevated: Right-click → Run with PowerShell (Admin)
#   powershell -ExecutionPolicy Bypass -File .\remove-wsl-mqtt-forward.ps1

$ErrorActionPreference = "Continue"
$ports = @(1883, 5672, 15672, 15675, 1880)

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
$p = New-Object Security.Principal.WindowsPrincipal($id)
if (-not $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "ERROR: need Admin PowerShell." -ForegroundColor Red
    exit 1
}

Write-Host "Deleting portproxy on $($ports -join ', ')..."
foreach ($listen in $ports) {
    netsh interface portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=$listen | Out-Null
    netsh interface portproxy delete v4tov4 listenaddress=127.0.0.1 listenport=$listen | Out-Null
}

Get-NetFirewallRule -DisplayName "Crearts mqtt*" -ErrorAction SilentlyContinue |
    Remove-NetFirewallRule -ErrorAction SilentlyContinue
try {
    Get-NetFirewallHyperVRule -ErrorAction SilentlyContinue |
        Where-Object { $_.DisplayName -like "Crearts MQTT*" } |
        ForEach-Object { Remove-NetFirewallHyperVRule -Name $_.Name -ErrorAction SilentlyContinue }
} catch {}

Write-Host ""
Write-Host "portproxy:"
netsh interface portproxy show all
Write-Host "listeners :1883:"
netstat -ano | findstr "LISTENING" | findstr ":1883"
Write-Host "Done. Restart iot-platform-go so it can bind 0.0.0.0:1883."
