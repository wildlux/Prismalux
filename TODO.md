# Prismalux — TODO pendenti

> Aggiornato: 2026-05-28 (sessione implementazione) | Versione: 2.9
> Build: `cmake --build build_gui -j$(nproc)`

---

## 🔐 Sicurezza

- [ ] **[SEC] Supply chain MCP — `requirements.lock` con hash verificati** — `MCPs/gns3_mcp/requirements.lock` aggiornato con hash SHA256 reali per requests/certifi/charset-normalizer/idna/urllib3. Mancano ancora gli hash di `gns3fy` (non scaricabile nell'ambiente di build). Per completare: `cd MCPs/gns3_mcp && pip-compile --generate-hashes pyproject.toml -o requirements.lock`; stesso per `stable_diffusion_local` (dipendenze torch non scaricabili facilmente).

---

## 🎨 UI / UX

- [ ] **[UI] DPI/scaling su Linux Wayland HiDPI** — widget con px hardcoded nelle `gui/pages/` (es. `agenti_page_consiglio.cpp:49`, `agents_config_dialog.cpp:725`, `app_controller_page.cpp:488-505`, `impara_page.cpp:60,232,311`). Sostituire con `dpiSize(N)` da `dpi_utils.h`. `mainwindow.cpp` è già compliant.

- [ ] **[UI] Stati di errore muti** — `fetchModels()` e pipeline fallita non mostrano errore contestuale in tutti i punti. Applicare `AiErrorWidget::showError(msg, onRetry)` nei punti non coperti (verificare `matematica_page.cpp`, `ricerca_page.cpp` tab BLHM/RAB0L, `strumenti_page.cpp` categorie).

---

## 🔮 Futuri / nice-to-have

- [ ] **Ollama MCP** — MCP con SQLite cache lista modelli (sync da `/api/tags`), tool `get_model_info` compound, ricerca per size/name; sostituisce il fetch-on-demand in `AiClient` e riduce le chiamate HTTP a ogni avvio

- [ ] **[UX] i18n — internazionalizzazione UI** — tutta la UI è hardcoded in italiano. Introdurre `tr()` sistematico e file `.ts` Qt Linguist per supportare future traduzioni

---

## ✅ Implementati in questa sessione (2026-05-28)

- [x] **[Quiz] Più domande CCNA** — da 64 a **209 domande** in 15 temi (OSI, Switching, Routing, IPv4/IPv6, Sicurezza, DHCP/NAT, Wireless, WAN/VPN, IOS, Automazione, QoS, Cablaggio, IP Services, Troubleshooting, Fondamentali)
- [x] **[Python] Type checking MCP** — `[tool.mypy]` aggiunto a tutti e 3 i `pyproject.toml` (gns3_mcp, knowledge_mcp, stable_diffusion_local)
- [x] **[SEC] requirements.lock hash parziale** — hash SHA256 reali per requests, certifi, charset-normalizer, idna, urllib3 in `gns3_mcp/requirements.lock`

### Già implementati (rimossi dal TODO perché esistenti nel codice):
- [x] **asyncio.to_thread** — già in `knowledge_mcp/server.py` righe 553,575
- [x] **Tema Sistema auto** — già in `impostazioni_page_visuale.cpp` checkbox `followSystem`
- [x] **Lambda SSL lan_server.cpp** — non esistono lambda SSL nel codice attuale
- [x] **Lambda connect() senza context** — tutte le lambda nelle pages hanno context object
- [x] **BLE Scoperta peer attiva** — già implementata con `m_discoveryAgent` e `onClassicDeviceDiscovered`
- [x] **Chat cronologia persistente mobile** — già implementata con SQLite in `chat_page.cpp`
- [x] **Form factor tablet Android** — già implementato con `#ifdef PRISMALUX_FORM_FACTOR_TABLET`
- [x] **Focus trap dialog** — le QDialog esistenti usano `exec()` sincrono che gestisce il focus automaticamente via Qt
