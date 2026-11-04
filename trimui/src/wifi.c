/* Wi-Fi Transfer: a tiny HTTP server (only while the screen is open) with a drag-and-drop
 * upload page. Uploads land in the inbox and are auto-sorted by sort.c. */
#include "rainey.h"
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <unistd.h>

char *sort_inbox(int *moved);

#define INBOX DATA_DIR "/inbox"

static SDL_Thread *srv;
static SDL_atomic_t srv_stop;
static int lfd = -1, port;
static SDL_mutex *mx;
static char logs[7][200];
static int nlogs;
static char cur_name[256];
static long long cur_done, cur_total;
static char ip[64];

static void add_log(const char *s) {
    SDL_LockMutex(mx);
    if (nlogs == 7) {
        memmove(logs[0], logs[1], sizeof logs[0] * 6);
        nlogs--;
    }
    snprintf(logs[nlogs++], sizeof logs[0], "%s", s);
    SDL_UnlockMutex(mx);
}

static void find_ip(void) {
    ip[0] = 0;
    struct ifaddrs *ifs, *i;
    if (getifaddrs(&ifs) != 0) return;
    for (i = ifs; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
        if (!strcmp(i->ifa_name, "lo")) continue;
        inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, ip, sizeof ip);
        if (!strcmp(i->ifa_name, "wlan0")) break;
    }
    freeifaddrs(ifs);
}

static long long free_mb(void) {
    struct statvfs v;
    if (statvfs("/mnt/UDISK", &v) != 0) return -1;
    return (long long)v.f_bavail * v.f_frsize / (1024 * 1024);
}

/* ---------------- upload page ---------------- */
static const char PAGE[] =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Send to Brick</title><style>"
    ":root{--bg:#050812;--panel:#111827;--blue:#21D4FD;--purple:#B83BFF;--pink:#FF4FD8;--text:#F4F7FB;--muted:#A7B0C0}"
    "*{box-sizing:border-box}body{margin:0;background:linear-gradient(#050812,#0A1024 60%,#150B24);"
    "color:var(--text);font:16px ui-monospace,Menlo,monospace;min-height:100vh;padding:24px 16px}"
    "main{max-width:760px;margin:0 auto}h1{margin:0 0 4px;font-size:32px}p{color:var(--muted);margin:0 0 20px}"
    "#drop{border:3px dashed var(--purple);border-radius:14px;padding:48px 16px;text-align:center;"
    "background:rgba(17,24,39,.8);transition:.15s}#drop.on{border-color:var(--blue);box-shadow:0 0 24px var(--blue)}"
    "#drop b{display:block;font-size:22px;margin-bottom:8px}"
    "button,label.btn{display:inline-block;margin:14px 6px 0;padding:10px 18px;border-radius:8px;border:2px solid var(--purple);"
    "background:var(--panel);color:var(--text);font:inherit;cursor:pointer}button:hover,label.btn:hover{border-color:var(--blue)}"
    "input[type=file]{display:none}ul{list-style:none;padding:0;margin:18px 0}"
    "li{background:rgba(17,24,39,.85);border:1px solid #2A3350;border-radius:8px;padding:10px 12px;margin:6px 0}"
    "li .n{display:flex;justify-content:space-between;gap:12px;word-break:break-all}"
    "li .bar{height:6px;background:#2A3350;border-radius:3px;margin-top:8px;overflow:hidden}"
    "li .bar i{display:block;height:100%;width:0;background:var(--blue)}li.done .bar i{background:#00FF9D}"
    "li.err{border-color:#FF6B9A}pre{white-space:pre-wrap;background:rgba(17,24,39,.85);border:1px solid var(--blue);"
    "border-radius:8px;padding:12px;color:var(--text)}#free{color:var(--muted)}"
    "</style></head><body><main><h1>Send to Brick</h1>"
    "<p>Drop games, BIOS files or pictures. They're sorted into the right folders automatically. "
    "<span id=free></span></p>"
    "<div id=drop><b>Drag files or folders here</b>"
    "<label class=btn>Choose files<input id=f type=file multiple></label>"
    "<label class=btn>Choose a folder<input id=d type=file webkitdirectory></label></div>"
    "<ul id=q></ul><pre id=out hidden></pre></main><script>"
    "const q=document.getElementById('q'),out=document.getElementById('out'),drop=document.getElementById('drop');"
    "let queue=[],busy=false;"
    "function info(){fetch('/api/info').then(r=>r.json()).then(j=>{document.getElementById('free').textContent="
    "'Free space: '+(j.free_mb/1024).toFixed(1)+' GB'}).catch(()=>{})}info();"
    "function fmt(b){return b>1073741824?(b/1073741824).toFixed(2)+' GB':(b/1048576).toFixed(1)+' MB'}"
    "function add(file,path){const li=document.createElement('li');li.innerHTML='<div class=n><span></span><span></span></div>"
    "<div class=bar><i></i></div>';li.querySelector('span').textContent=path;li.querySelectorAll('span')[1].textContent=fmt(file.size);"
    "q.appendChild(li);queue.push({file,path,li});if(!busy)next()}"
    "function next(){const it=queue.shift();if(!it){busy=false;sort();return}busy=true;const x=new XMLHttpRequest();"
    "x.open('PUT','/upload?path='+encodeURIComponent(it.path));"
    "x.upload.onprogress=e=>{if(e.lengthComputable)it.li.querySelector('i').style.width=(100*e.loaded/e.total)+'%'};"
    "x.onload=()=>{it.li.classList.add(x.status==200?'done':'err');it.li.querySelector('i').style.width='100%';next()};"
    "x.onerror=()=>{it.li.classList.add('err');next()};x.send(it.file)}"
    "function sort(){fetch('/api/sort',{method:'POST'}).then(r=>r.text()).then(t=>{out.hidden=false;out.textContent=t;info()})}"
    "function walk(entry,prefix){if(entry.isFile){entry.file(f=>add(f,prefix+f.name))}else if(entry.isDirectory){"
    "const r=entry.createReader();const read=()=>r.readEntries(es=>{if(es.length){es.forEach(e=>walk(e,prefix+entry.name+'/'));read()}});read()}}"
    "drop.ondragover=e=>{e.preventDefault();drop.classList.add('on')};drop.ondragleave=()=>drop.classList.remove('on');"
    "drop.ondrop=e=>{e.preventDefault();drop.classList.remove('on');out.hidden=true;"
    "for(const it of e.dataTransfer.items){const en=it.webkitGetAsEntry&&it.webkitGetAsEntry();if(en)walk(en,'');"
    "else if(it.kind=='file')add(it.getAsFile(),it.getAsFile().name)}};"
    "document.getElementById('f').onchange=e=>{out.hidden=true;for(const f of e.target.files)add(f,f.name)};"
    "document.getElementById('d').onchange=e=>{out.hidden=true;for(const f of e.target.files)add(f,f.webkitRelativePath||f.name)};"
    "</script></body></html>";

