Write-Output "`nBacking up build folder"
if (Test-Path $PSScriptRoot\..\build\_backup)
{
    Remove-Item -Path $PSScriptRoot\..\build\_backup -Recurse -Force
}
if (Test-Path $PSScriptRoot\..\_backup)
{
    Remove-Item -Path $PSScriptRoot\..\_backup -Recurse -Force
}
New-Item $PSScriptRoot\..\_backup -ItemType Directory | Out-Null
Move-Item -Path $PSScriptRoot\..\build\* -Destination $PSScriptRoot\..\_backup -Force
Move-Item -Path $PSScriptRoot\..\_backup -Destination $PSScriptRoot\..\build -Force

Write-Output "`nCompiling all devices"
$files = Get-ChildItem $PSScriptRoot\..\devices\*.yaml -Exclude "secrets.yaml"
$failures = New-Object Collections.Generic.List[String]
foreach ($file in $files) {
    $fileName = $file.Name
    esphome compile  $PSScriptRoot\..\devices\$fileName
    if ($LASTEXITCODE -ne "0") {
        $failures.Add($file.Name)
    }
}

if ($failures -eq 0) {
    Write-Output "`n`nAll devices compiled successfully"
}
else {
    Write-Output "`n`nThe following devices failed to compile:"
    foreach ($failure in $failures) {
        Write-Output $failure
    }
}
