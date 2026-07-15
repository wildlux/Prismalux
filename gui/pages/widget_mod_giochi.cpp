/* ══════════════════════════════════════════════════════════════
   widget_mod_giochi.cpp
   Godot + Game Modding — spostati da AppControllerPage a UtilityPage
   (vedi widget_mod_giochi.h). Game Modding copre:
     - GTA V (ScriptHookVDotNet / Lua)
     - Skyrim SE / SKSE (Papyrus / XML)
     - Terraria tModLoader (C#)
     - World of Warcraft AddOn (Lua)
     - Noita (Lua)
     - Minecraft Datapack/Fabric (JSON/Java)
     - Stardew Valley SMAPI (C# / JSON)
     - RimWorld (XML / C# Harmony)
   ══════════════════════════════════════════════════════════════ */
#include "widget_mod_giochi.h"
#include "../dpi_utils.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include "../widgets/model_combo_box.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <QMessageBox>

ModGiochiWidget::ModGiochiWidget(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_aiProgress = new QProgressBar(this);
    m_aiProgress->setRange(0, 0);
    m_aiProgress->setFixedHeight(dpiScale(4));
    m_aiProgress->setTextVisible(false);
    m_aiProgress->setVisible(false);
    lay->addWidget(m_aiProgress);

    m_aiErrorPanel = new AiErrorWidget(this);
    lay->addWidget(m_aiErrorPanel);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildGodotTab(),       "\xf0\x9f\x8e\xae  Godot");
    tabs->addTab(buildGameModdingTab(), "\xf0\x9f\x97\xa1  Game Modding");
    lay->addWidget(tabs, 1);
}

/* ──────────────────────────────────────────────────────────────
   Helper: estrai codice dal primo blocco ```...```
   ────────────────────────────────────────────────────────────── */
QString ModGiochiWidget::extractCode(const QString& text)
{
    int start = text.indexOf("```python");
    if (start != -1) {
        start = text.indexOf('\n', start) + 1;
        int end = text.indexOf("```", start);
        if (end != -1) return text.mid(start, end - start).trimmed();
    }
    /* Fallback generico: salta il language tag (tutto fino al primo \n) */
    start = text.indexOf("```");
    if (start != -1) {
        start += 3;
        const int nl = text.indexOf('\n', start);
        if (nl != -1) {
            start = nl + 1;
            int end = text.indexOf("```", start);
            if (end != -1) return text.mid(start, end - start).trimmed();
        }
    }
    return text.trimmed();
}

/* ──────────────────────────────────────────────────────────────
   Helper: lancia AI con streaming — pattern connHolder one-shot
   ────────────────────────────────────────────────────────────── */
void ModGiochiWidget::runAi(int tabIdx, const QString& sys, const QString& userMsg,
                            QTextEdit* output, QPushButton* runBtn, QPushButton* stopBtn,
                            QComboBox* modelCombo)
{
    if (m_ai->busy()) {
        output->append("\xe2\x9a\xa0  AI occupata, attendi o premi Stop.");
        return;
    }
    if (userMsg.trimmed().isEmpty()) {
        output->append("\xe2\x9a\xa0  Inserisci la richiesta prima di eseguire.");
        return;
    }

    /* Applica modello selezionato */
    if (modelCombo && modelCombo->count() > 0) {
        const QString sel = modelCombo->currentData().toString();
        if (!sel.isEmpty() && sel != m_ai->model())
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), sel);
    }

    /* Salva stato sessione per gli slot nominati */
    m_runAiTabIdx    = tabIdx;
    m_runAiSys       = sys;
    m_runAiUserMsg   = userMsg;
    m_runAiOutput    = output;
    m_runAiRunBtn    = runBtn;
    m_runAiStopBtn   = stopBtn;
    m_runAiModelCombo = modelCombo;

    m_activeTab = tabIdx;
    runBtn->setEnabled(false);
    stopBtn->setEnabled(true);
    if (m_aiProgress) m_aiProgress->setVisible(true);
    output->append(
        "\n\xf0\x9f\x94\x84  Generazione in corso...\n"
        + QString(40, QChar(0x2500)));

    /* Disconnette connessioni precedenti e ricrea token holder */
    disconnect(m_connToken);
    disconnect(m_connFinished);
    disconnect(m_connError);
    delete m_tokenHolder;
    m_tokenHolder = new QObject(this);

    m_connToken    = connect(m_ai, &AiClient::token,    this, &ModGiochiWidget::onRunAiToken);
    m_connFinished = connect(m_ai, &AiClient::finished, this, &ModGiochiWidget::onRunAiFinished);
    m_connError    = connect(m_ai, &AiClient::error,    this, &ModGiochiWidget::onRunAiError);

    m_ai->chat(sys, userMsg);
}

void ModGiochiWidget::onRunAiToken(const QString& t)
{
    if (!m_runAiOutput) return;
    m_runAiOutput->moveCursor(QTextCursor::End);
    m_runAiOutput->insertPlainText(t);
}

