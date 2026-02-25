# Start CSV server op NAS (na reboot)
$plink = "$PSScriptRoot\tools\plink.exe"
& $plink -batch -ssh -hostkey "SHA256:b8IIj7CCSoIaSfrpBcu7kXE8RU5V7HPoMpxUnrLwUqA" sshd@192.168.2.23 -pw "Ssh_3732" "nohup python3 /shares/Public/Kwal/csv/csv_server.py > /tmp/csv.log 2>&1 &"
Write-Host "Done" -ForegroundColor Green
