#pragma once
// Transfer page served by the device's hotspot (http://192.168.4.1/).
// "Sabah Brifingi" look with system fonts only: the hotspot has no internet.
// On load it sends the browser's local time to /saat, then loads the
// current briefing from /brifing.txt into the editor; "Kaydet" posts it back.

static const char kTransferPage[] = R"HTML(<!doctype html>
<html lang="tr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>X3 Brifing</title>
<style>
:root{--bg:#F4F3F1;--sand:#EFEDE9;--line:#D6D3CE;--ink:#1A1A1A;--body:#4A4744;--muted:#5E5A56;--lila:#7A5BB0;--mor:#3E2063;--lila-soft:#EAE4F7}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--body);font:15px/1.55 -apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif}
.wrap{max-width:740px;margin:0 auto;padding:28px 16px 40px}
.eyebrow{font-size:10px;letter-spacing:.22em;text-transform:uppercase;color:var(--muted);font-weight:500}
h1{font-family:'Playfair Display',Georgia,'Times New Roman',serif;font-weight:500;color:var(--ink);font-size:clamp(34px,9vw,48px);line-height:1.04;margin:8px 0 0}
.rule{height:2px;background:var(--ink);margin:14px 0 0}
.dateline{padding:11px 0 0;font-size:11px;letter-spacing:.09em;font-weight:500;color:var(--muted)}
.sec{margin-top:34px}
h2{font-size:10.5px;letter-spacing:.2em;text-transform:uppercase;color:var(--ink);font-weight:600;margin:0}
.hr{height:1px;background:var(--ink);margin-top:7px}
textarea{width:100%;min-height:360px;margin-top:14px;padding:12px;background:var(--sand);border:1px solid var(--line);color:var(--ink);font:13px/1.6 ui-monospace,Menlo,Consolas,monospace;resize:vertical}
textarea:focus{outline:2px solid var(--lila);outline-offset:2px}
.row{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px;align-items:center}
button,label.file{font:600 11px/1 -apple-system,sans-serif;letter-spacing:.14em;text-transform:uppercase;padding:13px 18px;border:1px solid var(--ink);background:var(--ink);color:#fff;cursor:pointer;min-height:44px}
label.file{background:transparent;color:var(--ink);display:inline-flex;align-items:center}
input[type=file]{display:none}
.pill{display:inline-block;background:var(--lila-soft);color:var(--mor);border-radius:999px;font-size:10px;letter-spacing:.1em;text-transform:uppercase;padding:4px 10px;font-weight:600}
.note{font-size:12px;color:var(--muted);margin-top:10px}
.book{display:flex;gap:12px;align-items:center;padding:12px 2px;border-bottom:1px solid var(--line)}
.book .t{flex:1;min-width:0;color:var(--ink);font-weight:500;overflow-wrap:anywhere}
.book .m{font-size:12px;color:var(--muted);white-space:nowrap}
.book button{padding:8px 12px;min-height:36px;background:transparent;color:var(--ink)}
.empty{font-size:13.5px;color:var(--muted);padding:14px 2px;border-bottom:1px solid var(--line)}
progress{width:100%;height:6px;margin-top:12px;accent-color:var(--lila)}
[hidden]{display:none!important}
.fld{display:block;margin-top:12px}
.fld span{display:block;font-size:9.5px;letter-spacing:.17em;text-transform:uppercase;color:var(--muted);font-weight:600;margin-bottom:6px}
.fld input,.fld select{width:100%;font:15px -apple-system,sans-serif;padding:11px 12px;background:var(--sand);border:1px solid var(--line);color:var(--ink);min-height:44px}
.fld input:focus,.fld select:focus{outline:2px solid var(--lila);outline-offset:2px}
.opt{font-size:13px;color:var(--ink);display:inline-flex;gap:8px;align-items:center;min-height:44px}
.opt input{width:18px;height:18px;accent-color:var(--lila)}
</style></head><body><div class="wrap">
<div class="eyebrow">XTEINK X3 · WI-FI AKTARIM</div>
<h1>Aktarım</h1>
<div class="rule"></div>
<div class="dateline" id="clock">Saat ayarlanıyor…</div>

<div class="sec">
<h2>Eşitleme</h2><div class="hr"></div>
<div class="note" id="syncinfo">Yükleniyor…</div>
<form id="sf">
<label class="fld"><span>EV WI-FI ADI</span><input name="ssid" id="s_ssid" autocomplete="off"></label>
<label class="fld"><span>EV WI-FI ŞİFRESİ</span><input name="pass" type="password" id="s_pass" placeholder="değiştirmek için yaz" autocomplete="new-password"></label>
<label class="fld"><span>İKİNCİ WI-FI ADI (TELEFON)</span><input name="ssid2" id="s_ssid2" placeholder="isteğe bağlı" autocomplete="off"></label>
<label class="fld"><span>İKİNCİ WI-FI ŞİFRESİ</span><input name="pass2" type="password" id="s_pass2" placeholder="değiştirmek için yaz" autocomplete="new-password"></label>
<label class="fld"><span>GITHUB ERİŞİM ANAHTARI</span><input name="token" type="password" id="s_token" placeholder="değiştirmek için yaz" autocomplete="off"></label>
<label class="fld"><span>Depo</span><input name="repo" id="s_repo" placeholder="gkcnkl/x3-sync" autocomplete="off"></label>
<label class="fld"><span>Dal</span><input name="branch" id="s_branch" placeholder="main" autocomplete="off"></label>
<div class="row"><button type="submit">Kaydet</button><span class="pill" id="sst" hidden></span></div>
</form>
<div class="note">Cihaz uyanınca (en çok yarım saatte bir) çevrede hangi kayıtlı ağ varsa (ev ya da telefon) ona bağlanır, depodaki <code>brifing.txt</code>'yi alır, tamamladığın görevleri <code>cihaz.txt</code> olarak geri yazar ve saati internetten ayarlar. Anahtar yalnız bu depoya, yalnız "Contents" izniyle olmalı; cihaz onu hiçbir zaman geri göstermez.</div>
</div>

<div class="sec">
<h2>Okuma</h2><div class="hr"></div>
<form id="rf">
<label class="fld"><span>YAZI BOYUTU</span><select name="size" id="r_size">
<option value="21">Küçük · 21</option><option value="24">Orta · 24</option><option value="27">Büyük · 27</option><option value="30">Daha büyük · 30</option><option value="34">En büyük · 34</option></select></label>
<label class="fld"><span>SATIR ARALIĞI</span><select name="space" id="r_space">
<option value="108">Sık</option><option value="118">Normal</option><option value="132">Geniş</option></select></label>
<label class="fld"><span>KENAR BOŞLUĞU</span><select name="margin" id="r_margin">
<option value="18">Dar</option><option value="30">Normal</option><option value="42">Geniş</option></select></label>
<p id="r_demo" style="margin:14px 0 4px;font-family:Georgia,'Times New Roman',serif;text-align:justify;hyphens:auto;border:1px solid var(--line);background:#fff;color:#111">Örnek paragraf: ekranda yaklaşık böyle görünür. Kitap okurken TAMAM tuşu da yazı boyutunu sırayla büyütür; okuduğun yer korunur.</p>
<div class="row"><button type="submit">Kaydet</button><span class="pill" id="rst" hidden></span></div>
</form>
<div class="note">Değişiklik bir sonraki kitap açılışında geçerli olur. Her boyut ilk açılışta yeniden sayfalanır (birkaç saniye), sonra hızlıdır.</div>
</div>

<div class="sec">
<h2>Kitaplar</h2><div class="hr"></div>
<div id="books"><div class="empty">Yükleniyor…</div></div>
<div class="row">
<label class="file">Kitap ekle<input type="file" id="bf" accept=".epub,.txt,application/epub+zip,text/plain" multiple></label>
<label class="opt"><input type="checkbox" id="fit" checked> Cihaza uygun hale getir</label>
<span class="pill" id="bst" hidden></span>
</div>
<progress id="bp" max="100" value="0" hidden></progress>
<div class="note">EPUB (DRM'siz) ya da TXT. "Cihaza uygun hale getir" EPUB'ı yüklemeden önce bu tarayıcıda düzenler: gömülü fontları siler, resimleri ekran boyutuna (528×792) küçültüp gri tonlu yapar. Kitaplar SD karttaki <code>/kitaplar</code> klasörüne yazılır.</div>
</div>

<div class="sec">
<h2>brifing.txt</h2><div class="hr"></div>
<textarea id="t" spellcheck="false" placeholder="Yükleniyor…"></textarea>
<div class="row">
<button id="save">Kaydet</button>
<label class="file">Dosyadan yükle<input type="file" id="f" accept=".txt,text/plain"></label>
<span class="pill" id="st" hidden></span>
</div>
<div class="note">Kaydedince cihaz dosyayı SD karta yazar ve ekranı yeniler. Biçim: <code>[x] 09:15-10:00 | Görev</code>, <code>## Bölüm</code>, <code>gosterge: Etiket | Değer | Ayrıntı</code>.</div>
</div>
</div>
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
function show(m){st.textContent=m;st.hidden=false}
function p2(n){return(n<10?'0':'')+n}
var d=new Date();
var stamp=d.getFullYear()+'-'+p2(d.getMonth()+1)+'-'+p2(d.getDate())+' '+p2(d.getHours())+':'+p2(d.getMinutes())+':'+p2(d.getSeconds());
fetch('/saat',{method:'POST',body:stamp}).then(function(r){return r.text()}).then(function(m){ck.textContent=m}).catch(function(){ck.textContent='Saat ayarlanamadı'});
fetch('/brifing.txt').then(function(r){return r.text()}).then(function(x){t.value=x}).catch(function(){show('Dosya okunamadı')});
document.getElementById('save').onclick=function(){
  show('Gönderiliyor…');
  fetch('/brifing',{method:'POST',body:t.value}).then(function(r){return r.text()}).then(show).catch(function(){show('Gönderilemedi')});
};
var books=document.getElementById('books'),bst=document.getElementById('bst'),bp=document.getElementById('bp');
function bshow(m){bst.textContent=m;bst.hidden=false}
function esc(x){return x.replace(/[&<>"]/g,function(c){return{'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]})}
function loadBooks(){
  fetch('/kitaplar').then(function(r){return r.text()}).then(function(x){
    var rows=x.split('\n').filter(Boolean);
    if(!rows.length){books.innerHTML='<div class="empty">Raflar boş.</div>';return}
    books.innerHTML=rows.map(function(r){var c=r.split('\t');
      return '<div class="book"><span class="t">'+esc(c[1]||c[0])+'</span><span class="m">'+esc(c[2])+'</span><button data-n="'+esc(c[0])+'">Sil</button></div>'}).join('');
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
function sshow(m){sst.textContent=m;sst.hidden=false}
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
rf.size.value='27';rf.space.value='118';rf.margin.value='30';rdemo();
fetch('/okuma').then(function(r){return r.text()}).then(function(x){var c=x.split('\t');
  rf.size.value=c[0];rf.space.value=c[1];rf.margin.value=c[2];rdemo()}).catch(function(){});
rf.onsubmit=function(e){e.preventDefault();rst.hidden=false;rst.textContent='Kaydediliyor…';
  fetch('/okuma',{method:'POST',body:new URLSearchParams(new FormData(rf))}).then(function(r){return r.text()})
  .then(function(m){rst.textContent=m}).catch(function(){rst.textContent='Kaydedilemedi'})};
document.getElementById('f').onchange=function(e){
  var file=e.target.files[0];if(!file)return;
  var rd=new FileReader();rd.onload=function(){t.value=rd.result;show('Dosya yüklendi, Kaydet’e bas')};rd.readAsText(file,'utf-8');
};
})();
</script></body></html>
)HTML";
