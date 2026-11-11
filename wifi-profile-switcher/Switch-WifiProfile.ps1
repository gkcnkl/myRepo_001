<#
.SYNOPSIS
    Bağlanılan Wi-Fi ağına (SSID) göre Wi-Fi adaptörünün IPv4 ve DNS ayarlarını değiştirir.

.DESCRIPTION
    config.json içindeki "networks" listesinde bağlı olunan SSID aranır.
    Bulunursa o profilin ayarları, bulunamazsa "default" profil uygulanır.
    Ayarlar zaten istenen durumdaysa hiçbir şey değiştirilmez (bağlantı gereksiz yere kopmaz).

    Normalde Install.ps1 ile kurulan zamanlanmış görev tarafından, her Wi-Fi bağlantısında
    SYSTEM hesabıyla otomatik çalıştırılır. Elle de çalıştırılabilir (yönetici olarak).

.PARAMETER ConfigPath
    Yapılandırma dosyası. Varsayılan: betikle aynı klasördeki config.json

.PARAMETER DryRun
    Hiçbir şeyi değiştirmez; sadece ne yapılacağını yazar.

.PARAMETER Status
    Bağlı SSID'yi, eşleşen profili ve adaptörün şu anki ayarlarını gösterir.

.EXAMPLE
    .\Switch-WifiProfile.ps1 -Status
.EXAMPLE
    .\Switch-WifiProfile.ps1 -DryRun
#>
[CmdletBinding()]
param(
    [string]$ConfigPath,
    [switch]$DryRun,
    [switch]$Status
)

Set-StrictMode -Version 2.0

# Betiğin bulunduğu klasör. Windows PowerShell 5.1'de $PSScriptRoot bazen boş gelir; yedek yollar dene.
$ScriptDir = $PSScriptRoot
if (-not $ScriptDir -and $MyInvocation.MyCommand.Path) { $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $ScriptDir) { $ScriptDir = (Get-Location).Path }
if (-not $ConfigPath) { $ConfigPath = Join-Path $ScriptDir 'config.json' }
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Günlük
# ---------------------------------------------------------------------------
$script:LogPath = Join-Path $ScriptDir 'switcher.log'

function Write-Log {
    param([string]$Message, [string]$Level = 'INFO')
    $line = '{0} [{1}] {2}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Level, $Message
    Write-Host $line
    try {
        # Günlük 1 MB'ı aşarsa eskisini .old yap
        if ((Test-Path $script:LogPath) -and ((Get-Item $script:LogPath).Length -gt 1MB)) {
            Move-Item $script:LogPath "$($script:LogPath).old" -Force
        }
        Add-Content -Path $script:LogPath -Value $line -Encoding UTF8
    } catch { }
}

# ---------------------------------------------------------------------------
# Saf (yan etkisiz) yardımcılar — Tests\ altındaki Pester testleri bunları sınar
# ---------------------------------------------------------------------------

# "netsh wlan show interfaces" çıktısından bağlı SSID'yi çıkarır.
# Türkçe ve İngilizce Windows'ta anahtar "SSID" olarak kalır; "BSSID" satırı hariç tutulur.
function Get-SsidFromNetshOutput {
    param([string[]]$Lines, [string]$InterfaceName)
    $currentName = $null
    foreach ($raw in $Lines) {
        $line = $raw.TrimEnd()
        # İlk "Ad/Name : ..." satırı yeni bir arayüz bloğunu başlatır
        if ($line -match '^\s*(Name|Ad|Adı)\s*:\s*(.+)$') { $currentName = $Matches[2].Trim(); continue }
        if ($line -match '^\s*SSID\s*:\s*(.*)$') {
            $ssid = $Matches[1].Trim()
            if (-not $ssid) { continue }
            if (-not $InterfaceName -or -not $currentName -or $currentName -eq $InterfaceName) { return $ssid }
        }
    }
    return $null
}

# SSID için uygulanacak profili seçer. Eşleşme büyük/küçük harfe duyarlıdır (SSID'ler öyledir).
function Resolve-NetworkProfile {
    param($Config, [string]$Ssid)
    if ($Ssid -and $Config.PSObject.Properties['networks']) {
        foreach ($p in $Config.networks.PSObject.Properties) {
            if ($p.Name -ceq $Ssid) { return [pscustomobject]@{ Name = $p.Name; Settings = $p.Value } }
        }
    }
    if ($Config.PSObject.Properties['default'] -and $Config.default) {
        return [pscustomobject]@{ Name = '(default)'; Settings = $Config.default }
    }
    return $null
}

