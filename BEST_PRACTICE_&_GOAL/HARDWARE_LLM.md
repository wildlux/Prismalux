# Hardware per LLM locali — guida all'acquisto

> Aggiornato: 2026-05-31
> Scopo: che caratteristiche deve avere un computer per far girare modelli LLM locali
> con risposte di qualità ottima (coding, matematica, uso generale).
> Vedi anche [MODELLI_LOCALI.md](MODELLI_LOCALI.md) per la scelta dei modelli.

## Il fattore #1: la VRAM (memoria della GPU)

La qualità delle risposte dipende dalla **dimensione del modello** che riesci a far girare,
e la dimensione massima dipende dalla **VRAM** della scheda video. La VRAM è anche molto più
veloce della RAM di sistema → dà sia **qualità** che **velocità**. È la cosa su cui investire.

**Regola pratica:** un modello quantizzato Q4 occupa circa
`parametri (in miliardi) × 0,6 GB` (+ un po' per il contesto).

| VRAM | Modello più grande comodo (Q4) | Livello qualità risposte |
|------|--------------------------------|--------------------------|
| 4 GB | ~3-4B | base (la macchina attuale: GTX 1050) |
| 8 GB | ~7-8B | discreto |
| 12 GB | ~13-14B | buono |
| **16 GB** | ~14-22B | molto buono |
| **24 GB** | ~32B comodo | **ottimo — il vero salto di qualità** |
| 48 GB (2×24) | ~70B | eccellente, vicino ai cloud (ChatGPT/Claude) |

**Scala di qualità reale dei modelli:**
- **7-8B** → assistente con errori frequenti su task complessi
- **14B** → buono
- **32B** → molto buono (sweet spot qualità/hardware)
- **70B** → "ottimo come risposte", livello vicino ai modelli cloud
- **100B+ (MoE)** → eccellente, ma serve molta RAM/VRAM

Per risposte **ottime** il minimo serio è **24 GB di VRAM**; l'ideale è **48 GB**.

## Fasce concrete — GPU NVIDIA

| Fascia | GPU | VRAM | Cosa fa girare |
|--------|-----|------|----------------|
| 💰 **Best value** | **RTX 3090 usata** | 24 GB | 32B fluidi, 70B con offload. Miglior VRAM/€ per LLM. |
| 🆕 Nuovo equilibrato | RTX 4060 Ti / 5060 Ti 16 GB | 16 GB | fino a ~22B, garanzia |
| 🚀 Top consumer | RTX 4090 / 5090 | 24 / 32 GB | massima velocità; 5090 si avvicina ai 70B |
| 🏆 Workstation | 2× RTX 3090 / schede 48 GB | 48 GB+ | 70B comodi |

> La **RTX 3090 usata da 24 GB** è oggi *la* scelta per LLM locali in casa:
> rapporto VRAM/prezzo imbattibile.

## Alternativa Apple (memoria unificata)

Su Mac Apple Silicon la RAM è **condivisa con la GPU** ad alta banda → con tanta RAM unificata
fai girare modelli grandi senza una GPU dedicata.

- **Mac Mini / Mac Studio M4 con 48-64 GB unified** → fino a 70B.
- ✅ Silenzioso, consumi bassissimi, compatto.
- ❌ Costa di più per GB, non espandibile, ecosistema chiuso.

## Gli altri componenti (in ordine di importanza)

1. **RAM di sistema: 32 GB minimo, 64 GB ideale.** Serve per l'offload quando il modello
   eccede la VRAM. È lenta → rete di sicurezza, non il motore principale.
2. **SSD NVMe 1-2 TB.** I modelli pesano decine di GB l'uno; si accumulano in fretta.
3. **CPU: 6-8 core recenti bastano.** Con GPU dedicata non è il collo di bottiglia.
   Conta solo per inference su CPU pura e prompt processing.
4. **Alimentatore adeguato** per GPU grandi (RTX 3090/4090 → 350-450 W).
5. **Raffreddamento** decente: l'inference tiene la GPU sotto carico a lungo.

## Consiglio sintetico (uso: coding + matematica + Prismalux)

- **"Ottimo senza spendere una follia":**
  **RTX 3090 usata (24 GB) + 64 GB RAM + SSD 2 TB.**
  Fa girare 32B fluidi (Qwen2.5-Coder 32B, Qwen2.5 32B) con qualità alta, e i 70B con pazienza.
  Miglior euro/qualità per LLM locali.
- **"Top, compra e dimentica":**
  **RTX 5090 (32 GB)** oppure **Mac Studio 64 GB unified** (se preferisci silenzio/consumi
  e i 70B comodi).

> Nota: la VRAM è quasi sempre l'investimento giusto. Tra "GPU più veloce con meno VRAM" e
> "GPU più lenta con più VRAM", per gli LLM scegli **più VRAM**: un modello che entra in GPU,
> anche su scheda più lenta, batte un modello che deve fare offload su CPU.