void ModGiochiWidget::onRunAiFinished(const QString& full)
{
    if (m_runAiRunBtn)  m_runAiRunBtn->setEnabled(true);
    if (m_runAiStopBtn) m_runAiStopBtn->setEnabled(false);
    if (m_aiProgress)   m_aiProgress->setVisible(false);
    if (m_runAiOutput)  m_runAiOutput->append("\n" + QString(40, QChar(0x2500)));
    /* Disconnette segnali AI e libera token holder */
    disconnect(m_connToken);
    disconnect(m_connFinished);
    disconnect(m_connError);
    if (m_tokenHolder) {
        m_tokenHolder->deleteLater();
        m_tokenHolder = nullptr;
    }

    /* Abilita exec/salva solo se c'era un blocco backtick reale */
    const bool hasBlock = full.contains("```");
    const QString code = extractCode(full);
    if (m_activeTab == 0 && hasBlock && !code.isEmpty()) {
        m_godotCode = code;
        m_godotExecBtn->setEnabled(true);
        m_godotStatusLbl->setText(
            tr("\xf0\x9f\x8e\xae  Codice pronto \xe2\x80\x94 premi Esegui in Godot"));
    } else if (m_activeTab == 1 && hasBlock && !code.isEmpty()) {
        m_moddingCode = code;
        m_moddingSaveBtn->setEnabled(true);
        m_moddingStatusLbl->setText(
            tr("\xe2\x9c\x85  Codice pronto \xe2\x80\x94 premi Salva nel gioco"));
    }
}

void ModGiochiWidget::onRunAiError(const QString& msg)
{
    if (m_runAiRunBtn)  m_runAiRunBtn->setEnabled(true);
    if (m_runAiStopBtn) m_runAiStopBtn->setEnabled(false);
    if (m_aiProgress)   m_aiProgress->setVisible(false);
    /* Disconnette segnali AI e libera token holder */
    disconnect(m_connToken);
    disconnect(m_connFinished);
    disconnect(m_connError);
    if (m_tokenHolder) {
        m_tokenHolder->deleteLater();
        m_tokenHolder = nullptr;
    }
    /* Cattura locale per il retry (copia i membri correnti) */
    const int      tabIdx    = m_runAiTabIdx;
    const QString  sys       = m_runAiSys;
    const QString  userMsg   = m_runAiUserMsg;
    QTextEdit*     output    = m_runAiOutput;
    QPushButton*   runBtn    = m_runAiRunBtn;
    QPushButton*   stopBtn   = m_runAiStopBtn;
    QComboBox*     modelCombo = m_runAiModelCombo;

    /* "Forbidden" = modello cloud selezionato ma Ollama è locale */
    if (msg.contains("Forbidden", Qt::CaseInsensitive)) {
        const QString model = modelCombo ? modelCombo->currentData().toString() : "?";
        m_aiErrorPanel->showError(
            "Modello non disponibile localmente: \"" + model + "\"\n"
            "Seleziona un modello locale (es. deepseek-coder:6.7b, llama3.2:3b) "
            "dalla combo Modello e riprova.",
            [this, tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo]{
                runAi(tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo);
            });
    } else {
        m_aiErrorPanel->showError(msg,
            [this, tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo]{
                runAi(tabIdx, sys, userMsg, output, runBtn, stopBtn, modelCombo);
            });
    }
}

/* ══════════════════════════════════════════════════════════════
   System prompts — Godot
   ══════════════════════════════════════════════════════════════ */
static const char* kGodotSys[] = {
    "Sei un esperto di Godot 4.x e GDScript. "
    "Genera SOLO codice GDScript (sintassi Godot 4, non 3). "
    "Il codice verra' eseguito via godot-mcp (JSON-RPC su localhost:9080). "
    "Usa: Node2D, Control, RigidBody2D, AnimationPlayer, ecc. "
    "Rispondi SOLO con il blocco codice GDScript tra ``` e ```, senza spiegazioni.",

    "Sei un esperto di Godot 4.x. "
    "Genera SOLO codice GDScript per creare una scena 2D completa. "
    "Definisci nodi, proprieta', segnali e interazioni. "
    "Rispondi SOLO con il blocco codice GDScript tra ``` e ```.",

    "Sei un esperto di Godot 4.x. "
    "Genera SOLO codice GDScript per una meccanica di gioco (movimento, collisioni, punteggio). "
    "Commenta ogni sezione. Rispondi SOLO con codice GDScript tra ``` e ```.",

    "Sei un esperto di Godot 4.x e shader GLSL. "
    "Genera SOLO codice shader Godot (shader_type canvas_item o spatial). "
    "Rispondi SOLO con il blocco shader tra ``` e ```.",

    "Sei un esperto di Godot 4.x. "
    "Genera SOLO codice GDScript per UI (Control, Button, Label, TextureRect, ecc.). "
    "Rispondi SOLO con il blocco codice GDScript tra ``` e ```.",

    "Sei un esperto di Godot 4.x e GDScript. "
    "Genera SOLO codice GDScript libero come richiesto. "
    "Rispondi SOLO con il blocco codice tra ``` e ```.",

    nullptr
};
static const char* kGodotActions[] = {
    QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x8e\xae  Script gameplay"),
    QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x8c\x8d  Crea scena 2D"),
    QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x95\xb9  Meccanica di gioco"),
    QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x8c\x9f  Shader GLSL"),
    QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x96\xa5  UI & Menu"),
    QT_TRANSLATE_NOOP("Tabelle", "\xf0\x9f\x90\x8d  Script libero"),
    nullptr
};

/* ══════════════════════════════════════════════════════════════
   buildGodotTab
   ══════════════════════════════════════════════════════════════ */
