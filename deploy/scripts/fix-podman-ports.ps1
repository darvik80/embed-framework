# OBSOLETE for iot-platform-go (MQTT is now embedded in the Go process).
# Do NOT run this while the platform listens on :1883 — portproxy steals
# LAN CONNECT (ESP sees transport EOF) and shares the port with svchost.
#
# Only for the old Podman RabbitMQ stack. To tear leftovers down:
#
#   cd deploy
#   .\scripts\fix-podman-ports.ps1 -Remove
#
# Historical: open MQTT :1883 from LAN to RabbitMQ in Podman (Windows/WSL).
# MUST run in elevated (Admin) PowerShell — firewall / portproxy need it.
#
#   .\scripts\fix-podman-ports.ps1
#
# Mirrored WSL (LAN IP on eth4/eth2 = Windows Wi-Fi): publish often already
# listens on 192.168.x.x inside the VM; LAN is blocked by Hyper-V firewall.
# Classic NAT (172.x eth0): needs relay + netsh portproxy.

param(
    [switch]$Remove
)

$ErrorActionPreference = "Stop"
$Machine = "podman-machine-default"
$MqttPort = 1883
$WslVmCreatorId = "{40E0AC32-4F6C-4C2B-AC98-A41B9B923FBD}"
$LegacyPorts = @(5672, 15672, 15675, 1880)

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-RabbitIp {
    $ip = (podman inspect cogitor-rabbitmq --format "{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}").Trim()
    if (-not $ip) { throw "cogitor-rabbitmq not running / no IP" }
    return $ip
}

function Get-WslListenEndpoint {
    $raw = podman machine ssh -- "ip -4 -o addr show up"
    if (-not $raw) { throw "Cannot read UP addresses from podman machine" }

    $candidates = @()
    foreach ($line in ($raw -split "`n")) {
        # e.g. "7: eth4    inet 192.168.1.22/24 brd ..."
        if ($line -notmatch '^\d+:\s+(eth\d+)\s+inet\s+(\d+\.\d+\.\d+\.\d+)/\d+') { continue }
        $candidates += [pscustomobject]@{ Iface = $Matches[1]; Ip = $Matches[2] }
    }
    if (-not $candidates) {
        $raw2 = podman machine ssh -- "ip -4 -o addr show"
        foreach ($line in ($raw2 -split "`n")) {
            if ($line -notmatch '^\d+:\s+(eth\d+)\s+inet\s+(\d+\.\d+\.\d+\.\d+)/\d+') { continue }
            $candidates += [pscustomobject]@{ Iface = $Matches[1]; Ip = $Matches[2] }
        }
    }
    if (-not $candidates) { throw "No eth* IPv4 on podman machine" }

    $lan = $candidates | Where-Object { $_.Ip -notlike '172.*' } | Select-Object -First 1
    if ($lan) { return $lan }
    return $candidates[0]
}

function Test-MirroredLan {
    param([string]$WslIp)
    $hostIps = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Where-Object { $_.IPAddress -notlike '127.*' } |
        Select-Object -ExpandProperty IPAddress
    return [bool]($hostIps -contains $WslIp)
}

function Test-Tcp {
    param([string]$HostName, [int]$Port, [int]$TimeoutMs = 1500)
    try {
        $c = New-Object System.Net.Sockets.TcpClient
        $ok = $c.ConnectAsync($HostName, $Port).Wait($TimeoutMs)
        $r = $ok -and $c.Connected
        $c.Close()
        return $r
    } catch { return $false }
}

