# Bülten rutini: kurulum

claude.ai/code/routines → **New routine** (Yeni rutin):

- **Ad:** Bültenler · Gmail → X3 → Beyin
- **Zaman:** her gün 06:41 (Europe/Istanbul)
- **Depo (Repository):** `gkcnkl/x3-sync`
- **Connectors:** Gmail, Claude Docs
- **Talimat:** aşağıdaki çizginin altındaki metnin tamamı

---

Her sabah çalışan bülten görevi. Gmail'de "X3" etiketli bültenleri e-kitaba çevirip Xteink X3 okuyucunun kitaplığına koyar; cihazda sonuna kadar okunan bültenler için Beyin kasasına özetli not hazırlar; notu yazılmış bültenleri 7 gün sonra kitaplıktan kaldırır. Yeni oturum, hafızan yok. Türkçe yaz; em dash (uzun tire) hiçbir yerde kullanma. Zaman dilimi Europe/Istanbul. Onay bekleme, soru sorma.

DEPO: gkcnkl/x3-sync (özel, dal main). Bu rutine depo olarak bağlıdır; klonu çalışma klasöründe bul. Klon yoksa mcp__claude-code-remote__add_repo ile {owner: "gkcnkl", repo: "x3-sync", access: "push"} ekle ve söylediği klonlama komutunu çalıştır. Depoya erişilemezse hiçbir şeye dokunma, PushNotification ile tek satır bildir ve bitir. Depodaki README.md düzeni anlatır. Dosyalar:
- bultenler.txt: kayıt defteri. Satır biçimi: "gmail_id | dosya | eklendi | not yazıldı | silindi" (tarihler YYYY-MM-DD, boşsa "-"). "#" satırları açıklamadır.
- kitaplar/: cihazın indirdiği e-kitaplar. Cihaz buradan silinen ve depodan gelmiş kitapları kendinden de siler.
- kaynak/<gmail_id>.md: bültenin markdown hali.
- cihaz.txt: cihazın yazdığı durum. Kitap satırları: "# kitap | %<yüzde> | <okunuyor|bitti|baslanmadi> | <son okuma> | <dosya>". Dosya yoksa adım B'yi atla.
- tools/md2epub.py: dönüştürücü.

A. YENİ BÜLTENLER
1. Gmail'de label:X3 araması yap (search_threads, sayfalayarak). Her thread'in mesajlarını get_thread ile al. bultenler.txt'de gmail_id'si olmayan her mesaj yenidir. Tek çalıştırmada en fazla 8 yeni bülten işle; kalanı yarına bırak ve raporda yaz.
2. Her yeni bülten için mesajı get_message ile düz metin olarak oku ve kaynak/<gmail_id>.md dosyasını yaz:
---
baslik: <konu satırı; "Fwd:" gibi önekler atılmış>
gonderen: <gönderenin görünen adı>
tarih: <mesaj tarihi YYYY-MM-DD>
web: <metinde "View this post on the web" ya da tarayıcıda gör bağlantısı varsa o URL, yoksa satırı koyma>
dil: <metnin dili, tr ya da en>
---
   Gövde: bültenin metni BİREBİR, kısaltmadan ve yeniden yazmadan. Yalnız biçim: bölüm başlıkları "## ", alt başlıklar "### ", madde listeleri "- ", vurgular **...**. Abonelikten çık, paylaş, beğen, uygulamayı indir, takip pikseli ve yasal altbilgi satırlarını at (md2epub de bunların bir kısmını atar). Görsel gömme; görsel yerine bir şey yazma.
3. Dosya adını al: python3 -I tools/md2epub.py kaynak/<gmail_id>.md (çıktı tek satır ad). Sonra: python3 -I tools/md2epub.py kaynak/<gmail_id>.md "kitaplar/<ad>". kitaplar/ altında aynı ad varsa adın sonuna (uzantıdan önce) " (2)" ekle. Komut hata verirse o bülteni atla, raporda yaz.
4. bultenler.txt'nin sonuna "<gmail_id> | <ad> | <bugün> | - | -" satırını ekle.

