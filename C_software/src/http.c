/*
 * http.c — SEZIONE 4+5: TCP/HTTP helpers, JSON, Ollama NDJSON, llama-server SSE
 */
#include "common.h"
#include "http.h"

/* ══════════════════════════════════════════════════════════════
   SEZIONE 4 — TCP/HTTP helpers
   ══════════════════════════════════════════════════════════════ */

#ifdef _WIN32
static int _wsa_ok=0;
static void wsa_init(void){if(!_wsa_ok){WSADATA w;WSAStartup(MAKEWORD(2,2),&w);_wsa_ok=1;}}
#else
#define wsa_init() (void)0
#endif

/* ── Controllo anti-data-leak: solo connessioni verso localhost ──────────
   Impedisce che dati utente (prompt, task, documenti) vengano inviati
   accidentalmente verso host esterni.  Restituisce 1 se l'host è locale. */
static int _host_is_local(const char* host) {
    if (!host) return 0;
    if (strcmp(host, "127.0.0.1") == 0) return 1;
    if (strcmp(host, "::1")       == 0) return 1;
    if (strcasecmp(host, "localhost") == 0) return 1;
    return 0;
}

sock_t tcp_connect(const char* host, int port) {
    wsa_init();
    /* Blocca connessioni verso host non-locali: data-leak prevention */
    if (!_host_is_local(host)) {
        fprintf(stderr, "[SECURITY] tcp_connect: host '%s' non è localhost — "
                        "connessione bloccata (data-leak prevention).\n",
                host ? host : "(null)");
        return SOCK_INV;
    }
    /* getaddrinfo() è thread-safe (a differenza di gethostbyname) e supporta IPv6 */
    char port_str[8];
    snprintf(port_str, sizeof port_str, "%d", port);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;   /* IPv4 o IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port_str, &hints, &res) != 0) return SOCK_INV;
    sock_t s = SOCK_INV;
    for (struct addrinfo* p = res; p; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == SOCK_INV) continue;
#ifdef _WIN32
        /* Connessione non-bloccante con timeout 5 s — evita blocco infinito se server down */
        { u_long nb = 1; ioctlsocket(s, FIONBIO, &nb); }
        int cr = connect(s, p->ai_addr, (int)p->ai_addrlen);
        if (cr == 0) { /* connessione immediata */
            u_long nb0 = 0; ioctlsocket(s, FIONBIO, &nb0);
            break;
        }
        if (WSAGetLastError() != WSAEWOULDBLOCK) { sock_close(s); s = SOCK_INV; continue; }
        { fd_set wfds; FD_ZERO(&wfds); FD_SET(s, &wfds);
          struct timeval tv = {5, 0};
          int r = select(0, NULL, &wfds, NULL, &tv);
          u_long nb0 = 0; ioctlsocket(s, FIONBIO, &nb0);
          if (r <= 0) { sock_close(s); s = SOCK_INV; continue; }
        }
        break;
#else
        /* Connessione non-bloccante con timeout 5 s */
        int flags = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, flags | O_NONBLOCK);
        int cr = connect(s, p->ai_addr, p->ai_addrlen);
        if (cr == 0) { /* connessione immediata (es. localhost) */
            fcntl(s, F_SETFL, flags);
            break;
        }
        if (errno != EINPROGRESS) { sock_close(s); s = SOCK_INV; continue; }
        { fd_set wfds; FD_ZERO(&wfds); FD_SET(s, &wfds);
          struct timeval tv = {5, 0};
          if (select(s + 1, NULL, &wfds, NULL, &tv) <= 0) {
              sock_close(s); s = SOCK_INV; continue;
          }
          int err = 0; socklen_t elen = sizeof err;
          getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &elen);
          if (err) { sock_close(s); s = SOCK_INV; continue; }
        }
        fcntl(s, F_SETFL, flags); /* ripristina bloccante */
        break;
#endif
    }
    freeaddrinfo(res);
    return s;
}

/* Invia tutti i byte garantendo che send() non tronchi (può inviarne meno di len) */
void send_all(sock_t s,const char* buf,int len){
    int sent=0; while(sent<len){int n=send(s,buf+sent,len-sent,0);if(n<=0)return;sent+=n;}
}

/* Legge un singolo byte dal socket; restituisce -1 se la connessione è chiusa */
int sock_getbyte(sock_t s){
    unsigned char c; return (recv(s,(char*)&c,1,0)==1)?(int)c:-1;
}

