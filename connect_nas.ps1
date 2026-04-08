<#
.SYNOPSIS
    Start csv_server.py on the NAS via SSH.
.DESCRIPTION
    Checks if csv_server.py is already running on the NAS (192.168.2.23:8081).
    If not, starts it via SSH. User/pw: sshd/sshd (WD My Cloud default).
#>
$nasIp   = "192.168.2.23"
$port    = 8081
$plink   = "$PSScriptRoot\tools\plink.exe"
$hostKey = "SHA256:b8IIj7CCSoIaSfrpBcu7kXE8RU5V7HPoMpxUnrLwUqA"
$script  = "/shares/Public/Kwal/csv/csv_server.py"

# 1. Already running?
Write-Host "Checking ${nasIp}:${port}..." -NoNewline
try {
    Invoke-WebRequest -Uri "http://${nasIp}:${port}/api/health" -TimeoutSec 3 -ErrorAction Stop | Out-Null
    Write-Host " already running" -ForegroundColor Green
    exit 0
} catch {}
Write-Host " not running" -ForegroundColor Yellow

# 2. Start via SSH
Write-Host "Starting via SSH..." -NoNewline
& $plink -batch -ssh -hostkey $hostKey "sshd@${nasIp}" -pw "Ssh_3732" "nohup python3 $script > /tmp/csv.log 2>&1 &" 2>$null
Start-Sleep -Seconds 2

try {
    Invoke-WebRequest -Uri "http://${nasIp}:${port}/api/health" -TimeoutSec 3 -ErrorAction Stop | Out-Null
    Write-Host " OK" -ForegroundColor Green
} catch {
    Write-Host " FAILED" -ForegroundColor Red
    Write-Host ""
    Write-Host "SSH is waarschijnlijk uitgeschakeld na een NAS-update." -ForegroundColor Yellow
    Write-Host "Fix: ga naar http://${nasIp} -> Settings -> Network -> SSH -> Enable" -ForegroundColor Yellow
    Write-Host "     Password: Ssh_3732" -ForegroundColor Yellow
    Write-Host "     en run daarna opnieuw: .\connect_nas.ps1" -ForegroundColor Yellow
    exit 1
}
