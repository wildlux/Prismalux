# Stable Diffusion Locale (senza AUTOMATIC1111)

**Backend**: Python diffusers  
**Tipo**: subprocess lanciato da Prismalux Qt GUI

## Funzione
Genera immagini AI direttamente in locale usando la libreria `diffusers` di HuggingFace,
senza installare AUTOMATIC1111 o altri server WebUI.

## Installazione veloce
```bash
# CPU (sempre funziona, lento)
pip install diffusers transformers accelerate
pip install torch --index-url https://download.pytorch.org/whl/cpu

# GPU NVIDIA (più veloce)
pip install diffusers transformers accelerate
pip install torch --index-url https://download.pytorch.org/whl/cu121
```

## Modelli supportati (HuggingFace Hub o path locale)
| Modello | VRAM | Qualità |
|---------|------|---------|
| `runwayml/stable-diffusion-v1-5` | ~4 GB | buona |
| `stabilityai/stable-diffusion-2-1` | ~5 GB | migliore |
| `stabilityai/stable-diffusion-xl-base-1.0` | ~8 GB | ottima |
| `path/locale/modello` | dipende | qualsiasi GGUF-diffusers |

## Uso da riga di comando
```bash
python3 sd_local.py \
  --prompt "un tramonto sul mare, fotorealistico" \
  --steps 20 --cfg 7.0 --width 512 --height 512 \
  --seed 42 --out /tmp/output.png
```

## Integrazione GUI
La GUI Prismalux lancia automaticamente questo script selezionando
"Locale (diffusers)" nel tab Genera Immagini.
