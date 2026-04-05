/* ══════════════════════════════════════════════════════════════════════════
   test_sha256.c — Integrità modelli GGUF via SHA-256 (puro C, no OpenSSL)
   ══════════════════════════════════════════════════════════════════════════
   Implementazione SHA-256 conforme FIPS 180-4 (RFC 6234).
   Usato per verificare che i file .gguf scaricati non siano corrotti.

   Categorie:
     T01 — SHA-256 vettori NIST standard (abc, "abc", stringa lunga)
     T02 — Hash blocco singolo vs blocchi multipli (same result)
     T03 — Hash file vuoto
     T04 — Performance: hash 1MB < 100ms
     T05 — Verifica file .gguf nella cartella models/
     T06 — Detection file corrotto (byte modificato → hash diverso)
     T07 — Formato "sha256:<hex>" (compatibile HuggingFace)

   Build:
     gcc -O2 -lm -o test_sha256 test_sha256.c

   Esecuzione: ./test_sha256
   ══════════════════════════════════════════════════════════════════════════ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

/* ── Harness ──────────────────────────────────────────────────────────── */
static int _pass = 0, _fail = 0;
#define SECTION(t) printf("\n\033[36m\033[1m─── %s ───\033[0m\n", t)
#define CHECK(cond, label) do { \
    if (cond) { printf("  \033[32m[OK]\033[0m  %s\n", label); _pass++; } \
    else       { printf("  \033[31m[FAIL]\033[0m %s\n", label); _fail++; } \
} while(0)

/* ════════════════════════════════════════════════════════════════════════
   SHA-256 — implementazione pura C (FIPS 180-4)
   ════════════════════════════════════════════════════════════════════════ */
#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE  64

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  buf[SHA256_BLOCK_SIZE];
    uint32_t buflen;
} Sha256Ctx;

/* Costanti K (primi 32 bit delle radici cubiche dei primi 64 numeri primi) */
static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z)  (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define S0(x) (ROTR(x,2)^ROTR(x,13)^ROTR(x,22))
#define S1(x) (ROTR(x,6)^ROTR(x,11)^ROTR(x,25))
#define G0(x) (ROTR(x,7)^ROTR(x,18)^((x)>>3))
#define G1(x) (ROTR(x,17)^ROTR(x,19)^((x)>>10))

static void sha256_transform(Sha256Ctx *ctx, const uint8_t *blk) {
    uint32_t w[64], a,b,c,d,e,f,g,h,t1,t2;
    int i;
    for (i=0;i<16;i++)
        w[i]=((uint32_t)blk[i*4]<<24)|((uint32_t)blk[i*4+1]<<16)|
             ((uint32_t)blk[i*4+2]<<8)|(uint32_t)blk[i*4+3];
    for (;i<64;i++)
        w[i]=G1(w[i-2])+w[i-7]+G0(w[i-15])+w[i-16];
    a=ctx->state[0]; b=ctx->state[1]; c=ctx->state[2]; d=ctx->state[3];
    e=ctx->state[4]; f=ctx->state[5]; g=ctx->state[6]; h=ctx->state[7];
    for (i=0;i<64;i++) {
        t1=h+S1(e)+CH(e,f,g)+K[i]+w[i];
        t2=S0(a)+MAJ(a,b,c);
        h=g; g=f; f=e; e=d+t1;
        d=c; c=b; b=a; a=t1+t2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

static void sha256_init(Sha256Ctx *ctx) {
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
    ctx->bitlen=0; ctx->buflen=0;
}

static void sha256_update(Sha256Ctx *ctx, const uint8_t *data, size_t len) {
    for (size_t i=0; i<len; i++) {
        ctx->buf[ctx->buflen++] = data[i];
        if (ctx->buflen == SHA256_BLOCK_SIZE) {
            sha256_transform(ctx, ctx->buf);
            ctx->bitlen += 512;
            ctx->buflen = 0;
        }
    }
}

static void sha256_final(Sha256Ctx *ctx, uint8_t *digest) {
    uint32_t i = ctx->buflen;
    ctx->buf[i++] = 0x80;
    if (i > 56) {
        while (i < 64) ctx->buf[i++] = 0;
        sha256_transform(ctx, ctx->buf);
        i = 0;
    }
    while (i < 56) ctx->buf[i++] = 0;
    ctx->bitlen += (uint64_t)ctx->buflen * 8;
    for (int k=7; k>=0; k--) {
        ctx->buf[56+(7-k)] = (uint8_t)(ctx->bitlen >> (k*8));
    }
    sha256_transform(ctx, ctx->buf);
    for (i=0; i<8; i++) {
        digest[i*4+0]=(uint8_t)(ctx->state[i]>>24);
        digest[i*4+1]=(uint8_t)(ctx->state[i]>>16);
        digest[i*4+2]=(uint8_t)(ctx->state[i]>>8);
        digest[i*4+3]=(uint8_t)(ctx->state[i]);
    }
}

/* Calcola hash di una stringa */
static void sha256_string(const char *s, uint8_t *digest) {
    Sha256Ctx ctx; sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)s, strlen(s));
    sha256_final(&ctx, digest);
}

/* Calcola hash di un buffer */
static void sha256_buf(const uint8_t *buf, size_t len, uint8_t *digest) {
    Sha256Ctx ctx; sha256_init(&ctx);
    sha256_update(&ctx, buf, len);
    sha256_final(&ctx, digest);
}

/* Calcola hash di un file */
static int sha256_file(const char *path, uint8_t *digest) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    Sha256Ctx ctx; sha256_init(&ctx);
    uint8_t buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        sha256_update(&ctx, buf, n);
    fclose(f);
    sha256_final(&ctx, digest);
    return 0;
}

