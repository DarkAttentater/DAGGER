# Builds ..\DAGGER-<version>-r<rev>.zip (FOMOD installer) from dist-template plus the built DLL.
# The revision comes from DAGGER_REVISION in CMakeLists.txt (single source of truth); only the newest zip is kept.
# Layout: README.md, LICENSE, NOTICE, fomod\, SKSE\Plugins\<dll>, options\{Redone,Original,Both}\<ini>
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$out = Split-Path $root -Parent
$cml = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
$version = [regex]::Match($cml, 'project\(DAGGER VERSION ([\d\.]+)').Groups[1].Value
if (-not $version) { throw "project VERSION not found in CMakeLists.txt" }
$rev = [regex]::Match((Get-Content (Join-Path $root "CMakeLists.txt") -Raw), 'set\(DAGGER_REVISION\s+(\d+)\)').Groups[1].Value
if (-not $rev) { throw "DAGGER_REVISION not found in CMakeLists.txt" }

$stage = Join-Path $root "dist\stage"
if (Test-Path (Join-Path $root "dist")) { Remove-Item (Join-Path $root "dist") -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item (Join-Path $root "dist-template\fomod") $stage -Recurse
foreach ($f in "README.md", "LICENSE", "NOTICE") { Copy-Item (Join-Path $out $f) $stage }
foreach ($f in "info.xml", "ModuleConfig.xml") {
    $p = Join-Path $stage "fomod\$f"
    (Get-Content $p -Raw).Replace("{REV}", $rev).Replace("{VER}", $version) | Set-Content $p -Encoding UTF8
}
New-Item -ItemType Directory -Path (Join-Path $stage "SKSE\Plugins") -Force | Out-Null
Copy-Item (Join-Path $root "build\Release\DAGGER.dll") (Join-Path $stage "SKSE\Plugins")

$template = (Get-Content (Join-Path $root "dist-template\DAGGER.ini.in") -Raw).Replace("{REV}", $rev).Replace("{VER}", $version)
$variants = [ordered]@{
    Redone   = "GoamSm, GoamLrg, GoamXL"
    Original = "GoamElvenEars"
    Both     = "GoamSm, GoamLrg, GoamXL, GoamElvenEars"
}
foreach ($k in $variants.Keys) {
    $dir = Join-Path $stage "options\$k"
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $template.Replace("{NODES}", $variants[$k]) | Set-Content (Join-Path $dir "DAGGER.ini") -Encoding ASCII
}

Get-ChildItem $out -Filter "DAGGER-*.zip" | Remove-Item -Force
$zip = Join-Path $out "DAGGER-$version-r$rev.zip"
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$z = [IO.Compression.ZipFile]::Open($zip, 'Create')
Get-ChildItem $stage -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
    [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($z, $_.FullName, $rel, 'Optimal')
}
$z.Dispose()
$zip
