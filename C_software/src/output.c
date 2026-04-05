/*
 * output.c — SEZIONE 9: Salvataggio output su file
 */
#include "common.h"
#include "backend.h"
#include "output.h"
#include "prismalux_ui.h"

/* ══════════════════════════════════════════════════════════════
   SEZIONE 9 — Salvataggio output
   ══════════════════════════════════════════════════════════════ */

void salva_output(const char* task, const char* output) {
    /* Prova le directory nell'ordine: shared con Python → locale → corrente.
     * "../esportazioni" è la cartella condivisa con Python_Software (struttura monorepo). */
    const char *dirs[]={"../esportazioni","esportazioni",".",NULL};

    /* Timestamp nel formato YYYYMMDD_HHMMSS per il nome file univoco */
    time_t now=time(NULL); struct tm *ti=localtime(&now);
    char ts[32]; strftime(ts,sizeof ts,"%Y%m%d_%H%M%S",ti);

    char fp[512]; FILE *f=NULL;
    for(int i=0;dirs[i];i++){
        /* Crea la directory se non esiste (ignora errori: forse esiste già) */
#ifdef _WIN32
        CreateDirectoryA(dirs[i],NULL);
#else
        char cmd[256]; snprintf(cmd,sizeof cmd,"mkdir -p '%s' 2>/dev/null",dirs[i]);
        { int _rc = system(cmd); (void)_rc; }
#endif
        snprintf(fp,sizeof fp,"%s/multiagente_%s.txt",dirs[i],ts);
        f=fopen(fp,"w"); if(f)break; /* usa la prima directory scrivibile */
    }
    if(!f){printf(RED "  Impossibile salvare il file.\n" RST);return;}

    /* Intestazione del file con metadati sessione + output */
    fprintf(f,"=== PRISMALUX MULTI-AGENTE ===\nData: %s\nTask: %s\n"
              "Backend: %s  Modello: %s\n\n=== OUTPUT ===\n\n%s\n",
            ts,task,backend_name(g_ctx.backend),
            g_ctx.backend==BACKEND_LLAMA?lw_model_name():g_ctx.model,output);
    fclose(f);
    printf(GRN "  \xe2\x9c\x93 Salvato: %s\n" RST,fp);
}
