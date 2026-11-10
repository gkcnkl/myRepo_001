# Xteink X3 — özel firmware

Adım 1: "Merhaba" ekranı + buton testi. Dikey ekranda karşılama yazısı çıkar,
bastığın her tuşun adı ekranda görünür (fiziksel tuşları eşlemek için).
Ekranın üst kenarında kalın siyah bir şerit var: yön doğru mu diye kontrol et.

Donanım katmanı: [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk) (MIT).
Repoya dahil değil, derlemeden önce bu klasöre klonlanır.

## Derleme

```bash
git clone https://github.com/Free-Ink/freeink-sdk.git freeink-sdk
git -C freeink-sdk checkout 5095ddddb301f92d40dcb884db8fb436446022ea
pio run -e x3
```

## Fabrika firmware'ine dönüş

Orijinal 16 MB yedek (`x3-yedek.bin`) ile:

```bash
esptool --port COM14 --baud 921600 write-flash 0 x3-yedek.bin
```
