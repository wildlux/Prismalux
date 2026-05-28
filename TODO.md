# Prismalux — TODO pendenti

> Consolidato: 2026-05-28 | Versione: 2.9 | Build: `cmake --build build_gui -j$(nproc)`
>
> Solo voci ancora aperte. Voci completate rimosse; duplicati eliminati; voci marcate `[ ]` ma già implementate nel codice corrette.

---

## 📱 Mobile Android

- [ ] **[Quiz] Più domande CCNA** — `quiz_ccna_db.cpp` ha 64 domande; target 200+ per coprire tutti i domini exam 200-301; aggiungere anche CompTIA Network+

- [ ] **[Chat] Cronologia persistente** — la chat mobile è in memoria soltanto. Salvare i turni in SQLite locale (stesso DB usato dal quiz) per recuperarli tra sessioni

- [ ] **[BLE] Scoperta peer attiva** — `BlePage` avvia il server BT ma il client deve connettersi manualmente inserendo l'indirizzo. Aggiungere scansione dispositivi Bluetooth Classic + UI per scegliere il peer dalla lista

- [ ] **[C++] Form factor tablet in `android_app/mainwindow.cpp`** — leggere `PRISMALUX_FORM_FACTOR_TABLET` a runtime (`#ifdef`) per scegliere tra `BottomBar` e `TabletNavRail`; layout split-panel (sidebar + content stack) per tablet

---

## 🔐 Sicurezza

- [ ] **[SEC] Supply chain MCP — `requirements.lock` con hash** — i `requirements.txt` dei MCP non hanno versioni pinnate con hash SHA256. Un `pip install --upgrade` può portare una dipendenza compromessa. Fix: `pip-compile --generate-hashes` → `requirements.lock` per ogni MCP con dipendenze esterne

- [ ] **[C++] Lambda SSL in `lan_server.cpp:225,234`** — due lambda in `connect(sslSock, &QSslSocket::encrypted, ...)` e `connect(sslSock, &QSslSocket::disconnected, ...)` senza context object. Convertire a slot nominati `onSslEncrypted()` / `onSslClientDisconnected()` per rispettare la regola lambda del progetto

---

## ⚙️ C++ / Qt

- [ ] **[C++] Lambda `connect()` senza context esplicito — sistematizzazione residua** — l'audit 2026-05-14 trovò ~462 occorrenze; molte risolte nelle sessioni successive. Passata sistematica finale su tutti i file `gui/pages/` per aggiungere `this` come 3° argomento dove mancante o convertire a slot nominato se la logica supera 2 righe

---

## 🎨 UI / UX

- [ ] **[UI] DPI/scaling su Linux Wayland HiDPI** — alcuni widget usano dimensioni hardcoded in px (`setFixedWidth(80)`, `setFixedHeight(52)`) che risultano minuscoli su display 2×. Fix: `dpiSize(N)` da `dpi_utils.h` (già presente nel progetto) al posto dei valori fissi

- [ ] **[UI] Tema "Sistema" — Dark/Light automatico da OS** — 23 temi disponibili ma nessuno segue `QStyleHints::colorScheme()` (Qt 6.5+). Aggiungere voce "Sistema (auto)" in `ThemeManager` che seleziona il tema dark/light in base alla preferenza OS

- [ ] **[UI] Focus trap nei dialog** — i `QDialog` si aprono ma il focus non parte dal primo campo interattivo. Aggiungere `firstWidget->setFocus()` in `showEvent()` per ogni dialog custom

- [ ] **[UI] Stati di errore muti** — `fetchModels()`, fetch RAG e pipeline fallita non mostrano un messaggio contestuale: il bottone torna idle senza spiegare l'errore. Applicare il pattern `AiErrorWidget::showError(msg, onRetry)` nei punti ancora non coperti

---

## 🐍 Python / MCP

- [ ] **[Python] `asyncio.to_thread` per I/O sync in `knowledge_mcp`** — `_write_raw()` e `_read_raw()` usano `fcntl.flock` bloccante all'interno di `async def`, bloccando l'event loop su file lenti. Fix: `await asyncio.to_thread(_write_raw, content)`

- [ ] **[Python] Type checking MCP — `mypy` / `pyright`** — nessun type checker configurato sui server MCP. Aggiungere `[tool.mypy]` in ogni `pyproject.toml` e integrare il check in CI (o pre-commit hook)

---

## 🔮 Futuri / nice-to-have

- [ ] **Ollama MCP** — MCP con SQLite cache lista modelli (sync da `/api/tags`), tool `get_model_info` compound, ricerca per size/name; sostituisce il fetch-on-demand in `AiClient` e riduce le chiamate HTTP a ogni avvio

- [ ] **[UX] i18n — internazionalizzazione UI** — tutta la UI è hardcoded in italiano. Introdurre `tr()` sistematico e file `.ts` Qt Linguist per supportare future traduzioni (punto di partenza: le ~30 stringhe già con `tr()` nei file più recenti)
