# search_chats.ps1
# Search all VS Code Copilot chat sessions for this workspace
# Usage: .\search_chats.ps1 "handleiding"
#        .\search_chats.ps1 "handleiding" -Context 200
#        .\search_chats.ps1 "sensor" -MaxResults 5
#        .\search_chats.ps1 "brightness" -UserOnly   (only search user messages)

param(
    [Parameter(Position=0)]
    [string]$Query = "",
    
    [int]$Context = 120,          # chars of context around each match
    [int]$MaxResults = 30,        # max matches to show
    [switch]$UserOnly,            # only search user message texts
    [switch]$ListSessions         # just list sessions, don't search
)

if (-not $ListSessions -and $Query -eq "") {
    Write-Host "Usage: .\search_chats.ps1 `"search term`"" -ForegroundColor Yellow
    Write-Host "       .\search_chats.ps1 -ListSessions" -ForegroundColor Yellow
    Write-Host "       .\search_chats.ps1 `"term`" -Context 200 -MaxResults 10" -ForegroundColor Yellow
    Write-Host "       .\search_chats.ps1 `"term`" -UserOnly" -ForegroundColor Yellow
    exit 0
}

$wsId = "bd14078c2904d4d8b225738a351debb9"  # Kwal27 workspace
$chatDir = "$env:APPDATA\Code\User\workspaceStorage\$wsId\chatSessions"

if (-not (Test-Path $chatDir)) {
    Write-Host "Chat directory not found: $chatDir" -ForegroundColor Red
    exit 1
}

# Gather all chat files
$jsonFiles  = Get-ChildItem $chatDir -Filter "*.json"  | Sort-Object Length -Descending
$jsonlFiles = Get-ChildItem $chatDir -Filter "*.jsonl" | Sort-Object Length -Descending

# Build session index: map sessionId -> title + date
$sessions = @{}
foreach ($jlf in $jsonlFiles) {
    $sid = $jlf.BaseName
    $title = ""
    $date = $jlf.LastWriteTime
    # Read first few lines for title and creation date (kind=0 header, kind=1 customTitle)
    $reader = [System.IO.StreamReader]::new($jlf.FullName)
    $lineNum = 0
    while ($lineNum -lt 10 -and -not $reader.EndOfStream) {
        $line = $reader.ReadLine()
        $lineNum++
        if ($line -match '"customTitle"\],"v":"([^"]+)"') {
            $title = $matches[1]
        }
        if ($line -match '"creationDate":(\d+)') {
            $epoch = [long]$matches[1]
            $date = [DateTimeOffset]::FromUnixTimeMilliseconds($epoch).LocalDateTime
        }
    }
    $reader.Close()
    $sessions[$sid] = @{ Title = $title; Date = $date; SizeKB = [math]::Round($jlf.Length/1024); File = $jlf.FullName; Format = "jsonl" }
}

foreach ($jf in $jsonFiles) {
    $sid = $jf.BaseName
    if (-not $sessions.ContainsKey($sid)) {
        $sessions[$sid] = @{ Title = ""; Date = $jf.LastWriteTime; SizeKB = [math]::Round($jf.Length/1024); File = $jf.FullName; Format = "json" }
    }
    # Try to get title from JSON (first few KB)
    if ($sessions[$sid].Title -eq "") {
        $reader = [System.IO.StreamReader]::new($jf.FullName)
        $buf = New-Object char[] 2000
        $reader.Read($buf, 0, 2000) | Out-Null
        $header = [string]::new($buf)
        $reader.Close()
        if ($header -match '"customTitle"\s*:\s*"([^"]+)"') {
            $sessions[$sid].Title = $matches[1]
        }
    }
}

# --- List mode ---
if ($ListSessions) {
    Write-Host "`nCopilot Chat Sessions for Kwal27" -ForegroundColor Cyan
    Write-Host ("-" * 80)
    $sorted = $sessions.GetEnumerator() | Sort-Object { $_.Value.Date } -Descending
    $i = 0
    foreach ($s in $sorted) {
        $i++
        $d = $s.Value.Date.ToString("yyyy-MM-dd HH:mm")
        $sz = "$($s.Value.SizeKB) KB".PadLeft(10)
        $t = if ($s.Value.Title) { $s.Value.Title } else { "(untitled)" }
        $fmt = $s.Value.Format.PadLeft(5)
        Write-Host ("{0,3}. {1}  {2}  {3}  {4}" -f $i, $d, $sz, $fmt, $t)
    }
    Write-Host "`nTotal: $i sessions"
    exit 0
}

