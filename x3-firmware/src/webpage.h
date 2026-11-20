#pragma once
// Transfer page served by the device's hotspot (http://192.168.4.1/).
// "Sabah Brifingi" design system; its fonts are served by the device (the hotspot has no internet).
// On load it sends the browser's local time to /saat, then loads the
// current briefing from /brifing.txt into the editor; "Kaydet" posts it back.

static const char kTransferPage[] = R"HTML(<!doctype html>
<html lang="tr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>X3 Brifing</title>
<style>
/* "Sabah Brifingi" design system (tokens, Masthead, SectionHeading, RowList,
   StatusLabel, EmptyState, FooterBar): one column up to 740px, hairlines, a
   serif masthead, one lilac accent that always means a state. Its fonts are
   served by the device. */
@font-face{font-family:'Playfair Display';font-weight:500;font-display:swap;src:url(/f/p500.woff2) format('woff2')}
@font-face{font-family:'Inter';font-weight:400;font-display:swap;src:url(/f/i400.woff2) format('woff2')}
@font-face{font-family:'Inter';font-weight:500;font-display:swap;src:url(/f/i500.woff2) format('woff2')}
@font-face{font-family:'Inter';font-weight:600;font-display:swap;src:url(/f/i600.woff2) format('woff2')}
:root{color-scheme:light;--bg:#f4f3f1;--sand:#efede9;--line:#d6d3ce;--ink:#1a1a1a;--body:#4a4744;--muted:#5e5a56;--lila:#7a5bb0;--lila-soft:#eae4f7;--mor:#3e2063;
--display:'Playfair Display',Georgia,'Times New Roman',serif;--text:Inter,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif}
*{box-sizing:border-box}
html,body{margin:0}
body{background:var(--bg);color:var(--body);font-family:var(--text);font-size:15px;font-weight:400;line-height:1.55;-webkit-font-smoothing:antialiased;padding-bottom:72px;overflow-x:hidden}
.wrap{max-width:740px;margin:0 auto;padding:30px max(16px,env(safe-area-inset-right,0px)) 0 max(16px,env(safe-area-inset-left,0px))}
.eyebrow{font-size:10px;letter-spacing:.22em;text-transform:uppercase;color:var(--muted);font-weight:500;line-height:1.3}
h1{font-family:var(--display);font-weight:500;color:var(--ink);font-size:clamp(34px,9vw,50px);line-height:1.04;margin:8px 0 0;letter-spacing:-.01em}
.rule{height:2px;background:var(--ink);margin:14px 0 0}
.dateline{display:flex;gap:6px 16px;flex-wrap:wrap;align-items:baseline;padding:11px 0 0;font-size:11px;letter-spacing:.09em;font-weight:500;color:var(--muted);line-height:1.4;font-variant-numeric:tabular-nums}
.dateline .d{color:var(--ink)}
.fixed{margin:10px 0 0;font-size:13.5px;line-height:1.55;color:var(--body)}
.sec{margin-top:38px}
h2{font-size:10.5px;letter-spacing:.2em;text-transform:uppercase;color:var(--ink);font-weight:600;margin:0;line-height:1.3}
.hr{height:1px;background:var(--ink);margin-top:7px}
.subhead{font-size:9.5px;letter-spacing:.17em;text-transform:uppercase;color:var(--muted);font-weight:600;margin-top:20px;line-height:1.3}
.empty{font-size:13.5px;color:var(--body);padding:14px 0;border-bottom:1px solid var(--line)}
.kaynak{font-size:11px;color:var(--muted);margin-top:10px;line-height:1.55}
code{font:inherit;color:var(--ink)}
/* RowList, without the mark: a book is a row with a title and a meta line. */
.row{display:flex;gap:12px;align-items:flex-start;padding:14px 0;border-bottom:1px solid var(--line)}
.rb{flex:1 1 auto;min-width:0}
.rt{font-size:15px;color:var(--ink);font-weight:500;line-height:1.4;overflow-wrap:anywhere}
.rm{font-size:12px;color:var(--muted);margin-top:4px;line-height:1.55;font-variant-numeric:tabular-nums}
/* StatusLabel: a word, never colour alone. */
.state{font-size:10px;letter-spacing:.1em;text-transform:uppercase;font-weight:600;color:var(--lila);line-height:1.3}
.state.late{color:var(--mor)}
.rt .state{margin-left:8px;white-space:nowrap;display:inline-block}
/* Fields follow NoteField's frame (bg, 1px line, square; focus turns the border ink) in the text face. */
.fld{display:block;margin-top:14px}
.fld span{display:block;font-size:9.5px;letter-spacing:.17em;text-transform:uppercase;color:var(--muted);font-weight:600;margin-bottom:7px;line-height:1.3}
.fld input,.fld select,textarea{width:100%;font:15px/1.4 var(--text);color:var(--ink);background:var(--bg);border:1px solid var(--line);border-radius:0;padding:11px 13px;min-height:44px;touch-action:manipulation;-webkit-tap-highlight-color:transparent}
.fld input::placeholder{color:var(--muted)}
.fld input:focus,.fld select:focus,textarea:focus{outline:none;border-color:var(--ink)}
textarea{min-height:320px;margin-top:14px;font:13.5px/1.6 var(--text);resize:vertical}
/* Actions: the system has no buttons; the few this device page needs are
   quiet uppercase labels on a 1px ink frame, 44px tall. */
