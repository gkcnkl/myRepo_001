# Xteink X3 — "Bugün" özel firmware

Günlük brifing + 25 dakikalık odak sayacı, "Sabah Brifingi" tasarım
diliyle (Playfair Display başlıklar, büyük harf etiketler, ince ayraçlar).
Brifing SD karttaki `brifing.txt` dosyasından okunur ve telefondan Wi-Fi ile
güncellenebilir; işaretlediğin görevler aynı dosyaya geri yazılır.

![Ekranlar](release/x3-bugun-preview.png)

## Kullanım

| Ekran        | Tuş              | Ne yapar                              |
|--------------|------------------|---------------------------------------|
| Liste        | YUKARI / AŞAĞI   | Görev seç                             |
| Liste        | TAMAM            | Bitti / bitmedi işaretle (SD'ye yazar)|
| Liste        | SAĞ              | Seçili görev için 25 dk odak başlat   |
| Liste        | GERİ             | Ekranı tazele (e-ink gölgelerini sil) |
| Liste        | SOL              | Wi-Fi aktarım ekranı                  |
| Liste        | GÜÇ              | Uyku (3 dk dokunulmazsa kendiliğinden)|
| Wi-Fi        | GERİ             | Wi-Fi'ı kapat, listeye dön            |
| Odak         | TAMAM            | Duraklat / devam et                   |
| Odak         | GERİ             | İptal, listeye dön                    |
| Süre doldu   | TAMAM / GERİ     | Görevi bitti işaretle / işaretleme    |

Tuşların cihazdaki yerleri: [docs-hardware.md](docs-hardware.md).

## Wi-Fi aktarım

Listede **SOL** tuşuna bas. Cihaz kendi Wi-Fi ağını açar (`GK's Xteink X3`, şifre
her seferinde yeni, ekranda yazar) ve bir QR kod gösterir:

1. Telefon kamerasıyla QR kodu okut (ya da ağa elle bağlan).
2. Tarayıcıda `http://192.168.4.1` adresini aç.
3. Sayfa açılınca cihazın saati telefonun saatiyle ayarlanır.
4. `brifing.txt` içeriğini düzenle, yapıştır ya da dosyadan yükle; **Kaydet**.
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

`release/x3-bugun-v0.8.1.bin` dosyasını esptool klasörüne kopyala, sonra:

```bash
esptool --port COM14 --baud 921600 write-flash 0 x3-bugun-v0.8.1.bin
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

- `src/app.cpp` — ekran durumları ve tuş mantığı (donanımdan bağımsız)
- `src/tasks.cpp` — `brifing.txt` okuma/yazma
- `src/ui.cpp` — ekran çizimi; `src/text.cpp` — UTF-8 → font kodları
- `src/clock.cpp` — Türkçe tarih, `saat.txt` okuma
- `src/webpage.h` — Wi-Fi aktarım sayfası (HTML)
- `src/fonts/` — Inter ve Playfair Display (ikisi de SIL OFL 1.1) ile üretildi: `tools/fontgen/build_fonts.sh`
- Testler (PC'de): `tools/test/run.sh`
- Ekran önizlemesi (PC'de, cihazdaki çizim koduyla aynı): `tools/preview/build.sh /tmp/x3-preview [brifing.txt]`
