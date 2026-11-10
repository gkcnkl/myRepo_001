# Xteink X3 — "Bugün" özel firmware

Günlük brifing + 25 dakikalık odak sayacı, "Sabah Brifingi" tasarım
diliyle (Playfair Display başlıklar, büyük harf etiketler, ince ayraçlar).
Brifing SD karttaki `brifing.txt` dosyasından okunur; işaretlediğin görevler
aynı dosyaya geri yazılır.

![Ekranlar](release/x3-bugun-preview.png)

## Kullanım

| Ekran        | Tuş              | Ne yapar                              |
|--------------|------------------|---------------------------------------|
| Liste        | YUKARI / AŞAĞI   | Görev seç                             |
| Liste        | TAMAM            | Bitti / bitmedi işaretle (SD'ye yazar)|
| Liste        | SAĞ              | Seçili görev için 25 dk odak başlat   |
| Liste        | GERİ             | Ekranı tazele (e-ink gölgelerini sil) |
| Liste        | GÜÇ              | Uyku (3 dk dokunulmazsa kendiliğinden)|
| Odak         | TAMAM            | Duraklat / devam et                   |
| Odak         | GERİ             | İptal, listeye dön                    |
| Süre doldu   | TAMAM / GERİ     | Görevi bitti işaretle / işaretleme    |

Tuşların cihazdaki yerleri: [docs-hardware.md](docs-hardware.md).

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
"Tarih ayarlı değil" yazıyorsa SD kartın köküne `saat.txt` koy, içine şu
anki saati yaz:

```
10.11.2026 15:30
```

Cihaz bir sonraki açılışta saati ayarlar ve dosyayı siler. Dosya silinmediyse
biçim anlaşılmamıştır (`2026-11-10 15:30` biçimi de kabul edilir).

## Hazır imajı yükleme (Windows)

`release/x3-bugun-v0.7.bin` dosyasını esptool klasörüne kopyala, sonra:

```bash
esptool --port COM14 --baud 921600 write-flash 0 x3-bugun-v0.7.bin
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
- `src/fonts/` — Inter ve Playfair Display (ikisi de SIL OFL 1.1) ile üretildi: `tools/fontgen/build_fonts.sh`
- Testler (PC'de): `tools/test/run.sh`
- Ekran önizlemesi (PC'de, cihazdaki çizim koduyla aynı): `tools/preview/build.sh /tmp/x3-preview [brifing.txt]`
