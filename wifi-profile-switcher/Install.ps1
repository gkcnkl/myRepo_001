<#
.SYNOPSIS
    Wi-Fi Profil Değiştirici'yi kurar: dosyaları kopyalar ve her Wi-Fi bağlantısında
    çalışan bir zamanlanmış görev oluşturur. Yönetici olarak çalıştırın.
#>
[CmdletBinding()]
param(
    [string]$InstallDir = (Join-Path $env:ProgramData 'WifiProfileSwitcher')
)

$ErrorActionPreference = 'Stop'

# Betiğin bulunduğu klasör. Windows PowerShell 5.1'de $PSScriptRoot bazen boş gelir; yedek yollar dene.
$ScriptDir = $PSScriptRoot
if (-not $ScriptDir -and $MyInvocation.MyCommand.Path) { $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $ScriptDir) { $ScriptDir = (Get-Location).Path }
$TaskName = 'WifiProfileSwitcher'

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { throw 'Bu betiği "Yönetici olarak" açılmış bir PowerShell penceresinde çalıştırın.' }

New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
Copy-Item (Join-Path $ScriptDir 'Switch-WifiProfile.ps1') $InstallDir -Force

$configTarget = Join-Path $InstallDir 'config.json'
if (-not (Test-Path $configTarget)) {
    $src = Join-Path $ScriptDir 'config.json'
    if (-not (Test-Path $src)) { $src = Join-Path $ScriptDir 'config.example.json' }
    Copy-Item $src $configTarget
    Write-Host "Yapılandırma kopyalandı: $configTarget  (kendi ağlarınıza göre düzenleyin)" -ForegroundColor Yellow
} else {
    Write-Host "Mevcut yapılandırma korundu: $configTarget"
}

# Klasöre sadece yöneticiler ve SYSTEM yazabilsin (görev SYSTEM olarak çalıştığı için önemli).
& icacls.exe $InstallDir /inheritance:r /grant:r '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-544:(OI)(CI)F' '*S-1-5-32-545:(OI)(CI)RX' | Out-Null

$script = Join-Path $InstallDir 'Switch-WifiProfile.ps1'
$args_  = "-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$script`" -ConfigPath `"$configTarget`""

# Tetikleyiciler:
#  - WLAN-AutoConfig olay 8001: "Kablosuz ağa başarıyla bağlanıldı"
#  - Oturum açılışı ve bilgisayar açılışı: o anda zaten bağlıysanız ayarları düzeltmek için
$xml = @"
<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo>
    <Description>Bağlanılan Wi-Fi ağına göre adaptörün IP/DNS ayarlarını değiştirir.</Description>
  </RegistrationInfo>
  <Triggers>
    <EventTrigger>
      <Enabled>true</Enabled>
      <Subscription>&lt;QueryList&gt;&lt;Query Id="0" Path="Microsoft-Windows-WLAN-AutoConfig/Operational"&gt;&lt;Select Path="Microsoft-Windows-WLAN-AutoConfig/Operational"&gt;*[System[(EventID=8001)]]&lt;/Select&gt;&lt;/Query&gt;&lt;/QueryList&gt;</Subscription>
    </EventTrigger>
    <BootTrigger>
      <Enabled>true</Enabled>
      <Delay>PT30S</Delay>
    </BootTrigger>
    <LogonTrigger>
      <Enabled>true</Enabled>
      <Delay>PT10S</Delay>
    </LogonTrigger>
  </Triggers>
  <Principals>
    <Principal id="Author">
      <UserId>S-1-5-18</UserId>
      <RunLevel>HighestAvailable</RunLevel>
    </Principal>
  </Principals>
  <Settings>
    <MultipleInstancesPolicy>Queue</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <ExecutionTimeLimit>PT2M</ExecutionTimeLimit>
    <StartWhenAvailable>true</StartWhenAvailable>
    <Enabled>true</Enabled>
  </Settings>
  <Actions Context="Author">
    <Exec>
      <Command>powershell.exe</Command>
      <Arguments>$([Security.SecurityElement]::Escape($args_))</Arguments>
    </Exec>
  </Actions>
</Task>
"@

Register-ScheduledTask -TaskName $TaskName -Xml $xml -Force | Out-Null

Write-Host ''
Write-Host "Kuruldu. Görev adı: $TaskName" -ForegroundColor Green
Write-Host "Yapılandırma : $configTarget"
Write-Host "Günlük       : $(Join-Path $InstallDir 'switcher.log')"
Write-Host ''
Write-Host 'Durumu görmek için:'
Write-Host "  & '$script' -Status"
