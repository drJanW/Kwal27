<#
.SYNOPSIS
    Synchroniseer je lokale map met de NAS-versie bij opstarten
.DESCRIPTION
    Dit script haalt de nieuwste wijzigingen van de NAS en past ze toe.
    Controleert eerst op open Excel bestanden om problemen te voorkomen.
    
    GEBRUIK:
    - Bij opstarten: Begin altijd met dit script om up-to-date te zijn
    - Tussendoor: Als je denkt dat de andere computer iets heeft opgeslagen
    
    BELANGRIJK:
    - Als je zelf ook wijzigingen hebt (nog niet opgeslagen op NAS):
      EERST 'rondaf.ps1' draaien, DAN dit script!
.EXAMPLE
    .\opstart.ps1
#>

Write-Host "========================================"
Write-Host "SYNCHRONISATIE MET NAS - OPSTARTEN"
Write-Host "Computer: $env:COMPUTERNAME"
Write-Host "Tijd: $(Get-Date -Format 'HH:mm')"
Write-Host "========================================"

# EXCEL CONTROLE
Write-Host "`n[0/5] Controleren op open Excel bestanden..."
$allLockFiles = Get-ChildItem -Path . -Recurse -Filter "~`$*" -ErrorAction SilentlyContinue
$excelExtensions = @('.xlsx', '.xls', '.xlsm', '.xlsb')
$excelLockFiles = @()

foreach ($file in $allLockFiles) {
    if ($excelExtensions -contains $file.Extension.ToLower()) {
        $excelLockFiles += $file
    }
}

if ($excelLockFiles.Count -gt 0) {
    Write-Host "  ✗ GEVONDEN: $($excelLockFiles.Count) open Excel bestand(en)!"
    Write-Host "  Sluit eerst Excel:"
    
    foreach ($lockFile in $excelLockFiles) {
        $originalName = $lockFile.Name -replace '~\$', ''
        Write-Host "    - $originalName"
    }
    
    Write-Host "`n  OPLOSSING:"
    Write-Host "  1. Ga naar Excel"
    Write-Host "  2. Sluit de genoemde Excel bestanden"
    Write-Host "  3. Wacht even tot Windows lock verwijderd is"
    Write-Host "  4. Draai dit script opnieuw"
    exit 1
}
Write-Host "  ✓ Geen open Excel bestanden gevonden."

# STAP 1: Zitten we in het goede project?
Write-Host "`n[1/5] Project controleren..."
if (-not (Test-Path ".git")) {
    Write-Host "  ✗ FOUT: Dit is niet het Arduino project!"
    Write-Host "  Ga naar de map waar je .ino bestand staat."
    exit 1
}
Write-Host "  ✓ Juiste project gevonden."

# STAP 2: Kan ik bij de NAS?
Write-Host "`n[2/5] Contact met NAS maken..."
git fetch nas --dry-run 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "  ✗ FOUT: NAS niet bereikbaar!"
    Write-Host "  Controleer:"
    Write-Host "  1. Is NAS aan? (netwerk/stroom)"
    Write-Host "  2. Zit je op hetzelfde netwerk?"
    Write-Host "  3. Remote naam correct? (moet 'nas' zijn)"
    exit 1
}
Write-Host "  ✓ NAS is online en bereikbaar."

# STAP 3: Wat is er nieuw op de NAS?
Write-Host "`n[3/5] Wijzigingen vergelijken..."
try {
    $nieuwOpNAS = git rev-list --count HEAD..nas/master
    $alleenLokaal = git rev-list --count nas/master..HEAD
} catch {
    Write-Host "  ⚠️  Kan niet precies vergelijken, maar we gaan door..."
    $nieuwOpNAS = 1
    $alleenLokaal = 0
}

if ($nieuwOpNAS -eq 0 -and $alleenLokaal -eq 0) {
    Write-Host "  ✓ Alles is al gelijk! Niks te doen."
    Write-Host "`n========================================"
    Write-Host "KLAAR! Je kunt beginnen met programmeren."
    Write-Host "========================================"
    exit 0
}

Write-Host "  Verschillen gevonden:"
Write-Host "  - $nieuwOpNAS wijziging(en) op NAS die jij nog niet hebt"
Write-Host "  - $alleenLokaal wijziging(en) alleen op jouw computer"

# WAARSCHUWING: als je lokale wijzigingen hebt
if ($alleenLokaal -gt 0) {
    Write-Host "`n  ⚠️  LET OP: Je hebt werk dat nog niet op NAS staat!"
    Write-Host "  Dit kan tot problemen leiden als de NAS iets anders heeft."
    Write-Host "`n  OPLOSSINGEN:"
    Write-Host "  A. Eerst opslaan op NAS: .\naspush.ps1 'mijn wijzigingen'"
    Write-Host "  B. Tijdelijk parkeren en later afmaken (gevorderd)"
    Write-Host "  C. Doorgaan en hopen op geen problemen (niet aanbevolen)"
    
    $keuze = Read-Host "`nKies: A=opslaan, C=doorgaan, X=stoppen (a/c/x)"
    if ($keuze -eq 'x') {
        Write-Host "Gestopt. Los eerst je lokale wijzigingen op."
        exit 1
    }
    elseif ($keuze -eq 'a') {
        Write-Host "Eerst 'rondaf.ps1' draaien, daarna dit script opnieuw."
        exit 1
    }
}

# STAP 4: NAS wijzigingen ophalen
Write-Host "`n[4/5] Nieuwste versie van NAS ophalen..."
Write-Host "  Dit bewaart jouw werk bovenaan de geschiedenis."
git pull --rebase nas master
if ($LASTEXITCODE -ne 0) {
    Write-Host "`n  ⚠️  BOTSING GEVONDEN!"
    Write-Host "  Jij en de NAS hebben hetzelfde bestand anders aangepast."
    Write-Host "`n  VOLG DEZE STAPPEN:"
    Write-Host "  1. Zie welke bestanden: git status"
    Write-Host "  2. Open in VS Code, zoek naar '<<<<<<<' markers"
    Write-Host "  3. Kies welke versie je wilt houden (of combineer)"
    Write-Host "  4. Sla op en voeg toe: git add ."
    Write-Host "  5. Ga verder: git rebase --continue"
    Write-Host "  6. Of neem NAS versie: git rebase --abort"
    Write-Host "`n  Daarna dit script opnieuw draaien."
    exit 1
}

Write-Host "  ✓ Succesvol gesynchroniseerd!"

Write-Host "`n========================================"
Write-Host "GELUKT! Je hebt de nieuwste versie."
Write-Host "Succes met je Arduino project!"
Write-Host "========================================"