/*
 * strumenti.c — SEZIONE 11: Tutor AI e Strumento Pratico
 * =======================================================
 * menu_tutor(), pratico_730(), pratico_piva(), menu_pratico()
 */
#include "common.h"
#include "backend.h"
#include "terminal.h"
#include "ai.h"
#include "strumenti.h"
#include "prismalux_ui.h"

/* ══════════════════════════════════════════════════════════════
   SEZIONE 11 — Tutor e Strumento Pratico
   ══════════════════════════════════════════════════════════════ */

void menu_tutor(void) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x8f\x9b\xef\xb8\x8f  Modalit\xc3\xa0 Oracolo \xe2\x80\x94 Tutor AI\n" RST);
    printf(GRY "  Scrivi la tua domanda. " EM_0 " o lascia vuoto per tornare.\n\n" RST);
    if(!backend_ready(&g_ctx)){ ensure_ready_or_return(&g_ctx); return; }
    if(!check_risorse_ok()) return;
    const char *sys="Sei un tutor esperto e paziente. Rispondi SEMPRE e SOLO in italiano in modo chiaro, "
                    "didattico e incoraggiante. Usa esempi pratici quando possibile.";
    char domanda[2048];
    while(1){
        printf(YLW "Tu: " RST); if(!fgets(domanda,sizeof domanda,stdin))break;
        domanda[strcspn(domanda,"\n")]='\0';
        if(!domanda[0]||domanda[0]=='0'||strcmp(domanda,"esci")==0||strcmp(domanda,"q")==0)break;
        printf(MAG "Oracolo: " RST);
        ai_chat_stream(&g_ctx,sys,domanda,stream_cb,NULL,NULL,0);
        printf("\n");
    }
}

static void pratico_730(void) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x93\x84  Assistente Dichiarazione 730\n" RST);
    printf(GRY "  Scrivi la tua domanda. " EM_0 " o lascia vuoto per tornare.\n\n" RST);
    if(!backend_ready(&g_ctx)){ ensure_ready_or_return(&g_ctx); return; }
    if(!check_risorse_ok()) return;
    const char *sys="Sei un esperto fiscalista italiano specializzato nella dichiarazione 730. "
                    "Rispondi SEMPRE e SOLO in italiano. "
                    "Fornisci risposte precise su deduzioni, detrazioni, codici e scadenze. "
                    "Avvisa sempre di consultare un professionista per casi specifici.";
    char q[2048];
    while(1){
        printf(YLW "Domanda 730: " RST); if(!fgets(q,sizeof q,stdin))break;
        q[strcspn(q,"\n")]='\0';
        if(!q[0]||q[0]=='0'||strcmp(q,"esci")==0)break;
        printf(MAG "AI Fiscalista: " RST);
        ai_chat_stream(&g_ctx,sys,q,stream_cb,NULL,NULL,0);
        printf("\n");
    }
}

static void pratico_piva(void) {
    clear_screen(); print_header();
    printf(CYN BLD "\n  \xf0\x9f\x92\xbc  Assistente Partita IVA & Forfettario\n" RST);
    printf(GRY "  Scrivi la tua domanda. " EM_0 " o lascia vuoto per tornare.\n\n" RST);
    if(!backend_ready(&g_ctx)){ ensure_ready_or_return(&g_ctx); return; }
    if(!check_risorse_ok()) return;
    const char *sys="Sei un consulente esperto in partita IVA e regime forfettario italiano. "
                    "Rispondi SEMPRE e SOLO in italiano. "
                    "Conosci coefficienti di redditivit\xc3\xa0, soglie ricavi, contributi INPS, "
                    "fatturazione elettronica. Fornisci calcoli pratici e spiegazioni chiare.";
    char q[2048];
    while(1){
        printf(YLW "Domanda P.IVA: " RST); if(!fgets(q,sizeof q,stdin))break;
        q[strcspn(q,"\n")]='\0';
        if(!q[0]||q[0]=='0'||strcmp(q,"esci")==0)break;
        printf(MAG "AI Consulente: " RST);
        ai_chat_stream(&g_ctx,sys,q,stream_cb,NULL,NULL,0);
        printf("\n");
    }
}

void menu_pratico(void) {
    while(1){
        clear_screen(); print_header();
        printf(CYN BLD "\n  \xe2\x9a\x99\xef\xb8\x8f  Strumento Pratico\n\n" RST);
        printf("  " EM_1 "  Dichiarazione 730\n");
        printf("  " EM_2 "  Partita IVA & Forfettario\n");
        printf("  " EM_0 "  Torna\n\n  " GRN "Scelta: " RST); fflush(stdout);
        int c=getch_single(); printf("%c\n",c);
        if(c=='1') pratico_730();
        else if(c=='2') pratico_piva();
        else if(c=='0'||c=='q'||c=='Q'||c==27) break;
    }
}