QWidget* ModGiochiWidget::buildGodotTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x8e\xae  <i>Godot Engine \xe2\x80\x94 Motore di gioco open-source per lo sviluppo di videogiochi "
        "2D e 3D. Usa GDScript (simile a Python) o C#. Completamente gratuito, senza royalty e con editor integrato.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* Barra connessione */
    auto* connRow = new QWidget(w);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);

    m_godotStatusLbl = new QLabel(tr("\xe2\x9a\xaa  GDScript pronto"), connRow);
    m_godotStatusLbl->setObjectName("hintLabel");

    m_godotExecBtn = new QPushButton(
        tr("\xf0\x9f\x8e\xae  Salva script .gd"), connRow);
    m_godotExecBtn->setObjectName("actionBtn");
    m_godotExecBtn->setFixedWidth(dpiScale(160));
    m_godotExecBtn->setEnabled(false);

    connLay->addWidget(m_godotStatusLbl, 1);
    connLay->addWidget(m_godotExecBtn);
    lay->addWidget(connRow);

    auto* hintLbl = new QLabel(
        "\xf0\x9f\x8e\xae <b>Godot MCP:</b> genera GDScript da descrizione testuale. "
        "Copia o salva lo script nella cartella del progetto Godot.<br>"
        "Per integrazione MCP avanzata: "
        "<code>pip install godot-mcp</code> e abilita il plugin in Godot \xe2\x86\x92 "
        "Progetto \xe2\x86\x92 Plugin.", w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setOpenExternalLinks(true);
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* Azione + Modello */
    auto* toolRow = new QWidget(w);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);
    m_godotAction = new QComboBox(toolRow);
    for (int i = 0; kGodotActions[i]; i++)
        m_godotAction->addItem(P::trTab(kGodotActions[i]));
    m_godotModel = new ModelComboBox(m_ai, toolRow);
    toolLay->addWidget(new QLabel(tr("Tipo:"), toolRow));
    toolLay->addWidget(m_godotAction, 1);
    toolLay->addWidget(new QLabel(tr("Modello:"), toolRow));
    toolLay->addWidget(m_godotModel, 1);
    lay->addWidget(toolRow);

    /* Input */
    m_godotInput = new QTextEdit(w);
    m_godotInput->setPlaceholderText(
        "Descrivi cosa vuoi fare in Godot...\n"
        "Es: 'Crea un personaggio che si muove con WASD e salta con spazio'\n"
        "Es: 'Shader che simula acqua con onde'");
    m_godotInput->setFixedHeight(dpiScale(80));
    lay->addWidget(m_godotInput);

    /* Pulsanti */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_godotRunBtn  = new QPushButton(tr("\xf0\x9f\xa4\x96  Genera GDScript"), btnRow);
    m_godotRunBtn->setObjectName("actionBtn");
    m_godotStopBtn = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_godotStopBtn->setObjectName("actionBtn");
    m_godotStopBtn->setProperty("danger", true);
    m_godotStopBtn->setEnabled(false);
    btnLay->addWidget(m_godotRunBtn);
    btnLay->addWidget(m_godotStopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    auto* out = new QTextEdit(w);
    out->setReadOnly(true);
    out->setObjectName("outputView");
    out->setPlaceholderText(tr("GDScript generato dall'AI appare qui..."));
    m_godotOutput = out;
    lay->addWidget(m_godotOutput, 1);

    /* Connessioni */
    connect(m_godotExecBtn,  &QPushButton::clicked,
            this, &ModGiochiWidget::onGodotExecClicked);
    connect(m_godotRunBtn,   &QPushButton::clicked,
            this, &ModGiochiWidget::onGodotRunClicked);
    connect(m_godotStopBtn,  &QPushButton::clicked,
            this, &ModGiochiWidget::onGodotStopClicked);

    return w;
}

void ModGiochiWidget::onGodotExecClicked()
{
    if (m_godotCode.isEmpty()) return;
    const QString path = QDir::homePath() + "/ai_generated.gd";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(m_godotCode.toUtf8());
        m_godotStatusLbl->setText(tr("\xe2\x9c\x85  Salvato: ") + path);
    } else {
        m_godotStatusLbl->setText(tr("\xe2\x9d\x8c  Impossibile salvare il file"));
    }
}

void ModGiochiWidget::onGodotRunClicked()
{
    const int idx = m_godotAction->currentIndex();
    if (idx < 0 || !kGodotSys[idx]) return;
    runAi(0, QString::fromUtf8(kGodotSys[idx]),
          m_godotInput->toPlainText(),
          m_godotOutput, m_godotRunBtn, m_godotStopBtn,
          m_godotModel);
}

void ModGiochiWidget::onGodotStopClicked()
{
    m_ai->abort();
    m_godotRunBtn->setEnabled(true);
    m_godotStopBtn->setEnabled(false);
    m_godotOutput->append("\n\xe2\x8f\xb9  Fermato.");
}

/* ═══════════════════════════════════════════════════════════════
   System prompts per ogni gioco + tipo di mod
   ════════════════════════════════════════════════════════════════ */