/* ---------------- HTTP ---------------- */

static void send_all(int fd, const char *b, size_t n) {
    while (n) {
        ssize_t w = send(fd, b, n, MSG_NOSIGNAL);
        if (w <= 0) return;
        b += w;
        n -= (size_t)w;
    }
}

static void respond(int fd, int code, const char *type, const char *body, size_t len) {
    char h[256];
    int n = snprintf(h, sizeof h,
                     "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                     "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                     code, code == 200 ? "OK" : "Error", type, len);
    send_all(fd, h, (size_t)n);
    send_all(fd, body, len);
}

static void url_decode(const char *s, char *out, size_t n) {
    size_t o = 0;
    while (*s && o + 1 < n) {
        if (*s == '%' && s[1] && s[2]) {
            char hx[3] = {s[1], s[2], 0};
            out[o++] = (char)strtol(hx, NULL, 16);
            s += 3;
        } else {
            out[o++] = *s == '+' ? ' ' : *s;
            s++;
        }
    }
    out[o] = 0;
}

/* inbox-relative path: no "..", no absolute, max 4 levels */
static bool safe_path(const char *p) {
    if (!*p || *p == '/' || strlen(p) > 400) return false;
    int depth = 0;
    const char *seg = p;
    for (const char *c = p;; c++) {
        if (*c == '/' || !*c) {
            size_t l = (size_t)(c - seg);
            if (!l || (seg[0] == '.' && (l == 1 || (l == 2 && seg[1] == '.')))) return false;
            if (++depth > 5) return false;
            if (!*c) break;
            seg = c + 1;
        }
        if ((unsigned char)*c < 32 || *c == '\\') return false;
    }
    return true;
}

