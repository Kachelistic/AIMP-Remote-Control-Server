$ErrorActionPreference = 'Stop'
$stage = Join-Path $PSScriptRoot '../build/package/AimpHttpControl'
New-Item -ItemType Directory -Force -Path "$stage/x64/Langs", "$stage/Langs", "$stage/web", "$stage/x64/web" | Out-Null
Copy-Item web/pocket.html "$stage/web/pocket.html"
Copy-Item web/pocket.html "$stage/x64/web/pocket.html"
Copy-Item build/AimpHttpControl32.dll "$stage/AimpHttpControl.dll"
Copy-Item build/AimpHttpControl64.dll "$stage/x64/AimpHttpControl.dll"
Copy-Item LICENSE "$stage/LICENSE.txt"
Copy-Item POCKET_README_KO.md "$stage/README_KO.md"
[System.Text.Encoding]::RegisterProvider([System.Text.CodePagesEncodingProvider]::Instance)
$russian = [IO.File]::ReadAllText((Resolve-Path Langs/russian.lng))
foreach ($folder in @("$stage/Langs", "$stage/x64/Langs")) {
 Copy-Item Langs/english.lng "$folder/english.lng"
 [IO.File]::WriteAllText("$folder/russian.lng", $russian, [Text.Encoding]::GetEncoding(1251))
}
Compress-Archive -Path $stage -DestinationPath build/AimpHttpControl.zip -Force
Move-Item -LiteralPath build/AimpHttpControl.zip -Destination build/AimpHttpControl.aimppack -Force
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead((Resolve-Path build/AimpHttpControl.aimppack))
try {
 foreach ($name in @('AimpHttpControl/AimpHttpControl.dll','AimpHttpControl/x64/AimpHttpControl.dll','AimpHttpControl/Langs/english.lng','AimpHttpControl/x64/Langs/russian.lng','AimpHttpControl/web/pocket.html','AimpHttpControl/x64/web/pocket.html')) {
  if (-not ($archive.Entries | Where-Object { $_.FullName.Replace('\','/') -eq $name })) { throw "Missing package entry: $name" }
 }
} finally { $archive.Dispose() }
Get-FileHash build/AimpHttpControl.aimppack,build/AimpHttpControl32.dll,build/AimpHttpControl64.dll -Algorithm SHA256 |
 ForEach-Object { "$($_.Hash.ToLower())  $([IO.Path]::GetFileName($_.Path))" } |
 Set-Content build/SHA256SUMS.txt
