# Installs the mods in this folder into Exanima. Run it with Exanima closed (double-click Install.cmd).
# It backs up your saves first, copies files over (never deletes anything), keeps your ScriptRuntime.ini,
# and checks every copied file against SHA256SUMS.txt.
param([string]$GameDir)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot

function Find-Exanima {
    $roots = @()
    $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    if ($steam) {
        $roots += $steam
        $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
        if (Test-Path $vdf) {
            $roots += Select-String -Path $vdf -Pattern '"path"\s+"([^"]+)"' | ForEach-Object { $_.Matches[0].Groups[1].Value -replace '\\\\', '\' }
        }
    }
    foreach ($root in $roots | Select-Object -Unique) {
        $game = Join-Path $root 'steamapps\common\Exanima'
        if (Test-Path (Join-Path $game 'Exanima.exe')) { return $game }
    }
}

if (-not $GameDir) { $GameDir = Find-Exanima }
if ($GameDir) {
    $answer = Read-Host "Install into $GameDir ? [Y/n]"
    if ($answer -match '^[nN]') { $GameDir = $null }
}
if (-not $GameDir) { $GameDir = Read-Host 'Path to your Exanima folder (the one with Exanima.exe)' }
$GameDir = $GameDir.Trim('"')
if (-not (Test-Path (Join-Path $GameDir 'Exanima.exe'))) { throw "No Exanima.exe in $GameDir" }
if (Get-Process -Name 'Exanima*', 'ExCamera*' -ErrorAction SilentlyContinue) { throw 'Close Exanima first.' }

# Saves: Hellmode installs keep them in <game>\hm_saves, the standard game in %APPDATA%\Exanima. Both are backed up.
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = Join-Path ([Environment]::GetFolderPath('MyDocuments')) "Exanima save backups\$stamp"
foreach ($saves in @((Join-Path $GameDir 'hm_saves'), (Join-Path $env:APPDATA 'Exanima'))) {
    if (Test-Path $saves) {
        $target = Join-Path $backup (Split-Path $saves -Leaf)
        New-Item -ItemType Directory -Force $target | Out-Null
        Copy-Item (Join-Path $saves '*') $target -Recurse -Force
    }
}
if (Test-Path $backup) { Write-Host "Saves backed up to $backup" } else { Write-Host 'No saves found to back up.' }

$expected = @{}
Get-Content (Join-Path $here 'SHA256SUMS.txt') | ForEach-Object {
    $hash, $path = $_ -split '  ', 2
    $expected[$path] = $hash
}
$bad = 0
function Copy-Tree($from, $to, $skip = @()) {
    $count = 0
    Get-ChildItem $from -Recurse -File | Where-Object { $_.Name -notin $skip } | ForEach-Object {
        $relative = $_.FullName.Substring($here.Length + 1).Replace('\', '/')
        $target = Join-Path $to $_.FullName.Substring($from.Length + 1)
        New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
        Copy-Item $_.FullName $target -Force
        $count++
        if ((Get-FileHash $target).Hash -ne $expected[$relative]) { $script:bad++; Write-Warning "Hash mismatch: $target" }
    }
    Write-Host ('{0,-34} {1} files' -f (Split-Path $to -Leaf), $count)
}

Get-ChildItem (Join-Path $here 'mods') -Directory | ForEach-Object {
    Copy-Tree $_.FullName (Join-Path $GameDir "mods\$($_.Name)") @('ScriptRuntime.ini')
}
Copy-Tree (Join-Path $here 'EMTK') (Join-Path $GameDir 'EMTK')
Copy-Tree (Join-Path $here 'profile') (Join-Path $GameDir '.emtk\profiles\Necrosis Mods')

# EMTK remembers the game folder it last launched. On a PC that has never run EMTK, point it at this one.
$history = Join-Path $env:APPDATA 'exanima-modding-toolkit\cache\instance_history.ron'
if (-not (Test-Path $history)) {
    New-Item -ItemType Directory -Force (Split-Path $history) | Out-Null
    $path = ('\\?\' + (Resolve-Path $GameDir).Path).Replace('\', '\\')
    Set-Content -Path $history -Encoding ascii -Value "[`n    `"$path`",`n]"
}

if ($bad) { throw "$bad copied files do not match SHA256SUMS.txt." }
Write-Host ''
Write-Host "Done. Start the game with: $GameDir\EMTK\Launch Exanima (modded).vbs"