static void handle_upload(int fd, const char *query, long long clen, const char *pre, size_t prelen) {
    char rel[512] = "";
    const char *pp = strstr(query, "path=");
    if (pp) url_decode(pp + 5, rel, sizeof rel);
    char *amp = strchr(rel, '&');
    if (amp) *amp = 0;
    if (!safe_path(rel) || clen < 0) {
        respond(fd, 400, "text/plain", "bad path", 8);
        return;
    }
    if (clen > free_mb() * 1024 * 1024 - 32 * 1024 * 1024) {
        respond(fd, 507, "text/plain", "not enough space", 16);
        add_log("Not enough space on the Brick");
        return;
    }
    char path[700], part[720];
    snprintf(path, sizeof path, "%s/%s", INBOX, rel);
    char *sl = strrchr(path, '/');
    *sl = 0;
    mkdirs(path);
    *sl = '/';
    snprintf(part, sizeof part, "%s.part", path);
    FILE *f = fopen(part, "wb");
    /* /mnt/UDISK is mounted "sync": write in 1 MB blocks, not per network packet */
    static char wbuf[1 << 20];
    if (f) setvbuf(f, wbuf, _IOFBF, sizeof wbuf);
    if (!f) {
        respond(fd, 500, "text/plain", "cannot write", 12);
        return;
    }
    SDL_LockMutex(mx);
    snprintf(cur_name, sizeof cur_name, "%s", rel);
    cur_total = clen;
    cur_done = 0;
    SDL_UnlockMutex(mx);
    static char buf[256 * 1024];
    long long got = 0;
    if (prelen) {
        size_t n = prelen > (size_t)clen ? (size_t)clen : prelen;
        fwrite(pre, 1, n, f);
        got = (long long)n;
    }
    while (got < clen && !SDL_AtomicGet(&srv_stop)) {
        long long want = clen - got;
        ssize_t r = recv(fd, buf, want < (long long)sizeof buf ? (size_t)want : sizeof buf, 0);
        if (r <= 0) break;
        fwrite(buf, 1, (size_t)r, f);
        got += r;
        cur_done = got;
    }
    fclose(f);
    SDL_LockMutex(mx);
    cur_name[0] = 0;
    SDL_UnlockMutex(mx);
    char msg[300];
    if (got == clen) {
        rename(part, path);
        snprintf(msg, sizeof msg, "Received %s", rel);
        add_log(msg);
        respond(fd, 200, "text/plain", "ok", 2);
    } else {
        remove(part);
        snprintf(msg, sizeof msg, "Interrupted: %s", rel);
        add_log(msg);
        respond(fd, 500, "text/plain", "interrupted", 11);
    }
}

static void handle(int fd) {
    struct timeval tv = {60, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    static char hdr[16384];
    size_t n = 0;
    char *end = NULL;
    while (n < sizeof hdr - 1) {
        ssize_t r = recv(fd, hdr + n, sizeof hdr - 1 - n, 0);
        if (r <= 0) return;
        n += (size_t)r;
        hdr[n] = 0;
        if ((end = strstr(hdr, "\r\n\r\n"))) break;
    }
    if (!end) return;
    char method[8] = "", target[1024] = "";
    sscanf(hdr, "%7s %1023s", method, target);
    long long clen = -1;
    for (char *l = strstr(hdr, "\r\n"); l && l < end; l = strstr(l + 2, "\r\n"))
        if (!strncasecmp(l + 2, "Content-Length:", 15)) clen = atoll(l + 17);
    char *q = strchr(target, '?');
    const char *query = q ? q + 1 : "";
    if (q) *q = 0;
    const char *body = end + 4;
    size_t prelen = n - (size_t)(body - hdr);

    if (!strcmp(method, "GET") && (!strcmp(target, "/") || !strcmp(target, "/index.html"))) {
        respond(fd, 200, "text/html; charset=utf-8", PAGE, sizeof PAGE - 1);
    } else if (!strcmp(method, "GET") && !strcmp(target, "/api/info")) {
        char j[96];
        int l = snprintf(j, sizeof j, "{\"free_mb\":%lld}", free_mb());
        respond(fd, 200, "application/json", j, (size_t)l);
    } else if (!strcmp(method, "PUT") && !strcmp(target, "/upload")) {
        handle_upload(fd, query, clen, body, prelen);
    } else if (!strcmp(method, "POST") && !strcmp(target, "/api/sort")) {
        int moved = 0;
        char *rep = sort_inbox(&moved);
        char msg[64];
        snprintf(msg, sizeof msg, "Sorted %d item%s", moved, moved == 1 ? "" : "s");
        add_log(msg);
        respond(fd, 200, "text/plain; charset=utf-8", rep ? rep : "", rep ? strlen(rep) : 0);
        free(rep);
    } else {
        respond(fd, 404, "text/plain", "not found", 9);
    }
}

static int server_fn(void *arg) {
    (void)arg;
    while (!SDL_AtomicGet(&srv_stop)) {
        fd_set fs;
        FD_ZERO(&fs);
        FD_SET(lfd, &fs);
        struct timeval tv = {0, 200000};
        if (select(lfd + 1, &fs, NULL, NULL, &tv) <= 0) continue;
        int c = accept(lfd, NULL, NULL);
        if (c < 0) continue;
        handle(c);
        shutdown(c, SHUT_WR);
        close(c);
    }
    return 0;
}

bool wifi_start(void) {
    if (srv) return true;
    if (!mx) mx = SDL_CreateMutex();
    nlogs = 0;
    find_ip();
    mkdirs(INBOX);
    const int ports[] = {80, 8080};
    for (int i = 0; i < 2; i++) {
        lfd = socket(AF_INET, SOCK_STREAM, 0);
        int one = 1;
        setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(ports[i])};
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(lfd, (struct sockaddr *)&a, sizeof a) == 0 && listen(lfd, 8) == 0) {
            port = ports[i];
            break;
        }
        close(lfd);
        lfd = -1;
    }
    if (lfd < 0) return false;
    SDL_AtomicSet(&srv_stop, 0);
    srv = SDL_CreateThread(server_fn, "wifi-xfer", NULL);
    /* Wi-Fi power saving roughly halves upload speed; off while this screen is open */
    if (system("iw dev wlan0 set power_save off >/dev/null 2>&1")) {}
    add_log("Ready. Waiting for files…");
    return true;
}