/* ── GTA V ── */
static const char* kGtaActions[] = {
    QT_TRANSLATE_NOOP("Tabelle", "Script missione (C#)"),
    QT_TRANSLATE_NOOP("Tabelle", "Menu trainer (C#)"),
    QT_TRANSLATE_NOOP("Tabelle", "Script Lua (FiveM/RAGE)"),
    QT_TRANSLATE_NOOP("Tabelle", "Modifica veicolo / skin"),
    QT_TRANSLATE_NOOP("Tabelle", "Teleporter / cheat F"),
    nullptr
};
static const char* kGtaSys[] = {
    "Sei un esperto di modding GTA V con ScriptHookVDotNet v3 (SHVDN). "
    "Genera SOLO codice C# con namespace GTA, GTA.Math, GTA.Native. "
    "La classe deriva da GTA.Script e implementa override void OnTick(). "
    "Commenta ogni sezione. Rispondi SOLO con blocco ```csharp ... ```.",

    "Sei un esperto di modding GTA V. "
    "Genera SOLO codice C# per un menu trainer usando LemonUI (NativeUI alternativa). "
    "Includi: NativeMenu, NativeItem, toggle e slider. "
    "Rispondi SOLO con blocco ```csharp ... ```.",

    "Sei un esperto di scripting Lua per GTA V (FiveM o RAGE Multiplayer). "
    "Genera SOLO codice Lua compatibile con FiveM (client-side o server-side). "
    "Usa Citizen.CreateThread, AddEventHandler, TriggerNetEvent dove opportuno. "
    "Rispondi SOLO con blocco ```lua ... ```.",

    "Sei un esperto di modding GTA V. "
    "Genera SOLO codice C# che usa SHVDN per modificare veicoli: "
    "spawn, paint (VehiclePaint), mods (VehicleMods), performance, livrea. "
    "Rispondi SOLO con blocco ```csharp ... ```.",

    "Sei un esperto di modding GTA V con SHVDN. "
    "Genera SOLO codice C# per un cheat/funzione attivabile da tasto: "
    "teletrasporto, God mode, wanted level, spawn NPC. "
    "Usa Game.Player, Vehicle, Ped, World. Rispondi SOLO con blocco ```csharp ... ```.",

    nullptr
};
static const char* kGtaFolder =
    "steamapps/common/Grand Theft Auto V/scripts";

/* ── Skyrim SE / SKSE ── */
static const char* kSkyrimActions[] = {
    QT_TRANSLATE_NOOP("Tabelle", "Script Papyrus (.psc)"),
    QT_TRANSLATE_NOOP("Tabelle", "Incantesimo / Perk / Abilita'"),
    QT_TRANSLATE_NOOP("Tabelle", "Quest scripting Papyrus"),
    QT_TRANSLATE_NOOP("Tabelle", "Item / Arma / Armatura (XML)"),
    nullptr
};
static const char* kSkyrimSys[] = {
    "Sei un esperto di modding Skyrim Special Edition con SKSE64. "
    "Genera SOLO codice Papyrus (.psc) con Event, Function, Property. "
    "Segui le convenzioni CK (Creation Kit). Commenta le sezioni critiche. "
    "Rispondi SOLO con blocco ```papyrus ... ```.",

    "Sei un esperto di modding Skyrim. "
    "Genera SOLO codice Papyrus per un incantesimo/perk personalizzato. "
    "Includi: MagicEffect, Spell, o Perk con condizioni e script. "
    "Rispondi SOLO con blocco ```papyrus ... ```.",

    "Sei un esperto di modding Skyrim con SKSE64. "
    "Genera SOLO script Papyrus per una quest: alias, dialogo, stage, "
    "oggettivi (SetObjectiveDisplayed), condizioni. "
    "Rispondi SOLO con blocco ```papyrus ... ```.",

    "Sei un esperto di modding Skyrim. "
    "Genera SOLO codice XML per aggiungere un item/arma/armatura nel Creation Kit. "
    "Struttura: ARMO/WEAP record con campi obbligatori. "
    "Rispondi SOLO con blocco ```xml ... ```.",

    nullptr
};
static const char* kSkyrimFolder =
    "steamapps/common/Skyrim Special Edition/Data/Scripts";

/* ── Terraria tModLoader ── */
static const char* kTerrariaActions[] = {
    QT_TRANSLATE_NOOP("Tabelle", "Nuovo item (C#)"),
    QT_TRANSLATE_NOOP("Tabelle", "Nuovo boss / NPC (C#)"),
    QT_TRANSLATE_NOOP("Tabelle", "Meccanica / Hook (C#)"),
    QT_TRANSLATE_NOOP("Tabelle", "Nuovo proiettile (C#)"),
    nullptr
};
static const char* kTerrariaSys[] = {
    "Sei un esperto di modding Terraria con tModLoader (.NET 6). "
    "Genera SOLO codice C# per un nuovo item: classe derivata da ModItem, "
    "override SetDefaults(), AddRecipes(). "
    "Rispondi SOLO con blocco ```csharp ... ```.",

    "Sei un esperto di modding Terraria. "
    "Genera SOLO codice C# per un nuovo boss (classe ModNPC) con: "
    "override SetDefaults(), AI(), FindFrame(), HitEffect(). "
    "Includi fasi di attacco e movimento intelligente. "
    "Rispondi SOLO con blocco ```csharp ... ```.",

    "Sei un esperto di modding Terraria con tModLoader. "
    "Genera SOLO codice C# per modificare meccaniche esistenti con GlobalHook, "
    "ModSystem, GlobalItem o GlobalNPC. "
    "Rispondi SOLO con blocco ```csharp ... ```.",

    "Sei un esperto di modding Terraria. "
    "Genera SOLO codice C# per un proiettile personalizzato (ModProjectile): "
    "override SetDefaults(), AI(), Kill(), OnHitNPC(). "
    "Rispondi SOLO con blocco ```csharp ... ```.",

    nullptr
};
static const char* kTerrariaFolder =
    ".local/share/Terraria/ModLoader/Mods";

