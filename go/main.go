package main

import (
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"time"
)

var (
	addr    = ":8080"
	dataDir = "./data"
	maxSize = 100 << 20
)

func main() {
	if a := os.Getenv("ADDR"); a != "" {
		addr = a
	}
	if d := os.Getenv("DATA_DIR"); d != "" {
		dataDir = d
	}
	os.MkdirAll(dataDir, 0755)

	mux := http.NewServeMux()
	mux.HandleFunc("/", homePage)
	mux.HandleFunc("/upload", uploadFile)
	mux.HandleFunc("/f/", serveFile)
	mux.HandleFunc("/list", listFiles)

	log.Printf("listening on %s, data dir: %s", addr, dataDir)
	log.Fatal(http.ListenAndServe(addr, logMiddleware(mux)))
}

func logMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		next.ServeHTTP(w, r)
		log.Printf("%s %s %s", r.Method, r.URL.Path, time.Since(start))
	})
}

func homePage(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/" {
		http.NotFound(w, r)
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	fmt.Fprint(w, pageHTML)
}

func uploadFile(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	r.Body = http.MaxBytesReader(w, r.Body, int64(maxSize))
	if err := r.ParseMultipartForm(32 << 20); err != nil {
		http.Error(w, "file too large or bad form", http.StatusBadRequest)
		return
	}
	file, header, err := r.FormFile("file")
	if err != nil {
		http.Error(w, "missing file field", http.StatusBadRequest)
		return
	}
	defer file.Close()

	name := filepath.Base(header.Filename)
	if name == "" || name == "." || name == ".." {
		http.Error(w, "invalid filename", http.StatusBadRequest)
		return
	}
	dst, err := os.Create(filepath.Join(dataDir, name))
	if err != nil {
		http.Error(w, "server error", http.StatusInternalServerError)
		return
	}
	defer dst.Close()
	written, err := io.Copy(dst, file)
	if err != nil {
		os.Remove(dst.Name())
		http.Error(w, "write failed", http.StatusInternalServerError)
		return
	}
	fmt.Fprintf(w, "%s (%s)", name, fmtSize(written))
}

func serveFile(w http.ResponseWriter, r *http.Request) {
	name := filepath.Base(r.URL.Path)
	http.ServeFile(w, r, filepath.Join(dataDir, name))
}

func listFiles(w http.ResponseWriter, r *http.Request) {
	entries, err := os.ReadDir(dataDir)
	if err != nil {
		http.Error(w, "server error", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	w.Write([]byte{'['})
	for i, e := range entries {
		if e.IsDir() {
			continue
		}
		info, _ := e.Info()
		if i > 0 {
			w.Write([]byte{','})
		}
		fmt.Fprintf(w, `{"n":"%s","s":%d}`, e.Name(), info.Size())
	}
	w.Write([]byte{']'})
}

func fmtSize(b int64) string {
	switch {
	case b >= 1<<30:
		return fmt.Sprintf("%.1f GB", float64(b)/(1<<30))
	case b >= 1<<20:
		return fmt.Sprintf("%.1f MB", float64(b)/(1<<20))
	case b >= 1<<10:
		return fmt.Sprintf("%.1f KB", float64(b)/(1<<10))
	default:
		return fmt.Sprintf("%d B", b)
	}
}

const pageHTML = `<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>File Server</title>
<style>
*{box-sizing:border-box}
body{font-family:system-ui,sans-serif;max-width:800px;margin:0 auto;padding:20px;background:#f5f5f5}
h1{color:#333}
.card{background:#fff;padding:20px;border-radius:8px;box-shadow:0 1px 3px rgba(0,0,0,0.1);margin-bottom:20px}
input[type=file]{display:block;margin:10px 0}
button{background:#4f46e5;color:#fff;border:none;padding:10px 20px;border-radius:6px;cursor:pointer}
button:hover{background:#4338ca}
#st{margin-top:10px;white-space:pre-wrap}
ul{list-style:none;padding:0}
li{padding:8px 0;border-bottom:1px solid #eee;display:flex;justify-content:space-between}
a{color:#4f46e5;text-decoration:none}
a:hover{text-decoration:underline}
.sz{color:#666;font-size:0.9em}
</style>
</head>
<body>
<h1>File Server</h1>
<div class="card">
	<h2>Upload</h2>
	<form id="fm" enctype="multipart/form-data">
		<input type="file" name="file" id="file" required>
		<button type="submit">Upload</button>
	</form>
	<div id="st"></div>
</div>
<div class="card">
	<h2>Files</h2>
	<ul id="fl"><li>Loading...</li></ul>
</div>
<script>
const sz = b => b >= 1073741824 ? (b/1073741824).toFixed(1)+' GB' : b >= 1048576 ? (b/1048576).toFixed(1)+' MB' : b >= 1024 ? (b/1024).toFixed(1)+' KB' : b+' B';
document.getElementById('fm').onsubmit = async function(e) {
	e.preventDefault();
	const st = document.getElementById('st');
	const fd = new FormData();
	fd.append('file', document.getElementById('file').files[0]);
	st.textContent = 'Uploading...';
	try {
		const r = await fetch('/upload', {method:'POST',body:fd});
		st.textContent = r.ok ? 'OK: ' + await r.text() : 'Error: ' + await r.text();
		ls();
	} catch(e){st.textContent='Error: '+e;}
};
async function ls() {
	try {
		const r = await fetch('/list');
		const a = await r.json();
		const ul = document.getElementById('fl');
		ul.innerHTML = a.length === 0 ? '<li>No files</li>' : a.map(f => '<li><a href="/f/'+f.n+'">'+f.n+'</a><span class="sz">'+sz(f.s)+'</span></li>').join('');
	} catch(e){}
}
ls();
</script>
</body>
</html>`