.acts{display:flex;gap:16px;flex-wrap:wrap;margin-top:14px;align-items:center}
button,label.file{font:600 10.5px/1.3 var(--text);letter-spacing:.14em;text-transform:uppercase;color:var(--ink);background:transparent;border:1px solid var(--ink);border-radius:0;padding:0 16px;min-height:44px;display:inline-flex;align-items:center;cursor:pointer;touch-action:manipulation;-webkit-tap-highlight-color:transparent}
button:focus-visible,label.file:focus-within{outline:2px solid var(--lila);outline-offset:2px}
.row button{min-height:36px;padding:0 12px;border-color:var(--muted);color:var(--muted)}
input[type=file]{display:none}
.opt{font-size:13.5px;color:var(--body);display:inline-flex;gap:8px;align-items:center;min-height:44px}
.opt input{width:18px;height:18px;margin:0;accent-color:var(--lila)}
progress{width:100%;height:4px;margin-top:12px;accent-color:var(--lila)}
details{margin-top:14px}
summary{cursor:pointer;font-size:9.5px;letter-spacing:.17em;text-transform:uppercase;color:var(--muted);font-weight:600;min-height:44px;display:flex;align-items:center}
summary:focus-visible{outline:2px solid var(--lila);outline-offset:2px}
[hidden]{display:none!important}
/* The briefing as the X3 shows it, in this system's type: its own masthead,
   section headings, rows with a state mark, measures with a hairline bar. */
.bm{margin-top:14px}
.bt{font-family:var(--display);font-weight:500;color:var(--ink);font-size:clamp(28px,7vw,38px);line-height:1.08;margin:6px 0 0}
.bm .rule{margin-top:10px}
.bsec{margin-top:26px}
.bsec .hr{background:var(--line)}
.mk{flex:none;width:18px;height:18px;border:1.5px solid var(--ink);border-radius:50%;margin-top:2px}
.mk.s{background:linear-gradient(90deg,var(--ink) 50%,transparent 50%)}
.mk.x{background:var(--ink)}
.mk.c{border-color:var(--muted);background:linear-gradient(135deg,transparent 46%,var(--muted) 46%,var(--muted) 54%,transparent 54%)}
.tm{font-family:var(--display);color:var(--ink);font-variant-numeric:tabular-nums;margin-right:10px;white-space:nowrap}
.row.x .rt,.row.c .rt{color:var(--muted);text-decoration:line-through;text-decoration-thickness:1px}
.ms{padding:12px 0;border-bottom:1px solid var(--line)}
.mh{display:flex;justify-content:space-between;gap:12px;align-items:baseline}
.ml{font-size:10px;letter-spacing:.14em;text-transform:uppercase;font-weight:600;color:var(--ink)}
.mv{font-size:13px;color:var(--body);font-variant-numeric:tabular-nums;text-align:right}
.mt{height:4px;background:var(--line);margin-top:8px}
.mt i{display:block;height:4px;background:var(--ink)}
.gs{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:0 16px}
.gs .ms .mv{font-family:var(--display);font-size:20px;color:var(--ink);text-align:left;margin-top:4px}
/* FooterBar */
.bar{position:fixed;left:0;right:0;bottom:0;background:var(--bg);border-top:1px solid var(--line);padding:12px max(16px,env(safe-area-inset-right,0px)) calc(12px + env(safe-area-inset-bottom,0px)) max(16px,env(safe-area-inset-left,0px));display:flex;justify-content:space-between;align-items:center;font-size:10.5px;letter-spacing:.14em;text-transform:uppercase;color:var(--muted);font-weight:500;line-height:1.3}
.bar b{font-family:var(--display);font-size:14px;font-weight:500;color:var(--ink);letter-spacing:0;font-variant-numeric:tabular-nums;line-height:1.4}
</style></head><body><div class="wrap">
<div class="eyebrow">XTEINK X3 · WI-FI AKTARIM</div>
<h1>Aktarım</h1>
<div class="rule"></div>
<div class="dateline"><span class="d" id="clock">Saat ayarlanıyor…</span><span id="dl_books"></span><span id="dl_tasks"></span></div>
<p class="fixed">Bu sayfa listede sağ yan tuşu basılı tutunca açılır. Cihaz yalnız eşitleme tuşuna (listede sağ yan tuşa kısa bas) basınca kayıtlı Wi-Fi'a bağlanıp depoyla eşitlenir. Bu sayfa yalnız ayar, kitap ve elle düzeltme içindir.</p>