/* Legge una riga dal socket (terminata da \n); scarta \r; restituisce lunghezza */
int sock_readline(sock_t s,char* buf,int maxlen){
    int i=0;
    while(i<maxlen-1){int c=sock_getbyte(s);if(c<0)break;if(c=='\n')break;if(c!='\r')buf[i++]=(char)c;}
    buf[i]='\0'; return i;
}

/* Scarta gli header HTTP leggendo righe fino alla riga vuota (\r\n\r\n) */
void sock_skip_headers(sock_t s){
    char line[1024];
    while(sock_readline(s,line,sizeof line)>0);
}

/* ── JSON helpers ────────────────────────────────────────────── */
/* Buffer thread-local: ogni thread ha la propria copia, evitando la race
 * condition quando i thread paralleli dei programmatori chiamano js_str()
 * contemporaneamente attraverso http_ollama_stream() / http_llamaserver_stream(). */
static __thread char _jsbuf[65536];

const char* js_str(const char* json, const char* key){
    if(!json||!key) return NULL;   /* guard: NULL input → NULL output (no crash) */
    /* Costruisce il pattern `"key":` e lo cerca nel testo JSON */
    char pat[128]; snprintf(pat,sizeof pat,"\"%s\":",key);
    const char* p=strstr(json,pat); if(!p)return NULL;
    p+=strlen(pat); while(*p==' ')p++;  /* salta eventuali spazi dopo i due punti */
    if(*p!='"') return NULL;
    p++;        /* avanza oltre la virgoletta di apertura del valore JSON */
    int i=0;
    /* Decodifica le escape sequence JSON più comuni */
    while(*p&&i<(int)sizeof(_jsbuf)-2){
        if(*p=='\\'){p++;switch(*p){
            case 'n':_jsbuf[i++]='\n';break; case 't':_jsbuf[i++]='\t';break;
            case '"':_jsbuf[i++]='"'; break; case '\\':_jsbuf[i++]='\\';break;
            default: _jsbuf[i++]=*p;break;
        }}else if(*p=='"'){break;}      /* virgoletta di chiusura stringa */
        else{_jsbuf[i++]=*p;}
        p++;
    }
    _jsbuf[i]='\0'; return _jsbuf;
}

void js_encode(const char* src, char* dst, int dmax){
    if(!dst||dmax<=0) return;          /* guard: dst NULL o buffer zero → no-op */
    if(!src){ dst[0]='\0'; return; }   /* guard: src NULL → stringa vuota */
    int j=0;
    for(const char* c=src;*c&&j<dmax-2;c++){
        if     (*c=='"') {dst[j++]='\\';dst[j++]='"';}
        else if(*c=='\\'){dst[j++]='\\';dst[j++]='\\';}
        else if(*c=='\n'){dst[j++]='\\';dst[j++]='n';}
        else if(*c=='\r'){dst[j++]='\\';dst[j++]='r';}
        else dst[j++]=*c;
    }
    dst[j]='\0';
}

/* ── Sanitizzazione anti-prompt-injection ────────────────────────────────
   Filtra sequenze che possono ingannare il modello a ignorare le istruzioni
   di sistema (role hijack) o a leakare il system prompt.
   Operazione sicura: usa solo memcpy/strcmp, no regex, O(N).

   Pattern bloccati:
     - "<|im_start|>system" / "<|im_end|>" — format token Qwen/ChatML
     - "###" usato come separatore di sezione da alcuni modelli
     - "\n\nSystem:" / "\n\nAssistant:" — common role override
     - "Ignore previous instructions" (case-insensitive, solo ASCII)
     - "[INST]" / "[/INST]" — Llama 2 format token
     - "<!-- " nei prompt (nasconde istruzioni in HTML-style)

   NON rimuove caratteri speciali legittimi: \n nel contenuto, virgolette
   in codice, Unicode, ecc.  La pulizia è conservativa: sostituisce il
   pattern con uno spazio oppure lo trunca prima del match.              */