void wifi_stop(void) {
    if (!srv) return;
    SDL_AtomicSet(&srv_stop, 1);
    SDL_WaitThread(srv, NULL);
    srv = NULL;
    close(lfd);
    lfd = -1;
    if (system("iw dev wlan0 set power_save on >/dev/null 2>&1")) {}
}

/* ---------------- screen ---------------- */

void wifi_screen_draw(void) {
    gfx_text(F_HUGE, "Wi-Fi Transfer", 28, 18, C_TEXT, 255);
    gfx_text(F_BODY, "Send games and pictures from any computer or phone", 28, 76, C_MUTED, 255);
    gfx_neon_button(SCREEN_W - 28 - 120, 24, 120, 56, "B Done", false);
    int y = 130;
    gfx_round_fill(28, y, SCREEN_W - 56, 220, 12, C_PANEL, 230);
    gfx_round_ring(28, y, SCREEN_W - 56, 220, 12, 2, C_NEON_BLUE, 255);
    if (!srv) {
        gfx_text(F_TITLE, "Couldn't start the transfer server", 56, y + 30, 0xFFC7D8, 255);
        gfx_text(F_BODY, "Restart RaineyUI and try again.", 56, y + 90, C_MUTED, 255);
    } else if (!ip[0]) {
        gfx_text(F_TITLE, "Wi-Fi is off or not connected", 56, y + 30, 0xFFC7D8, 255);
        gfx_text_wrap(F_BODY,
                      "Connect the Brick to Wi-Fi in the TrimUI settings, then open this screen again. "
                      "With the USB cable you can use the Send to Brick app on your Mac instead.",
                      56, y + 90, SCREEN_W - 112, 3, 30, C_MUTED, 255);
    } else {
        gfx_text(F_BODY, "On a computer or phone on the same Wi-Fi, open:", 56, y + 24, C_MUTED, 255);
        char url[96];
        if (port == 80) snprintf(url, sizeof url, "http://%s", ip);
        else snprintf(url, sizeof url, "http://%s:%d", ip, port);
        gfx_glow(56, y + 70, gfx_text_w(F_HUGE, url) + 32, 70, 10, C_NEON_BLUE, 0.6f);
        gfx_text(F_HUGE, url, 72, y + 78, C_NEON_BLUE, 255);
        gfx_text(F_SMALL, "Best for small files. For big disc games use the USB cable + Send to Brick.",
                 56, y + 168, C_MUTED, 255);
    }
    y += 244;
    char fb[64];
    snprintf(fb, sizeof fb, "Free space: %.1f GB", free_mb() / 1024.0);
    gfx_text(F_BODY, fb, 28, y, C_MUTED, 255);
    y += 40;
    SDL_LockMutex(mx);
    if (cur_name[0]) {
        gfx_text_ellipsis(F_BODY, cur_name, 28, y, SCREEN_W - 200, C_TEXT, 255);
        int pct = cur_total > 0 ? (int)(cur_done * 100 / cur_total) : 0;
        char p[16];
        snprintf(p, sizeof p, "%d%%", pct);
        gfx_text(F_BODY, p, SCREEN_W - 28 - gfx_text_w(F_BODY, p), y, C_NEON_BLUE, 255);
        gfx_round_fill(28, y + 34, SCREEN_W - 56, 10, 5, 0x2A3350, 255);
        gfx_round_fill(28, y + 34, (SCREEN_W - 56) * pct / 100, 10, 5, C_NEON_BLUE, 255);
    }
    y += 64;
    for (int i = 0; i < nlogs; i++)
        gfx_text_ellipsis(F_SMALL, logs[i], 28, y + i * 26, SCREEN_W - 56,
                          i == nlogs - 1 ? C_TEXT : C_MUTED, 255);
    SDL_UnlockMutex(mx);
    static const char *h[] = {"B", "Stop and go back"};
    ui_hints(h, 1);
}

bool wifi_screen_input(InputState *in) {
    if (in->pressed[B_B]) {
        wifi_stop();
        return true;
    }
    return false;
}
