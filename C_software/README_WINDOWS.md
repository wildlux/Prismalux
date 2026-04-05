# Prismalux — Guida compilazione Windows

## Requisiti

- **Ollama** per Windows: https://ollama.com/download
- **MinGW / gcc** — scegli UNO dei seguenti:
  - **MSYS2** (consigliato): https://www.msys2.org/
    Dopo l'installazione apri "MSYS2 MINGW64" e lancia:
    `pacman -S --needed mingw-w64-x86_64-gcc`
  - **WinLibs** (no install, solo estrarre + PATH): https://winlibs.com/

## Compilazione — 3 passi

```
1. Estrai lo ZIP in una cartella (es. C:\Prismalux)
2. Doppio click su  build.bat
3. Si apre prismalux.exe
```

Lo script `build.bat` trova gcc automaticamente (PATH / MSYS2 / WinLibs / Scoop / Chocolatey).

## Avvio

Assicurati che Ollama sia in esecuzione (`ollama serve`), poi:

```
prismalux.exe --backend ollama
```

Con un modello specifico già scaricato:
```
prismalux.exe --backend ollama --model qwen2.5-coder:7b
```

Con llama-server su porta diversa:
```
prismalux.exe --backend llama-server --port 8080
```

Su macchina remota nella LAN:
```
prismalux.exe --backend ollama --host 192.168.1.10 --port 11434
```

Aiuto completo: `prismalux.exe --help`

## AMD GPU su Windows

Se Ollama usa la GPU AMD lentamente, forza la modalità CPU:
```
set OLLAMA_NUM_GPU=0
ollama serve
```

## Struttura cartelle

```
Prismalux_Windows\
├── build.bat          <- doppio click per compilare
├── src\               <- sorgenti C
├── include\           <- header
├── models\            <- metti qui i .gguf (modalita llama-server)
└── README_WINDOWS.md  <- questa guida
```