void prompt_sanitize(char* buf, int bufmax) {
    if (!buf || bufmax <= 0) return;

    /* Tabella pattern da rimuovere (sostituzione con spazio) */
    static const char* const _INJECT_PATTERNS[] = {
        "<|im_start|>", "<|im_end|>", "<|endoftext|>",
        "[INST]", "[/INST]", "<<SYS>>", "<</SYS>>",
        "###",
        "<!-- ",
        NULL
    };
    /* Pattern role-override: "\n\n" + keyword — rimosso "\n\n" per spezzarlo */
    static const char* const _ROLE_OVERRIDE[] = {
        "\n\nSystem:", "\n\nAssistant:", "\n\nUser:",
        "\n\nHuman:", "\n\nAI:", "\n\nIgnore",
        NULL
    };

    int i;
    for (i = 0; _INJECT_PATTERNS[i]; i++) {
        const char* pat = _INJECT_PATTERNS[i];
        size_t plen = strlen(pat);
        char* p;
        while ((p = strstr(buf, pat)) != NULL) {
            /* Sostituisce il pattern con spazi per non alterare gli offset */
            memset(p, ' ', plen);
        }
    }
    for (i = 0; _ROLE_OVERRIDE[i]; i++) {
        const char* pat = _ROLE_OVERRIDE[i];
        char* p;
        while ((p = strstr(buf, pat)) != NULL) {
            /* Rimuove solo il doppio newline iniziale, lascia il testo */
            p[0] = ' '; p[1] = ' ';
        }
    }
    (void)bufmax;
}

/* ══════════════════════════════════════════════════════════════
   SEZIONE 5 — HTTP backends (Ollama NDJSON + llama-server SSE)
   ══════════════════════════════════════════════════════════════ */

/* Ollama /api/chat stream:true — NDJSON */
int http_ollama_stream(const char* host,int port,const char* model,
                       const char* sys_p,const char* usr,
                       lw_token_cb cb,void* ud,char* out,int outmax,
                       int keep_alive_secs){
    char *se=malloc(131072),*ue=malloc(131072);
    if(!se||!ue){free(se);free(ue);return -1;}
    /* js_encode: escaping JSON; prompt_sanitize: anti-injection sui contenuti */
    js_encode(sys_p,se,131072); prompt_sanitize(se,131072);
    js_encode(usr,ue,131072);   prompt_sanitize(ue,131072);
    /* Modello: solo caratteri sicuri (no virgolette o newline nel nome) */
    char me[256]; js_encode(model ? model : "", me, sizeof me);
    /* +strlen(me): evita heap overflow se il nome modello è lungo */
    size_t body_cap = strlen(se)+strlen(ue)+strlen(me)+512;
    char *body=malloc(body_cap);
    if(!body){free(se);free(ue);return -1;}
    /* keep_alive: -1 = lascia default Ollama (5m), 0 = scarica subito, >0 = N secondi.
     * Con RAM libera <20% il chiamante passa 0 per liberare memoria subito dopo la risposta. */
    if(keep_alive_secs < 0){
        snprintf(body,body_cap,"{\"model\":\"%s\",\"messages\":["
                     "{\"role\":\"system\",\"content\":\"%s\"},"
                     "{\"role\":\"user\",\"content\":\"%s\"}"
                     "],\"stream\":true}",me,se,ue);
    } else {
        snprintf(body,body_cap,"{\"model\":\"%s\",\"messages\":["
                     "{\"role\":\"system\",\"content\":\"%s\"},"
                     "{\"role\":\"user\",\"content\":\"%s\"}"
                     "],\"stream\":true,\"keep_alive\":%d}",me,se,ue,keep_alive_secs);
    }
    free(se); free(ue);
    int blen=strlen(body);
    char req[4096];
    int rlen=snprintf(req,sizeof req,
        "POST /api/chat HTTP/1.0\r\nHost: %s:%d\r\n"
        "Content-Type: application/json\r\nContent-Length: %d\r\n\r\n",
        host,port,blen);
    sock_t s=tcp_connect(host,port);
    if(s==SOCK_INV){free(body);return -1;}
    send_all(s,req,rlen); send_all(s,body,blen); free(body);
    sock_skip_headers(s);
    char *line=malloc(65536); if(!line){sock_close(s);return -1;}
    int li=0,ow=0; if(out)out[0]='\0';
    /* Parsing NDJSON: ogni riga è un oggetto JSON con {"message":{"content":"..."}, "done":...} */
    while(1){
        int c=sock_getbyte(s); if(c<0)break;
        if(c=='\n'){
            line[li]='\0'; li=0;
            if(line[0]!='{')continue;  /* salta righe vuote o non-JSON (header residui) */
            const char *mp=strstr(line,"\"message\":");
            if(mp){const char *tok=js_str(mp,"content");
                if(tok&&tok[0]){
                    if(cb)cb(tok,ud);  /* consegna il token all'interfaccia */
                    if(out){int tl=strlen(tok);
                        if(ow+tl<outmax-1){memcpy(out+ow,tok,tl);ow+=tl;out[ow]='\0';}}}}
            /* "done":true segnala fine stream (Ollama invia un ultimo oggetto con done=true) */
            const char *dp=strstr(line,"\"done\":");
            if(dp&&strncmp(dp+7,"true",4)==0)break;
        }else{if(li<65534)line[li++]=(char)c;}
    }
    free(line); sock_close(s); return 0;
}

