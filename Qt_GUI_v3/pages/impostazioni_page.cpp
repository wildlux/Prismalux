#include "impostazioni_page.h"
#include "personalizza_page.h"
#include "manutenzione_page.h"
#include <QVBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QScrollArea>
#include <QFrame>
#include <QGroupBox>

/* ══════════════════════════════════════════════════════════════
   ImpostazioniPage — QTabWidget diretto, nessun menu intermedio
   Tab 0: "🔧 Backend & Modelli"  (ManutenzioneePage)
   Tab 1: "🔩 Avanzato"           (PersonalizzaPage)
   ══════════════════════════════════════════════════════════════ */
ImpostazioniPage::ImpostazioniPage(AiClient* ai, HardwareMonitor* hw, QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("settingsTabs");
    m_tabs->setDocumentMode(true);  /* stile piatto, senza bordo extra */

    /* Tab 0 — Backend & Modelli */
    m_manutenzione = new ManutenzioneePage(ai, hw, m_tabs);
    m_tabs->addTab(m_manutenzione, "\xf0\x9f\x94\xa7  Backend & Modelli");

    /* Tab 1 — Avanzato (llama.cpp Studio, Cython Studio, VRAM bench) */
    m_personalizza = new PersonalizzaPage(m_tabs);
    m_tabs->addTab(m_personalizza, "\xf0\x9f\x94\xa9  Avanzato");

    /* Tab 2 — RAG (Retrieval-Augmented Generation) */
    {
        auto* ragPage   = new QWidget(m_tabs);
        auto* ragLay    = new QVBoxLayout(ragPage);
        ragLay->setContentsMargins(24, 20, 24, 16);
        ragLay->setSpacing(16);

        auto* ragTitle = new QLabel("\xf0\x9f\x93\x8e  RAG \xe2\x80\x94 Retrieval-Augmented Generation", ragPage);
        ragTitle->setObjectName("pageTitle");
        ragLay->addWidget(ragTitle);

        auto* grpWhatIs = new QGroupBox("Cos'\xc3\xa8 il RAG?", ragPage);
        grpWhatIs->setStyleSheet(
            "QGroupBox { border:1px solid #1e2040; border-radius:6px; "
            "margin-top:12px; color:#8a8fb0; font-size:12px; }"
            "QGroupBox::title { subcontrol-origin:margin; left:10px; padding:0 4px; }");
        auto* grpLay = new QVBoxLayout(grpWhatIs);

        auto* desc = new QLabel(
            "<p style='color:#cdd6f4; font-size:13px; line-height:1.6;'>"
            "Il <b>RAG</b> (Retrieval-Augmented Generation) serve per <b>migliorare le informazioni "
            "contenute in un modello AI</b>, fornendogli dati aggiornati provenienti da file esterni "
            "come PDF, TXT, DOCX e altro ancora."
            "</p>"
            "<p style='color:#cdd6f4; font-size:13px; line-height:1.6;'>"
            "In pratica, il modello AI non viene ri-addestrato, ma <b>legge i tuoi documenti al momento "
            "della risposta</b>, arricchendo le sue conoscenze con il contenuto che gli fornisci."
            "</p>",
            ragPage);
        desc->setObjectName("cardDesc");
        desc->setWordWrap(true);
        grpLay->addWidget(desc);
        ragLay->addWidget(grpWhatIs);

        auto* grpFormati = new QGroupBox("Tipi di documenti supportati", ragPage);
        grpFormati->setStyleSheet(grpWhatIs->styleSheet());
        auto* fmtLay = new QVBoxLayout(grpFormati);

        auto* fmtDesc = new QLabel(
            "<ul style='color:#a6e3a1; font-size:13px; margin:4px 0; padding-left:18px;'>"
            "<li><b>PDF</b> \xe2\x80\x94 libri, dispense universitarie, manuali tecnici, ricerche scientifiche</li>"
            "<li><b>TXT</b> \xe2\x80\x94 note personali, appunti, documentazione plain-text</li>"
            "<li><b>MD</b> \xe2\x80\x94 documentazione Markdown (README, wiki)</li>"
            "<li><b>PY / CPP / C / JS</b> \xe2\x80\x94 codice sorgente da analizzare o correggere</li>"
            "<li><b>JSON / YAML / CSV</b> \xe2\x80\x94 dati strutturati, configurazioni, dataset</li>"
            "<li><b>HTML / HTM</b> \xe2\x80\x94 pagine web, documentazione HTML</li>"
            "</ul>",
            ragPage);
        fmtDesc->setObjectName("cardDesc");
        fmtDesc->setWordWrap(true);
        fmtLay->addWidget(fmtDesc);
        ragLay->addWidget(grpFormati);

        auto* grpMaterie = new QGroupBox("Esempi di utilizzo per materia", ragPage);
        grpMaterie->setStyleSheet(grpWhatIs->styleSheet());
        auto* matLay = new QVBoxLayout(grpMaterie);

        auto* matDesc = new QLabel(
            "<table style='color:#cdd6f4; font-size:12px; border-collapse:collapse; width:100%;'>"
            "<tr><td style='padding:4px 8px; color:#89b4fa; font-weight:bold;'>\xf0\x9f\x93\x90 Matematica</td>"
            "<td style='padding:4px 8px;'>Carica libri di testo, teoremi, esercizi risolti \xe2\x80\x94 l'AI risponde con precisione formale</td></tr>"
            "<tr><td style='padding:4px 8px; color:#89b4fa; font-weight:bold;'>\xe2\x9a\x9b\xef\xb8\x8f Fisica</td>"
            "<td style='padding:4px 8px;'>Dispense del professore, formule, esercizi \xe2\x80\x94 risposte contestualizzate al tuo programma</td></tr>"
            "<tr><td style='padding:4px 8px; color:#89b4fa; font-weight:bold;'>\xf0\x9f\xa7\xaa Chimica</td>"
            "<td style='padding:4px 8px;'>Tavole periodiche, reazioni, meccanismi \xe2\x80\x94 il modello conosce la tua nomenclatura</td></tr>"
            "<tr><td style='padding:4px 8px; color:#89b4fa; font-weight:bold;'>\xf0\x9f\x87\xae\xf0\x9f\x87\xb9 Italiano</td>"
            "<td style='padding:4px 8px;'>Testi letterari, analisi critiche, grammatica \xe2\x80\x94 analisi personalizzata sul testo</td></tr>"
            "<tr><td style='padding:4px 8px; color:#89b4fa; font-weight:bold;'>\xf0\x9f\x87\xac\xf0\x9f\x87\xa7 Inglese</td>"
            "<td style='padding:4px 8px;'>Articoli, romanzi, grammatica avanzata \xe2\x80\x94 traduzioni e spiegazioni contestuali</td></tr>"
            "<tr><td style='padding:4px 8px; color:#89b4fa; font-weight:bold;'>\xf0\x9f\x92\xbb Informatica</td>"
            "<td style='padding:4px 8px;'>Codice sorgente, documentazione API, RFC \xe2\x80\x94 analisi e correzione del codice reale</td></tr>"
            "<tr><td style='padding:4px 8px; color:#89b4fa; font-weight:bold;'>\xf0\x9f\x8c\x8d Qualsiasi materia</td>"
            "<td style='padding:4px 8px;'>Il RAG non ha limiti di argomento: biologia, storia, diritto, economia e molto altro</td></tr>"
            "</table>",
            ragPage);
        matDesc->setObjectName("cardDesc");
        matDesc->setWordWrap(true);
        matLay->addWidget(matDesc);
        ragLay->addWidget(grpMaterie);

        auto* grpUsage = new QGroupBox("Come usare il RAG in Prismalux", ragPage);
        grpUsage->setStyleSheet(grpWhatIs->styleSheet());
        auto* useLay = new QVBoxLayout(grpUsage);

        auto* useDesc = new QLabel(
            "<ol style='color:#cdd6f4; font-size:13px; line-height:1.8; padding-left:18px;'>"
            "<li>Vai su <b>Agenti AI</b> \xe2\x86\x92 clicca su <b>\xe2\x9a\x99 Agenti...</b></li>"
            "<li>Per ogni agente trovi un'area <b>\xf0\x9f\x93\x8e Trascina file</b></li>"
            "<li>Trascina i tuoi documenti (PDF, TXT, MD, codice...) su quell'area</li>"
            "<li>Il contenuto viene iniettato automaticamente nel prompt di quell'agente</li>"
            "<li>L'AI risponde tenendo conto del contenuto dei tuoi file</li>"
            "</ol>"
            "<p style='color:#fab387; font-size:12px; margin-top:8px;'>"
            "\xe2\x9a\xa0  Limite attuale: 16 KB totali per sessione (4 KB per file). "
            "Per documenti lunghi usa file TXT con le sezioni rilevanti estratte."
            "</p>",
            ragPage);
        useDesc->setObjectName("cardDesc");
        useDesc->setWordWrap(true);
        useLay->addWidget(useDesc);
        ragLay->addWidget(grpUsage);

        ragLay->addStretch(1);

        auto* scroll = new QScrollArea(m_tabs);
        scroll->setWidgetResizable(true);
        scroll->setWidget(ragPage);
        scroll->setFrameShape(QFrame::NoFrame);
        m_tabs->addTab(scroll, "\xf0\x9f\x93\x8e  RAG");
    }

    lay->addWidget(m_tabs);
}

void ImpostazioniPage::onHWReady(HWInfo hw) {
    if (m_manutenzione) m_manutenzione->onHWReady(hw);
}
