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

sock_t tcp_connect(const char* host, int port) {
    wsa_init();
    struct sockaddr_in addr; memset(&addr,0,sizeof addr);
    addr.sin_family=AF_INET; addr.sin_port=htons((unsigned short)port);
    struct hostent *he=gethostbyname(host);
    if(!he)return SOCK_INV;
    memcpy(&addr.sin_addr,he->h_addr_list[0],he->h_length);
    sock_t s=socket(AF_INET,SOCK_STREAM,0);
    if(s==SOCK_INV)return SOCK_INV;
    if(connect(s,(struct sockaddr*)&addr,sizeof addr)<0){sock_close(s);return SOCK_INV;}
    return s;
}

void send_all(sock_t s,const char* buf,int len){
    int sent=0; while(sent<len){int n=send(s,buf+sent,len-sent,0);if(n<=0)return;sent+=n;}
}

int sock_getbyte(sock_t s){
    unsigned char c; return (recv(s,(char*)&c,1,0)==1)?(int)c:-1;
}

int sock_readline(sock_t s,char* buf,int maxlen){
    int i=0;
    while(i<maxlen-1){int c=sock_getbyte(s);if(c<0)break;if(c=='\n')break;if(c!='\r')buf[i++]=(char)c;}
    buf[i]='\0'; return i;
}

void sock_skip_headers(sock_t s){
    char line[1024];
    while(sock_readline(s,line,sizeof line)>0);
}

/* ── JSON helpers ────────────────────────────────────────────── */
static char _jsbuf[65536];

const char* js_str(const char* json, const char* key){
    char pat[128]; snprintf(pat,sizeof pat,"\"%s\":",key);
    const char* p=strstr(json,pat); if(!p)return NULL;
    p+=strlen(pat); while(*p==' ')p++;
    if(*p!='"')return NULL; p++;
    int i=0;
    while(*p&&i<(int)sizeof(_jsbuf)-2){
        if(*p=='\\'){p++;switch(*p){
            case 'n':_jsbuf[i++]='\n';break; case 't':_jsbuf[i++]='\t';break;
            case '"':_jsbuf[i++]='"'; break; case '\\':_jsbuf[i++]='\\';break;
            default: _jsbuf[i++]=*p;break;
        }}else if(*p=='"'){break;}else{_jsbuf[i++]=*p;}
        p++;
    }
    _jsbuf[i]='\0'; return _jsbuf;
}

void js_encode(const char* src, char* dst, int dmax){
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

/* ══════════════════════════════════════════════════════════════
   SEZIONE 5 — HTTP backends (Ollama NDJSON + llama-server SSE)
   ══════════════════════════════════════════════════════════════ */

/* Ollama /api/chat stream:true — NDJSON */
int http_ollama_stream(const char* host,int port,const char* model,
                       const char* sys_p,const char* usr,
                       lw_token_cb cb,void* ud,char* out,int outmax){
    char *se=malloc(131072),*ue=malloc(131072);
    if(!se||!ue){free(se);free(ue);return -1;}
    js_encode(sys_p,se,131072); js_encode(usr,ue,131072);
    char *body=malloc(strlen(se)+strlen(ue)+512);
    if(!body){free(se);free(ue);return -1;}
    sprintf(body,"{\"model\":\"%s\",\"messages\":["
                 "{\"role\":\"system\",\"content\":\"%s\"},"
                 "{\"role\":\"user\",\"content\":\"%s\"}"
                 "],\"stream\":true}",model,se,ue);
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
    while(1){
        int c=sock_getbyte(s); if(c<0)break;
        if(c=='\n'){
            line[li]='\0'; li=0;
            if(line[0]!='{')continue;
            const char *mp=strstr(line,"\"message\":");
            if(mp){const char *tok=js_str(mp,"content");
                if(tok&&tok[0]){
                    if(cb)cb(tok,ud);
                    if(out){int tl=strlen(tok);
                        if(ow+tl<outmax-1){memcpy(out+ow,tok,tl);ow+=tl;out[ow]='\0';}}}}
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
    js_encode(sys_p,se,131072); js_encode(usr,ue,131072);
    char *body=malloc(strlen(se)+strlen(ue)+512);
    if(!body){free(se);free(ue);return -1;}
    sprintf(body,"{\"model\":\"%s\",\"messages\":["
                 "{\"role\":\"system\",\"content\":\"%s\"},"
                 "{\"role\":\"user\",\"content\":\"%s\"}"
                 "],\"stream\":true}",model,se,ue);
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
    while(1){
        int c=sock_getbyte(s); if(c<0)break;
        if(c=='\n'){
            line[li]='\0'; li=0;
            if(!line[0])continue;
            if(strcmp(line,"data: [DONE]")==0)break;
            if(strncmp(line,"data: ",6)!=0)continue;
            const char *json=line+6;
            /* trova choices[0].delta.content */
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

/* Elenca modelli da Ollama /api/tags */
int http_ollama_list(const char* host,int port,char names[][128],int max){
    char req[256];
    int rlen=snprintf(req,sizeof req,"GET /api/tags HTTP/1.0\r\nHost: %s:%d\r\n\r\n",host,port);
    sock_t s=tcp_connect(host,port); if(s==SOCK_INV)return 0;
    send_all(s,req,rlen);
    char *resp=malloc(131072); if(!resp){sock_close(s);return 0;}
    int total=0,n;
    while(total<131070&&(n=recv(s,resp+total,131070-total,0))>0)total+=n;
    resp[total]='\0'; sock_close(s);
    char *body=strstr(resp,"\r\n\r\n");
    if(!body){free(resp);return 0;} body+=4;
    int count=0;
    const char *p=strstr(body,"\"models\"");
    while(p&&count<max){
        p=strstr(p,"\"name\":"); if(!p)break;
        const char *v=js_str(p,"name");
        if(v&&v[0]){strncpy(names[count],v,127);names[count][127]='\0';count++;}
        p++;
    }
    free(resp); return count;
}

/* Elenca modelli da llama-server /v1/models */
int http_llamaserver_list(const char* host,int port,char names[][128],int max){
    char req[256];
    int rlen=snprintf(req,sizeof req,"GET /v1/models HTTP/1.0\r\nHost: %s:%d\r\n\r\n",host,port);
    sock_t s=tcp_connect(host,port); if(s==SOCK_INV)return 0;
    send_all(s,req,rlen);
    char *resp=malloc(65536); if(!resp){sock_close(s);return 0;}
    int total=0,n;
    while(total<65534&&(n=recv(s,resp+total,65534-total,0))>0)total+=n;
    resp[total]='\0'; sock_close(s);
    char *body=strstr(resp,"\r\n\r\n");
    if(!body){free(resp);return 0;} body+=4;
    int count=0;
    const char *p=strstr(body,"\"data\"");
    while(p&&count<max){
        p=strstr(p,"\"id\":"); if(!p)break;
        const char *v=js_str(p,"id");
        if(v&&v[0]){strncpy(names[count],v,127);names[count][127]='\0';count++;}
        p++;
    }
    free(resp); return count;
}
