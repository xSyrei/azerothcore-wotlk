# Publishes the client patch and the Emberforge addons to the launcher's CDN (Cloudflare R2), on the PC that has
# rclone set up (remote "r2", see the launcher's docs/CLIENT_DISTRIBUTION.md):
#
#   powershell -ExecutionPolicy Bypass -File "$HOME\azerothcore-wotlk\modules\mod-titan\client-patch\publish-client.ps1"
#
# Compares dist\patch-enUS-4/5/6/7/8.MPQ (gameplay, retextures, HD graphics, reflective water, atmosphere), the
# Emberforge Engine (dist\Emberforge-d3d9.dll) and every file of AddOns\Emberforge_* with the live manifest, uploads
# only what changed, drops files no longer shipped (old addon files, ReShade), raises the version (1.0.2 -> 1.0.3,
# or -Version 1.1.0)
# and uploads the manifest last. Launchers then show Update. Purge Cloudflare's cache after.
param([string]$Version)

$ErrorActionPreference = 'Stop'
$cdn = 'https://cdn.projectemberforge.com/client/'
$bucket = 'r2:emberforge-cdn/client/'
$here = $PSScriptRoot

# path in the client -> file here
$local = [ordered]@{}
foreach ($mpq in 'patch-enUS-4.MPQ', 'patch-enUS-5.MPQ', 'patch-enUS-6.MPQ', 'patch-enUS-7.MPQ',
    'patch-enUS-8.MPQ') {
    $local["Data/enUS/$mpq"] = Join-Path $here "dist\$mpq"
}
# The Emberforge Engine (engine\, built by engine/build.sh): the launcher puts it in the game folder as d3d9.dll while
# Lighting is on.
$local['Emberforge-d3d9.dll'] = Join-Path $here 'dist\Emberforge-d3d9.dll'
# No longer shipped (ReShade, which the engine replaced): dropped from the manifest.
$retired = @('ReShade32.dll', 'ReShade.default.ini', 'ReShadePreset.default.ini', 'ReShade-LICENSE.md',
    'Emberforge.addon32', 'Data/enUS/Water-ReShade.MPQ')
foreach ($addon in Get-ChildItem (Join-Path $here 'AddOns') -Directory -Filter 'Emberforge_*') {
    foreach ($file in Get-ChildItem $addon.FullName -Recurse -File) {
        $rel = $file.FullName.Substring($addon.FullName.Length + 1).Replace('\', '/')
        $local["Interface/AddOns/$($addon.Name)/$rel"] = $file.FullName
    }
}

# A fresh copy of the manifest (the query keeps Cloudflare's cache out of it).
$manifest = Invoke-RestMethod "$($cdn)manifest.json?fresh=$([guid]::NewGuid())"
$live = @{}
foreach ($entry in $manifest.files) { $live[$entry.path] = $entry }

$files = New-Object System.Collections.ArrayList
foreach ($entry in $manifest.files) {
    $ours = $local.Contains($entry.path) -or $entry.path -like 'Interface/AddOns/Emberforge_*' `
        -or $entry.path -like 'reshade-shaders/*' -or $retired -contains $entry.path
    if (-not $ours) { [void]$files.Add($entry) }
}
$changed = @()
foreach ($rel in $local.Keys) {
    $full = $local[$rel]
    $sha = (Get-FileHash $full -Algorithm SHA256).Hash.ToLower()
    if (-not $live.ContainsKey($rel) -or $live[$rel].sha256 -ne $sha) { $changed += $rel }
    [void]$files.Add([pscustomobject]@{ path = $rel; size = (Get-Item $full).Length; sha256 = $sha })
}
$removed = @($manifest.files | Where-Object {
    ($_.path -like 'Interface/AddOns/Emberforge_*' -or $_.path -like 'reshade-shaders/*' `
        -or $retired -contains $_.path) -and -not $local.Contains($_.path)
})

if (-not $changed -and -not $removed) {
    Write-Host "Nothing to publish: the CDN already has these files (version $($manifest.version))."
    return
}
if (-not $Version) {
    $parts = $manifest.version.Split('.')
    $parts[-1] = [string]([int]$parts[-1] + 1)
    $Version = $parts -join '.'
}

foreach ($rel in $changed) {
    Write-Host "Uploading $rel"
    rclone copyto $local[$rel] "$bucket$rel" --s3-no-check-bucket
    if ($LASTEXITCODE -ne 0) { throw "Upload failed: $rel. Nothing else was changed; run this again." }
}
foreach ($entry in $removed) { Write-Host "No longer shipped: $($entry.path)" }

$manifest.files = $files.ToArray()
$manifest.version = $Version
$out = Join-Path $env:TEMP 'emberforge-manifest.json'
$manifest | ConvertTo-Json -Depth 5 -Compress | Set-Content $out -Encoding ascii
rclone copyto $out "$($bucket)manifest.json" --header-upload 'Cache-Control: no-cache' --s3-no-check-bucket
if ($LASTEXITCODE -ne 0) { throw 'Manifest upload failed: run this again.' }

Write-Host ''
Write-Host "Published version $Version ($($changed.Count) files changed, $($removed.Count) removed)."
Write-Host 'Now purge Cloudflare''s cache (Caching > Configuration > Purge Everything), then open the launcher: Update.'
