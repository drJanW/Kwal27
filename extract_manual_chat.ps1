# extract_manual_chat.ps1 — one-time use: find the manual/handleiding discussion
$chatDir = "$env:APPDATA\Code\User\workspaceStorage\bd14078c2904d4d8b225738a351debb9\chatSessions"
$outFile = Join-Path $PSScriptRoot "docs\_manual_chat_extract.txt"

$sessionFile = Join-Path $chatDir "5e5e40c2-d4db-4a47-9fcb-67659588e801.json"
$text = [System.IO.File]::ReadAllText($sessionFile)

Write-Host "Loaded $([math]::Round($text.Length/1024/1024, 1)) MB from session 5e5e40c2" -ForegroundColor Cyan

# Find "Planning full manual rewrite" and extract a big chunk
$patterns = @(
    "Planning full manual rewrite",
    "Created documentation draft",
    "manual rewrite",
    "KWAL27 GEBRUIKERSHANDLEIDING",
    "Kwal27 Manual",
    "Kwal27 User",
    "WEB INTERFACE",
    "webgui handleiding"
)

$output = @()
foreach ($p in $patterns) {
    $idx = 0
    while ($true) {
        $idx = $text.IndexOf($p, $idx, [StringComparison]::OrdinalIgnoreCase)
        if ($idx -lt 0) { break }
        $start = [Math]::Max(0, $idx - 100)
        $end = [Math]::Min($text.Length, $idx + 5000)
        $chunk = $text.Substring($start, $end - $start)
        $chunk = $chunk -replace '\\n', "`n" -replace '\\r', '' -replace '\\"', '"' -replace '\\t', '  '
        $output += "`n`n====== MATCH: '$p' at position $idx ======"
        $output += $chunk
        Write-Host "Found '$p' at $idx" -ForegroundColor Green
        $idx = $end
    }
}

# Also search for any create_file with handleiding content
$cfIdx = 0
while ($true) {
    $cfIdx = $text.IndexOf("gebruikers_handleiding", $cfIdx, [StringComparison]::OrdinalIgnoreCase)
    if ($cfIdx -lt 0) { break }
    # Check if this is a create_file or write context
    $start = [Math]::Max(0, $cfIdx - 200)
    $end = [Math]::Min($text.Length, $cfIdx + 8000)
    $chunk = $text.Substring($start, $end - $start) -replace '\\n', "`n" -replace '\\r', '' -replace '\\"', '"'
    if ($chunk -match 'create_file|content|filePath') {
        $output += "`n`n====== FILE CREATE: gebruikers_handleiding at $cfIdx ======"
        $output += $chunk
        Write-Host "Found file create at $cfIdx" -ForegroundColor Yellow
    }
    $cfIdx += 100
}

$output -join "`n" | Set-Content $outFile -Encoding UTF8
Write-Host "`nWrote $($output.Count) matches to $outFile" -ForegroundColor Cyan