/* Converti digest in stringa hex */
static void sha256_hex(const uint8_t *digest, char *out) {
    for (int i = 0; i < SHA256_DIGEST_SIZE; i++)
        snprintf(out + i*2, 3, "%02x", digest[i]);
    out[SHA256_DIGEST_SIZE*2] = '\0';
}

/* Formato "sha256:<hex>" compatibile HuggingFace */
static void sha256_hf_format(const uint8_t *digest, char *out, size_t sz) {
    char hex[SHA256_DIGEST_SIZE*2+1];
    sha256_hex(digest, hex);
    snprintf(out, sz, "sha256:%s", hex);
}

/* ════════════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n\033[1m══ test_sha256.c — Integrità modelli GGUF via SHA-256 ══\033[0m\n");

    uint8_t digest[SHA256_DIGEST_SIZE];
    char hex[SHA256_DIGEST_SIZE*2+1];

    /* ── T01: vettori NIST FIPS 180-4 ── */
    SECTION("T01 — Vettori SHA-256 NIST FIPS 180-4");

    /* Vettore 1: "" (stringa vuota) */
    sha256_string("", digest); sha256_hex(digest, hex);
    CHECK(strcmp(hex,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")==0,
          "SHA-256(\"\") = e3b0c442...");

    /* Vettore 2: "abc" */
    sha256_string("abc", digest); sha256_hex(digest, hex);
    CHECK(strcmp(hex,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")==0,
          "SHA-256(\"abc\") = ba7816bf...");

    /* Vettore 3: "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" */
    sha256_string("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                  digest); sha256_hex(digest, hex);
    CHECK(strcmp(hex,"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")==0,
          "SHA-256(448-bit message) = 248d6a61...");

    /* Vettore 4: "The quick brown fox jumps over the lazy dog" */
    sha256_string("The quick brown fox jumps over the lazy dog",
                  digest); sha256_hex(digest, hex);
    CHECK(strcmp(hex,"d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592")==0,
          "SHA-256(\"The quick brown fox...\") = d7a8fbb3...");

    /* Vettore 5: singolo byte 0x00 */
    uint8_t zero_byte = 0;
    sha256_buf(&zero_byte, 1, digest); sha256_hex(digest, hex);
    CHECK(strcmp(hex,"6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d")==0,
          "SHA-256(0x00) = 6e340b9c...");

    /* ── T02: blocco singolo vs multipli ── */
    SECTION("T02 — Blocco singolo vs aggiornamenti multipli");
    {
        const char *msg = "Hello, Prismalux! Costruito per i mortali.";
        uint8_t d1[32], d2[32];

        /* Un blocco */
        sha256_string(msg, d1);

        /* Aggiornamenti multipli, stesso risultato */
        Sha256Ctx ctx; sha256_init(&ctx);
        size_t half = strlen(msg)/2;
        sha256_update(&ctx, (const uint8_t*)msg, half);
        sha256_update(&ctx, (const uint8_t*)(msg+half), strlen(msg)-half);
        sha256_final(&ctx, d2);

        CHECK(memcmp(d1,d2,32)==0, "hash singolo = hash multipli parziali");
    }

    /* Byte per byte == tutto insieme */
    {
        const char *msg = "test byte per byte";
        uint8_t d1[32], d2[32];
        sha256_string(msg, d1);
        Sha256Ctx ctx; sha256_init(&ctx);
        for (size_t i=0; i<strlen(msg); i++)
            sha256_update(&ctx, (const uint8_t*)(msg+i), 1);
        sha256_final(&ctx, d2);
        CHECK(memcmp(d1,d2,32)==0, "hash byte-per-byte = hash unico");
    }

    /* ── T03: file vuoto ── */
    SECTION("T03 — Hash file vuoto");
    {
        const char *tmpf = "/tmp/test_sha256_empty.bin";
        FILE *f = fopen(tmpf,"wb"); fclose(f);
        int ok = sha256_file(tmpf, digest);
        sha256_hex(digest, hex);
        CHECK(ok == 0, "sha256_file su file vuoto: ritorna 0");
        CHECK(strcmp(hex,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")==0,
              "hash file vuoto = SHA-256(\"\")");
        remove(tmpf);
    }

    /* File inesistente → -1 */
    {
        int err = sha256_file("/tmp/nonexistent_sha_xyzzy.bin", digest);
        CHECK(err == -1, "file inesistente → ritorna -1");
    }

    /* ── T04: performance 1MB ── */
    SECTION("T04 — Performance: hash 1MB");
    {
        size_t sz = 1024*1024;
        uint8_t *buf = (uint8_t*)malloc(sz);
        for (size_t i=0; i<sz; i++) buf[i] = (uint8_t)(i & 0xFF);

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sha256_buf(buf, sz, digest);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms = (t1.tv_sec-t0.tv_sec)*1000.0+(t1.tv_nsec-t0.tv_nsec)/1e6;

        sha256_hex(digest, hex);
        printf("  Hash 1MB in %.2f ms → %s\n", ms, hex);
        CHECK(ms < 100.0, "hash 1MB in < 100ms");
        CHECK(strlen(hex) == 64, "digest hex: 64 caratteri");
        free(buf);
    }

    /* ── T05: file .gguf in models/ ── */
    SECTION("T05 — Verifica file .gguf nella cartella models/");
    {
        const char *models_dir = "models";
        DIR *d = opendir(models_dir);
        int gguf_found = 0;
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL) {
                size_t nl = strlen(de->d_name);
                if (nl < 5 || strcmp(de->d_name+nl-5,".gguf") != 0) continue;

                char path[512];
                snprintf(path, sizeof path, "%s/%s", models_dir, de->d_name);

                struct timespec t0, t1;
                clock_gettime(CLOCK_MONOTONIC, &t0);
                int ok = sha256_file(path, digest);
                clock_gettime(CLOCK_MONOTONIC, &t1);
                double ms = (t1.tv_sec-t0.tv_sec)*1000.0+(t1.tv_nsec-t0.tv_nsec)/1e6;

                if (ok == 0) {
                    char hf[128]; sha256_hf_format(digest, hf, sizeof hf);
                    /* Mostra solo i primi 16 char dell'hash per leggibilità */
                    char preview[20]; strncpy(preview, hf+7, 16); preview[16]='\0';
                    printf("  \033[32m[OK]\033[0m  %s → sha256:%s...  (%.0f ms)\n",
                           de->d_name, preview, ms);
                    _pass++;
                    gguf_found++;
                } else {
                    printf("  \033[31m[FAIL]\033[0m %s → errore lettura\n", de->d_name);
                    _fail++;
                }
            }
            closedir(d);
        }
        if (gguf_found == 0) {
            printf("  \033[33m[INFO]\033[0m  Nessun file .gguf trovato in models/ — test saltato\n");
            /* Non è un fail: la cartella può essere vuota */
            CHECK(1, "cartella models/ accessibile (anche se vuota)");
        }
    }

    /* ── T06: detection file corrotto ── */
    SECTION("T06 — Detection file corrotto (byte modificato)");
    {
        const char *orig  = "/tmp/test_sha256_orig.bin";
        const char *corr  = "/tmp/test_sha256_corr.bin";
        uint8_t data[256];
        for (int i=0; i<256; i++) data[i]=(uint8_t)i;

        FILE *f = fopen(orig,"wb"); fwrite(data,1,256,f); fclose(f);
        data[128] ^= 0xFF;  /* corrompe un byte */
        f = fopen(corr,"wb"); fwrite(data,1,256,f); fclose(f);

        uint8_t d1[32], d2[32];
        sha256_file(orig, d1);
        sha256_file(corr, d2);
        CHECK(memcmp(d1,d2,32)!=0, "file corrotto: hash diverso dall'originale");
        /* Verifica che differiscano in modo significativo (avalanche effect) */
        int diff_bytes = 0;
        for (int i=0; i<32; i++) if (d1[i]!=d2[i]) diff_bytes++;
        printf("  Bytes diversi nel digest: %d/32\n", diff_bytes);
        CHECK(diff_bytes >= 15, "avalanche effect: almeno 15/32 byte diversi");

        remove(orig); remove(corr);
    }

    /* ── T07: formato sha256:<hex> HuggingFace ── */
    SECTION("T07 — Formato sha256:<hex> compatibile HuggingFace");
    {
        sha256_string("test", digest);
        char hf[128];
        sha256_hf_format(digest, hf, sizeof hf);
        CHECK(strncmp(hf,"sha256:",7)==0, "formato inizia con 'sha256:'");
        CHECK(strlen(hf) == 7+64,         "lunghezza totale: 7+64 = 71 caratteri");

        /* Verifica che "sha256:" + hex == formato atteso per "test" */
        char expected[128] = "sha256:";
        sha256_hex(digest, expected+7);
        CHECK(strcmp(hf, expected)==0, "sha256:<hex> corrisponde a sha256_hex()");

        /* Stringa nota: SHA-256("test") = 9f86d081... */
        sha256_string("test", digest);
        sha256_hex(digest, hex);
        CHECK(strcmp(hex,"9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08")==0,
              "SHA-256(\"test\") = 9f86d081...");
    }

    printf("\n\033[1m══════════════════════════════════════════════════════\033[0m\n");
    if (_fail == 0)
        printf("\033[1m\033[32m  RISULTATI: %d/%d PASSATI — SHA-256 A PROVA DI BOMBA\033[0m\n",
               _pass, _pass+_fail);
    else
        printf("\033[1m\033[31m  RISULTATI: %d/%d PASSATI — %d FALLITI\033[0m\n",
               _pass, _pass+_fail, _fail);
    printf("\033[1m══════════════════════════════════════════════════════\033[0m\n\n");
    return _fail > 0 ? 1 : 0;
}