<div class="sec">
<h2>Brifing</h2><div class="hr"></div>
<div id="brief"><div class="empty">Yükleniyor…</div></div>
<div class="kaynak">Kaynak: cihazdaki <code>brifing.txt</code>, işaretlerinle birlikte; X3'teki düzenle. Değiştirmek için cihazda TAMAM tuşunu ya da aşağıdaki elle düzenlemeyi kullan.</div>
</div>

<div class="sec">
<h2>Eşitleme</h2><div class="hr"></div>
<div class="kaynak" id="syncinfo">Yükleniyor…</div>
<form id="sf">
<label class="fld"><span>EV WI-FI ADI</span><input name="ssid" id="s_ssid" autocomplete="off"></label>
<label class="fld"><span>EV WI-FI ŞİFRESİ</span><input name="pass" type="password" id="s_pass" placeholder="değiştirmek için yaz" autocomplete="new-password"></label>
<label class="fld"><span>İKİNCİ WI-FI ADI (TELEFON)</span><input name="ssid2" id="s_ssid2" placeholder="isteğe bağlı" autocomplete="off"></label>
<label class="fld"><span>İKİNCİ WI-FI ŞİFRESİ</span><input name="pass2" type="password" id="s_pass2" placeholder="değiştirmek için yaz" autocomplete="new-password"></label>
<label class="fld"><span>GITHUB ERİŞİM ANAHTARI</span><input name="token" type="password" id="s_token" placeholder="değiştirmek için yaz" autocomplete="off"></label>
<label class="fld"><span>DEPO</span><input name="repo" id="s_repo" placeholder="gkcnkl/x3-sync" autocomplete="off"></label>
<label class="fld"><span>DAL</span><input name="branch" id="s_branch" placeholder="main" autocomplete="off"></label>
<div class="acts"><button type="submit">Kaydet</button><span class="state" id="sst" hidden></span></div>
</form>
<div class="kaynak">Eşitleme tuşuna basınca cihaz çevrede hangi kayıtlı ağ varsa (ev ya da telefon) ona bağlanır, depodaki <code>brifing.txt</code>'yi alır, tamamladığın görevleri <code>cihaz.txt</code> olarak geri yazar ve saati internetten ayarlar. Anahtar yalnız bu depoya, yalnız "Contents" izniyle olmalı; cihaz onu hiçbir zaman geri göstermez.</div>
</div>

