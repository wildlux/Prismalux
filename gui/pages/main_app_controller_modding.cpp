/* ══════════════════════════════════════════════════════════════
   main_app_controller_modding.cpp
   Tab Game Modding per AppControllerPage:
     - GTA V (ScriptHookVDotNet / Lua)
     - Skyrim SE / SKSE (Papyrus / XML)
     - Terraria tModLoader (C#)
     - World of Warcraft AddOn (Lua)
     - Noita (Lua)
     - Minecraft Datapack/Fabric (JSON/Java)
     - Stardew Valley SMAPI (C# / JSON)
     - RimWorld (XML / C# Harmony)
   ══════════════════════════════════════════════════════════════ */
#include "main_app_controller.h"
#include "../prismalux_paths.h"
#include "../widgets/model_combo_box.h"
#include "../dpi_utils.h"
namespace P = PrismaluxPaths;

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
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

/* ═══════════════════════════════════════════════════════════════
   System prompts per ogni gioco + tipo di mod
   ════════════════════════════════════════════════════════════════ */

/* ── GTA V ── */
static const char* kGtaActions[] = {
    "Script missione (C#)",
    "Menu trainer (C#)",
    "Script Lua (FiveM/RAGE)",
    "Modifica veicolo / skin",
    "Teleporter / cheat F",
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
    "Script Papyrus (.psc)",
    "Incantesimo / Perk / Abilita'",
    "Quest scripting Papyrus",
    "Item / Arma / Armatura (XML)",
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
    "Nuovo item (C#)",
    "Nuovo boss / NPC (C#)",
    "Meccanica / Hook (C#)",
    "Nuovo proiettile (C#)",
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
    "AddOn UI (Lua)",
    "WeakAura (Lua)",
    "Macro avanzata (Lua)",
    "Boss tracking frame (Lua)",
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
    "Spell personalizzata (Lua)",
    "Entity / creature (Lua + XML)",
    "Generazione mondo (Lua)",
    "Script barrel in-game (Lua)",
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
    "Datapack (JSON / mcfunction)",
    "Fabric Mod (Java)",
    "WorldGen bioma (JSON)",
    "ResourcePack (JSON / lang)",
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
    "Nuovo item / crop (JSON)",
    "Evento / dialogo NPC (JSON)",
    "Mod SMAPI (C#)",
    "Modifica negozio (JSON patch)",
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
    "Def XML (ThingDef / RecipeDef)",
    "Patch XML (PatchOperation)",
    "Harmony C# patch",
    "Scenario XML",
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
   buildGameModdingTab — Tab Game Modding (tabIdx logico = 15)
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildGameModdingTab()
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
        "\xf0\x9f\x93\x82  Sfoglia", folderRow);
    browseBtn->setObjectName("actionBtn");
    browseBtn->setFixedWidth(dpiScale(100));

    auto* openBtn = new QPushButton(
        "\xf0\x9f\x93\x82  Apri", folderRow);
    openBtn->setObjectName("actionBtn");
    openBtn->setFixedWidth(dpiScale(80));

    m_moddingStatusLbl = new QLabel(
        "\xe2\x9a\xaa  Pronto", folderRow);
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
        "\xf0\x9f\xa4\x96  Genera mod", btnRow);
    m_moddingRunBtn->setObjectName("actionBtn");

    m_moddingStopBtn = new QPushButton(
        "\xe2\x8f\xb9  Stop", btnRow);
    m_moddingStopBtn->setObjectName("actionBtn");
    m_moddingStopBtn->setProperty("danger", true);
    m_moddingStopBtn->setEnabled(false);

    m_moddingSaveBtn = new QPushButton(
        "\xf0\x9f\x92\xbe  Salva nel gioco", btnRow);
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
            this, &AppControllerPage::onModdingGameChanged);
    connect(browseBtn, &QPushButton::clicked,
            this, &AppControllerPage::onModdingBrowseClicked);
    connect(openBtn, &QPushButton::clicked,
            this, &AppControllerPage::onModdingOpenFolderClicked);
    connect(m_moddingRunBtn, &QPushButton::clicked,
            this, &AppControllerPage::onModdingRunClicked);
    connect(m_moddingStopBtn, &QPushButton::clicked,
            this, &AppControllerPage::onModdingStopClicked);
    connect(m_moddingSaveBtn, &QPushButton::clicked,
            this, &AppControllerPage::onModdingSaveClicked);

    /* Inizializza il combo tipi con il primo gioco */
    onModdingGameChanged(0);

    return w;
}

/* ══════════════════════════════════════════════════════════════
   Slot Game Modding
   ══════════════════════════════════════════════════════════════ */

void AppControllerPage::onModdingGameChanged(int idx)
{
    if (idx < 0 || !kModGames[idx].label) return;
    m_moddingTypeCombo->blockSignals(true);
    m_moddingTypeCombo->clear();
    for (int i = 0; kModGames[idx].actions[i]; i++)
        m_moddingTypeCombo->addItem(
            QString::fromUtf8(kModGames[idx].actions[i]));
    m_moddingTypeCombo->blockSignals(false);

    /* Aggiorna il percorso cartella suggerito */
    const QString folder =
        QDir::homePath() + "/" +
        QString::fromUtf8(kModGames[idx].folderSuffix);
    m_moddingFolderEdit->setText(folder);
    m_moddingCode.clear();
    if (m_moddingSaveBtn) m_moddingSaveBtn->setEnabled(false);
    if (m_moddingStatusLbl)
        m_moddingStatusLbl->setText("\xe2\x9a\xaa  Pronto");
}

void AppControllerPage::onModdingRunClicked()
{
    const int gameIdx = m_moddingGameCombo->currentIndex();
    const int typeIdx = m_moddingTypeCombo->currentIndex();
    if (gameIdx < 0 || !kModGames[gameIdx].label) return;
    if (typeIdx < 0 || !kModGames[gameIdx].sysPrompts[typeIdx]) return;

    const QString sys = QString::fromUtf8(
        kModGames[gameIdx].sysPrompts[typeIdx]);

    runAi(15, sys,
          m_moddingInput->toPlainText(),
          m_moddingOutput,
          m_moddingRunBtn,
          m_moddingStopBtn,
          m_moddingModel);
}

void AppControllerPage::onModdingStopClicked()
{
    m_ai->abort();
    m_moddingRunBtn->setEnabled(true);
    m_moddingStopBtn->setEnabled(false);
    m_moddingOutput->append("\n\xe2\x8f\xb9  Generazione fermata.");
}

void AppControllerPage::onModdingSaveClicked()
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

void AppControllerPage::onModdingBrowseClicked()
{
    const QString start = m_moddingFolderEdit->text().trimmed().isEmpty()
        ? QDir::homePath()
        : m_moddingFolderEdit->text().trimmed();

    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Seleziona cartella mod"), start);
    if (!dir.isEmpty())
        m_moddingFolderEdit->setText(dir);
}

void AppControllerPage::onModdingOpenFolderClicked()
{
    const QString folder = m_moddingFolderEdit->text().trimmed();
    if (folder.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

/* Helper: indovina l'estensione dal codice generato */
QString AppControllerPage::detectModExtension(const QString& code)
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
