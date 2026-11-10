# Xteink X3 — donanım notları (cihazda doğrulandı)

- Çip: ESP32-C3 (QFN32) rev v0.4, 16 MB flash, USB-Serial/JTAG
- Ekran: 792x528 e-ink, dikey kullanımda `setRotation(3)` düz görünür
- Ekran pinleri: SCLK 8, MOSI 10, CS 21, DC 4, RST 5, BUSY 6; SD: MISO 7, CS 12, güç 13

## Tuşlar (cihaz dik tutulurken, ön yüzden bakınca)

| Konum            | Yazılımdaki tuş      |
|------------------|----------------------|
| Sağ üst          | GÜÇ (`BTN_POWER`)    |
| Sağ orta         | AŞAĞI (`BTN_DOWN`)   |
| Sağ alt, sağdaki | SAĞ (`BTN_RIGHT`)    |
| Sağ alt, soldaki | SOL (`BTN_LEFT`)     |
| Sol alt, sağdaki | TAMAM (`BTN_CONFIRM`)|
| Sol alt, soldaki | GERİ (`BTN_BACK`)    |
| Sol orta         | YUKARI (`BTN_UP`)    |
| Sol üst          | Donanım reset (yazılım okuyamaz) |

## Bellek (PSRAM yok)

Cihazda ölçülen: okuyucu açılmadan önce boş heap 157 KB, en büyük blok 111 KB
(v0.9.2, 20 KB loop yığınıyla). Okuyucu bir kitap açılınca yedi ayrı blok
ayırır ve kapanınca bırakır: sayfalama 56 KB, ayrıştırma (sıkıştırma + XML)
50 KB, katalog 16 KB, fontlar 12 + 8 KB, sayfa dizini 6 KB, CSS 2 KB; toplam
~150 KB, en büyük blok 56 KB. Loop yığını 16 KB.
Ölçülen tepe değerler (Dubliners, 290 sayfa): sayfalama 52 KB, ayrıştırma 46 KB.
Sınırlar: bölüm başına ~750 sayfa (dizin), ~16 KB katalog (çok bölümlü dev
kitaplar "çok büyük" der). Wi-Fi'dan çıkınca cihaz kendini yeniden başlatır.