/* llama-server /v1/chat/completions stream:true — SSE (OpenAI) */
int http_llamaserver_stream(const char* host,int port,const char* model,
                            const char* sys_p,const char* usr,
                            lw_token_cb cb,void* ud,char* out,int outmax){
    char *se=malloc(131072),*ue=malloc(131072);
    if(!se||!ue){free(se);free(ue);return -1;}
    js_encode(sys_p,se,131072); prompt_sanitize(se,131072);
    js_encode(usr,ue,131072);   prompt_sanitize(ue,131072);
    char me[256]; js_encode(model ? model : "", me, sizeof me);
    size_t body_cap = strlen(se)+strlen(ue)+strlen(me)+512;
    char *body=malloc(body_cap);
    if(!body){free(se);free(ue);return -1;}
    snprintf(body,body_cap,"{\"model\":\"%s\",\"messages\":["
                 "{\"role\":\"system\",\"content\":\"%s\"},"
                 "{\"role\":\"user\",\"content\":\"%s\"}"
                 "],\"stream\":true}",me,se,ue);
    free(se); free(ue);
    int blen=strlen(body);
    char req[4096];
    int rlen=snprintf(req,sizeof req,
        "POST /v1/chat/completions HTTP/1.0\r\nHost: %s:%d\r\n"
        "Content-Type: application/json\r\nContent-Length: %d\r\n\r\n",
        host,port,blen);
    sock_t s=tcp_connect(host,port);
    if(s==SOCK_INV){free(body);return -1;}
    send_all(s,req,rlen); send_all(s,body,blen); free(body);
    sock_skip_headers(s);
    char *line=malloc(65536); if(!line){sock_close(s);return -1;}
    int li=0,ow=0; if(out)out[0]='\0';
    /* Parsing SSE (Server-Sent Events) formato OpenAI:
     * ogni riga di dati inizia con "data: " seguito da un oggetto JSON;
     * "data: [DONE]" segnala la fine dello stream */
    while(1){
        int c=sock_getbyte(s); if(c<0)break;
        if(c=='\n'){
            line[li]='\0'; li=0;
            if(!line[0])continue;                       /* riga vuota = separatore SSE */
            if(strcmp(line,"data: [DONE]")==0)break;    /* fine stream OpenAI */
            if(strncmp(line,"data: ",6)!=0)continue;    /* salta righe "event:", "id:", ecc. */
            const char *json=line+6;                    /* payload JSON dopo il prefisso */
            /* Il token è in choices[0].delta.content secondo le specifiche OpenAI */
            const char *dp=strstr(json,"\"delta\":");
            if(!dp)continue;
            const char *tok=js_str(dp,"content");
            if(tok&&tok[0]){
                if(cb)cb(tok,ud);
                if(out){int tl=strlen(tok);
                    if(ow+tl<outmax-1){memcpy(out+ow,tok,tl);ow+=tl;out[ow]='\0';}}}
        }else{if(li<65534)line[li++]=(char)c;}
    }
    free(line); sock_close(s); return 0;
}

/* Elenca modelli da Ollama GET /api/tags → JSON {"models":[{"name":"..."},…]} */
int http_ollama_list(const char* host,int port,char names[][128],int max){
    char req[256];
    int rlen=snprintf(req,sizeof req,"GET /api/tags HTTP/1.0\r\nHost: %s:%d\r\n\r\n",host,port);
    sock_t s=tcp_connect(host,port); if(s==SOCK_INV)return 0;
    send_all(s,req,rlen);
    /* Legge tutta la risposta in un unico buffer (non in streaming) */
    char *resp=malloc(131072); if(!resp){sock_close(s);return 0;}
    int total=0,n;
    while(total<131070&&(n=recv(s,resp+total,131070-total,0))>0)total+=n;
    resp[total]='\0'; sock_close(s);
    char *body=strstr(resp,"\r\n\r\n"); /* salta gli header HTTP */
    if(!body){free(resp);return 0;} body+=4;
    int count=0;
    /* Itera sull'array "models" estraendo ogni campo "name" */
    const char *p=strstr(body,"\"models\"");
    while(p&&count<max){
        p=strstr(p,"\"name\":"); if(!p)break;
        const char *v=js_str(p,"name");
        if(v&&v[0]){strncpy(names[count],v,127);names[count][127]='\0';count++;}
        p++;
    }
    free(resp); return count;
}