/* ── World of Warcraft ── */
static const char* kWowActions[] = {
    QT_TRANSLATE_NOOP("Tabelle", "AddOn UI (Lua)"),
    QT_TRANSLATE_NOOP("Tabelle", "WeakAura (Lua)"),
    QT_TRANSLATE_NOOP("Tabelle", "Macro avanzata (Lua)"),
    QT_TRANSLATE_NOOP("Tabelle", "Boss tracking frame (Lua)"),
    nullptr
};
static const char* kWowSys[] = {
    "Sei un esperto di AddOn World of Warcraft (Retail 10.x o Classic). "
    "Genera SOLO codice Lua con frame, eventi, API Blizzard (UIParent, GameTooltip, ecc.). "
    "Includi struttura .toc + file .lua principale con commenti. "
    "Rispondi SOLO con blocco ```lua ... ```.",

    "Sei un esperto di WeakAuras 2 per WoW. "
    "Genera SOLO codice Lua per: condizione custom, trigger function, "
    "animazione e testo dinamico. Usa 'aura_env' dove necessario. "
    "Rispondi SOLO con blocco ```lua ... ```.",

    "Sei un esperto di macro WoW avanzate. "
    "Genera SOLO codice macro con /run, /cast, /use, castsequence, "
    "condizioni [mod:shift], [combat], [nocombat], [@mouseover]. "
    "Rispondi SOLO con blocco ```lua ... ```.",

    "Sei un esperto di AddOn WoW. "
    "Genera SOLO codice Lua per tracciare cooldown o cast di un boss con frame timer. "
    "Usa LibStub, Ace3, o frame puro con onUpdate. "
    "Rispondi SOLO con blocco ```lua ... ```.",

    nullptr
};
static const char* kWowFolder =
    "Games/World of Warcraft/_retail_/Interface/AddOns";

/* ── Noita ── */
static const char* kNoitaActions[] = {
    QT_TRANSLATE_NOOP("Tabelle", "Spell personalizzata (Lua)"),
    QT_TRANSLATE_NOOP("Tabelle", "Entity / creature (Lua + XML)"),
    QT_TRANSLATE_NOOP("Tabelle", "Generazione mondo (Lua)"),
    QT_TRANSLATE_NOOP("Tabelle", "Script barrel in-game (Lua)"),
    nullptr
};
static const char* kNoitaSys[] = {
    "Sei un esperto di modding Noita. "
    "Genera SOLO codice Lua per uno spell personalizzato (gun_actions.lua). "
    "Struttura: id, name, description, draw_actions, related_projectiles. "
    "Usa ProjectileFromFile(), CreateProjectile(), ecc. "
    "Rispondi SOLO con blocco ```lua ... ```.",

    "Sei un esperto di modding Noita. "
    "Genera SOLO codice Lua + XML per creare o modificare un'entity/creatura. "
    "XML: entity_file .xml con componenti (PhysicsBodyComponent, HealthComponent, ecc.). "
    "Lua: script AI del nemico. Rispondi SOLO con blocchi ```lua``` e ```xml```.",

    "Sei un esperto di modding Noita. "
    "Genera SOLO codice Lua per modificare la generazione del mondo: "
    "biomi, strutture, materiali, ore. Usa BiomeMap, BiomeVariables, ProcGen. "
    "Rispondi SOLO con blocco ```lua ... ```.",

    "Sei un esperto di modding Noita. "
    "Genera SOLO script Lua da inserire in un barrel/container in-game. "
    "Lo script si attiva all'esplosione. Usa GameGetWorldStateEntity(), "
    "EntityGetWithTag(), ecc. Rispondi SOLO con blocco ```lua ... ```.",

    nullptr
};
static const char* kNoitaFolder =
    ".steam/steam/steamapps/common/Noita/mods";

/* ── Minecraft ── */
static const char* kMinecraftActions[] = {
    QT_TRANSLATE_NOOP("Tabelle", "Datapack (JSON / mcfunction)"),
    QT_TRANSLATE_NOOP("Tabelle", "Fabric Mod (Java)"),
    QT_TRANSLATE_NOOP("Tabelle", "WorldGen bioma (JSON)"),
    QT_TRANSLATE_NOOP("Tabelle", "ResourcePack (JSON / lang)"),
    nullptr
};
static const char* kMinecraftSys[] = {
    "Sei un esperto di Minecraft Datapack (1.20+). "
    "Genera SOLO file JSON validi per advancement, recipe, loot_table, predicate "
    "o codice mcfunction. Specifica il path relativo in data/namespace/. "
    "Rispondi SOLO con blocco ```json ... ``` o ```mcfunction ... ```.",

    "Sei un esperto di modding Minecraft con Fabric (1.20+, Java 17). "
    "Genera SOLO codice Java con Fabric API. Includi: "
    "classe main @Mod, registrazione item/block, mixin se necessari. "
    "Rispondi SOLO con blocco ```java ... ```.",

    "Sei un esperto di Minecraft World Generation (1.18+ vanilla worldgen). "
    "Genera SOLO file JSON per: biome, configured_feature, placed_feature, "
    "noise_settings o structure. Segui la struttura registry namespace. "
    "Rispondi SOLO con blocco ```json ... ```.",

    "Sei un esperto di Minecraft ResourcePack. "
    "Genera SOLO file JSON per: item models, block models, lang (traduzioni), "
    "sounds.json o texture pack.mcmeta. Descrivi la struttura cartelle. "
    "Rispondi SOLO con blocco ```json ... ```.",

    nullptr
};
static const char* kMinecraftFolder =
    ".minecraft/mods";