# --- Search mode ---
Write-Host "`nSearching Kwal27 chats for: " -NoNewline -ForegroundColor Cyan
Write-Host "`"$Query`"" -ForegroundColor Yellow
Write-Host ("-" * 80)

$totalMatches = 0
$matchedSessions = @()

# Search all files (.jsonl first since they're newer, then .json)
$allFiles = @()
foreach ($jlf in $jsonlFiles) { $allFiles += $jlf }
foreach ($jf in $jsonFiles) { 
    # Skip .json if we already have .jsonl for same session (jsonl is newer/updated)
    if (-not ($jsonlFiles | Where-Object { $_.BaseName -eq $jf.BaseName })) {
        $allFiles += $jf
    }
}

foreach ($chatFile in $allFiles) {
    if ($totalMatches -ge $MaxResults) { break }
    
    $sid = $chatFile.BaseName
    $info = $sessions[$sid]
    $sessionLabel = if ($info.Title) { $info.Title } else { $sid.Substring(0, 8) + "..." }
    $sessionDate  = $info.Date.ToString("yyyy-MM-dd")
    
    # Fast text search using .NET StreamReader for large files
    $reader = [System.IO.StreamReader]::new($chatFile.FullName)
    $content = $reader.ReadToEnd()
    $reader.Close()
    
    # For UserOnly mode, extract user messages and search those
    if ($UserOnly) {
        $pattern = '"text"\s*:\s*"([^"]{0,2000})"'
        $msgMatches = [regex]::Matches($content, $pattern)
        foreach ($m in $msgMatches) {
            if ($totalMatches -ge $MaxResults) { break }
            $msgText = $m.Groups[1].Value
            if ($msgText -match [regex]::Escape($Query)) {
                $totalMatches++
                if ($sid -notin $matchedSessions) { 
                    $matchedSessions += $sid
                    Write-Host "`n[$sessionDate] $sessionLabel ($($info.SizeKB) KB)" -ForegroundColor Green
                }
                # Show context
                $idx = $msgText.IndexOf($Query, [StringComparison]::OrdinalIgnoreCase)
                $start = [Math]::Max(0, $idx - $Context)
                $end = [Math]::Min($msgText.Length, $idx + $Query.Length + $Context)
                $snippet = $msgText.Substring($start, $end - $start)
                Write-Host "  USER: ...$snippet..." -ForegroundColor White
            }
        }
    } else {
        # General search: find query anywhere in file content
        $idx = 0
        while ($idx -lt $content.Length -and $totalMatches -lt $MaxResults) {
            $idx = $content.IndexOf($Query, $idx, [StringComparison]::OrdinalIgnoreCase)
            if ($idx -lt 0) { break }
            
            if ($sid -notin $matchedSessions) { 
                $matchedSessions += $sid
                Write-Host "`n[$sessionDate] $sessionLabel ($($info.SizeKB) KB)" -ForegroundColor Green
            }
            
            # Extract context around match
            $start = [Math]::Max(0, $idx - $Context)
            $end   = [Math]::Min($content.Length, $idx + $Query.Length + $Context)
            $snippet = $content.Substring($start, $end - $start)
            
            # Clean up JSON escaping for readability
            $snippet = $snippet -replace '\\n', "`n" -replace '\\t', "  " -replace '\\"', '"' -replace '\\\\', '\'
            # Collapse to single line for display
            $snippet = $snippet -replace "`n", " " -replace "`r", ""
            # Truncate if still too long
            if ($snippet.Length -gt ($Context * 2 + $Query.Length + 20)) {
                $snippet = $snippet.Substring(0, $Context * 2 + $Query.Length + 20)
            }
            
            $totalMatches++
            
            # Color the match within the snippet
            $matchIdx = $snippet.IndexOf($Query, [StringComparison]::OrdinalIgnoreCase)
            if ($matchIdx -ge 0) {
                $before = $snippet.Substring(0, $matchIdx)
                $match  = $snippet.Substring($matchIdx, $Query.Length)
                $after  = $snippet.Substring($matchIdx + $Query.Length)
                Write-Host "  ..." -NoNewline -ForegroundColor DarkGray
                Write-Host $before -NoNewline -ForegroundColor Gray
                Write-Host $match -NoNewline -ForegroundColor Yellow -BackgroundColor DarkBlue
                Write-Host $after -NoNewline -ForegroundColor Gray
                Write-Host "..." -ForegroundColor DarkGray
            } else {
                Write-Host "  ...$snippet..." -ForegroundColor Gray
            }
            
            # Skip ahead to avoid duplicate hits in same region
            $idx += $Query.Length + 200
        }
    }
}

Write-Host "`n" ("-" * 80)
Write-Host "Found $totalMatches matches in $($matchedSessions.Count) sessions" -ForegroundColor Cyan
