# Bülten rutini: talimat

https://claude.ai/code/routines/trig_014tSfZZe8wqHBGHat1zXsBd → Edit → talimat kutusunun içeriğini SİL ve çizginin altındaki metnin tamamını yapıştır. Depo: `gkcnkl/x3-sync`. Connectors: Composio (Gmail hesapları), Claude Docs.

Bülten kaynakları Composio'daki Gmail hesaplarından doğrudan okunur: desklettershq (Substack, Medium) ve gkgeziyor. Elle eklemek için: gokcenkul Gmail'de X3 etiketi.

---

Her sabah çalışan bülten görevi. desklettershq ve gkgeziyor Gmail kutularındaki bültenleri EN ESKİDEN başlayarak sırayla e-kitaba çevirip Xteink X3 okuyucunun kitaplığına koyar; cihazda aynı anda en fazla 5 okunmamış bülten bulunur, biri bitince sıradaki eklenir. Cihazda sonuna kadar okunan bültenler için Beyin kasasına özetli not hazırlar; notu yazılmış bültenleri 7 gün sonra kitaplıktan kaldırır. Yeni oturum, hafızan yok. Türkçe yaz; em dash (uzun tire) hiçbir yerde kullanma. Zaman dilimi Europe/Istanbul. Onay bekleme, soru sorma.

DEPO: gkcnkl/x3-sync (özel, dal main). Rutinin ayarlarında depo olarak seçilidir; klonu çalışma klasöründe (ör. /home/user/x3-sync) bul. Klon yoksa ve mcp__claude-code-remote__add_repo aracı varsa onunla {owner: "gkcnkl", repo: "x3-sync", access: "push"} ekle ve söylediği klonlama komutunu çalıştır. Depoya erişilemezse hiçbir şeye dokunma, sorunu tek satırla bildir (PushNotification yoksa sohbete yaz) ve bitir. Depodaki README.md düzeni anlatır. Dosyalar:
- bultenler.txt: kuyruk ve kayıt defteri. Satır biçimi: "kimlik | dosya | eklendi | not yazıldı | silindi" (tarihler YYYY-MM-DD, boşsa "-"). kimlik = <hesap>-<gmail_id>, hesap: dl = desklettershq, gk = gkgeziyor, gc = gokcenkul. Bülten olmayan mailler "kimlik | atlandi | <tarih> | - | -" satırıyla kayıtlıdır. "# imlec dl: YYYY/MM/DD" ve "# imlec gk: YYYY/MM/DD" satırları o hesapta incelenen en yeni mailin tarihidir. Diğer "#" satırları açıklamadır.
- kitaplar/: cihazın indirdiği e-kitaplar. Cihaz buradan silinen ve depodan gelmiş kitapları kendinden de siler.
- kaynak/<kimlik>.md: bültenin markdown hali.
- cihaz.txt: cihazın yazdığı durum. Kitap satırları: "# kitap | %<yüzde> | <okunuyor|bitti|baslanmadi> | <son okuma> | <dosya>". Dosya yoksa tüm bültenler okunmamış sayılır ve adım B atlanır.
- tools/md2epub.py: dönüştürücü.

GMAIL (Composio, YALNIZ OKUMA): Hesaplar: desklettershq@gmail.com (Composio hesap kimliği gmail_rib-narra, kısaltma dl), gkgeziyor@gmail.com (gmail_soft-tharf, gk), gokcenkul@gmail.com (gmail_rimu-horror, gc). Araçlar: GMAIL_LIST_THREADS (query, max_results, page_token), GMAIL_FETCH_MESSAGE_BY_THREAD_ID. Hesap kimlikleri değişmişse COMPOSIO_MANAGE_CONNECTIONS ile gmail hesaplarını listele ve e-posta adresine göre eşle. Gmail'de hiçbir şeyi değiştirme: etiket, okundu, silme, taşıma YOK. Claude Docs bağlayıcısı yoksa adım B'yi atla ve raporda belirt.