function Test-IPv4 { param([string]$s) return [bool]($s -match '^((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)$') }

# Bir profil ayarındaki hataları listeler (boş liste = geçerli).
function Test-ProfileSettings {
    param($Settings, [string]$Name)
    $errors = @()
    $mode = if ($Settings.PSObject.Properties['mode']) { [string]$Settings.mode } else { '' }
    if ($mode -notin @('dhcp', 'static')) { $errors += "[$Name] 'mode' 'dhcp' veya 'static' olmalı (şu an: '$mode')" }
    if ($mode -eq 'static') {
        foreach ($k in 'ip', 'prefixLength', 'gateway') {
            if (-not $Settings.PSObject.Properties[$k]) { $errors += "[$Name] static profilde '$k' gerekli" }
        }
        if ($Settings.PSObject.Properties['ip'] -and -not (Test-IPv4 $Settings.ip)) { $errors += "[$Name] geçersiz ip: $($Settings.ip)" }
        if ($Settings.PSObject.Properties['gateway'] -and -not (Test-IPv4 $Settings.gateway)) { $errors += "[$Name] geçersiz gateway: $($Settings.gateway)" }
        if ($Settings.PSObject.Properties['prefixLength']) {
            $pl = 0
            if (-not [int]::TryParse([string]$Settings.prefixLength, [ref]$pl) -or $pl -lt 1 -or $pl -gt 32) {
                $errors += "[$Name] prefixLength 1-32 arası olmalı (255.255.255.0 = 24)"
            }
        }
    }
    if ($Settings.PSObject.Properties['dns'] -and $Settings.dns) {
        foreach ($d in @($Settings.dns)) { if (-not (Test-IPv4 $d)) { $errors += "[$Name] geçersiz dns: $d" } }
    }
    return , $errors
}

function Read-SwitcherConfig {
    param([string]$Path)
    if (-not (Test-Path $Path)) { throw "Yapılandırma dosyası bulunamadı: $Path (config.example.json'u kopyalayın)" }
    $config = Get-Content -Path $Path -Raw -Encoding UTF8 | ConvertFrom-Json
    $errors = @()
    if ($config.PSObject.Properties['networks']) {
        foreach ($p in $config.networks.PSObject.Properties) { $errors += Test-ProfileSettings $p.Value $p.Name }
    }
    if ($config.PSObject.Properties['default'] -and $config.default) { $errors += Test-ProfileSettings $config.default 'default' }
    if ($errors.Count) { throw ("config.json hatalı:`n  " + ($errors -join "`n  ")) }
    return $config
}

# Mevcut durum, istenen durumla aynı mı? (Aynıysa dokunmayız.)
function Test-StateMatches {
    param($Settings, $Current)
    $wantDns = @()
    if ($Settings.PSObject.Properties['dns'] -and $Settings.dns) { $wantDns = @($Settings.dns) }

    if ($Settings.mode -eq 'static') {
        if ($Current.Dhcp) { return $false }
        if (@($Current.IPs).Count -ne 1 -or $Current.IPs[0] -ne $Settings.ip) { return $false }
        if ([int]$Current.PrefixLength -ne [int]$Settings.prefixLength) { return $false }
        if ($Current.Gateway -ne $Settings.gateway) { return $false }
        if (($wantDns -join ',') -ne (@($Current.Dns) -join ',')) { return $false }
        return $true
    }
    # dhcp
    if (-not $Current.Dhcp) { return $false }
    if ($wantDns.Count) { return (($wantDns -join ',') -eq (@($Current.Dns) -join ',')) }
    return (-not $Current.DnsIsStatic)
}

# ---------------------------------------------------------------------------
# Windows ile konuşan kısımlar
# ---------------------------------------------------------------------------

function Get-WifiAdapter {
    param([string]$Name)
    if ($Name) { return Get-NetAdapter -Name $Name }
    # NdisPhysicalMedium 9 = Native 802.11 (Wi-Fi)
    $wifi = @(Get-NetAdapter -Physical | Where-Object { $_.NdisPhysicalMedium -eq 9 })
    if ($wifi.Count -eq 0) { throw 'Wi-Fi adaptörü bulunamadı. config.json içinde "adapter" alanını doldurun.' }
    $up = @($wifi | Where-Object Status -eq 'Up')
    if ($up.Count) { return $up[0] }
    return $wifi[0]
}

