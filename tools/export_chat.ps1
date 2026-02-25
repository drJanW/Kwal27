param(
    [string]$JsonPath,
    [string]$OutPath = "docs\chat.txt"
)

$json = Get-Content $JsonPath -Raw | ConvertFrom-Json
$out = @()

foreach($r in $json.requests) {
    $userMsg = $r.message.text
    $out += "=== USER ==="
    $out += $userMsg
    $out += ""
    
    $copilotMsg = ""
    foreach($resp in $r.response) {
        if ($resp.value -and $resp.value.Length -gt 10 -and $resp.kind -ne "thinking") {
            $copilotMsg += $resp.value
        }
    }
    if ($copilotMsg) {
        $out += "=== COPILOT ==="
        $out += $copilotMsg
        $out += ""
    }
    $out += "---"
    $out += ""
}

$out | Out-File $OutPath -Encoding UTF8
Write-Host "Done - $($json.requests.Count) messages exported to $OutPath"
