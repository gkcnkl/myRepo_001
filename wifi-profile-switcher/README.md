# Wi-Fi Profil Değiştirici (Windows)

Windows'ta "Adaptör ayarları"ndaki IP, ağ geçidi ve DNS bilgileri **ağa göre değil adaptöre göre** saklanır.
Evde statik IP kullanıp kafeye geçtiğinizde her seferinde elle değiştirmeniz gerekir.
Bu araç bunu otomatik yapar: hangi Wi-Fi'ye (SSID) bağlandığınıza bakar ve o ağ için tanımladığınız ayarları uygular.

- Her Wi-Fi bağlantısında, bilgisayar açılışında ve oturum açılışında otomatik çalışır.
- Tanımlamadığınız ağlarda `default` profil (normalde DHCP) uygulanır; böylece evin statik IP'si başka ağda kalmaz.
- Ayarlar zaten doğruysa hiçbir şeye dokunmaz, bağlantıyı gereksiz yere koparmaz.
- Ek program gerekmez; Windows 10/11'deki PowerShell yeterli.

## Kurulum

1. Bu klasörü bilgisayarınıza indirin.
2. `config.example.json` dosyasını `config.json` olarak kopyalayıp kendi ağlarınıza göre düzenleyin (aşağıya bakın).
3. Başlat menüsünde **PowerShell**'e sağ tıklayıp **Yönetici olarak çalıştır**'ı seçin ve:

```powershell
cd "C:\indirdiğiniz\yer\wifi-profile-switcher"
```

```powershell
powershell -ExecutionPolicy Bypass -File .\Install.ps1
```

Kurulum dosyaları `C:\ProgramData\WifiProfileSwitcher` klasörüne kopyalar ve `WifiProfileSwitcher`
adında bir zamanlanmış görev oluşturur. Kurulumdan sonra ayarları değiştirmek için
`C:\ProgramData\WifiProfileSwitcher\config.json` dosyasını (yönetici olarak) düzenleyin; yeniden kurmanız gerekmez.

## Yapılandırma (`config.json`)

```json
{
  "adapter": "",
  "startupDelaySeconds": 3,
  "networks": {
    "EvWifi": {
      "mode": "static",
      "ip": "192.168.1.50",
      "prefixLength": 24,
      "gateway": "192.168.1.1",
      "dns": ["192.168.1.1", "1.1.1.1"]
    },
    "Telefonum": { "mode": "dhcp", "dns": ["1.1.1.1", "8.8.8.8"] }
  },
  "default": { "mode": "dhcp" }
}
```

| Alan | Anlamı |
|---|---|
| `adapter` | Adaptör adı (ör. `"Wi-Fi"`). Boş bırakılırsa Wi-Fi adaptörü otomatik bulunur. |
| `startupDelaySeconds` | Bağlantıdan sonra ayar uygulamadan önce kaç saniye beklensin. |
| `networks` | Anahtar = Wi-Fi adı (SSID), **büyük/küçük harf birebir** aynı yazılmalı. |
| `mode` | `"static"` (elle IP) veya `"dhcp"` (otomatik IP). |
| `ip`, `gateway` | Sadece `static` için. |
| `prefixLength` | Alt ağ maskesi: `255.255.255.0` = `24`, `255.255.254.0` = `23`, `255.255.0.0` = `16`. |
| `dns` | İsteğe bağlı. Verilmezse DNS otomatik alınır. `dhcp` ile birlikte de kullanılabilir. |
| `default` | Listede olmayan her ağ için uygulanacak profil. Silerseniz bilinmeyen ağlarda hiçbir şey değişmez. |

Bağlı olduğunuz ağın tam adını görmek için: `netsh wlan show interfaces` → `SSID` satırı.

## Kullanım

Normalde hiçbir şey yapmanız gerekmez. Kontrol için (yönetici PowerShell'de):

```powershell
& "C:\ProgramData\WifiProfileSwitcher\Switch-WifiProfile.ps1" -Status
```

```powershell
& "C:\ProgramData\WifiProfileSwitcher\Switch-WifiProfile.ps1" -DryRun
```

`-Status` bağlı SSID'yi, seçilen profili ve adaptörün şu anki ayarlarını gösterir.
`-DryRun` hiçbir şeyi değiştirmeden ne yapılacağını yazar. Parametresiz çalıştırırsanız ayarları hemen uygular.

Yapılan her değişiklik `C:\ProgramData\WifiProfileSwitcher\switcher.log` dosyasına yazılır.

## Kaldırma

```powershell
powershell -ExecutionPolicy Bypass -File .\Uninstall.ps1 -ResetToDhcp
```

`-ResetToDhcp` Wi-Fi adaptörünü otomatik IP/DNS'e döndürür. `-KeepConfig` ile `config.json` saklanır.

## Sorun giderme

- **Hiçbir şey olmuyor:** Görev Zamanlayıcı'da `WifiProfileSwitcher` görevinin "Son çalışma sonucu"na ve `switcher.log`'a bakın.
- **SSID bulunamıyor (Windows 11 24H2+):** `netsh wlan` konum izni isteyebilir. Araç bu durumda Windows'un ağ profili adını kullanır;
  bu ad bazen SSID'den farklıdır (ör. `EvWifi 2`). `-Status` çıktısındaki "Bağlı SSID" değerini `config.json`'a aynen yazın
  ya da Ayarlar → Gizlilik → Konum'u açın.
- **Statik IP verdim, internet yok:** IP'nin o ağın aralığında ve başka cihazla çakışmadığından, ağ geçidinin doğru olduğundan emin olun.

## Testler

Saf mantık (SSID ayrıştırma, profil seçimi, yapılandırma doğrulama) [Pester](https://pester.dev) 5 ile test edilir:

```powershell
Invoke-Pester .\Tests
```