function Get-ConnectedSsid {
    param($Adapter)
    $ssid = $null
    try {
        $out = & netsh.exe wlan show interfaces 2>$null
        if ($LASTEXITCODE -eq 0 -and $out) { $ssid = Get-SsidFromNetshOutput -Lines $out -InterfaceName $Adapter.Name }
    } catch { }
    if (-not $ssid) {
        # Yedek yol: Windows 11 24H2+'da netsh konum izni isteyebilir. Ağ profili adı genelde SSID'dir.
        $prof = Get-NetConnectionProfile -InterfaceIndex $Adapter.ifIndex -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($prof) { $ssid = $prof.Name }
    }
    return $ssid
}

function Get-CurrentIPv4State {
    param([int]$IfIndex)
    $iface = Get-NetIPInterface -InterfaceIndex $IfIndex -AddressFamily IPv4
    $addrs = @(Get-NetIPAddress -InterfaceIndex $IfIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue |
               Where-Object { $_.IPAddress -notlike '169.254.*' })
    $gw = Get-NetRoute -InterfaceIndex $IfIndex -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue |
          Sort-Object RouteMetric | Select-Object -First 1
    $dns = Get-DnsClientServerAddress -InterfaceIndex $IfIndex -AddressFamily IPv4

    # DNS'in elle mi girildiğini kayıt defterinden anlarız ("NameServer" doluysa statik).
    $dnsStatic = $false
    try {
        $guid = (Get-NetAdapter -InterfaceIndex $IfIndex).InterfaceGuid
        $key = "HKLM:\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Interfaces\$guid"
        $ns = (Get-ItemProperty -Path $key -Name NameServer -ErrorAction SilentlyContinue).NameServer
        $dnsStatic = [bool]$ns
    } catch { }

    [pscustomobject]@{
        Dhcp         = ($iface.Dhcp -eq 'Enabled')
        IPs          = @($addrs | ForEach-Object IPAddress)
        PrefixLength = if ($addrs.Count) { $addrs[0].PrefixLength } else { $null }
        Gateway      = if ($gw) { $gw.NextHop } else { $null }
        Dns          = @($dns.ServerAddresses)
        DnsIsStatic  = $dnsStatic
    }
}

