#include "llama_wrapper.h"
#include "llama.cpp/include/llama.h"
#include "llama.cpp/ggml/include/ggml.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <dirent.h>

static llama_model*   g_model         = nullptr;
static llama_context* g_ctx           = nullptr;
static char           g_model_name[256] = "";
static int            g_n_batch       = 512;
static int            g_backend_init  = 0;   /* llama_backend_init chiamata? */

int lw_is_loaded(void) { return (g_model && g_ctx) ? 1 : 0; }
const char* lw_model_name(void) { return g_model_name; }

/* Callback no-op: sopprime tutti i messaggi di log di llama.cpp da stderr */
static void _llama_log_suppress(ggml_log_level, const char*, void*) {}

void lw_free(void) {
    if (g_ctx)   { llama_free(g_ctx);         g_ctx   = nullptr; }
    if (g_model) { llama_model_free(g_model); g_model = nullptr; }
    if (g_backend_init) { llama_backend_free(); g_backend_init = 0; }
    g_model_name[0] = '\0';
}

int lw_init(const char* model_path, int n_gpu_layers, int n_ctx_size) {
    lw_free();   /* libera modello precedente + backend → RAM azzerata */
    llama_backend_init();
    g_backend_init = 1;
    llama_log_set(_llama_log_suppress, nullptr);

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = n_gpu_layers;

    g_model = llama_model_load_from_file(model_path, mparams);
    if (!g_model) {
        /* Fallimento: libera subito il backend per non tenere RAM occupata */
        llama_backend_free(); g_backend_init = 0;
        return -1;
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx     = (uint32_t)n_ctx_size;
    cparams.n_batch   = 2048;
    cparams.n_ubatch  = 512;
    /* Thread fisici: metà dei logici, minimo 2 */
    int n_thr = (int)std::thread::hardware_concurrency() / 2;
    if (n_thr < 2) n_thr = 2;
    cparams.n_threads = (uint32_t)n_thr;
    /* Flash Attention: riduce VRAM e accelera inferenza */
    cparams.flash_attn = true;
    /* KV cache quantizzata q8_0: modelli più grandi con meno RAM */
    cparams.type_k = GGML_TYPE_Q8_0;
    cparams.type_v = GGML_TYPE_Q8_0;
    g_n_batch = (int)cparams.n_batch;

    g_ctx = llama_init_from_model(g_model, cparams);
    if (!g_ctx) {
        llama_model_free(g_model); g_model = nullptr;
        llama_backend_free();      g_backend_init = 0;
        return -1;
    }

    const char* slash = strrchr(model_path, '/');
    if (!slash) slash = strrchr(model_path, '\\');
    strncpy(g_model_name, slash ? slash + 1 : model_path, 255);
    g_model_name[255] = '\0';
    return 0;
}

int lw_chat(const char* system_prompt, const char* user_message,
            lw_token_cb callback, void* udata, char* out_buf, int out_max)
{
    if (!g_model || !g_ctx) return -1;
    const llama_vocab* vocab = llama_model_get_vocab(g_model);

    /* Build prompt via chat template */
    std::vector<llama_chat_message> msgs = {
        {"system", system_prompt},
        {"user",   user_message}
    };
    std::vector<char> tpl_buf(65536, 0);
    int tpl_len = llama_chat_apply_template(
        nullptr,           /* model template (nullptr = auto) */
        msgs.data(), (int)msgs.size(),
        true,              /* add_ass */
        tpl_buf.data(), (int)tpl_buf.size()
    );

    std::string prompt;
    if (tpl_len > 0 && tpl_len < (int)tpl_buf.size()) {
        prompt.assign(tpl_buf.data(), tpl_len);
    } else {
        /* Fallback: ChatML */
        prompt = "<|im_start|>system\n" + std::string(system_prompt) +
                 "\n<|im_end|>\n<|im_start|>user\n" + user_message +
                 "\n<|im_end|>\n<|im_start|>assistant\n";
    }

    /* Tokenize */
    std::vector<llama_token> tokens(prompt.size() + 64);
    int n_tokens = llama_tokenize(
        vocab, prompt.c_str(), (int32_t)prompt.size(),
        tokens.data(), (int32_t)tokens.size(),
        true,  /* add_special */
        true   /* parse_special */
    );
    if (n_tokens < 0) {
        tokens.resize(-n_tokens + 4);
        n_tokens = llama_tokenize(
            vocab, prompt.c_str(), (int32_t)prompt.size(),
            tokens.data(), (int32_t)tokens.size(),
            true, true
        );
        if (n_tokens < 0) return -1;
    }
    tokens.resize(n_tokens);

    /* Clear KV cache and decode prompt in chunks to avoid n_batch assertion */
    llama_memory_clear(llama_get_memory(g_ctx), false);
    llama_batch batch = llama_batch_get_one(tokens.data(), 1); /* inizializzato, usato nel loop generazione */
    {
        int processed = 0;
        int n_tok = (int)tokens.size();
        while (processed < n_tok) {
            int chunk = n_tok - processed;
            if (chunk > g_n_batch) chunk = g_n_batch;
            batch = llama_batch_get_one(tokens.data() + processed, (int32_t)chunk);
            if (llama_decode(g_ctx, batch) != 0) return -1;
            processed += chunk;
        }
    }

    /* Build sampler chain — parametri ottimizzati per pipeline multi-agente */
    llama_sampler* smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(smpl, llama_sampler_init_top_p(0.90f, 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.3f));           /* bassa: codice deterministico */
    llama_sampler_chain_add(smpl, llama_sampler_init_penalties(
        64,    /* last_n: finestra controllo ripetizioni */
        1.2f,  /* repeat_penalty */
        0.1f,  /* frequency_penalty */
        0.0f   /* presence_penalty */
    ));
    llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    /* Generation loop — max 512 token, anti-loop integrato */
    char piece[256];
    int  out_written = 0;

    /* Cerca prima occorrenza di un marker di loop nell'output.
     * Se "needle" (primi 16 car dell'output) si ripete 3+ volte → loop. */
#define ANTILOOP_NEEDLE 16
#define ANTILOOP_REPS    3

    char antiloop_seed[ANTILOOP_NEEDLE + 1];
    antiloop_seed[0] = '\0';
    int antiloop_seed_set = 0;

    for (int i = 0; i < 512; i++) {
        llama_token tok = llama_sampler_sample(smpl, g_ctx, -1);
        llama_sampler_accept(smpl, tok);

        if (llama_vocab_is_eog(vocab, tok)) break;

        int piece_len = llama_token_to_piece(
            vocab, tok, piece, (int)sizeof(piece) - 1,
            0,    /* special offset */
            true  /* special */
        );
        if (piece_len < 0) break;
        piece[piece_len] = '\0';

        if (out_buf && out_written + piece_len < out_max - 1) {
            memcpy(out_buf + out_written, piece, piece_len);
            out_written += piece_len;
            out_buf[out_written] = '\0';

            /* Acquisisce il seed (primi ANTILOOP_NEEDLE caratteri non-blank) */
            if (!antiloop_seed_set && out_written >= ANTILOOP_NEEDLE) {
                /* Salta whitespace iniziale */
                const char* start = out_buf;
                while (*start && (*start == ' ' || *start == '\n' || *start == '\r')) start++;
                if ((int)strlen(start) >= ANTILOOP_NEEDLE) {
                    memcpy(antiloop_seed, start, ANTILOOP_NEEDLE);
                    antiloop_seed[ANTILOOP_NEEDLE] = '\0';
                    antiloop_seed_set = 1;
                }
            }

            /* Conta quante volte il seed appare nell'output → loop detection */
            if (antiloop_seed_set && out_written > ANTILOOP_NEEDLE * (ANTILOOP_REPS + 1)) {
                int count = 0;
                const char* p = out_buf;
                while ((p = strstr(p, antiloop_seed)) != nullptr) {
                    count++;
                    if (count >= ANTILOOP_REPS) break;
                    p++;
                }
                if (count >= ANTILOOP_REPS) {
                    /* Tronca dopo la prima occorrenza del seed */
                    const char* first = strstr(out_buf, antiloop_seed);
                    if (first) {
                        int keep = (int)(first - out_buf) + ANTILOOP_NEEDLE;
                        out_buf[keep] = '\0';
                        out_written = keep;
                    }
                    break;
                }
            }
        }

        if (callback) callback(piece, udata);

        batch = llama_batch_get_one(&tok, 1);
        if (llama_decode(g_ctx, batch) != 0) break;
    }

    llama_sampler_free(smpl);
    return 0;
}

int lw_list_models(const char* dir, char names[][256], int max_names) {
    int count = 0;
    DIR* d = opendir(dir);
    if (!d) return 0;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr && count < max_names) {
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".gguf") == 0) {
            strncpy(names[count], entry->d_name, 255);
            names[count][255] = '\0';
            count++;
        }
    }
    closedir(d);
    return count;
}