/* ── Stardew Valley ── */
static const char* kStardewActions[] = {
    QT_TRANSLATE_NOOP("Tabelle", "Nuovo item / crop (JSON)"),
    QT_TRANSLATE_NOOP("Tabelle", "Evento / dialogo NPC (JSON)"),
    QT_TRANSLATE_NOOP("Tabelle", "Mod SMAPI (C#)"),
    QT_TRANSLATE_NOOP("Tabelle", "Modifica negozio (JSON patch)"),
    nullptr
};
static const char* kStardewSys[] = {
    "Sei un esperto di modding Stardew Valley con Content Patcher (formato 1.6). "
    "Genera SOLO file JSON per aggiungere item o crop personalizzati. "
    "Includi: ObjectInformation, Crops, CookingRecipes se necessari. "
    "Rispondi SOLO con blocco ```json ... ```.",

    "Sei un esperto di modding Stardew Valley. "
    "Genera SOLO JSON per Content Patcher per aggiungere o modificare "
    "dialoghi NPC, eventi stagionali, amicizia. "
    "Rispondi SOLO con blocco ```json ... ```.",

    "Sei un esperto di modding Stardew Valley con SMAPI. "
    "Genera SOLO codice C# (.NET 6): classe che implementa Mod con "
    "override Entry(IModHelper helper), uso di helper.Events, helper.Content. "
    "Rispondi SOLO con blocco ```csharp ... ```.",

    "Sei un esperto di modding Stardew Valley con Content Patcher. "
    "Genera SOLO patch JSON per modificare negozi (ShopMenu): "
    "aggiunta item, prezzi, condizioni stagionali. "
    "Rispondi SOLO con blocco ```json ... ```.",

    nullptr
};
static const char* kStardewFolder =
    ".steam/steam/steamapps/common/Stardew Valley/Mods";

/* ── RimWorld ── */
static const char* kRimworldActions[] = {
    QT_TRANSLATE_NOOP("Tabelle", "Def XML (ThingDef / RecipeDef)"),
    QT_TRANSLATE_NOOP("Tabelle", "Patch XML (PatchOperation)"),
    QT_TRANSLATE_NOOP("Tabelle", "Harmony C# patch"),
    QT_TRANSLATE_NOOP("Tabelle", "Scenario XML"),
    nullptr
};
static const char* kRimworldSys[] = {
    "Sei un esperto di modding RimWorld (1.5). "
    "Genera SOLO definizioni XML valide: ThingDef, RecipeDef, JobDef, "
    "TraitDef, ResearchProjectDef, ecc. "
    "Rispetta la struttura namespace Verse/RimWorld. "
    "Rispondi SOLO con blocco ```xml ... ```.",

    "Sei un esperto di modding RimWorld. "
    "Genera SOLO operazioni Patch XML: PatchOperationAdd, PatchOperationReplace, "
    "PatchOperationInsert, PatchOperationRemove con XPath corretti. "
    "Rispondi SOLO con blocco ```xml ... ```.",

    "Sei un esperto di modding RimWorld con Harmony. "
    "Genera SOLO codice C# per patch Harmony: [HarmonyPatch], Prefix, Postfix "
    "o Transpiler. Includi HarmonyLib using e annotazione target. "
    "Rispondi SOLO con blocco ```csharp ... ```.",

    "Sei un esperto di modding RimWorld. "
    "Genera SOLO codice XML per uno scenario personalizzato (ScenarioDef) "
    "con ScenarioPart_SetDifficulty, StartingThing, ecc. "
    "Rispondi SOLO con blocco ```xml ... ```.",

    nullptr
};
static const char* kRimworldFolder =
    ".steam/steam/steamapps/common/RimWorld/Mods";

/* ══════════════════════════════════════════════════════════════
   Tabella giochi
   ══════════════════════════════════════════════════════════════ */
struct ModGameDef {
    const char* label;
    const char* const* actions;
    const char* const* sysPrompts;
    const char* folderSuffix;   // relativo a QDir::homePath()
};

static const ModGameDef kModGames[] = {
    { "GTA V (ScriptHookV / Lua)",         kGtaActions,      kGtaSys,      kGtaFolder      },
    { "Skyrim SE / SKSE (Papyrus / XML)",   kSkyrimActions,   kSkyrimSys,   kSkyrimFolder   },
    { "Terraria (tModLoader C#)",           kTerrariaActions, kTerrariaSys, kTerrariaFolder },
    { "World of Warcraft (AddOn Lua)",      kWowActions,      kWowSys,      kWowFolder      },
    { "Noita (Lua)",                        kNoitaActions,    kNoitaSys,    kNoitaFolder    },
    { "Minecraft (Datapack / Fabric)",      kMinecraftActions,kMinecraftSys,kMinecraftFolder},
    { "Stardew Valley (SMAPI / CP)",        kStardewActions,  kStardewSys,  kStardewFolder  },
    { "RimWorld (XML / Harmony C#)",        kRimworldActions, kRimworldSys, kRimworldFolder },
    { nullptr, nullptr, nullptr, nullptr }
};

/* ══════════════════════════════════════════════════════════════
   buildGameModdingTab
   ══════════════════════════════════════════════════════════════ */
