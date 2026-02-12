<#
.SYNOPSIS
    Commit en push naar GitHub
.DESCRIPTION
    Commit alle wijzigingen en pusht naar GitHub.
    Voegt automatisch de 'origin' remote toe als die niet bestaat.
.EXAMPLE
    .\hubpush.ps1 "mijn wijzigingen"
    .\hubpush.ps1 bug gefixt
    .\hubpush.ps1 -Force "force push"
    .\hubpush.ps1 -SkipExcel "zonder Excel check"
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]] $Message,
    [switch] $Force,
    [switch] $SkipExcel
)

$beschrijving = ($Message -join " ").Trim()
$hasDescription = -not [string]::IsNullOrWhiteSpace($beschrijving)
$branch = git rev-parse --abbrev-ref HEAD
if ($LASTEXITCODE -ne 0) {
    Write-Host "x FOUT: Geen git repository gevonden."
    exit 1
}

$repoRoot = git rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0 -or -not $repoRoot) {
    Write-Host "x FOUT: Kan git root niet bepalen."
    exit 1
}
$repoName = Split-Path -Leaf $repoRoot

$remoteName = "origin"
$githubUrl = "https://github.com/janwilmans/$repoName.git"

# Ensure remote exists
$currentRemote = git remote get-url $remoteName 2>$null
if ($LASTEXITCODE -ne 0 -or -not $currentRemote) {
    Write-Host "i Remote '$remoteName' niet gevonden, toevoegen: $githubUrl"
    git remote add $remoteName $githubUrl
    if ($LASTEXITCODE -ne 0) {
        Write-Host "x FOUT: Kon remote '$remoteName' niet toevoegen."
        exit 1
    }
}

Write-Host "========================================"
Write-Host "COMMIT & PUSH NAAR GITHUB"
Write-Host "Branch: $branch"
Write-Host "Repo: $repoName"
Write-Host "Remote: $remoteName ($githubUrl)"
Write-Host "Beschrijving: $beschrijving"
if ($Force) { Write-Host "Mode: FORCE PUSH" }
Write-Host "========================================"

# Excel controle
Write-Host "`n[1/4] Controleren op open Excel bestanden..."
if (-not $SkipExcel) {
    $excelLocks = Get-ChildItem -Path . -Recurse -Filter "~`$*" -ErrorAction SilentlyContinue | 
        Where-Object { @('.xlsx','.xls','.xlsm','.xlsb') -contains $_.Extension.ToLower() }
    if ($excelLocks) {
        Write-Host "  x FOUT: Excel bestanden zijn open!"
        $excelLocks | ForEach-Object { Write-Host "    - $($_.Name -replace '~\$', '')" }
        exit 1
    }
    Write-Host "  v OK"
} else {
    Write-Host "  i Excel check overgeslagen"
}

# Wijzigingen checken
Write-Host "`n[2/4] Controleren op wijzigingen..."
$wijzigingen = git status --porcelain
$skipCommit = $false
if (-not $wijzigingen) {
    Write-Host "  i Geen uncommitted wijzigingen."
    git fetch $remoteName 2>$null | Out-Null
    git show-ref --verify --quiet "refs/remotes/$remoteName/$branch"
    if ($LASTEXITCODE -eq 0) {
        $ahead = git rev-list --count "$remoteName/$branch..HEAD" 2>$null
        if ($ahead -gt 0) {
            Write-Host "  v Lokaal $ahead commit(s) voor op GitHub - doorgaan met push"
            $skipCommit = $true
        } else {
            Write-Host "  i Lokaal gelijk met GitHub - niets te doen."
            exit 0
        }
    } else {
        Write-Host "  i GitHub heeft nog geen branch '$branch' - doorgaan met push"
        $skipCommit = $true
    }
} else {
    Write-Host "  v Wijzigingen gevonden"
}

# Commit
if (-not $skipCommit) {
    if (-not $hasDescription) {
        Write-Host "x FOUT: Geef een beschrijving op!"
        Write-Host "Voorbeeld: .\hubpush.ps1 'wat je hebt gedaan'"
        exit 1
    }
    Write-Host "`n[3/4] Committen..."
    git add -A
    git commit -m "$beschrijving"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  x FOUT: Commit mislukt."
        exit 1
    }
    Write-Host "  v Commit aangemaakt"
} else {
    Write-Host "`n[3/4] Committen... (overgeslagen)"
}

# Push
Write-Host "`n[4/4] Pushen naar GitHub..."
if ($Force) {
    git push --force-with-lease $remoteName $branch
} else {
    git push -u $remoteName $branch
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "  x FOUT: Push naar GitHub mislukt."
    Write-Host "  Controleer: GitHub login, repo bestaat, netwerk"
    exit 1
}

Write-Host "`nv KLAAR - Gepusht naar GitHub"
