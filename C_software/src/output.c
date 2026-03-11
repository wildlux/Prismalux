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
    const char *dirs[]={"../esportazioni","esportazioni",".",NULL};
    time_t now=time(NULL); struct tm *ti=localtime(&now);
    char ts[32]; strftime(ts,sizeof ts,"%Y%m%d_%H%M%S",ti);
    char fp[512]; FILE *f=NULL;
    for(int i=0;dirs[i];i++){
#ifdef _WIN32
        CreateDirectoryA(dirs[i],NULL);
#else
        char cmd[256]; snprintf(cmd,sizeof cmd,"mkdir -p '%s' 2>/dev/null",dirs[i]);
        (void)system(cmd);
#endif
        snprintf(fp,sizeof fp,"%s/multiagente_%s.txt",dirs[i],ts);
        f=fopen(fp,"w"); if(f)break;
    }
    if(!f){printf(RED "  Impossibile salvare il file.\n" RST);return;}
    fprintf(f,"=== PRISMALUX MULTI-AGENTE ===\nData: %s\nTask: %s\n"
              "Backend: %s  Modello: %s\n\n=== OUTPUT ===\n\n%s\n",
            ts,task,backend_name(g_ctx.backend),
            g_ctx.backend==BACKEND_LLAMA?lw_model_name():g_ctx.model,output);
    fclose(f);
    printf(GRN "  \xe2\x9c\x93 Salvato: %s\n" RST,fp);
}
