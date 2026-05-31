# Stable Diffusion Local MCP — Prismalux

**Tipo**: subprocess Python (sd_local.py, nessun server esterno)

## Funzione
Genera immagini AI da testo in locale tramite `diffusers` di Hugging Face.
Supporta GPU NVIDIA (CUDA), AMD (ROCm) e CPU.

## Installazione

### 1. GPU NVIDIA (consigliato)
```bash
pip install torch --index-url https://download.pytorch.org/whl/cu121
pip install diffusers transformers accelerate Pillow
```

### 2. CPU (nessuna GPU richiesta, ma lento)
```bash
pip install torch diffusers transformers accelerate Pillow
```
> La generazione su CPU richiede 5-30 minuti per immagine.

### 3. GPU AMD ROCm (solo Linux)
```bash
pip install torch --index-url https://download.pytorch.org/whl/rocm5.7
pip install diffusers transformers accelerate Pillow
```

### 4. Verifica
```bash
python3 -c "
import torch; print('CUDA:', torch.cuda.is_available())
import diffusers; print('Diffusers:', diffusers.__version__)
"
```

### 5. Test rapido (scarica il modello ~4 GB al primo avvio)
```bash
python3 MCPs/stable_diffusion_local/sd_local.py \
    --prompt "un tramonto sul mare" \
    --width 512 --height 512 --steps 20
```

## Note
- Modelli salvati in `~/.cache/huggingface/`
- Immagini generate in `~/.prismalux/sd_output/`
- Requisiti minimi GPU: 4 GB VRAM (512×512)

## Aggiungere a Claude Code
In `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "stable_diffusion": {
      "type": "stdio",
      "command": "python3",
      "args": ["/home/wildlux/Desktop/Prismalux/MCPs/stable_diffusion_local/sd_local.py"]
    }
  }
}
```