/* Genera embedding tramite Ollama POST /api/embeddings — risposta non-streaming */
int http_ollama_embed(const char *host, int port, const char *embed_model,
                      const char *text, float *vec, int *out_dim, int max_dim) {
    if (!host || !embed_model || !text || !vec || !out_dim || max_dim <= 0) return -1;
    *out_dim = 0;

    /* Codifica testo e nome modello per inclusione sicura in JSON */
    char *te = malloc(131072);
    if (!te) return -1;
    js_encode(text, te, 131072);
    prompt_sanitize(te, 131072);
    char me[256]; js_encode(embed_model, me, sizeof me);

    /* Body JSON: Ollama /api/embeddings usa "prompt" (non "messages") */
    size_t body_cap = strlen(te) + strlen(me) + 64;
    char *body = malloc(body_cap);
    if (!body) { free(te); return -1; }
    snprintf(body, body_cap, "{\"model\":\"%s\",\"prompt\":\"%s\"}", me, te);
    free(te);

    int blen = (int)strlen(body);
    char req[512];
    int rlen = snprintf(req, sizeof req,
        "POST /api/embeddings HTTP/1.0\r\nHost: %s:%d\r\n"
        "Content-Type: application/json\r\nContent-Length: %d\r\n\r\n",
        host, port, blen);

    sock_t s = tcp_connect(host, port);
    if (s == SOCK_INV) { free(body); return -1; }
    send_all(s, req, rlen);
    send_all(s, body, blen);
    free(body);

    /* Legge tutta la risposta: un vettore 768-dim occupa ~8 KB in JSON */
    int resp_cap = max_dim * 14 + 4096;
    char *resp = malloc(resp_cap);
    if (!resp) { sock_close(s); return -1; }
    int total = 0, n;
    while (total < resp_cap - 1 && (n = recv(s, resp + total, resp_cap - 1 - total, 0)) > 0)
        total += n;
    resp[total] = '\0';
    sock_close(s);

    /* Trova il body JSON dopo gli header HTTP */
    char *bstart = strstr(resp, "\r\n\r\n");
    if (!bstart) { free(resp); return -1; }
    bstart += 4;

    /* Cerca "embedding":[ nel body — modello non-embedding ritorna {"error":"..."} */
    const char *p = strstr(bstart, "\"embedding\":[");
    if (!p) { free(resp); return -1; }
    p += 13; /* avanza oltre "embedding":[ */

    /* Parsa array float: salta spazi/virgole, legge numeri con strtof */
    int dim = 0;
    while (*p && *p != ']' && dim < max_dim) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',') p++;
        if (*p == ']' || *p == '\0') break;
        char *end;
        float v = strtof(p, &end);
        if (end == p) break;
        vec[dim++] = v;
        p = end;
    }

    free(resp);
    if (dim == 0) return -1;
    *out_dim = dim;
    return 0;
}

/* Elenca modelli da llama-server GET /v1/models → JSON {"data":[{"id":"..."},…]} */
int http_llamaserver_list(const char* host,int port,char names[][128],int max){
    char req[256];
    int rlen=snprintf(req,sizeof req,"GET /v1/models HTTP/1.0\r\nHost: %s:%d\r\n\r\n",host,port);
    sock_t s=tcp_connect(host,port); if(s==SOCK_INV)return 0;
    send_all(s,req,rlen);
    char *resp=malloc(65536); if(!resp){sock_close(s);return 0;}
    int total=0,n;
    while(total<65534&&(n=recv(s,resp+total,65534-total,0))>0)total+=n;
    resp[total]='\0'; sock_close(s);
    char *body=strstr(resp,"\r\n\r\n"); /* salta gli header HTTP */
    if(!body){free(resp);return 0;} body+=4;
    int count=0;
    /* L'API OpenAI usa il campo "id" (non "name") per identificare i modelli */
    const char *p=strstr(body,"\"data\"");
    while(p&&count<max){
        p=strstr(p,"\"id\":"); if(!p)break;
        const char *v=js_str(p,"id");
        if(v&&v[0]){strncpy(names[count],v,127);names[count][127]='\0';count++;}
        p++;
    }
    free(resp); return count;
}
