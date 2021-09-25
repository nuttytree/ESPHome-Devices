if ($Args.Count -ne 2 -or ($Args[0] -ne "add" -and $Args[0] -ne "get"))
{
    Write-Output "Invalid parameters"
    exit 1
}

Add-Type -AssemblyName 'System.Web'

if (!(Get-InstalledModule -Name powershell-yaml -ErrorAction SilentlyContinue)) 
{
    Install-Module -Scope CurrentUser powershell-yaml
}

$device = $Args[1]
if ($device.Contains("_"))
{
    Write-Output "Invalid device id"
    exit 1
}

[string[]]$secretsFile = Get-Content $PSScriptRoot\..\secrets.yaml
$yaml = ''
foreach ($line in $secretsFile) { $yaml = $yaml + "`n" + $line }
$secrets = ConvertFrom-YAML $yaml -Ordered

if ($Args[0] -eq "add") 
{
    if ($secrets.Contains("${device}-ip"))
    {
        Write-Output "Device already exists"
        exit 1
    }

    $lastIp = ([IPAddress] "192.168.0.0").GetAddressBytes()
    foreach ($key in $secrets.Keys)
    {
        if ($key.EndsWith("ip"))
        {
            $ip = ([IPAddress] $secrets.$key).GetAddressBytes()
            if ($ip[2] -gt $lastIp[2] -or ($ip[2] -eq $lastIp[2] -and $ip[3] -gt $lastIp[3]))
            {
                $lastIp = $ip
            }
        }
    }
    $nextIp = $lastIp
    if ($nextIp[3] -eq 255)
    {
        $nextIp[3] = 0
        $nextIp[2] += 1
    }
    else
    {
        $nextIp[3] += 1
    }

    $secrets["${device}-ip"] = ([IPAddress]$nextIp).ToString()
    $secrets["${device}-api-pwd"] = -join ((48..57) + (65..90) + (97..122) | Get-Random -Count 16 | % {[char]$_})
    $secrets["${device}-ota-pwd"] = -join ((48..57) + (65..90) + (97..122) | Get-Random -Count 16 | % {[char]$_})
    $secrets["${device}-ap-pwd"] = -join ((48..57) + (65..90) + (97..122) | Get-Random -Count 16 | % {[char]$_})

    Out-File $PSScriptRoot\..\secrets.yaml -InputObject (ConvertTo-YAML $secrets)
}

if (!$secrets.Contains("${device}-ip"))
{
    Write-Output "Device not found"
    exit 1
}

$ip = $secrets["${device}-ip"]
$api = $secrets["${device}-api-pwd"]
$ota = $secrets["${device}-ota-pwd"]
$ap = $secrets["${device}-ap-pwd"]
Write-Output ${device}:
Write-Output "  IP Address:   ${ip}"
Write-Output "  API Password: ${api}"
Write-Output "  OTA Password: ${ota}"
Write-Output "  AP Password:  ${ap}"