B. OKUNANLARIN NOTU
5. cihaz.txt'de durumu "bitti" olan ve bultenler.txt'de "not yazıldı" alanı "-" olan her bülten için kaynak/<gmail_id>.md dosyasını oku ve not hazırla. Not, Claude Docs'taki "Tarama Gelen Kutusu" dokümanına yazılır: https://claude.ai/artifact/N9zT3zZSnXbSN3BdgL3Zfk (Claude_Docs araçlarıyla oku ve düzenle; yeni doküman açma). Hafta içi ofis bilgisayarındaki aktarma görevi "### Bülten · ..." kayıtlarını Beyin kasasında Kaynaklar/Bültenler/ klasörüne taşır.
   a. "## Aktarılacaklar" bölümünde aynı başlıkla kayıt varsa ekleme; bultenler.txt'de not alanını yine doldur.
   b. Bölümde yalnız "Kuyruk boş." satırı varsa önce o satırı kaldır.
   c. Bölümün sonuna, "## Son alan taraması" başlığından ÖNCE şu kaydı ekle:

### Bülten · <baslik>
# <baslik>

**Gönderen:** <gonderen> · **Tarih:** <tarih> · **Okundu:** <cihaz.txt'deki son okuma tarihi, YYYY-MM-DD> (X3)

**Kaynak:** <web URL'si; yoksa Gmail bağlantısı https://mail.google.com/mail/u/0/#all/<gmail_id>>

**Alan:** [[Okuma]]

## Özet

3-5 madde: bültenin ana fikirleri, yalnız metinde yazanlar.

## Öne çıkanlar

En fazla 3 kısa alıntı, metinden birebir, tırnak içinde. Metinde verilen kaynak ya da çalışma adlarını aynen yaz.

## Notlarım

(BU BÖLÜMÜ BOŞ BIRAK: başlığı koy, altına hiçbir şey yazma. Gökçen kendisi dolduracak.)

   "###" satırında baslik içindeki / \ : * ? " < > | karakterlerini "-" ile değiştir. Not içindeki başlıklar "#" ve "##" kalır (aktarma görevi bunları ayrı not olarak yazar); "###" kullanma. Özet uydurma: metinde olmayan bilgi, sayı ya da kaynak ekleme.
   d. Dokümanı yeniden oku, kaydın yerinde olduğunu doğrula; doğrulanınca bultenler.txt'de o satırın "not yazıldı" alanına bugünü yaz. "## Son alan taraması" ve "## 3GPP sürüm durumu" bölümlerine dokunma.

C. TEMİZLİK
6. bultenler.txt'de "not yazıldı" tarihi bugünden en az 7 gün önce olan ve "silindi" alanı "-" olan her satır için: kitaplar/<dosya> ve kaynak/<gmail_id>.md dosyalarını git rm ile sil ve "silindi" alanına bugünü yaz. Satırı defterden SİLME (aynı mail etiketli kaldığı için yeniden eklenmesin diye). Gmail'e dokunma: etiket kaldırma, silme, okundu işaretleme yapma.

D. KAYDET
7. Değişiklik varsa tek commit at ("Bültenler: +N yeni, N not, N silindi") ve doğrudan main dalına push et (git push origin HEAD:main). Push reddedilirse git pull --rebase origin main yapıp bir kez daha dene (cihaz cihaz.txt'yi değiştirmiş olabilir). cihaz.txt ve brifing.txt dosyalarına ASLA yazma.

Kurallar: Depoda yalnız bultenler.txt, kitaplar/ ve kaynak/ altına dokun. Gmail'de yalnız oku. Yorum, öneri yazma. Bir bülten başarısız olursa diğerleriyle devam et.

RAPOR: En az bir şey değiştiyse PushNotification ile <routine_summary> içinde tek satır: eklenen bültenlerin başlıkları, not hazırlananlar, silinenler, hata varsa hangisi ve neden. Hiçbir şey değişmediyse bildirim gönderme, sohbete "değişiklik yok" yaz.