A. KUYRUĞU DOLDUR
1. Boş yer hesapla: bultenler.txt'de dosya alanı "atlandi" olmayan, silindi alanı "-" olan ve cihaz.txt'de durumu "bitti" OLMAYAN satırlar okunmamış sayılır. Boş yer = 5 - okunmamış sayısı. Boş yer 0 ya da daha azsa A'yı atla.
2. Aday topla (en eskiden yeniye):
   - dl: "(from:substack.com OR from:medium.com) after:<imlec dl>" (imleç tarihinin günü dahil olsun diye imleçten bir gün öncesini kullan).
   - gk: "unsubscribe after:<imlec gk>" (aynı şekilde bir gün öncesinden).
   - gc: "label:X3".
   Sayfalayarak listele, her thread'in mesajlarını al. Kimliği bultenler.txt'de geçen mesajları atla. Kalanları üç hesaptan birlikte mesaj tarihine göre ESKİDEN YENİYE sırala.
3. Sırayla her adayı incele, boş yer dolana kadar:
   - dl: yalnız Substack ve Medium'un yazı içeren mailleri (yeni yazı, haftalık yazı, Medium Daily Digest gibi) bülten sayılır. Hesap/yazar paneli mailleri bülten DEĞİLDİR: kurulum, giriş bağlantısı, doğrulama kodu, "shareable assets", istatistik, yeni abone, yorum/beğeni bildirimi, ödeme/fatura, ürün duyurusu ("Introducing ...", "New ... tools are here").
   - gk: yalnız bültenler (abonelikten çık bağlantısı olan, yazı/makale içeren düzenli yayın). Kişisel yazışma, sipariş, rezervasyon, fatura, kargo, güvenlik, hesap, indirim/kampanya mailleri bülten DEĞİLDİR.
   - gc (X3 etiketi): hepsi bülten sayılır.
   - Gövdesi 150 kelimeden kısa olan ya da yalnız "devamını oku" bağlantısından oluşan mailler bülten DEĞİLDİR.
   Bülten değilse bultenler.txt'ye "<kimlik> | atlandi | <bugün> | - | -" yaz ve sonrakine geç. Bültense 4-6. adımları uygula. Tek çalıştırmada en fazla 60 aday incele; boş yer dolmadıysa kalanı yarına bırak.
4. kaynak/<kimlik>.md dosyasını yaz:
---
baslik: <konu satırı; "Fwd:", "Fw:", "İlt:" gibi önekler atılmış>
gonderen: <gönderenin görünen adı>
tarih: <mesaj tarihi YYYY-MM-DD>
web: <metinde "View this post on the web" / "Read in browser" bağlantısı varsa o URL, yoksa satırı koyma>
dil: <metnin dili, tr ya da en>
---
   Elle iletilmiş mesajda ("Forwarded message" / "Original Message" bloğu ya da "From:/Date:/Subject:" satırları) gonderen, tarih ve baslik bu bloktaki asıl bültenden alınır; iletme başlık bloğu ve iletenin imzası gövdeye girmez.
   Gövde: bültenin metni BİREBİR, kısaltmadan ve yeniden yazmadan. Yalnız biçim: bölüm başlıkları "## ", alt başlıklar "### ", madde listeleri "- ", vurgular **...**. Abonelikten çık, paylaş, beğen, uygulamayı indir, takip pikseli ve yasal altbilgi satırlarını at. Görsel gömme; görsel yerine bir şey yazma. Medium Daily Digest gibi yalnız başlık ve kısa özet listesi olan mailler olduğu gibi (başlık + özet + bağlantı) yazılır.
5. Dosya adını al: python3 -I tools/md2epub.py kaynak/<kimlik>.md (çıktı tek satır ad). Sonra: python3 -I tools/md2epub.py kaynak/<kimlik>.md "kitaplar/<ad>". kitaplar/ altında aynı ad varsa adın sonuna (uzantıdan önce) " (2)" ekle. Komut hata verirse o bülteni atla, satırı "atlandi" olarak yaz ve raporda belirt.
6. bultenler.txt'ye "<kimlik> | <ad> | <bugün> | - | -" satırını ekle.
7. İmleçleri güncelle: her hesap için bu çalıştırmada incelenen (eklenen ya da atlanan) en yeni mailin tarihini "# imlec <hesap>: YYYY/MM/DD" satırına yaz. Hiç mail incelenmediyse imleç değişmez.

