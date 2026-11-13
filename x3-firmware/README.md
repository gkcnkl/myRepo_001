# Xteink X3 — "Bugün" özel firmware

Günlük brifing, 25 dakikalık odak sayacı, kitap rafı ve e-kitap okuyucu,
"Sabah Brifingi" tasarım diliyle (Playfair Display başlıklar, büyük harf
etiketler, ince ayraçlar). Brifing SD karttaki `brifing.txt` dosyasından,
kitaplar `/kitaplar` klasöründen okunur; ikisi de telefondan Wi-Fi ile
güncellenir. Uykuda ekranda bir kütüphane ödünç kartı durur.

![Ekranlar](release/x3-bugun-preview.png)

## Kullanım

| Ekran        | Tuş              | Ne yapar                              |
|--------------|------------------|---------------------------------------|
| Liste        | YUKARI / AŞAĞI   | Görev seç                             |
| Liste        | TAMAM            | Açık → bitti → iptal → açık (SD'ye yazar; iptal `[-]`, sayaca girmez)|
| Liste        | SAĞ              | Seçili görev için 25 dk odak başlat   |
| Liste        | GERİ             | Kitaplık (raf)                        |
| Liste        | SOL              | Eşitleme ayarlıysa hemen eşitle (kayıtlı ev/telefon Wi-Fi'ı); ayarlı değilse Wi-Fi aktarım ekranı |
| Liste        | GÜÇ              | Uyku (3 dk dokunulmazsa kendiliğinden)|
| Kitaplık     | SOL / SAĞ        | Önceki / sonraki kitap                |
| Kitaplık     | YUKARI / AŞAĞI   | Üst / alt raf                         |
| Kitaplık     | TAMAM            | Kitabı aç (kaldığın yerden); "+ Kitap ekle" sırtında Wi-Fi |
| Kitaplık     | GERİ             | Brifinge dön                          |
| Okuyucu      | SAĞ / AŞAĞI      | Sonraki sayfa                         |
| Okuyucu      | SOL / YUKARI     | Önceki sayfa                          |
| Okuyucu      | GERİ             | Rafa dön (yer kaydedilir)             |
| Wi-Fi        | GERİ             | Wi-Fi'ı kapat, geldiğin ekrana dön    |
| Odak         | TAMAM            | Duraklat / devam et                   |
| Odak         | GERİ             | İptal, listeye dön                    |
| Süre doldu   | TAMAM / GERİ     | Görevi bitti işaretle / işaretleme    |

Tuşların cihazdaki yerleri: [docs-hardware.md](docs-hardware.md).

## GitHub ile eşitleme (Sabah Brifingi)

Cihaz uyanınca (son eşitlemeden en az 30 dakika geçtiyse) ya da listede SOL'a
basınca (cihazın kendi Wi-Fi ağı açılmadan) kayıtlı Wi-Fi'a bağlanır ve özel bir GitHub deposuyla
eşitlenir (iki ağ kaydedilebilir: ev ve telefon; cihaz çevrede hangisi
varsa, ikisi de varsa sinyali güçlü olana bağlanır):

- `brifing.txt` (Brifing → X3 rutini yazar) indirilir; cihazda tamamlanan ya
  da iptal edilen görevler (aynı saat + metin) yeni listede de öyle kalır.
  Rutin `cihaz.txt`'deki işaretleri Superfocus'a geri yazar: tamamlanan
  görev/blok tamamlanır, iptal edilen görev o günün planından çıkar ve notuna
  "İptal edildi (X3 · tarih)" düşülür (kendi bloğu silinir, başkasının
  etkinliğine dokunulmaz). Aktarım sayfası (cihazın kendi Wi-Fi'ı) yalnız
  ayar değiştirmek ve kitap yüklemek için gerekir: kitaplıkta "+ Kitap ekle".
- Deponun `kitaplar/` klasörü SD karttaki `/kitaplar`'a yansıtılır: yeni ya
  da değişen `.epub`/`.txt` dosyaları iner (eşitleme başına en çok 5, dosya
  başına en çok 8 MB), depodan silinen ve depodan gelmiş kitaplar cihazdan da
  silinir (okuma durumu ve sayfa önbelleğiyle). Wi-Fi ile yüklenen kitaplara
  dokunulmaz; depodan gelenlerin listesi `/kitaplar/.x3/esitlenen.txt`.
- `cihaz.txt` (cihazın gördüğü liste, işaretler ve kitapların okuma durumu)
  değiştiyse geri yazılır; rutin bir sonraki tazelemede bunu okur. Kitap
  satırları brifing ayrıştırıcısı için yorumdur:
  `# kitap | %42 | okunuyor | 2026-11-11 07:02 | Dubliners.epub`
  (`bitti` = son sayfaya gelindi, `baslanmadi` = hiç açılmadı).
- Saat NTP'den ayarlanır (UTC+3).
- Bağlantı GitHub'ın kök sertifikalarıyla doğrulanır (`src/github_roots.h`).

Kurulum: GitHub'da özel bir depo (ör. `gkcnkl/x3-sync`) ve yalnız o depoya
"Contents: Read and write" izni olan bir fine-grained anahtar. Wi-Fi aktarım
sayfasındaki **Eşitleme** bölümüne ev Wi-Fi adı/şifresi, anahtar, depo ve dal
girilir; cihaz şifreyi ve anahtarı geri göstermez. Eşitlemeden sonra cihaz
kendini yeniden başlatır (okuyucu için temiz heap); durum alt çubukta yazar
("EŞİTLENDİ 07:02 · 2 YENİ KİTAP" ya da hata).

Bültenler: `tools/newsletter/md2epub.py` ön bilgili (baslik, gonderen, tarih,
web, dil) bir markdown bülteni tek bölümlük EPUB'a çevirir; bir rutin Gmail
etiketindeki bültenleri bununla `kitaplar/`'a koyar, `cihaz.txt`'de `bitti`
görünenlerin notunu yazar ve 7 gün sonra depodan siler.

## Kitaplık ve okuyucu

- Raf: kitapların sırtları yan yana, başlık sırtta aşağıdan yukarıya. Sırt
  kalınlığı kitabın boyutuna göre; okunan kitaplarda ayraç kurdelesi var,
  seçili sırt siyah. Altta seçili kitabın adı, yazarı, yüzdesi ve başlama günü.
- Okuyucu: EPUB (DRM'siz) ve TXT; Literata (varsayılan 27 px), iki yana yaslı,
  Türkçe ve İngilizce heceleme. Okurken **TAMAM** yazı boyutunu sırayla
  değiştirir (21 · 24 · 27 · 30 · 34 px); okunan yer korunur. Boyut, satır
  aralığı ve kenar boşluğu aktarım sayfasındaki **Okuma** bölümünden de seçilir
  (NVS'de saklanır, her kombinasyonun sayfa önbelleği ayrıdır). Altta bölüm adı, sayfa ve yüzde. Her kitabın kaldığı
  yer, ilk açıldığı gün ve son okunma zamanı `/kitaplar/.x3/<id>/durum.txt`
  dosyasında; sayfa önbelleği de aynı klasörde (ilk açılışta bölüm başına
  birkaç saniye sürer).
- Uyku: "GK'nin Kitaplığı" ödünç kartı; okunan kitaplar en son okunandan
  başlayarak, başlama tarihi damgası ve yüzdesiyle.

## Wi-Fi aktarım

Listede **SOL** tuşuna bas (ya da kitaplıkta "+ Kitap ekle" sırtını seç). Cihaz kendi Wi-Fi ağını açar (`GK's Xteink X3`, şifre
her seferinde yeni, ekranda yazar) ve bir QR kod gösterir:

1. Telefon kamerasıyla QR kodu okut (ya da ağa elle bağlan).
2. Tarayıcıda `http://192.168.4.1` adresini aç.
3. Sayfa açılınca cihazın saati telefonun saatiyle ayarlanır.
4. **Kitaplar**: "Kitap ekle" ile bir ya da birkaç EPUB/TXT yükle, istemediğini sil.
   "Cihaza uygun hale getir" (varsayılan açık) EPUB'ı yüklemeden önce tarayıcıda
   düzenler: gömülü fontları ve `@font-face` tanımlarını siler, resimleri en çok
   528×792'ye küçültüp gri tonlu yapar, EPUB kurallarına uygun yeniden paketler
   (Dubliners: 2,9 MB → 249 KB). İnternet gerekmez (tarayıcının kendi
   CompressionStream/DecompressionStream'i).
5. **Okuma**: yazı boyutu, satır aralığı, kenar boşluğu; örnek paragraf ekran
   ölçeğinde önizler. Bir sonraki kitap açılışında geçerli olur.
6. `brifing.txt` içeriğini düzenle, yapıştır ya da dosyadan yükle; **Kaydet**.
   Cihaz SD karta yazar ve ekranı yeniler.

Ev Wi-Fi şifresi gerekmez. GERİ ile ya da 10 dakika kullanılmazsa kapanır.

## brifing.txt

SD kartın kök dizininde, UTF-8 (yoksa eski adı `gorevler.txt` okunur; ikisi de
yoksa örnek dosya oluşturulur). En fazla 30 görev, 3 gösterge, 4 bölüm:

```
# Bu satır yorum
ust: Günlük brifing
baslik: Sabah Brifingi
alt: Sprint haftası H6
gosterge: Hava | 12 °C | parçalı bulutlu
gosterge: Enerji | 70 · dengeli | HRV 40 ms
## Günün akışı
[x] 09:00-09:30 | Sabah e-postaları
[ ] 10:00-11:00 | Raporun ilk taslağı
## Bugün due
[ ] Toplantıya hazırlan
```

- `ust`, `baslik`, `alt`: başlığın üstü, başlık, tarihin yanındaki not.
- `gosterge`: `etiket | değer | ayrıntı` (ayrıntı isteğe bağlı).
- `## Ad`: bölüm başlığı; `[x]` tamamlanmış, `[ ]` açık; `saat | metin`
  saat sütununa yazılır.

## Tarih ve pil

Başlığın altında tarih ("SALI, 10 KASIM"), sağ üstte tamamlanan görev sayısı
ve pil yüzdesi görünür (şarj olurken "ŞARJ" yazar). Tarih cihazdaki saat çipinden (DS3231) okunur.
Saati ayarlamanın en kolay yolu Wi-Fi aktarım sayfasını açmak. Kart
okuyucun varsa SD kartın köküne `saat.txt` da koyabilirsin, içine şu anki
saati yaz:

```
10.11.2026 15:30
```

Cihaz bir sonraki açılışta saati ayarlar ve dosyayı siler. Dosya silinmediyse
biçim anlaşılmamıştır (`2026-11-10 15:30` biçimi de kabul edilir).

## Hazır imajı yükleme (Windows)

**Güncelleme** (cihazda zaten bu yazılım varsa): yalnız uygulamayı yazar;
eşitleme ayarları, Wi-Fi şifreleri ve okuma ayarları (NVS, 0x9000) korunur.
`release/x3-bugun-v0.15.0-guncelleme.bin` dosyasını esptool klasörüne kopyala:

```
.\esptool --port COM14 --baud 921600 write-flash 0x10000 x3-bugun-v0.15.0-guncelleme.bin
```

**İlk kurulum** (orijinal yazılımdan geçerken): tek parça imaj 0x0'dan yazılır
ve NVS'yi de siler, ayarlar yeniden girilir.

```
.\esptool --port COM14 --baud 921600 write-flash 0 x3-bugun-v0.15.0-ilk-kurulum.bin
```

## Fabrika firmware'ine dönüş

```bash
esptool --port COM14 --baud 921600 write-flash 0 x3-yedek.bin
```

## Geliştirme

Donanım katmanı [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) (MIT),
repoya dahil değil:

```bash
git clone https://github.com/Free-Ink/freeink-sdk.git freeink-sdk
git -C freeink-sdk checkout 5095ddddb301f92d40dcb884db8fb436446022ea
pio run -e x3
```

Derleme öncesi `tools/patch_sdk.py` SDK'da tek bir satırı ayarlanabilir yapar
(Small profilin sayfa başına 304 metin parçası sınırı; iki yana yaslı 21 px
sayfa ~400 ister, `FREEINK_BOOK_MAX_RUNS_PER_PAGE=448`). Betik tekrar
çalıştırılabilir; SDK sürümü değişirse durur.

- `src/app.cpp` — ekran durumları ve tuş mantığı (donanımdan bağımsız)
- `src/tasks.cpp` — `brifing.txt` okuma/yazma
- `src/ui.cpp` — ekran çizimi; `src/text.cpp` — UTF-8 → font kodları
- `src/clock.cpp` — Türkçe tarih, `saat.txt` okuma
- `src/webpage.h` — Wi-Fi aktarım sayfası (HTML)
- `src/library.cpp` — raf modeli, kitap durum dosyası, sırt yerleşimi
- `src/reader/` — okuma motoru bağlantısı (FreeInkBook, düşük bellek profili),
  gömülü Literata fontları ve Türkçe heceleme (`tools/fontgen/build_book_fonts.sh`)
- `src/sd_storage.h` — kitap ve sayfa önbelleği için SD adaptörleri
- `src/sync.cpp` — GitHub eşitlemesi (Wi-Fi, NTP, HTTPS); `src/sync_logic.cpp` — birleştirme, base64, JSON
- Okuyucu simülatörü (PC'de gerçek bir kitabı cihazdaki gibi sayfalar):
  `tools/reader-sim/build.sh /tmp/rs && /tmp/rs/reader-sim kitap.epub /tmp/rs/cache /tmp/rs/out 5`
- `src/fonts/` — Inter ve Playfair Display (SIL OFL 1.1) ile üretildi: `tools/fontgen/build_fonts.sh`
- Testler (PC'de): `tools/test/run.sh`
- Ekran önizlemesi (PC'de, cihazdaki çizim koduyla aynı): `tools/preview/build.sh /tmp/x3-preview [brifing.txt]`

## Üçüncü taraf içerik

- [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) (MIT): donanım katmanı ve FreeInkBook okuma motoru.
- Literata, Inter, Playfair Display: SIL Open Font License 1.1 (Google Fonts / rsms).
- Türkçe heceleme kalıpları: [hyph-utf8](https://github.com/hyphenation/tex-hyphen) `hyph-tr`, kendi lisansıyla.
