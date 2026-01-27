# Download CSV files from ESP32 SD card
# Usage: .\download_csv.ps1 [-ip "192.168.2.188"] [-dest ".\downloads"]

param(
    [string]$ip = "192.168.2.188",
    [string]$dest = ".\sd_downloads"
)

$baseUrl = "http://$ip"

# Create destination folder
if (!(Test-Path $dest)) {
    New-Item -ItemType Directory -Path $dest | Out-Null
    Write-Host "Created folder: $dest"
}

Write-Host "Fetching file list from $baseUrl/api/sd/list?path=/"

try {
    # Get root directory listing
    $response = Invoke-RestMethod -Uri "$baseUrl/api/sd/list?path=/" -TimeoutSec 30
    
    # Filter for .csv files
    $csvFiles = $response | Where-Object { $_.name -like "*.csv" }
    
    if ($csvFiles.Count -eq 0) {
        Write-Host "No CSV files found in root directory"
        exit
    }
    
    Write-Host "Found $($csvFiles.Count) CSV file(s):"
    $csvFiles | ForEach-Object { Write-Host "  - $($_.name)" }
    Write-Host ""
    
    # Download each file
    foreach ($file in $csvFiles) {
        $fileName = $file.name
        $downloadUrl = "$baseUrl/api/sd/download?path=/$fileName"
        $localPath = Join-Path $dest $fileName
        
        Write-Host "Downloading $fileName ... " -NoNewline
        try {
            Invoke-WebRequest -Uri $downloadUrl -OutFile $localPath -TimeoutSec 60
            $size = (Get-Item $localPath).Length
            Write-Host "OK ($size bytes)"
        }
        catch {
            Write-Host "FAILED: $($_.Exception.Message)" -ForegroundColor Red
        }
    }
    
    Write-Host "`nDone! Files saved to: $dest"
}
catch {
    Write-Host "Error fetching file list: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
    Write-Host "Fallback: Trying known CSV files directly..."
    
    # Known CSV files on the SD card
    $knownFiles = @(
        "audioShifts.csv",
        "calendar.csv",
        "colorsShifts.csv",
        "light_colors.csv",
        "light_patterns.csv",
        "patternShifts.csv",
        "theme_boxes.csv"
    )
    
    foreach ($fileName in $knownFiles) {
        $downloadUrl = "$baseUrl/api/sd/download?path=/$fileName"
        $localPath = Join-Path $dest $fileName
        
        Write-Host "Downloading $fileName ... " -NoNewline
        try {
            Invoke-WebRequest -Uri $downloadUrl -OutFile $localPath -TimeoutSec 60
            $size = (Get-Item $localPath).Length
            Write-Host "OK ($size bytes)"
        }
        catch {
            Write-Host "FAILED" -ForegroundColor Yellow
        }
    }
}
