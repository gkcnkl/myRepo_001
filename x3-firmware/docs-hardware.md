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

Cihazda ölçülen (okuyucu açılmadan önce): boş heap ~161 KB, en büyük blok
111 KB; geri kalanı daha küçük bölgelere dağınık. XML ayrıştırıcı (expat) ve
glif çizici arenaların dışında, genel heap'ten ~20 KB'a kadar bellek ister
(bilgisayarda ölçüm: 29 KB, dosya tamponları dahil). Bu yüzden okuyucu:
- sayfalama (56 KB) + ayrıştırma (50 KB) alanlarını tek 106 KB blokta alır
  (ölçülen tepe: 52 KB + 46 KB, Dubliners 290 sayfa),
- küçük alanları 28 KB'lık genel heap payı bırakacak bütçeyle planlar:
  önce zorunlu en küçükler (katalog 8, font 6, dizin 2 KB), sonra öncelikle
  font 12, katalog 16, dizin 6, gerçek kalın font 8 (yoksa çift baskı),
  CSS 2 KB (yoksa kitabın CSS'i atlanır).
Glif çizici (stb_truetype) bir harf çizerken 2000 kayıtlık kenar tablosu ister:
32 bit C3'te tek parça ~56 KB (64 bit PC'de 25 KB). Bu istek `stbtt_alloc.h`
ile genel heap yerine sayfa çizilirken boşta duran ayrıştırma alanından (+
sayfalama alanının boş kalan kısmı, ~100 KB bitişik) karşılanır; stb her
harften sonra hepsini bırakır. Kitap kapanınca hepsi bırakılır. Loop yığını 16 KB. Wi-Fi'dan çıkınca cihaz
kendini yeniden başlatır (temiz heap).
