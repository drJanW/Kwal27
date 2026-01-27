<#
.SYNOPSIS
    Commit en push naar NAS
.DESCRIPTION
    Commit alle wijzigingen en pusht naar de NAS git repository.
.EXAMPLE
    .\naspush.ps1 "mijn wijzigingen"
    .\naspush.ps1 bug gefixt
#>

if ($args.Count -eq 0) {
    Write-Host "FOUT: Geef een beschrijving op!"
    Write-Host "Voorbeeld: .\naspush.ps1 'wat je hebt gedaan'"
    exit 1
}

$beschrijving = $args -join " "
$branch = git rev-parse --abbrev-ref HEAD
if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ FOUT: Geen git repository gevonden."
    exit 1
}

Write-Host "========================================"
Write-Host "COMMIT & PUSH NAAR NAS"
Write-Host "Branch: $branch"
Write-Host "Remote: nas (T:\git\Kwal26.git)"
Write-Host "Beschrijving: $beschrijving"
Write-Host "========================================"

# Excel controle
Write-Host "`n[1/5] Controleren op open Excel bestanden..."
$excelLocks = Get-ChildItem -Path . -Recurse -Filter "~`$*" -ErrorAction SilentlyContinue | 
    Where-Object { @('.xlsx','.xls','.xlsm','.xlsb') -contains $_.Extension.ToLower() }
if ($excelLocks) {
    Write-Host "  ✗ FOUT: Excel bestanden zijn open!"
    $excelLocks | ForEach-Object { Write-Host "    - $($_.Name -replace '~\$', '')" }
    exit 1
}
Write-Host "  ✓ OK"

# Wijzigingen checken
Write-Host "`n[2/5] Controleren op wijzigingen..."
$wijzigingen = git status --porcelain
$skipCommit = $false
if (-not $wijzigingen) {
    Write-Host "  ⓘ Geen uncommitted wijzigingen."
    # Check of local ahead is van nas
    git fetch nas 2>$null
    $ahead = git rev-list --count nas/$branch..HEAD 2>$null
    if ($ahead -gt 0) {
        Write-Host "  ✓ Lokaal $ahead commit(s) voor op NAS - doorgaan met push"
        $skipCommit = $true
    } else {
        Write-Host "  ⓘ Lokaal gelijk met NAS - niets te doen."
        exit 0
    }
} else {
    Write-Host "  ✓ Wijzigingen gevonden"
}

# Commit
if (-not $skipCommit) {
    Write-Host "`n[3/5] Committen..."
    git add -A
    git commit -m "$beschrijving"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ✗ FOUT: Commit mislukt."
        exit 1
    }
    Write-Host "  ✓ Commit aangemaakt"
} else {
    Write-Host "`n[3/5] Committen... (overgeslagen)"
}

# Pull --rebase (voor laptop/desktop sync)
Write-Host "`n[4/5] Controleren of NAS nieuwere versie heeft..."
git pull --rebase nas $branch 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ⚠️  BOTSING MET NAS!"
    Write-Host "  Iemand anders heeft ook gepusht."
    Write-Host "`n  OPLOSSEN:"
    Write-Host "  1. git status"
    Write-Host "  2. Los conflicten op (zoek '<<<<<<<')"
    Write-Host "  3. git add ."
    Write-Host "  4. git rebase --continue"
    Write-Host "  Of annuleer: git rebase --abort"
    exit 1
}
Write-Host "  ✓ Up-to-date met NAS"

# Push
Write-Host "`n[5/5] Pushen naar NAS..."
git push nas $branch
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ FOUT: Push naar NAS mislukt."
    exit 1
}

Write-Host "`n✓ KLAAR - Gepusht naar NAS"