function Set-StaticIPv4 {
    param([int]$IfIndex, $Settings)
    Set-NetIPInterface -InterfaceIndex $IfIndex -AddressFamily IPv4 -Dhcp Disabled
    Get-NetIPAddress -InterfaceIndex $IfIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Remove-NetIPAddress -Confirm:$false -ErrorAction SilentlyContinue
    Remove-NetRoute -InterfaceIndex $IfIndex -DestinationPrefix '0.0.0.0/0' -Confirm:$false -ErrorAction SilentlyContinue
    New-NetIPAddress -InterfaceIndex $IfIndex -AddressFamily IPv4 -IPAddress $Settings.ip `
        -PrefixLength ([int]$Settings.prefixLength) -DefaultGateway $Settings.gateway | Out-Null
    if ($Settings.PSObject.Properties['dns'] -and $Settings.dns) {
        Set-DnsClientServerAddress -InterfaceIndex $IfIndex -ServerAddresses @($Settings.dns)
    } else {
        Set-DnsClientServerAddress -InterfaceIndex $IfIndex -ResetServerAddresses
    }
}

function Set-DhcpIPv4 {
    param([int]$IfIndex, [string]$Alias, $Settings)
    # Elle girilmiş IP ve ağ geçidini temizle, sonra DHCP'yi aç
    Get-NetIPAddress -InterfaceIndex $IfIndex -AddressFamily IPv4 -PrefixOrigin Manual -ErrorAction SilentlyContinue |
        Remove-NetIPAddress -Confirm:$false -ErrorAction SilentlyContinue
    Get-NetRoute -InterfaceIndex $IfIndex -DestinationPrefix '0.0.0.0/0' -ErrorAction SilentlyContinue |
        Where-Object { $_.Protocol -ne 'Dhcp' } |
        Remove-NetRoute -Confirm:$false -ErrorAction SilentlyContinue
    Set-NetIPInterface -InterfaceIndex $IfIndex -AddressFamily IPv4 -Dhcp Enabled
    if ($Settings.PSObject.Properties['dns'] -and $Settings.dns) {
        Set-DnsClientServerAddress -InterfaceIndex $IfIndex -ServerAddresses @($Settings.dns)
    } else {
        Set-DnsClientServerAddress -InterfaceIndex $IfIndex -ResetServerAddresses
    }
    # Hemen adres alsın
    & ipconfig.exe /renew "$Alias" | Out-Null
}

function Format-Settings {
    param($s)
    $dns = if ($s.PSObject.Properties['dns'] -and $s.dns) { (@($s.dns) -join ', ') } else { 'otomatik' }
    if ($s.mode -eq 'static') { return "STATİK $($s.ip)/$($s.prefixLength) gw $($s.gateway), DNS: $dns" }
    return "DHCP, DNS: $dns"
}

function Format-State {
    param($c)
    $mode = if ($c.Dhcp) { 'DHCP' } else { 'STATİK' }
    $dnsKind = if ($c.DnsIsStatic) { 'elle' } else { 'otomatik' }
    "$mode $(@($c.IPs) -join ', ')/$($c.PrefixLength) gw $($c.Gateway), DNS ($dnsKind): $(@($c.Dns) -join ', ')"
}

# ---------------------------------------------------------------------------
# Ana akış
# ---------------------------------------------------------------------------
function Invoke-Switcher {
    $config  = Read-SwitcherConfig -Path $ConfigPath
    $adName  = if ($config.PSObject.Properties['adapter']) { [string]$config.adapter } else { '' }
    $adapter = Get-WifiAdapter -Name $adName

    # Olay tetikleyicisi bağlantıdan hemen sonra çalışır; Windows'un arayüzü hazırlaması için kısa bekleme.
    $wait = 0
    if (-not $Status -and $config.PSObject.Properties['startupDelaySeconds']) { $wait = [int]$config.startupDelaySeconds }
    if ($wait -gt 0) { Start-Sleep -Seconds $wait }

    $ssid    = Get-ConnectedSsid -Adapter $adapter
    $current = Get-CurrentIPv4State -IfIndex $adapter.ifIndex

    if ($Status) {
        $p = Resolve-NetworkProfile -Config $config -Ssid $ssid
        Write-Host "Adaptör      : $($adapter.Name) ($($adapter.InterfaceDescription))"
        Write-Host "Bağlı SSID   : $(if ($ssid) { $ssid } else { '(bağlı değil)' })"
        Write-Host "Profil       : $(if ($p) { $p.Name } else { '(yok)' })"
        if ($p) { Write-Host "İstenen      : $(Format-Settings $p.Settings)" }
        Write-Host "Şu anki      : $(Format-State $current)"
        if ($p) { Write-Host "Durum uygun  : $(if (Test-StateMatches $p.Settings $current) { 'evet' } else { 'hayır' })" }
        return
    }

    if (-not $ssid) { Write-Log 'Wi-Fi bağlı değil; hiçbir şey yapılmadı.'; return }

    $match = Resolve-NetworkProfile -Config $config -Ssid $ssid
    if (-not $match) { Write-Log "'$ssid' için profil yok ve 'default' tanımlı değil; dokunulmadı."; return }

    if (Test-StateMatches $match.Settings $current) {
        Write-Log "'$ssid' -> $($match.Name): ayarlar zaten doğru."
        return
    }

    $target = Format-Settings $match.Settings
    if ($DryRun) {
        Write-Log "[DENEME] '$ssid' -> $($match.Name): $(Format-State $current)  ==>  $target"
        return
    }

    Write-Log "'$ssid' -> $($match.Name): $target uygulanıyor (önceki: $(Format-State $current))"
    if ($match.Settings.mode -eq 'static') {
        Set-StaticIPv4 -IfIndex $adapter.ifIndex -Settings $match.Settings
    } else {
        Set-DhcpIPv4 -IfIndex $adapter.ifIndex -Alias $adapter.Name -Settings $match.Settings
    }
    Write-Log 'Tamam.'
}

# Testler betiği nokta-kaynak (dot-source) olarak yükleyince ana akış çalışmasın.
if ($MyInvocation.InvocationName -ne '.') {
    try { Invoke-Switcher }
    catch { Write-Log $_.Exception.Message 'ERROR'; exit 1 }
}
