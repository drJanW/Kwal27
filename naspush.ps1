<#
.SYNOPSIS
    Commit en push naar NAS
.DESCRIPTION
    Commit alle wijzigingen en pusht naar de NAS git repository.
.EXAMPLE
    .\naspush.ps1 "mijn wijzigingen"
    .\naspush.ps1 bug gefixt
    .\naspush.ps1 -Force "force push naar NAS"
    .\naspush.ps1 -NoRebase "alleen push"
    .\naspush.ps1 -SkipExcel "zonder Excel check"
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]] $Message,
    [switch] $Force,
    [switch] $NoRebase,
    [switch] $SkipExcel
)

$beschrijving = ($Message -join " ").Trim()
$hasDescription = -not [string]::IsNullOrWhiteSpace($beschrijving)
$branch = git rev-parse --abbrev-ref HEAD
if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ FOUT: Geen git repository gevonden."
    exit 1
}

$repoRoot = git rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0 -or -not $repoRoot) {
    Write-Host "✗ FOUT: Kan git root niet bepalen."
    exit 1
}
$repoName = Split-Path -Leaf $repoRoot

$remoteName = "nas"
$remotePath = "T:\git\$repoName.git"

# Ensure NAS repo exists (auto-init)
if (-not (Test-Path $remotePath)) {
    $remoteRoot = Split-Path -Parent $remotePath
    if (-not (Test-Path $remoteRoot)) {
        Write-Host "✗ FOUT: NAS map bestaat niet: $remoteRoot"
        exit 1
    }
    Write-Host "ⓘ NAS repo bestaat niet, initialiseren: $remotePath"
    git init --bare $remotePath | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "✗ FOUT: Kon NAS repo niet initialiseren."
        exit 1
    }
}

# Ensure remote exists and points to the right path
$currentRemote = git remote get-url $remoteName 2>$null
if ($LASTEXITCODE -ne 0 -or -not $currentRemote) {
    git remote add $remoteName $remotePath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "✗ FOUT: Kon remote '$remoteName' niet toevoegen."
        exit 1
    }
} elseif ($currentRemote -ne $remotePath) {
    git remote set-url $remoteName $remotePath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "✗ FOUT: Kon remote '$remoteName' niet bijwerken."
        exit 1
    }
}

# Ensure Git trusts NAS repo path (avoids dubious ownership error)
git config --global --add safe.directory "$remotePath" 2>$null | Out-Null

Write-Host "========================================"
Write-Host "COMMIT & PUSH NAAR NAS"
Write-Host "Branch: $branch"
Write-Host "Repo: $repoName"
Write-Host "Remote: $remoteName ($remotePath)"
Write-Host "Beschrijving: $beschrijving"
if ($Force) { Write-Host "Mode: FORCE PUSH" }
if ($NoRebase) { Write-Host "Mode: GEEN REBASE" }
Write-Host "========================================"

# Excel controle
Write-Host "`n[1/5] Controleren op open Excel bestanden..."
if (-not $SkipExcel) {
    $excelLocks = Get-ChildItem -Path . -Recurse -Filter "~`$*" -ErrorAction SilentlyContinue | 
        Where-Object { @('.xlsx','.xls','.xlsm','.xlsb') -contains $_.Extension.ToLower() }
    if ($excelLocks) {
        Write-Host "  ✗ FOUT: Excel bestanden zijn open!"
        $excelLocks | ForEach-Object { Write-Host "    - $($_.Name -replace '~\$', '')" }
        exit 1
    }
    Write-Host "  ✓ OK"
} else {
    Write-Host "  ⓘ Excel check overgeslagen"
}

# Wijzigingen checken
Write-Host "`n[2/5] Controleren op wijzigingen..."
$wijzigingen = git status --porcelain
$skipCommit = $false
if (-not $wijzigingen) {
    Write-Host "  ⓘ Geen uncommitted wijzigingen."
    # Check of local ahead is van nas
    git fetch $remoteName 2>$null | Out-Null
    git show-ref --verify --quiet "refs/remotes/$remoteName/$branch"
    if ($LASTEXITCODE -eq 0) {
        $ahead = git rev-list --count "$remoteName/$branch..HEAD" 2>$null
        if ($ahead -gt 0) {
            Write-Host "  ✓ Lokaal $ahead commit(s) voor op NAS - doorgaan met push"
            $skipCommit = $true
        } else {
            Write-Host "  ⓘ Lokaal gelijk met NAS - niets te doen."
            exit 0
        }
    } else {
        Write-Host "  ⓘ NAS heeft nog geen branch '$branch' - doorgaan met push"
        $skipCommit = $true
    }
} else {
    Write-Host "  ✓ Wijzigingen gevonden"
}

# Commit
if (-not $skipCommit) {
    if (-not $hasDescription) {
        Write-Host "✗ FOUT: Geef een beschrijving op!"
        Write-Host "Voorbeeld: .\naspush.ps1 'wat je hebt gedaan'"
        exit 1
    }
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
if ($Force) {
    Write-Host "  ⓘ Force push actief - rebase wordt overgeslagen"
} elseif ($NoRebase) {
    Write-Host "  ⓘ Rebase overgeslagen"
} else {
    git pull --rebase $remoteName $branch 2>$null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ⚠️  BOTSING MET NAS!"
        Write-Host "  Iemand anders heeft ook gepusht."
        Write-Host "`n  OPLOSSEN:"
        Write-Host "  1. git status"
        Write-Host "  2. Los conflicten op (zoek '<<<<<<<')"
        Write-Host "  3. git add ."
        Write-Host "  4. git rebase --continue"
        Write-Host "  Of annuleer: git rebase --abort"
        Write-Host "  Of force: .\naspush.ps1 -Force 'reden'"
        exit 1
    }
    Write-Host "  ✓ Up-to-date met NAS"
}

# Push
Write-Host "`n[5/5] Pushen naar NAS..."
if ($Force) {
    git push --force-with-lease $remoteName $branch
} else {
    git push $remoteName $branch
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ FOUT: Push naar NAS mislukt."
    exit 1
}

Write-Host "`n✓ KLAAR - Gepusht naar NAS"