function Open-MqttFirewall {
    param([int]$Port)

    Get-NetFirewallRule -DisplayName "Cogitor mqtt $Port" -ErrorAction SilentlyContinue |
        Remove-NetFirewallRule -ErrorAction SilentlyContinue
    New-NetFirewallRule -DisplayName "Cogitor mqtt $Port" -Direction Inbound `
        -Protocol TCP -LocalPort $Port -Action Allow -Profile Any | Out-Null
    Write-Host "Windows firewall: allow TCP $Port"

    # Mirrored WSL: LAN hits Hyper-V firewall, not only the classic Windows one.
    try {
        Get-NetFirewallHyperVRule -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -like "Cogitor MQTT*" } |
            ForEach-Object { Remove-NetFirewallHyperVRule -Name $_.Name -ErrorAction SilentlyContinue }
        New-NetFirewallHyperVRule -Name "Cogitor-MQTT-$Port" -DisplayName "Cogitor MQTT $Port" `
            -Direction Inbound -VMCreatorId $WslVmCreatorId -Protocol TCP `
            -LocalPorts $Port -Action Allow | Out-Null
        Write-Host "Hyper-V firewall: allow TCP $Port (WSL mirrored)"
    } catch {
        Write-Host "WARN: Hyper-V firewall rule failed: $_"
    }
}

function Remove-MqttExposure {
    Write-Host "Removing relay / portproxy / firewall leftovers..."
    if (Get-Command podman -ErrorAction SilentlyContinue) {
        try {
            podman machine ssh -- "pkill -f cogitor-port-relay || true" 2>$null | Out-Null
        } catch {}
    }
    foreach ($listen in (@($MqttPort) + $LegacyPorts | Select-Object -Unique)) {
        netsh interface portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=$listen 2>$null | Out-Null
        netsh interface portproxy delete v4tov4 listenaddress=127.0.0.1 listenport=$listen 2>$null | Out-Null
    }
    Get-NetFirewallRule -DisplayName "Cogitor mqtt*" -ErrorAction SilentlyContinue |
        Remove-NetFirewallRule -ErrorAction SilentlyContinue
    try {
        Get-NetFirewallHyperVRule -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -like "Cogitor MQTT*" } |
            ForEach-Object { Remove-NetFirewallHyperVRule -Name $_.Name -ErrorAction SilentlyContinue }
    } catch {}
    Write-Host "Done."
    Write-Host "portproxy now:"
    netsh interface portproxy show all
}

function Start-NatRelay {
    param([string]$RabbitIp, [string]$WslIp, [int]$Port)

    $relayPy = @'
import socket, threading, sys
listen_ip, listen_port, dest_ip, dest_port = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4])
def pipe(a, b):
    try:
        while True:
            data = a.recv(65536)
            if not data: break
            b.sendall(data)
    except Exception:
        pass
    finally:
        try: a.close()
        except Exception: pass
        try: b.close()
        except Exception: pass
srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind((listen_ip, listen_port))
srv.listen(128)
print(f"cogitor-port-relay {listen_ip}:{listen_port} -> {dest_ip}:{dest_port}", flush=True)
while True:
    c, _ = srv.accept()
    u = socket.create_connection((dest_ip, dest_port))
    threading.Thread(target=pipe, args=(c, u), daemon=True).start()
    threading.Thread(target=pipe, args=(u, c), daemon=True).start()
'@

    $b64 = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($relayPy))
    podman machine ssh -- "pkill -f cogitor-port-relay || true; echo $b64 | base64 -d > /tmp/cogitor-port-relay.py; nohup /usr/sbin/python3 /tmp/cogitor-port-relay.py 0.0.0.0 $Port $RabbitIp $Port >/tmp/cogitor-relay-$Port.log 2>&1 & sleep 1; cat /tmp/cogitor-relay-$Port.log"

    netsh interface portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=$Port 2>$null | Out-Null
    netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=$Port connectaddress=$WslIp connectport=$Port | Out-Null
    Write-Host "portproxy 0.0.0.0:$Port -> ${WslIp}:$Port"
}

if (-not (Test-IsAdmin)) {
    Write-Host "ERROR: run this script in elevated Admin PowerShell." -ForegroundColor Red
    Write-Host "  Right-click PowerShell -> Run as administrator"
    Write-Host "  cd D:\Projects\cpp\embed-framework\deploy"
    Write-Host "  .\scripts\fix-podman-ports.ps1"
    exit 1
}

if ($Remove) {
    Remove-MqttExposure
    exit 0
}

$rabbitIp = Get-RabbitIp
$wsl = Get-WslListenEndpoint
$mirrored = Test-MirroredLan -WslIp $wsl.Ip

Write-Host "RabbitMQ container IP: $rabbitIp"
Write-Host ("WSL {0} IP:           {1}" -f $wsl.Iface, $wsl.Ip)
Write-Host ("Mirrored LAN:         {0}" -f $(if ($mirrored) { "yes" } else { "no" }))

# Clean leftover portproxy from older script versions
foreach ($listen in $LegacyPorts) {
    netsh interface portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=$listen 2>$null | Out-Null
    netsh interface portproxy delete v4tov4 listenaddress=127.0.0.1 listenport=$listen 2>$null | Out-Null
}

Open-MqttFirewall -Port $MqttPort

if ($mirrored) {
    # Publish already owns :1883 inside the VM. Windows->own LAN IP is hairpin-broken;
    # expose LAN by proxying host 0.0.0.0:1883 -> 127.0.0.1:1883 (localhost forward works).
    podman machine ssh -- "pkill -f cogitor-port-relay || true" 2>$null | Out-Null
    netsh interface portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=$MqttPort 2>$null | Out-Null
    netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=$MqttPort `
        connectaddress=127.0.0.1 connectport=$MqttPort | Out-Null
    Write-Host "portproxy 0.0.0.0:$MqttPort -> 127.0.0.1:$MqttPort (mirrored LAN via localhost)"
} else {
    Start-NatRelay -RabbitIp $rabbitIp -WslIp $wsl.Ip -Port $MqttPort
}

Start-Sleep -Seconds 1
Write-Host ""
Write-Host "Checks:"
foreach ($h in @("127.0.0.1", $wsl.Ip)) {
    $ok = Test-Tcp -HostName $h -Port $MqttPort
    Write-Host ("  {0}:{1} {2}" -f $h, $MqttPort, $(if ($ok) { "OK" } else { "FAIL" }))
}

Write-Host ""
Write-Host "ESP broker host = $($wsl.Ip) (this PC Wi-Fi/Ethernet IPv4)."
if (-not (Test-Tcp -HostName $wsl.Ip -Port $MqttPort)) {
    Write-Host "WARN: $($wsl.Ip):$MqttPort still FAIL from this PC — check portproxy above; re-run as Admin." -ForegroundColor Yellow
}
Write-Host "Remove with: .\scripts\fix-podman-ports.ps1 -Remove"
