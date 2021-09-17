Push-Location $PSScriptRoot\..\devices

$files = Get-ChildItem "*.yaml" -Exclude "secrets.yaml"
$failures = New-Object Collections.Generic.List[String]
foreach ($file in $files) {
    esphome compile $file.Name
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

Pop-Location