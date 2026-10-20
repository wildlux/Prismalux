# AGENTS.md — Regole fisse per agenti (Prismalux)

Queste regole valgono per ogni agente che lavora su questo repository.

## 1. Report all'utente: sempre in tabella

Quando l'utente chiede un **report**, un **riepilogo**, uno **stato dei lavori**, un elenco di **cosa manca** o l'**esito di test/modifiche**, la risposta deve essere una **tabella Markdown**.

- Una riga per voce, con colonne chiare (es. `Cosa`, `Stato`, `Dettaglio`, `Prossimo passo`).
- Usare `✅` fatto, `🟡` parziale/da verificare, `⬜` da fare, `❌` bloccato.
- Dopo la tabella sono ammesse solo poche righe di note o eccezioni.
- Non sostituire la tabella con elenchi puntati o prosa come formato principale.
- Le risposte conversazionali brevi (una domanda secca, un chiarimento) possono restare testo.

## 2. Fonti di verità

- `TODO.md` — cose da fare e stato.
- `MAP_CODE_AGENT.md` — mappa del codice, flussi e trappole.
- `gui/CLAUDE.md` — convenzioni di sviluppo della GUI.
- `TODO.md` e `MAP_CODE_AGENT.md` vanno aggiornati quando si completa un task.
