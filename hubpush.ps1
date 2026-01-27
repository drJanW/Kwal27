<#
.SYNOPSIS
    Commit en push naar GitHub
.DESCRIPTION
    Commit alle wijzigingen en pusht naar GitHub.
.EXAMPLE
    .\hubpush.ps1 "mijn wijzigingen"
    .\hubpush.ps1 bug gefixt
#>

if ($args.Count -eq 0) {
    Write-Host "FOUT: Geef een beschrijving op!"
    Write-Host "Voorbeeld: .\hubpush.ps1 'wat je hebt gedaan'"
    exit 1
}

$beschrijving = $args -join " "
$branch = git rev-parse --abbrev-ref HEAD
if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ FOUT: Geen git repository gevonden."
    exit 1
}

Write-Host "========================================"
Write-Host "COMMIT & PUSH NAAR GITHUB"
Write-Host "Branch: $branch"
Write-Host "Remote: origin (github.com)"
Write-Host "Beschrijving: $beschrijving"
Write-Host "========================================"

# Excel controle
Write-Host "`n[1/4] Controleren op open Excel bestanden..."
$excelLocks = Get-ChildItem -Path . -Recurse -Filter "~`$*" -ErrorAction SilentlyContinue | 
    Where-Object { @('.xlsx','.xls','.xlsm','.xlsb') -contains $_.Extension.ToLower() }
if ($excelLocks) {
    Write-Host "  ✗ FOUT: Excel bestanden zijn open!"
    $excelLocks | ForEach-Object { Write-Host "    - $($_.Name -replace '~\$', '')" }
    exit 1
}
Write-Host "  ✓ OK"

# Wijzigingen checken
Write-Host "`n[2/4] Controleren op wijzigingen..."
$wijzigingen = git status --porcelain
$unpushed = git rev-list --count "@{u}..HEAD" 2>$null
if (-not $wijzigingen) {
    if ($unpushed -gt 0) {
        Write-Host "  ⓘ Geen nieuwe wijzigingen, maar $unpushed commit(s) nog niet gepusht."
        Write-Host "`n[4/4] Pushen naar GitHub..."
        git push origin $branch
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  ✗ FOUT: Push naar GitHub mislukt."
            exit 1
        }
        Write-Host "`n✓ KLAAR - Gepusht naar GitHub"
        exit 0
    }
    Write-Host "  ⓘ Geen wijzigingen om te committen of pushen."
    exit 0
}
Write-Host "  ✓ Wijzigingen gevonden"

# Commit
Write-Host "`n[3/4] Committen..."
git add -A
git commit -m "$beschrijving"
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ FOUT: Commit mislukt."
    exit 1
}
Write-Host "  ✓ Commit aangemaakt"

# Push
Write-Host "`n[4/4] Pushen naar GitHub..."
git push origin $branch
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ FOUT: Push naar GitHub mislukt."
    exit 1
}

Write-Host "`n✓ KLAAR - Gepusht naar GitHub"
