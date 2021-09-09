$version = esphome version
$version = $version.Replace("Version: ", "")

Write-Output "Tagging the current branch with version: $version"

git tag -a $version -m "This commit is compatible with ESPHome version $version"

git push origin $version