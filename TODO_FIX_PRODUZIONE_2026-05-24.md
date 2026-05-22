# Prismalux — Fix Produzione (sessione 2026-05-24)

Stima: 2-3 ore totali.

---

## 🔴 Fix 1 — ai_client.cpp:1063 — QPointer su QNetworkReply (15 min)

**Problema:** lambda cattura `reply` raw. Se Ollama è lento e l'utente cambia tab,
`reply` viene distrutto prima che la lambda venga eseguita → crash.

**File:** `gui/ai_client.cpp` riga ~1063

**Fix:**
```cpp
// Prima
connect(reply, &QNetworkReply::finished, this, [reply, callback] {
    ...
});

// Dopo
QPointer<QNetworkReply> safeReply(reply);
connect(reply, &QNetworkReply::finished, this, [safeReply, callback] {
    if (!safeReply) return;
    ...
});
```

---

## 🔴 Fix 2 — Stati di errore muti in tutti i *_page.cpp (2-3 ore)

**Problema:** quando Ollama è giù, timeout, o JSON malformato, il pulsante torna
idle senza mostrare nulla. L'utente non capisce cosa è successo.

**Pattern corretto** (già usato in alcune pagine con `m_sciErrorPanel`):
```cpp
m_sciErrorPanel->showError(msg, [this]{ onRunClicked(); });
```

**File da correggere** (in ordine di priorità):
- [ ] `gui/pages/agenti_page.cpp` — pipeline + autonomo
- [ ] `gui/pages/programmazione_page.cpp` — editor + interprete
- [ ] `gui/pages/strumenti_page.cpp` — scrittura, ricerca
- [ ] `gui/pages/matematica_page.cpp` — parser formule
- [ ] `gui/pages/lavoro_page.cpp` — analisi offerte (QPointer già ok)
- [ ] `gui/pages/ricerca_page.cpp` — paper, analisi fenomeni
- [ ] `gui/pages/multimedia_page.cpp` — generazione immagini

**Approccio:** cercare tutti i blocchi `onError` / `onFinished` che fanno solo
`btn->setText(...)` senza chiamare `showError()` e aggiungere il banner.

---

## 🟡 Fix 3 — 462 lambda connect() senza context object (bassa priorità, dopo i due sopra)

**Problema:** lambda catturano `this` senza passarlo come 3° argomento a `connect()`.
Se il sender sopravvive al receiver → use-after-free teorico.

**Ricerca rapida:**
```bash
grep -rn "connect(.*\[this\]" gui/pages/ | grep -v ", this," | wc -l
```

**Fix pattern:**
```cpp
// Prima (pericoloso)
connect(btn, &QPushButton::clicked, [this]{ ... });

// Dopo (sicuro)
connect(btn, &QPushButton::clicked, this, [this]{ ... });
```

**Strategia:** fixare solo i `connect()` su oggetti che sopravvivono alla pagina
(es. `m_ai`, timer globali). Le lambda su widget figli sono già safe per parent ownership.

---

## Note

- Fix 1 e Fix 2 sono sufficienti per dichiarare Prismalux stabile in produzione
- Fix 3 è teorico per la maggior parte dei casi — affrontarlo solo se c'è tempo
- Dopo i fix: `./aggiorna.sh` per rigenerare AppImage + ZIP
