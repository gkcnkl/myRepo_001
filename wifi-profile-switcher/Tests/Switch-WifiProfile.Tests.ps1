# Pester 5 testleri — Windows gerektirmeyen saf mantığı sınar.
#   Invoke-Pester .\Tests

BeforeAll {
    . (Join-Path $PSScriptRoot '..\Switch-WifiProfile.ps1')
}

Describe 'Get-SsidFromNetshOutput' {
    It 'İngilizce çıktıdan SSID alır, BSSID satırını atlar' {
        $out = @(
            'There is 1 interface on the system:',
            '',
            '    Name                   : Wi-Fi',
            '    State                  : connected',
            '    SSID                   : EvWifi',
            '    BSSID                  : aa:bb:cc:dd:ee:ff'
        )
        Get-SsidFromNetshOutput -Lines $out -InterfaceName 'Wi-Fi' | Should -Be 'EvWifi'
    }
    It 'Türkçe çıktıdan SSID alır (boşluklu ad)' {
        $out = @(
            '    Ad                     : Wi-Fi',
            '    Durum                  : bağlandı',
            '    BSSID                  : aa:bb:cc:dd:ee:ff',
            '    SSID                   : Kafe Misafir'
        )
        Get-SsidFromNetshOutput -Lines $out -InterfaceName 'Wi-Fi' | Should -Be 'Kafe Misafir'
    }
    It 'Bağlı değilse $null döner' {
        $out = @('    Name                   : Wi-Fi', '    State                  : disconnected')
        Get-SsidFromNetshOutput -Lines $out -InterfaceName 'Wi-Fi' | Should -BeNullOrEmpty
    }
    It 'Birden çok arayüzde doğru olanı seçer' {
        $out = @(
            '    Name                   : Wi-Fi 2', '    SSID                   : Baska',
            '    Name                   : Wi-Fi',   '    SSID                   : EvWifi'
        )
        Get-SsidFromNetshOutput -Lines $out -InterfaceName 'Wi-Fi' | Should -Be 'EvWifi'
    }
}

Describe 'Resolve-NetworkProfile' {
    BeforeAll {
        $cfg = '{"networks":{"EvWifi":{"mode":"static","ip":"192.168.1.50","prefixLength":24,"gateway":"192.168.1.1"}},"default":{"mode":"dhcp"}}' | ConvertFrom-Json
    }
    It 'Tanımlı SSID için kendi profilini seçer' {
        (Resolve-NetworkProfile -Config $cfg -Ssid 'EvWifi').Name | Should -Be 'EvWifi'
    }
    It 'Bilinmeyen SSID için default seçer' {
        (Resolve-NetworkProfile -Config $cfg -Ssid 'Kafe').Name | Should -Be '(default)'
    }
    It 'Eşleşme büyük/küçük harfe duyarlı' {
        (Resolve-NetworkProfile -Config $cfg -Ssid 'evwifi').Name | Should -Be '(default)'
    }
    It 'Default yoksa $null döner' {
        $c = '{"networks":{}}' | ConvertFrom-Json
        Resolve-NetworkProfile -Config $c -Ssid 'X' | Should -BeNullOrEmpty
    }
}

Describe 'Test-ProfileSettings' {
    It 'Geçerli static profil hata vermez' {
        $s = '{"mode":"static","ip":"10.0.0.5","prefixLength":24,"gateway":"10.0.0.1","dns":["1.1.1.1"]}' | ConvertFrom-Json
        @(Test-ProfileSettings $s 'x').Count | Should -Be 0
    }
    It 'Eksik ve hatalı alanları bildirir' {
        $s = '{"mode":"static","ip":"10.0.0.300","prefixLength":40}' | ConvertFrom-Json
        $e = Test-ProfileSettings $s 'x'
        ($e -join "`n") | Should -Match 'gateway'
        ($e -join "`n") | Should -Match 'geçersiz ip'
        ($e -join "`n") | Should -Match 'prefixLength'
    }
    It 'Bilinmeyen modu reddeder' {
        $s = '{"mode":"auto"}' | ConvertFrom-Json
        @(Test-ProfileSettings $s 'x').Count | Should -Be 1
    }
}

Describe 'Test-StateMatches' {
    BeforeAll {
        $static = '{"mode":"static","ip":"192.168.1.50","prefixLength":24,"gateway":"192.168.1.1","dns":["1.1.1.1","8.8.8.8"]}' | ConvertFrom-Json
        $dhcp   = '{"mode":"dhcp"}' | ConvertFrom-Json
    }
    It 'Aynı statik ayarları eşleşmiş sayar' {
        $cur = [pscustomobject]@{ Dhcp = $false; IPs = @('192.168.1.50'); PrefixLength = 24; Gateway = '192.168.1.1'; Dns = @('1.1.1.1', '8.8.8.8'); DnsIsStatic = $true }
        Test-StateMatches $static $cur | Should -BeTrue
    }
    It 'DHCP açıkken statik istenirse eşleşmez' {
        $cur = [pscustomobject]@{ Dhcp = $true; IPs = @('192.168.1.50'); PrefixLength = 24; Gateway = '192.168.1.1'; Dns = @('1.1.1.1', '8.8.8.8'); DnsIsStatic = $true }
        Test-StateMatches $static $cur | Should -BeFalse
    }
    It 'DHCP + otomatik DNS istenip DNS elle ayarlıysa eşleşmez' {
        $cur = [pscustomobject]@{ Dhcp = $true; IPs = @('10.0.0.7'); PrefixLength = 24; Gateway = '10.0.0.1'; Dns = @('1.1.1.1'); DnsIsStatic = $true }
        Test-StateMatches $dhcp $cur | Should -BeFalse
    }
    It 'DHCP + otomatik DNS zaten öyleyse eşleşir' {
        $cur = [pscustomobject]@{ Dhcp = $true; IPs = @('10.0.0.7'); PrefixLength = 24; Gateway = '10.0.0.1'; Dns = @('10.0.0.1'); DnsIsStatic = $false }
        Test-StateMatches $dhcp $cur | Should -BeTrue
    }
}
