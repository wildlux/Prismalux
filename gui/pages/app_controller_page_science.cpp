/* ══════════════════════════════════════════════════════════════
   app_controller_page_science.cpp
   Tab scientifici per AppControllerPage:
     9  Godot MCP   — game engine (GDScript via godot-mcp)
   ══════════════════════════════════════════════════════════════ */
#include "app_controller_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>
#include <QTcpSocket>

/* ══════════════════════════════════════════════════════════════
   System prompts — Godot MCP
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
    "\xf0\x9f\x8e\xae  Script gameplay",
    "\xf0\x9f\x8c\x8d  Crea scena 2D",
    "\xf0\x9f\x95\xb9  Meccanica di gioco",
    "\xf0\x9f\x8c\x9f  Shader GLSL",
    "\xf0\x9f\x96\xa5  UI & Menu",
    "\xf0\x9f\x90\x8d  Script libero",
    nullptr
};

/* ══════════════════════════════════════════════════════════════
   buildGodotTab — Tab 9
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildGodotTab()
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

    m_godotStatusLbl = new QLabel("\xe2\x9a\xaa  GDScript pronto", connRow);
    m_godotStatusLbl->setObjectName("hintLabel");

    m_godotExecBtn = new QPushButton(
        "\xf0\x9f\x8e\xae  Salva script .gd", connRow);
    m_godotExecBtn->setObjectName("actionBtn");
    m_godotExecBtn->setFixedWidth(160);
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
        m_godotAction->addItem(QString::fromUtf8(kGodotActions[i]));
    m_godotModel = new QComboBox(toolRow);
    m_godotModel->setMinimumWidth(180);
    populateModels(m_godotModel);
    toolLay->addWidget(new QLabel("Tipo:", toolRow));
    toolLay->addWidget(m_godotAction, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_godotModel, 1);
    lay->addWidget(toolRow);

    /* Input */
    m_godotInput = new QTextEdit(w);
    m_godotInput->setPlaceholderText(
        "Descrivi cosa vuoi fare in Godot...\n"
        "Es: 'Crea un personaggio che si muove con WASD e salta con spazio'\n"
        "Es: 'Shader che simula acqua con onde'");
    m_godotInput->setFixedHeight(80);
    lay->addWidget(m_godotInput);

    /* Pulsanti */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_godotRunBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera GDScript", btnRow);
    m_godotRunBtn->setObjectName("actionBtn");
    m_godotStopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
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
    out->setPlaceholderText("GDScript generato dall'AI appare qui...");
    m_godotOutput = out;
    lay->addWidget(m_godotOutput, 1);

    /* Salva script .gd */
    connect(m_godotExecBtn, &QPushButton::clicked, this, [this](){
        if (m_godotCode.isEmpty()) return;
        const QString path = QDir::homePath() + "/ai_generated.gd";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(m_godotCode.toUtf8());
            m_godotStatusLbl->setText(
                "\xe2\x9c\x85  Salvato: " + path);
        } else {
            m_godotStatusLbl->setText("\xe2\x9d\x8c  Impossibile salvare il file");
        }
    });

    connect(m_godotRunBtn, &QPushButton::clicked, this, [this](){
        const int idx = m_godotAction->currentIndex();
        if (idx < 0 || !kGodotSys[idx]) return;
        runAi(9, QString::fromUtf8(kGodotSys[idx]),
              m_godotInput->toPlainText(),
              m_godotOutput, m_godotRunBtn, m_godotStopBtn,
              m_godotModel);
    });
    connect(m_godotStopBtn, &QPushButton::clicked, this, [this](){
        m_ai->abort();
        m_godotRunBtn->setEnabled(true);
        m_godotStopBtn->setEnabled(false);
        m_godotOutput->append("\n\xe2\x8f\xb9  Fermato.");
    });

    return w;
}