<div class="sec">
<h2>Okuma</h2><div class="hr"></div>
<form id="rf">
<label class="fld"><span>YAZI BOYUTU</span><select name="size" id="r_size">
<option value="27">Küçük · 27</option><option value="30">Orta · 30</option><option value="34">Büyük · 34</option><option value="38">Daha büyük · 38</option><option value="42">En büyük · 42</option></select></label>
<label class="fld"><span>SATIR ARALIĞI</span><select name="space" id="r_space">
<option value="108">Sık</option><option value="118">Normal</option><option value="132">Geniş</option></select></label>
<label class="fld"><span>KENAR BOŞLUĞU</span><select name="margin" id="r_margin">
<option value="18">Dar</option><option value="30">Normal</option><option value="42">Geniş</option></select></label>
<div class="subhead">ÖRNEK SAYFA</div>
<p id="r_demo" style="margin:8px 0 4px;font-family:Georgia,'Times New Roman',serif;text-align:justify;hyphens:auto;border:1px solid var(--line);background:#fff;color:#111">Örnek paragraf: ekranda yaklaşık böyle görünür. Kitap okurken TAMAM tuşu da yazı boyutunu sırayla büyütür; okuduğun yer korunur.</p>
<div class="acts"><button type="submit">Kaydet</button><span class="state" id="rst" hidden></span></div>
</form>
<div class="kaynak">Değişiklik bir sonraki kitap açılışında geçerli olur. Her boyut ilk açılışta yeniden sayfalanır (birkaç saniye), sonra hızlıdır.</div>
</div>

