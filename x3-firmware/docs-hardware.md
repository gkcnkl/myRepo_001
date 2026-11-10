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

Cihazda ölçülen (okuyucu açılmadan önce): boş heap 157-161 KB, en büyük blok
111 KB; geri kalanı daha küçük bölgelere dağınık. Bu yüzden okuyucu:
- sayfalama (56 KB) + ayrıştırma (50 KB) alanlarını tek 106 KB blokta alır
  (ölçülen tepe: 52 KB + 46 KB, Dubliners 290 sayfa),
- küçük alanları önce tercih edilen, sığmazsa daha küçük boyutla ister:
  katalog 16→9 KB, font 12→7 KB, kalın font 8→6 KB, dizin 6→3 KB, CSS 2→0,5 KB.
Toplam 150 KB (en iyi) ile 131 KB (en sıkışık) arası; kitap kapanınca hepsi
bırakılır. Yetmezse rafta hangi alanın sığmadığı yazar. Loop yığını 16 KB.
Wi-Fi'dan çıkınca cihaz kendini yeniden başlatır (temiz heap).