QWidget* ModGiochiWidget::buildGameModdingTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    /* Descrizione */
    auto* descLbl = new QLabel(
        "\xf0\x9f\x8e\xae  <i>Game Modding AI \xe2\x80\x94 Genera script, plugin e mod per "
        "GTA V, Skyrim, Terraria, WoW, Noita, Minecraft, Stardew Valley e RimWorld. "
        "Scegli il gioco, il tipo di mod e descrivi cosa vuoi creare.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Riga 1: Gioco + Tipo ── */
    auto* gameRow = new QWidget(w);
    auto* gameLay = new QHBoxLayout(gameRow);
    gameLay->setContentsMargins(0, 0, 0, 0);
    gameLay->setSpacing(8);

    auto* gameLbl = new QLabel(tr("Gioco:"), gameRow);
    gameLbl->setObjectName("hintLabel");
    m_moddingGameCombo = new QComboBox(gameRow);
    m_moddingGameCombo->setMinimumWidth(dpiScale(240));
    for (int i = 0; kModGames[i].label; i++)
        m_moddingGameCombo->addItem(
            QString::fromUtf8(kModGames[i].label));

    auto* typeLbl = new QLabel(tr("Tipo mod:"), gameRow);
    typeLbl->setObjectName("hintLabel");
    m_moddingTypeCombo = new QComboBox(gameRow);
    m_moddingTypeCombo->setMinimumWidth(dpiScale(200));

    gameLay->addWidget(gameLbl);
    gameLay->addWidget(m_moddingGameCombo, 2);
    gameLay->addWidget(typeLbl);
    gameLay->addWidget(m_moddingTypeCombo, 2);
    lay->addWidget(gameRow);

    /* ── Riga 2: Cartella mod ── */
    auto* folderRow = new QWidget(w);
    auto* folderLay = new QHBoxLayout(folderRow);
    folderLay->setContentsMargins(0, 0, 0, 0);
    folderLay->setSpacing(8);

    auto* folderLbl = new QLabel(tr("Cartella mod:"), folderRow);
    folderLbl->setObjectName("hintLabel");
    m_moddingFolderEdit = new QLineEdit(folderRow);
    m_moddingFolderEdit->setPlaceholderText(
        tr("Percorso cartella in cui salvare il mod..."));

    auto* browseBtn = new QPushButton(
        tr("\xf0\x9f\x93\x82  Sfoglia"), folderRow);
    browseBtn->setObjectName("actionBtn");
    browseBtn->setFixedWidth(dpiScale(100));

    auto* openBtn = new QPushButton(
        tr("\xf0\x9f\x93\x82  Apri"), folderRow);
    openBtn->setObjectName("actionBtn");
    openBtn->setFixedWidth(dpiScale(80));

    m_moddingStatusLbl = new QLabel(
        tr("\xe2\x9a\xaa  Pronto"), folderRow);
    m_moddingStatusLbl->setObjectName("hintLabel");

    folderLay->addWidget(folderLbl);
    folderLay->addWidget(m_moddingFolderEdit, 1);
    folderLay->addWidget(browseBtn);
    folderLay->addWidget(openBtn);
    folderLay->addWidget(m_moddingStatusLbl, 1);
    lay->addWidget(folderRow);

    /* ── Riga 3: Modello AI ── */
    auto* modelRow = new QWidget(w);
    auto* modelLay = new QHBoxLayout(modelRow);
    modelLay->setContentsMargins(0, 0, 0, 0);
    modelLay->setSpacing(8);

    auto* modelLbl = new QLabel(tr("Modello AI:"), modelRow);
    modelLbl->setObjectName("hintLabel");
    m_moddingModel = new ModelComboBox(m_ai, modelRow);

    modelLay->addWidget(modelLbl);
    modelLay->addWidget(m_moddingModel, 1);
    modelLay->addStretch();
    lay->addWidget(modelRow);

    /* ── Input descrizione ── */
    m_moddingInput = new QTextEdit(w);
    m_moddingInput->setPlaceholderText(
        tr("Descrivi il mod che vuoi creare...\n"
           "Es: 'Script GTA V che spawna un jet davanti al giocatore premendo F9'\n"
           "Es: 'Skyrim: incantesimo che genera un muro di fuoco attorno al player'\n"
           "Es: 'WoW addon che mostra un timer per la CD di Ira dell'Eroe'"));
    m_moddingInput->setFixedHeight(dpiScale(90));
    lay->addWidget(m_moddingInput);

    /* ── Pulsanti Genera / Stop / Salva ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_moddingRunBtn = new QPushButton(
        tr("\xf0\x9f\xa4\x96  Genera mod"), btnRow);
    m_moddingRunBtn->setObjectName("actionBtn");

    m_moddingStopBtn = new QPushButton(
        tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_moddingStopBtn->setObjectName("actionBtn");
    m_moddingStopBtn->setProperty("danger", true);
    m_moddingStopBtn->setEnabled(false);

    m_moddingSaveBtn = new QPushButton(
        tr("\xf0\x9f\x92\xbe  Salva nel gioco"), btnRow);
    m_moddingSaveBtn->setObjectName("actionBtn");
    m_moddingSaveBtn->setEnabled(false);

    btnLay->addWidget(m_moddingRunBtn);
    btnLay->addWidget(m_moddingStopBtn);
    btnLay->addWidget(m_moddingSaveBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* ── Output codice generato ── */
    m_moddingOutput = new QTextEdit(w);
    m_moddingOutput->setReadOnly(true);
    m_moddingOutput->setObjectName("outputView");
    m_moddingOutput->setPlaceholderText(
        tr("Il codice del mod generato dall'AI appare qui..."));
    lay->addWidget(m_moddingOutput, 1);

    /* ── Connessioni ── */
    connect(m_moddingGameCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModGiochiWidget::onModdingGameChanged);
    connect(browseBtn, &QPushButton::clicked,
            this, &ModGiochiWidget::onModdingBrowseClicked);
    connect(openBtn, &QPushButton::clicked,
            this, &ModGiochiWidget::onModdingOpenFolderClicked);
    connect(m_moddingRunBtn, &QPushButton::clicked,
            this, &ModGiochiWidget::onModdingRunClicked);
    connect(m_moddingStopBtn, &QPushButton::clicked,
            this, &ModGiochiWidget::onModdingStopClicked);
    connect(m_moddingSaveBtn, &QPushButton::clicked,
            this, &ModGiochiWidget::onModdingSaveClicked);

    /* Inizializza il combo tipi con il primo gioco */
    onModdingGameChanged(0);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Slot Game Modding
   ══════════════════════════════════════════════════════════════ */

void ModGiochiWidget::onModdingGameChanged(int idx)
{
    if (idx < 0 || !kModGames[idx].label) return;
    m_moddingTypeCombo->blockSignals(true);
    m_moddingTypeCombo->clear();
    for (int i = 0; kModGames[idx].actions[i]; i++)
        m_moddingTypeCombo->addItem(
            P::trTab(kModGames[idx].actions[i]));
    m_moddingTypeCombo->blockSignals(false);

    /* Aggiorna il percorso cartella suggerito */
    const QString folder =
        QDir::homePath() + "/" +
        QString::fromUtf8(kModGames[idx].folderSuffix);
    m_moddingFolderEdit->setText(folder);
    m_moddingCode.clear();
    if (m_moddingSaveBtn) m_moddingSaveBtn->setEnabled(false);
    if (m_moddingStatusLbl)
        m_moddingStatusLbl->setText(tr("\xe2\x9a\xaa  Pronto"));
}

void ModGiochiWidget::onModdingRunClicked()
{
    const int gameIdx = m_moddingGameCombo->currentIndex();
    const int typeIdx = m_moddingTypeCombo->currentIndex();
    if (gameIdx < 0 || !kModGames[gameIdx].label) return;
    if (typeIdx < 0 || !kModGames[gameIdx].sysPrompts[typeIdx]) return;

    const QString sys = QString::fromUtf8(
        kModGames[gameIdx].sysPrompts[typeIdx]);

    runAi(1, sys,
          m_moddingInput->toPlainText(),
          m_moddingOutput,
          m_moddingRunBtn,
          m_moddingStopBtn,
          m_moddingModel);
}

void ModGiochiWidget::onModdingStopClicked()
{
    m_ai->abort();
    m_moddingRunBtn->setEnabled(true);
    m_moddingStopBtn->setEnabled(false);
    m_moddingOutput->append("\n\xe2\x8f\xb9  Generazione fermata.");
}

void ModGiochiWidget::onModdingSaveClicked()
{
    if (m_moddingCode.isEmpty()) return;

    QString folder = m_moddingFolderEdit->text().trimmed();
    if (folder.isEmpty())
        folder = QDir::homePath();

    QDir dir(folder);
    if (!dir.exists()) {
        const bool ok = QMessageBox::question(
            this,
            tr("Crea cartella?"),
            tr("La cartella '%1' non esiste. Crearla?").arg(folder),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
        if (!ok) return;
        dir.mkpath(".");
    }

    /* Nome file automatico in base al gioco e tipo */
    const int gameIdx = m_moddingGameCombo->currentIndex();
    const int typeIdx = m_moddingTypeCombo->currentIndex();
    QString gameName  = (gameIdx >= 0 && kModGames[gameIdx].label)
        ? QString::fromUtf8(kModGames[gameIdx].label)
              .section('(', 0, 0).trimmed().replace(' ', '_')
        : "mod";
    Q_UNUSED(typeIdx)

    const QString ts   = QDateTime::currentDateTime()
                             .toString("yyyyMMdd_HHmmss");
    const QString ext  = detectModExtension(m_moddingCode);
    const QString name = gameName + "_" + ts + "." + ext;
    const QString path = dir.filePath(name);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_moddingStatusLbl->setText(
            "\xe2\x9d\x8c  Impossibile scrivere: " + path);
        return;
    }
    f.write(m_moddingCode.toUtf8());
    f.close();

    m_moddingStatusLbl->setText(
        "\xe2\x9c\x85  Salvato: " + path);
}

void ModGiochiWidget::onModdingBrowseClicked()
{
    const QString start = m_moddingFolderEdit->text().trimmed().isEmpty()
        ? QDir::homePath()
        : m_moddingFolderEdit->text().trimmed();

    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Seleziona cartella mod"), start);
    if (!dir.isEmpty())
        m_moddingFolderEdit->setText(dir);
}

void ModGiochiWidget::onModdingOpenFolderClicked()
{
    const QString folder = m_moddingFolderEdit->text().trimmed();
    if (folder.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

/* Helper: indovina l'estensione dal codice generato */
QString ModGiochiWidget::detectModExtension(const QString& code)
{
    if (code.contains("```csharp") || code.contains("using GTA")
        || code.contains("using Terraria") || code.contains("IModHelper"))
        return "cs";
    if (code.contains("```java"))
        return "java";
    if (code.contains("```lua") || code.contains("Citizen.Create")
        || code.contains("AddEventHandler") || code.contains("draw_actions"))
        return "lua";
    if (code.contains("```papyrus") || code.contains("ScriptName "))
        return "psc";
    if (code.contains("```xml") || code.startsWith('<'))
        return "xml";
    if (code.contains("```json") || code.startsWith('{'))
        return "json";
    if (code.contains("```mcfunction"))
        return "mcfunction";
    return "txt";
}
