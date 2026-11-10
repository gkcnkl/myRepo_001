<#
.SYNOPSIS
    Zamanlanmış görevi ve kurulum klasörünü kaldırır. Yönetici olarak çalıştırın.
    Adaptör ayarları olduğu gibi bırakılır; DHCP'ye dönmek için -ResetToDhcp kullanın.
#>
[CmdletBinding()]
param(
    [string]$InstallDir = (Join-Path $env:ProgramData 'WifiProfileSwitcher'),
    [switch]$KeepConfig,
    [switch]$ResetToDhcp
)

$ErrorActionPreference = 'Stop'

Unregister-ScheduledTask -TaskName 'WifiProfileSwitcher' -Confirm:$false -ErrorAction SilentlyContinue

if ($ResetToDhcp) {
    $wifi = Get-NetAdapter -Physical | Where-Object { $_.NdisPhysicalMedium -eq 9 } | Select-Object -First 1
    if ($wifi) {
        Get-NetIPAddress -InterfaceIndex $wifi.ifIndex -AddressFamily IPv4 -PrefixOrigin Manual -ErrorAction SilentlyContinue |
            Remove-NetIPAddress -Confirm:$false -ErrorAction SilentlyContinue
        Remove-NetRoute -InterfaceIndex $wifi.ifIndex -DestinationPrefix '0.0.0.0/0' -Confirm:$false -ErrorAction SilentlyContinue
        Set-NetIPInterface -InterfaceIndex $wifi.ifIndex -AddressFamily IPv4 -Dhcp Enabled
        Set-DnsClientServerAddress -InterfaceIndex $wifi.ifIndex -ResetServerAddresses
        & ipconfig.exe /renew "$($wifi.Name)" | Out-Null
        Write-Host "$($wifi.Name) DHCP'ye döndürüldü."
    }
}

if (Test-Path $InstallDir) {
    if ($KeepConfig) {
        Get-ChildItem $InstallDir -Exclude 'config.json' | Remove-Item -Recurse -Force
    } else {
        Remove-Item $InstallDir -Recurse -Force
    }
}
Write-Host 'Kaldırıldı.' -ForegroundColor Green