B. OKUNANLARIN NOTU
8. cihaz.txt'de durumu "bitti" olan ve bultenler.txt'de "not yazıldı" alanı "-" olan her bülten için kaynak/<kimlik>.md dosyasını oku ve not hazırla. Not, Claude Docs'taki "Tarama Gelen Kutusu" dokümanına yazılır: https://claude.ai/artifact/N9zT3zZSnXbSN3BdgL3Zfk (Claude_Docs araçlarıyla oku ve düzenle; yeni doküman açma). Hafta içi ofis bilgisayarındaki aktarma görevi "### Bülten · ..." kayıtlarını Beyin kasasında Kaynaklar/Bültenler/ klasörüne taşır.
   a. "## Aktarılacaklar" bölümünde aynı başlıkla kayıt varsa ekleme; bultenler.txt'de not alanını yine doldur.
   b. Bölümde yalnız "Kuyruk boş." satırı varsa önce o satırı kaldır.
   c. Bölümün sonuna, "## Son alan taraması" başlığından ÖNCE şu kaydı ekle:

### Bülten · <baslik>
# <baslik>

**Gönderen:** <gonderen> · **Tarih:** <tarih> · **Okundu:** <cihaz.txt'deki son okuma tarihi, YYYY-MM-DD> (X3)

**Kaynak:** <web URL'si; yoksa "Gmail · <hesabın e-posta adresi>">

**Alan:** [[Okuma]]

## Özet

3-5 madde: bültenin ana fikirleri, yalnız metinde yazanlar.

## Öne çıkanlar

En fazla 3 kısa alıntı, metinden birebir, tırnak içinde. Metinde verilen kaynak ya da çalışma adlarını aynen yaz.

## Notlarım

(BU BÖLÜMÜ BOŞ BIRAK: başlığı koy, altına hiçbir şey yazma. Gökçen kendisi dolduracak.)

   "###" satırında baslik içindeki / \ : * ? " < > | karakterlerini "-" ile değiştir. Not içindeki başlıklar "#" ve "##" kalır; "###" kullanma. Özet uydurma: metinde olmayan bilgi, sayı ya da kaynak ekleme.
   d. Dokümanı yeniden oku, kaydın yerinde olduğunu doğrula; doğrulanınca bultenler.txt'de o satırın "not yazıldı" alanına bugünü yaz. "## Son alan taraması" ve "## 3GPP sürüm durumu" bölümlerine dokunma.

C. TEMİZLİK
9. bultenler.txt'de "not yazıldı" tarihi bugünden en az 7 gün önce olan ve "silindi" alanı "-" olan her satır için: kitaplar/<dosya> ve kaynak/<kimlik>.md dosyalarını git rm ile sil ve "silindi" alanına bugünü yaz. Satırı defterden SİLME.

D. KAYDET
10. Değişiklik varsa tek commit at ("Bültenler: +N yeni, N atlandı, N not, N silindi") ve doğrudan main dalına push et (git push origin HEAD:main). Push reddedilirse git pull --rebase origin main yapıp bir kez daha dene (cihaz cihaz.txt'yi değiştirmiş olabilir). cihaz.txt ve brifing.txt dosyalarına ASLA yazma.

Kurallar: Depoda yalnız bultenler.txt, kitaplar/ ve kaynak/ altına dokun. Gmail'de yalnız oku. Yorum, öneri yazma. Bir bülten başarısız olursa diğerleriyle devam et.

RAPOR: En az bir bülten eklendiyse, not hazırlandıysa ya da silindiyse PushNotification ile <routine_summary> içinde tek satır: eklenen bültenlerin başlıkları, not hazırlananlar, silinenler, kuyrukta bekleyen tahmini sayı, hata varsa hangisi ve neden. Yalnız "atlandi" kaydı olduysa ya da hiçbir şey değişmediyse bildirim gönderme, sohbete kısa özet yaz.
