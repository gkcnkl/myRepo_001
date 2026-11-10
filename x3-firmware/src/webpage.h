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
</style></head><body><div class="wrap">
<div class="eyebrow">XTEINK X3 · WI-FI AKTARIM</div>
<h1>Aktarım</h1>
<div class="rule"></div>
<div class="dateline" id="clock">Saat ayarlanıyor…</div>

<div class="sec">
<h2>Kitaplar</h2><div class="hr"></div>
<div id="books"><div class="empty">Yükleniyor…</div></div>
<div class="row">
<label class="file">Kitap ekle<input type="file" id="bf" accept=".epub,.txt,application/epub+zip,text/plain" multiple></label>
<span class="pill" id="bst" hidden></span>
</div>
<progress id="bp" max="100" value="0" hidden></progress>
<div class="note">EPUB (DRM'siz) ya da TXT. Kitaplar SD karttaki <code>/kitaplar</code> klasörüne yazılır; cihazda rafta görünür.</div>
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
function upload(files,i){
  if(i>=files.length){bp.hidden=true;loadBooks();return}
  var f=files[i],fd=new FormData();fd.append('kitap',f,f.name);
  var x=new XMLHttpRequest();x.open('POST','/kitap');
  x.upload.onprogress=function(e){if(e.lengthComputable){bp.value=e.loaded*100/e.total}};
  x.onload=function(){bshow(x.responseText);upload(files,i+1)};
  x.onerror=function(){bshow(f.name+' yüklenemedi');bp.hidden=true};
  bp.hidden=false;bp.value=0;bshow((i+1)+'/'+files.length+' · '+f.name);x.send(fd);
}
document.getElementById('bf').onchange=function(e){upload(e.target.files,0);e.target.value=''};
loadBooks();
document.getElementById('f').onchange=function(e){
  var file=e.target.files[0];if(!file)return;
  var rd=new FileReader();rd.onload=function(){t.value=rd.result;show('Dosya yüklendi, Kaydet’e bas')};rd.readAsText(file,'utf-8');
};
})();
</script></body></html>
)HTML";