<div class="sec">
<h2>Kitaplar</h2><div class="hr"></div>
<div id="books"><div class="empty">Yükleniyor…</div></div>
<div class="acts">
<label class="file">Kitap ekle<input type="file" id="bf" accept=".epub,.txt,application/epub+zip,text/plain" multiple></label>
<label class="opt"><input type="checkbox" id="fit" checked> Cihaza uygun hale getir</label>
<span class="state" id="bst" hidden></span>
</div>
<progress id="bp" max="100" value="0" hidden></progress>
<div class="kaynak">Kaynak: SD karttaki <code>/kitaplar</code> klasörü, sayfa açılınca okundu. EPUB (DRM'siz) ya da TXT yüklenir. "Cihaza uygun hale getir" EPUB'ı yüklemeden önce bu tarayıcıda düzenler: gömülü fontları siler, resimleri ekran boyutuna (528×792) küçültüp gri tonlu yapar.</div>
</div>

<div class="sec">
<h2>Brifing dosyası</h2><div class="hr"></div>
<details><summary>BRIFING.TXT DOSYASINI ELLE DÜZENLE</summary>
<textarea id="t" spellcheck="false" placeholder="Yükleniyor…"></textarea>
<div class="acts">
<button id="save">Kaydet</button>
<label class="file">Dosyadan yükle<input type="file" id="f" accept=".txt,text/plain"></label>
</div>
</details>
<div class="acts" style="margin-top:6px"><span class="state" id="st" hidden></span></div>
<div class="kaynak">Kaynak: cihazdaki liste, işaretlerinle birlikte. Kaydedince cihaz dosyayı SD karta yazar ve ekranı yeniler. Biçim: <code>[ ] 09:15-10:00 | Görev</code>, <code>## Bölüm</code>, <code>olcu: Etiket | değer | hedef | birim</code>.</div>
</div>
</div>
<div class="bar"><span><b id="b_done">· / ·</b> tamamlandı</span><span id="b_day"></span></div>
<script>
// EPUB optimizer for the X3, all in the browser (the hotspot has no internet,
// so no libraries): ZIP read/write with the native (De)CompressionStream.
window.x3Optimize = (function(){
  var MAXW = 528, MAXH = 792;
  var crcTable = (function(){var t=new Uint32Array(256);for(var n=0;n<256;n++){var c=n;for(var k=0;k<8;k++)c=c&1?0xEDB88320^(c>>>1):c>>>1;t[n]=c>>>0}return t})();
  function crc32(b){var c=0xFFFFFFFF;for(var i=0;i<b.length;i++)c=crcTable[(c^b[i])&255]^(c>>>8);return (c^0xFFFFFFFF)>>>0}
  function pipe(bytes, stream){return new Response(new Blob([bytes]).stream().pipeThrough(stream)).arrayBuffer().then(function(a){return new Uint8Array(a)})}
  function readZip(buf){
    var d=new DataView(buf), u8=new Uint8Array(buf), e=-1;
    for(var i=buf.byteLength-22;i>=Math.max(0,buf.byteLength-65557);i--){if(d.getUint32(i,true)===0x06054b50){e=i;break}}
    if(e<0)throw new Error('ZIP değil');
    var n=d.getUint16(e+10,true), p=d.getUint32(e+16,true), out=[], dec=new TextDecoder();
    for(var k=0;k<n;k++){
      if(d.getUint32(p,true)!==0x02014b50)throw new Error('ZIP dizini bozuk');
      var method=d.getUint16(p+10,true), csize=d.getUint32(p+20,true), nlen=d.getUint16(p+28,true),
          xlen=d.getUint16(p+30,true), clen=d.getUint16(p+32,true), off=d.getUint32(p+42,true);
      var name=dec.decode(u8.subarray(p+46,p+46+nlen));
      var start=off+30+d.getUint16(off+26,true)+d.getUint16(off+28,true);
      out.push({name:name, method:method, data:u8.subarray(start,start+csize)});
      p+=46+nlen+xlen+clen;
    }
    return out;
  }
  function inflate(en){return en.method===0?Promise.resolve(en.data):pipe(en.data,new DecompressionStream('deflate-raw'))}
  function shrinkImage(bytes, name){
    var png=/\.png$/i.test(name), type=png?'image/png':'image/jpeg';
    return createImageBitmap(new Blob([bytes])).then(function(img){
      var s=Math.min(1, MAXW/img.width, MAXH/img.height), w=Math.max(1,Math.round(img.width*s)), h=Math.max(1,Math.round(img.height*s));
      var c=document.createElement('canvas');c.width=w;c.height=h;
      var g=c.getContext('2d');g.fillStyle='#fff';g.fillRect(0,0,w,h);g.filter='grayscale(1)';g.drawImage(img,0,0,w,h);
      return new Promise(function(res){c.toBlob(res,type,0.75)});
    }).then(function(b){return b.arrayBuffer()}).then(function(a){
      var r=new Uint8Array(a);return r.length<bytes.length?r:bytes;
    }).catch(function(){return bytes});
  }
  var FONT=/\.(ttf|otf|woff2?|eot)$/i, IMG=/\.(jpe?g|png)$/i;
  function writeZip(entries){
    var parts=[], central=[], off=0, enc=new TextEncoder();
    entries.forEach(function(en){
      var name=enc.encode(en.name), h=new DataView(new ArrayBuffer(30));
      h.setUint32(0,0x04034b50,true);h.setUint16(4,20,true);h.setUint16(6,0x0800,true);h.setUint16(8,en.method,true);
      h.setUint16(12,0x21,true);h.setUint32(14,en.crc,true);h.setUint32(18,en.comp.length,true);h.setUint32(22,en.size,true);h.setUint16(26,name.length,true);
      var c=new DataView(new ArrayBuffer(46));
      c.setUint32(0,0x02014b50,true);c.setUint16(4,20,true);c.setUint16(6,20,true);c.setUint16(8,0x0800,true);c.setUint16(10,en.method,true);
      c.setUint16(14,0x21,true);c.setUint32(16,en.crc,true);c.setUint32(20,en.comp.length,true);c.setUint32(24,en.size,true);c.setUint16(28,name.length,true);c.setUint32(42,off,true);
      parts.push(new Uint8Array(h.buffer),name,en.comp);central.push(new Uint8Array(c.buffer),name);
      off+=30+name.length+en.comp.length;
    });
    var csize=central.reduce(function(a,b){return a+b.length},0), e=new DataView(new ArrayBuffer(22));
    e.setUint32(0,0x06054b50,true);e.setUint16(8,entries.length,true);e.setUint16(10,entries.length,true);e.setUint32(12,csize,true);e.setUint32(16,off,true);
    return new Blob(parts.concat(central,[new Uint8Array(e.buffer)]),{type:'application/epub+zip'});
  }
  return function(file, progress){
    if(!('DecompressionStream' in window)||!('CompressionStream' in window))return Promise.reject(new Error('Bu tarayıcı desteklemiyor'));
    return file.arrayBuffer().then(function(buf){
      var list=readZip(buf).filter(function(en){return !/\/$/.test(en.name)&&!FONT.test(en.name)});
      var done=0;
      return Promise.all(list.map(function(en){
        return inflate(en).then(function(raw){
          if(IMG.test(en.name))return shrinkImage(raw,en.name);
          if(/\.css$/i.test(en.name))return new TextEncoder().encode(new TextDecoder().decode(raw).replace(/@font-face\s*\{[^}]*\}/gi,''));
          if(/\.opf$/i.test(en.name))return new TextEncoder().encode(new TextDecoder().decode(raw).replace(/<item\b[^>]*href="[^"]*\.(ttf|otf|woff2?|eot)"[^>]*\/>/gi,''));
          return raw;
        }).then(function(data){
          var store=en.name==='mimetype'||IMG.test(en.name);
          return (store?Promise.resolve(data):pipe(data,new CompressionStream('deflate-raw'))).then(function(comp){
            if(progress)progress(++done/list.length);
            return {name:en.name, method:store?0:8, size:data.length, crc:crc32(data), comp:store?data:comp};
          });
        });
      })).then(function(entries){
        entries.sort(function(a,b){return a.name==='mimetype'?-1:b.name==='mimetype'?1:0});
        return writeZip(entries);
      });
    });
  };
})();
</script>
<script>
(function(){
var t=document.getElementById('t'),st=document.getElementById('st'),ck=document.getElementById('clock');
function mark(el,m){el.textContent=m;el.hidden=false;el.className='state'+(/amad|alınamad|okunamad|gönderilemed|kaydedilemed|hata/i.test(m)?' late':'')}
function show(m){mark(st,m)}
function p2(n){return(n<10?'0':'')+n}
var d=new Date();
var stamp=d.getFullYear()+'-'+p2(d.getMonth()+1)+'-'+p2(d.getDate())+' '+p2(d.getHours())+':'+p2(d.getMinutes())+':'+p2(d.getSeconds());
fetch('/saat',{method:'POST',body:stamp}).then(function(r){return r.text()}).then(function(m){ck.textContent=m}).catch(function(){ck.textContent='Saat ayarlanamadı'});
var DAYS=['Pazar','Pazartesi','Salı','Çarşamba','Perşembe','Cuma','Cumartesi'];
document.getElementById('b_day').textContent=DAYS[d.getDay()];
// FooterBar counter: done tasks / all tasks in the device's list.
function tally(){var all=0,done=0;t.value.split('\n').forEach(function(l){var m=l.match(/^\[(.)\]/);if(m){all++;if(m[1]==='x')done++}});
  document.getElementById('b_done').textContent=done+' / '+all;document.getElementById('dl_tasks').textContent=all+' görev'}
// The briefing view: brifing.txt parsed the way the device parses it.
var WX={gunes:'Güneşli',parcali:'Parçalı bulutlu',bulut:'Bulutlu',yagmur:'Yağmurlu',kar:'Karlı',firtina:'Fırtınalı',sis:'Sisli'};
function grp(n){return String(n).replace(/\B(?=(\d{3})+(?!\d))/g,'.')}
function hm(m){var h=Math.floor(m/60),k=m%60;return h?(k?h+'sa '+k+'dk':h+'sa'):k+'dk'}
function mtext(v,g,u){if(v<0||isNaN(v))return 'veri yok';if(u==='süre')return hm(v)+' / '+hm(g);
  if(u==='₺')return grp(v)+' ₺ / '+grp(g)+' ₺';return grp(v)+' / '+grp(g)+(u?' '+u:'')}
function brief(){
  var head={},secs=[],cur=null,top=[],ind=[];
  function sec(n){cur={name:n,items:[],ms:[]};secs.push(cur)}
  t.value.split('\n').forEach(function(raw){
    var l=raw.trim();if(!l||l[0]==='#'&&l.slice(0,2)!=='##')return;
    if(l.slice(0,2)==='##'){sec(l.replace(/^#+\s*/,''));return}
    var kv=l.match(/^([a-zçğıöşü]+):\s*(.*)$/i);
    if(kv&&!/^\[/.test(l)){var k=kv[1].toLowerCase(),v=kv[2],c=v.split('|').map(function(x){return x.trim()});
      if(k==='ust')head.ust=v;else if(k==='baslik'||k==='başlık')head.baslik=v;else if(k==='alt')head.alt=v;
      else if(k==='hava')head.hava=c;else if(k==='gosterge'||k==='gösterge')ind.push(c);
      else if(k==='olcu'||k==='ölçü')(cur?cur.ms:top).push(c);return}
    var m=l.replace(/^- /,'').match(/^\[(.)\]\s*(.*)$/);if(!m)return;
    var st=m[1].toLowerCase(),rest=m[2],tm='',p=rest.indexOf('|');
    if(p>0&&/^\d/.test(rest)){tm=rest.slice(0,p).trim();rest=rest.slice(p+1).trim()}
    if(!cur)sec('');
    cur.items.push({s:st==='x'?'x':(st==='-'||st==='~')?'c':st==='/'?'s':'',tm:tm,t:rest});
  });
  var h='<div class="bm">';
  if(head.ust)h+='<div class="eyebrow">'+esc(head.ust)+'</div>';
  h+='<div class="bt">'+esc(head.baslik||'Brifing')+'</div><div class="rule"></div><div class="dateline">';
  if(head.alt)h+='<span class="d">'+esc(head.alt)+'</span>';
  if(head.hava&&head.hava.length>2)h+='<span>'+esc(WX[head.hava[0]]||head.hava[0])+' · '+esc(head.hava[1])+'° / '+esc(head.hava[2])+'°</span>';
  h+='</div></div>';
  function msr(c){var v=c[1]==='-'?-1:+c[1],g=+c[2]||0,u=c[3]||'',w=v>0&&g>0?Math.min(100,v*100/g):0;
    return '<div class="ms"><div class="mh"><span class="ml">'+esc(c[0])+'</span><span class="mv">'+esc(mtext(v,g,u))+'</span></div><div class="mt"><i style="width:'+w.toFixed(1)+'%"></i></div></div>'}
  if(ind.length)h+='<div class="bsec"><div class="gs">'+ind.map(function(c){return '<div class="ms"><div class="ml">'+esc(c[0])+'</div><div class="mv">'+esc(c[1]||'')+'</div>'+(c[2]?'<div class="rm">'+esc(c[2])+'</div>':'')+'</div>'}).join('')+'</div></div>';
  if(top.length)h+='<div class="bsec">'+top.map(msr).join('')+'</div>';
  var LBL={s:'başladı',x:'bitti',c:'iptal'};
  secs.forEach(function(sc){
    if(!sc.items.length&&!sc.ms.length)return;
    h+='<div class="bsec">'+(sc.name?'<h2>'+esc(sc.name)+'</h2><div class="hr"></div>':'');
    h+=sc.ms.map(msr).join('');
    h+=sc.items.map(function(it){return '<div class="row '+it.s+'"><span class="mk '+it.s+'"></span><div class="rb"><div class="rt">'+
      (it.tm?'<span class="tm">'+esc(it.tm)+'</span>':'')+esc(it.t)+(it.s?'<span class="state">'+LBL[it.s]+'</span>':'')+'</div></div></div>'}).join('');
    h+='</div>';
  });
  document.getElementById('brief').innerHTML=h;
}
t.oninput=function(){tally();brief()};
fetch('/brifing.txt').then(function(r){return r.text()}).then(function(x){t.value=x;tally();brief()}).catch(function(){show('Dosya okunamadı');document.getElementById('brief').innerHTML='<div class="empty">Brifing okunamadı.</div>'});
document.getElementById('save').onclick=function(){
  show('Gönderiliyor…');
  fetch('/brifing',{method:'POST',body:t.value}).then(function(r){return r.text()}).then(show).catch(function(){show('Gönderilemedi')});
};
var books=document.getElementById('books'),bst=document.getElementById('bst'),bp=document.getElementById('bp');
function bshow(m){mark(bst,m)}
function esc(x){return x.replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function loadBooks(){
  fetch('/kitaplar').then(function(r){return r.text()}).then(function(x){
    var rows=x.split('\n').filter(Boolean);
    document.getElementById('dl_books').textContent=rows.length+' kitap';
    if(!rows.length){books.innerHTML='<div class="empty">Raflar boş: SD kartın /kitaplar klasöründe kitap yok.</div>';return}
    books.innerHTML=rows.map(function(r){var c=r.split('\t');
      return '<div class="row"><div class="rb"><div class="rt">'+esc(c[1]||c[0])+'</div><div class="rm">'+esc(c[2])+'</div></div><button data-n="'+esc(c[0])+'">Sil</button></div>'}).join('');
  }).catch(function(){books.innerHTML='<div class="empty">Liste alınamadı.</div>'});
}
books.onclick=function(e){
  var n=e.target.getAttribute&&e.target.getAttribute('data-n');if(!n)return;
  if(!confirm(n+' silinsin mi?'))return;
  fetch('/kitap-sil',{method:'POST',body:n}).then(function(r){return r.text()}).then(function(m){bshow(m);loadBooks()});
};
function kb(n){return n<1048576?Math.round(n/1024)+' KB':(n/1048576).toFixed(1)+' MB'}
function send(blob,name,next){
  var fd=new FormData();fd.append('kitap',blob,name);
  var x=new XMLHttpRequest();x.open('POST','/kitap');
  x.upload.onprogress=function(e){if(e.lengthComputable){bp.value=e.loaded*100/e.total}};
  x.onload=function(){bshow(x.responseText);next()};
  x.onerror=function(){bshow(name+' yüklenemedi');bp.hidden=true};
  x.send(fd);
}
function upload(files,i){
  if(i>=files.length){bp.hidden=true;loadBooks();return}
  var f=files[i],next=function(){upload(files,i+1)};
  bp.hidden=false;bp.value=0;bshow((i+1)+'/'+files.length+' · '+f.name);
  if(!document.getElementById('fit').checked||!/\.epub$/i.test(f.name)){send(f,f.name,next);return}
  bshow('Hazırlanıyor · '+f.name);
  x3Optimize(f,function(p){bp.value=p*100}).then(function(b){
    bshow(f.name+': '+kb(f.size)+' → '+kb(b.size));send(b,f.name,next);
  }).catch(function(err){bshow('Dönüştürülemedi ('+err.message+'), olduğu gibi yükleniyor');send(f,f.name,next)});
}
document.getElementById('bf').onchange=function(e){upload(e.target.files,0);e.target.value=''};
loadBooks();
var sst=document.getElementById('sst'),si=document.getElementById('syncinfo');
function sshow(m){mark(sst,m)}
function loadSettings(){
  fetch('/ayarlar').then(function(r){return r.text()}).then(function(x){
    var c=x.split('\t');
    document.getElementById('s_ssid').value=c[0]||'';
    document.getElementById('s_repo').value=c[1]||'';
    document.getElementById('s_branch').value=c[2]||'';
    document.getElementById('s_ssid2').value=c[5]||'';
    si.textContent=c[3]?('Kayıtlı: anahtar '+c[3]+(c[4]==='1'?' · ev Wi-Fi şifresi':'')+(c[6]==='1'?' · ikinci Wi-Fi şifresi':'')+'. Değiştirmediğin alanlar olduğu gibi kalır; şifre ve anahtar alanlarını boş bırakırsan eskisi korunur.'):'Eşitleme henüz ayarlanmadı.';
  }).catch(function(){si.textContent='Ayarlar okunamadı.'});
}
document.getElementById('sf').onsubmit=function(e){
  e.preventDefault();
  var body=new URLSearchParams(new FormData(e.target));
  sshow('Kaydediliyor…');
  fetch('/ayarlar',{method:'POST',body:body}).then(function(r){return r.text()}).then(function(m){
    sshow(m);['s_pass','s_pass2','s_token'].forEach(function(i){document.getElementById(i).value=''});loadSettings();
  }).catch(function(){sshow('Kaydedilemedi')});
};
loadSettings();
var rf=document.getElementById('rf'),rst=document.getElementById('rst'),demo=document.getElementById('r_demo');
// Preview at the device's scale: the 528 px wide panel shown 300 px wide.
function rdemo(){var k=300/528,sz=+rf.size.value,sp=+rf.space.value,mg=+rf.margin.value;
  demo.style.width='300px';demo.style.boxSizing='border-box';demo.style.fontSize=(sz*k)+'px';
  demo.style.lineHeight=(sp/100)+'';demo.style.padding='10px '+(mg*k)+'px'}
['size','space','margin'].forEach(function(n){rf[n].onchange=rdemo});
rf.size.value='34';rf.space.value='118';rf.margin.value='30';rdemo();
fetch('/okuma').then(function(r){return r.text()}).then(function(x){var c=x.split('\t');
  rf.size.value=c[0];rf.space.value=c[1];rf.margin.value=c[2];rdemo()}).catch(function(){});
rf.onsubmit=function(e){e.preventDefault();mark(rst,'Kaydediliyor…');
  fetch('/okuma',{method:'POST',body:new URLSearchParams(new FormData(rf))}).then(function(r){return r.text()})
  .then(function(m){mark(rst,m)}).catch(function(){mark(rst,'Kaydedilemedi')})};
document.getElementById('f').onchange=function(e){
  var file=e.target.files[0];if(!file)return;
  var rd=new FileReader();rd.onload=function(){t.value=rd.result;show('Dosya yüklendi, Kaydet’e bas')};rd.readAsText(file,'utf-8');
};
})();
</script></body></html>
)HTML";
