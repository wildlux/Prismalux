#include "settings_model_profiles.h"
#include "../model_processor.h"
#include "../dpi_utils.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QMessageBox>
#include <QUuid>

/* ── Costruttore ────────────────────────────────────────────── */
ModelProfilesTab::ModelProfilesTab(QWidget* parent) : QWidget(parent) {
    buildUi();
    refreshList();
}

/* ── UI ─────────────────────────────────────────────────────── */
void ModelProfilesTab::buildUi() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(dpiScale(8), dpiScale(8), dpiScale(8), dpiScale(8));
    root->setSpacing(dpiScale(10));

    auto* spl = new QSplitter(Qt::Horizontal, this);
    root->addWidget(spl);

    /* ── pannello sinistro: lista + bottoni ── */
    auto* leftW = new QWidget;
    auto* leftV = new QVBoxLayout(leftW);
    leftV->setContentsMargins(0, 0, 0, 0);
    leftV->setSpacing(dpiScale(6));

    auto* listLbl = new QLabel(tr("<b>Profili attivi</b>"), leftW);
    leftV->addWidget(listLbl);

    m_list = new QListWidget(leftW);
    m_list->setAlternatingRowColors(true);
    leftV->addWidget(m_list);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(dpiScale(4));
    m_btnAdd    = new QPushButton(tr("\xe2\x9e\x95  Aggiungi"), leftW);
    m_btnDel    = new QPushButton(tr("\xf0\x9f\x97\x91  Elimina"), leftW);
    m_btnToggle = new QPushButton(tr("\xe2\x9c\x85  Attiva/Dis."), leftW);
    m_btnDefs   = new QPushButton(tr("\xf0\x9f\x93\x8b  Predefiniti"), leftW);
    btnRow->addWidget(m_btnAdd);
    btnRow->addWidget(m_btnDel);
    btnRow->addWidget(m_btnToggle);
    btnRow->addWidget(m_btnDefs);
    leftV->addLayout(btnRow);

    leftW->setMinimumWidth(dpiScale(180));
    spl->addWidget(leftW);

    /* ── pannello destro: form editing ── */
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* formW = new QWidget;
    scroll->setWidget(formW);
    auto* formV = new QVBoxLayout(formW);
    formV->setContentsMargins(dpiScale(4), dpiScale(4), dpiScale(4), dpiScale(4));
    formV->setSpacing(dpiScale(8));

    /* Dati base */
    auto* grpBase = new QGroupBox(tr("\xf0\x9f\x93\x9d  Profilo"), formW);
    auto* fl = new QFormLayout(grpBase);
    fl->setSpacing(dpiScale(6));
    m_edLabel   = new QLineEdit(grpBase);
    m_edPattern = new QLineEdit(grpBase);
    m_edPattern->setPlaceholderText(tr("es: deepseek.* qwen.* mistral.*"));
    m_edPattern->setToolTip(tr("Espressione regolare (case-insensitive) abbinata al nome modello.\n"
                            "Il primo profilo che matcha viene applicato."));
    m_chkEnabled = new QCheckBox(tr("Attivato"), grpBase);
    m_chkEnabled->setChecked(true);
    fl->addRow("Nome profilo:", m_edLabel);
    fl->addRow("Pattern modello:", m_edPattern);
    fl->addRow("", m_chkEnabled);
    formV->addWidget(grpBase);

    /* Pre-processing */
    auto* grpPre = new QGroupBox(tr("\xe2\x9c\x8f\xef\xb8\x8f  Pre-processing (prima di inviare all'LLM)"), formW);
    auto* preV = new QVBoxLayout(grpPre);
    preV->setSpacing(dpiScale(6));

    auto* langRow = new QHBoxLayout;
    langRow->addWidget(new QLabel(tr("Forza lingua output:"), grpPre));
    m_cmbLang = new QComboBox(grpPre);
    m_cmbLang->addItems({
        "Nessuna",
        "\xf0\x9f\x87\xae\xf0\x9f\x87\xb9  Italiano",
        "\xf0\x9f\x87\xac\xf0\x9f\x87\xa7  Inglese",
        "\xf0\x9f\x87\xab\xf0\x9f\x87\xb7  Francese",
        "\xf0\x9f\x87\xa8\xf0\x9f\x87\xb3  Cinese",
        "\xf0\x9f\x87\xa9\xf0\x9f\x87\xaa  Tedesco"
    });
    m_cmbLang->setToolTip(tr("Aggiunge una riga al system prompt che impone la lingua di risposta."));
    langRow->addWidget(m_cmbLang);
    langRow->addStretch();
    preV->addLayout(langRow);

    auto* prepSysRow = new QHBoxLayout;
    m_chkPrepSys = new QCheckBox(tr("Aggiungi testo al system prompt:"), grpPre);
    prepSysRow->addWidget(m_chkPrepSys);
    preV->addLayout(prepSysRow);
    m_edPrepSys = new QTextEdit(grpPre);
    m_edPrepSys->setPlaceholderText(tr("Testo anteposto al system prompt di ogni chiamata..."));
    m_edPrepSys->setFixedHeight(dpiScale(60));
    preV->addWidget(m_edPrepSys);

    auto* prepUserRow = new QHBoxLayout;
    m_chkPrepUser = new QCheckBox(tr("Aggiungi prefisso al messaggio utente:"), grpPre);
    prepUserRow->addWidget(m_chkPrepUser);
    preV->addLayout(prepUserRow);
    m_edPrepUser = new QLineEdit(grpPre);
    m_edPrepUser->setPlaceholderText(tr("es: [Rispondi solo in italiano] "));
    preV->addWidget(m_edPrepUser);

    auto* replSysRow = new QHBoxLayout;
    m_chkReplSys = new QCheckBox(tr("Sostituisci completamente il system prompt:"), grpPre);
    replSysRow->addWidget(m_chkReplSys);
    preV->addLayout(replSysRow);
    m_edReplSys = new QTextEdit(grpPre);
    m_edReplSys->setPlaceholderText(tr("System prompt sostitutivo completo..."));
    m_edReplSys->setFixedHeight(dpiScale(60));
    preV->addWidget(m_edReplSys);

    formV->addWidget(grpPre);

    /* Post-processing */
    auto* grpPost = new QGroupBox(tr("\xf0\x9f\x94\x8d  Post-processing (dopo la risposta LLM)"), formW);
    auto* postV = new QVBoxLayout(grpPost);
    postV->setSpacing(dpiScale(6));

    m_chkPostThink = new QCheckBox(tr("Rimuovi blocchi <think>…</think>"), grpPost);
    m_chkPostMd    = new QCheckBox(tr("Rimuovi formattazione Markdown (**bold**, ##header, `code`)"), grpPost);
    postV->addWidget(m_chkPostThink);
    postV->addWidget(m_chkPostMd);

    m_chkPostRx = new QCheckBox(tr("Sostituzione con regex:"), grpPost);
    postV->addWidget(m_chkPostRx);
    auto* rxRow = new QHBoxLayout;
    rxRow->addWidget(new QLabel(tr("Da:"), grpPost));
    m_edRxFrom = new QLineEdit(grpPost);
    m_edRxFrom->setPlaceholderText(tr("regex es: \\bAI\\b"));
    rxRow->addWidget(m_edRxFrom, 2);
    rxRow->addWidget(new QLabel("A:", grpPost));
    m_edRxTo = new QLineEdit(grpPost);
    m_edRxTo->setPlaceholderText("sostituzione");
    rxRow->addWidget(m_edRxTo, 2);
    postV->addLayout(rxRow);

    /* Traduzione LLM — lingua */
    auto* transRow = new QHBoxLayout;
    transRow->addWidget(new QLabel(tr("Traduzione LLM:"), grpPost));
    m_cmbTranslate = new QComboBox(grpPost);
    m_cmbTranslate->addItems({
        "Nessuna",
        "\xf0\x9f\x87\xae\xf0\x9f\x87\xb9  In italiano",
        "\xf0\x9f\x87\xac\xf0\x9f\x87\xa7  In inglese"
    });
    m_cmbTranslate->setToolTip(tr("Avvia una seconda chiamata LLM dedicata per tradurre la risposta.\n"
                               "Usa un modello leggero e veloce per massimizzare la qualità senza perdere velocità."));
    transRow->addWidget(m_cmbTranslate);
    transRow->addStretch();
    postV->addLayout(transRow);

    /* Modello dedicato per la traduzione */
    auto* transModelBox = new QGroupBox(tr("  Modello traduzione"), grpPost);
    transModelBox->setToolTip(tr("Modello Ollama usato SOLO per tradurre.\n"
                              "Scegli un modello leggero e multilingue per massima velocità."));
    auto* tmV = new QVBoxLayout(transModelBox);
    tmV->setSpacing(dpiScale(4));
    tmV->setContentsMargins(dpiScale(6), dpiScale(6), dpiScale(6), dpiScale(6));

    m_cmbTransModel = new QComboBox(transModelBox);
    m_cmbTransModel->addItems({
        "(stesso modello principale)",
        "aya:8b  \xe2\x80\x94 Cohere Aya, multilingue",
        "aya:35b  \xe2\x80\x94 Cohere Aya large",
        "qwen2.5:1.5b  \xe2\x80\x94 ultra leggero, 29 lingue",
        "qwen2.5:3b  \xe2\x80\x94 leggero, 29 lingue",
        "gemma2:2b  \xe2\x80\x94 Google, veloce",
        "llama3.2:1b  \xe2\x80\x94 Meta, ultra leggero",
        "llama3.2:3b  \xe2\x80\x94 Meta, leggero",
        "mistral:7b  \xe2\x80\x94 buono per IT/EN/FR",
        "(personalizzato)"
    });
    m_cmbTransModel->setToolTip(tr("Seleziona il modello per la traduzione.\n"
                                "Deve essere installato in Ollama (ollama pull <nome>)."));
    tmV->addWidget(m_cmbTransModel);

    m_edTransModelCustom = new QLineEdit(transModelBox);
    m_edTransModelCustom->setPlaceholderText(tr("es: opus-mt-it-en:latest  /  nllb-200:latest"));
    m_edTransModelCustom->setEnabled(false);
    tmV->addWidget(m_edTransModelCustom);

    postV->addWidget(transModelBox);

    /* Mostra il GroupBox modello solo se è attiva una traduzione */
    auto updateTransModelVis = [this, transModelBox]() {
        transModelBox->setVisible(m_cmbTranslate->currentIndex() > 0);
    };
    connect(m_cmbTranslate, &QComboBox::currentIndexChanged,
            transModelBox, [updateTransModelVis](int){ updateTransModelVis(); });
    updateTransModelVis();

    /* Personalizzato ↔ predefinito */
    connect(m_cmbTransModel, &QComboBox::currentIndexChanged,
            m_edTransModelCustom, [this](int idx){
                m_edTransModelCustom->setEnabled(idx == m_cmbTransModel->count() - 1);
            });

    formV->addWidget(grpPost);

    /* Bottone salva */
    m_btnSave = new QPushButton(tr("\xf0\x9f\x92\xbe  Salva modifiche"), formW);
    m_btnSave->setFixedHeight(dpiScale(34));
    formV->addWidget(m_btnSave);
    formV->addStretch();

    spl->addWidget(scroll);
    spl->setStretchFactor(0, 1);
    spl->setStretchFactor(1, 2);

    /* Connessioni */
    connect(m_list,    &QListWidget::currentRowChanged, this, &ModelProfilesTab::onListSelectionChanged);
    connect(m_btnAdd,    &QPushButton::clicked, this, &ModelProfilesTab::onAddClicked);
    connect(m_btnDel,    &QPushButton::clicked, this, &ModelProfilesTab::onDeleteClicked);
    connect(m_btnDefs,   &QPushButton::clicked, this, &ModelProfilesTab::onLoadDefaultsClicked);
    connect(m_btnSave,   &QPushButton::clicked, this, &ModelProfilesTab::onSaveClicked);
    connect(m_btnToggle, &QPushButton::clicked, this, &ModelProfilesTab::onToggleEnabledClicked);

    /* Abilitazione widget condizionati */
    connect(m_chkPrepSys,  &QCheckBox::toggled, m_edPrepSys,  &QTextEdit::setEnabled);
    connect(m_chkPrepUser, &QCheckBox::toggled, m_edPrepUser, &QLineEdit::setEnabled);
    connect(m_chkReplSys,  &QCheckBox::toggled, m_edReplSys,  &QTextEdit::setEnabled);
    connect(m_chkPostRx,   &QCheckBox::toggled, m_edRxFrom,   &QLineEdit::setEnabled);
    connect(m_chkPostRx,   &QCheckBox::toggled, m_edRxTo,     &QLineEdit::setEnabled);

    m_edPrepSys->setEnabled(false);
    m_edPrepUser->setEnabled(false);
    m_edReplSys->setEnabled(false);
    m_edRxFrom->setEnabled(false);
    m_edRxTo->setEnabled(false);
}

/* ── Lista ──────────────────────────────────────────────────── */
void ModelProfilesTab::refreshList() {
    m_loading = true;
    m_list->clear();
    const auto& profiles = ModelProcessor::instance().profiles();
    for (const auto& p : profiles) {
        const QString ico = p.enabled ? "\xe2\x9c\x85  " : "\xe2\x9d\x8c  ";
        auto* item = new QListWidgetItem(ico + p.label + "  [" + p.namePattern + "]");
        item->setData(Qt::UserRole, p.id);
        m_list->addItem(item);
    }
    m_loading = false;
}

int ModelProfilesTab::currentRow() const {
    return m_list->currentRow();
}

/* ── Slots lista ────────────────────────────────────────────── */
void ModelProfilesTab::onListSelectionChanged() {
    if (m_loading) return;
    const int row = currentRow();
    if (row < 0 || row >= ModelProcessor::instance().profiles().size()) return;
    loadProfileIntoForm(ModelProcessor::instance().profiles().at(row));
}

void ModelProfilesTab::onAddClicked() {
    ModelProfile p;
    p.id    = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.label = "Nuovo profilo";
    p.namePattern = ".*";
    auto list = ModelProcessor::instance().profiles();
    list.append(p);
    ModelProcessor::instance().setProfiles(list);
    refreshList();
    m_list->setCurrentRow(m_list->count() - 1);
}

void ModelProfilesTab::onDeleteClicked() {
    const int row = currentRow();
    if (row < 0) return;
    if (QMessageBox::question(this, tr("Elimina profilo"),
            "Eliminare il profilo selezionato?",
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    auto list = ModelProcessor::instance().profiles();
    list.removeAt(row);
    ModelProcessor::instance().setProfiles(list);
    refreshList();
}

void ModelProfilesTab::onLoadDefaultsClicked() {
    if (QMessageBox::question(this, tr("Carica predefiniti"),
            "Aggiungere i profili predefiniti alla lista attuale?\n"
            "(I profili esistenti non vengono rimossi)",
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    auto list = ModelProcessor::instance().profiles();
    const auto defs = ModelProcessor::defaultProfiles();
    /* Non duplicare profili con stesso pattern */
    for (const auto& d : defs) {
        bool found = false;
        for (const auto& existing : list)
            if (existing.namePattern == d.namePattern) { found = true; break; }
        if (!found) list.append(d);
    }
    ModelProcessor::instance().setProfiles(list);
    refreshList();
}

void ModelProfilesTab::onToggleEnabledClicked() {
    const int row = currentRow();
    if (row < 0) return;
    auto list = ModelProcessor::instance().profiles();
    list[row].enabled = !list[row].enabled;
    ModelProcessor::instance().setProfiles(list);
    refreshList();
    m_list->setCurrentRow(row);
}

/* ── Form: carica/salva ─────────────────────────────────────── */
void ModelProfilesTab::loadProfileIntoForm(const ModelProfile& p) {
    m_loading = true;

    m_edLabel->setText(p.label);
    m_edPattern->setText(p.namePattern);
    m_chkEnabled->setChecked(p.enabled);

    /* Pre-processing: lingua */
    int langIdx = 0;
    for (auto a : p.preActions) {
        if (a == PreAction::ForceLangIt) { langIdx = 1; break; }
        if (a == PreAction::ForceLangEn) { langIdx = 2; break; }
        if (a == PreAction::ForceLangFr) { langIdx = 3; break; }
        if (a == PreAction::ForceLangZh) { langIdx = 4; break; }
        if (a == PreAction::ForceLangDe) { langIdx = 5; break; }
    }
    m_cmbLang->setCurrentIndex(langIdx);

    /* Pre-processing: prepend system */
    const bool hasPrepSys = p.preActions.contains(PreAction::PrependSystem);
    m_chkPrepSys->setChecked(hasPrepSys);
    m_edPrepSys->setEnabled(hasPrepSys);
    m_edPrepSys->setPlainText(p.params.value("pre_sys"));

    /* Pre-processing: prepend user */
    const bool hasPrepUser = p.preActions.contains(PreAction::PrependUser);
    m_chkPrepUser->setChecked(hasPrepUser);
    m_edPrepUser->setEnabled(hasPrepUser);
    m_edPrepUser->setText(p.params.value("pre_user"));

    /* Pre-processing: replace system */
    const bool hasReplSys = p.preActions.contains(PreAction::ReplaceSystem);
    m_chkReplSys->setChecked(hasReplSys);
    m_edReplSys->setEnabled(hasReplSys);
    m_edReplSys->setPlainText(p.params.value("repl_sys"));

    /* Post-processing */
    m_chkPostThink->setChecked(p.postActions.contains(PostAction::RemoveThinkTags));
    m_chkPostMd->setChecked(p.postActions.contains(PostAction::StripMarkdown));

    const bool hasRx = p.postActions.contains(PostAction::RegexReplace);
    m_chkPostRx->setChecked(hasRx);
    m_edRxFrom->setEnabled(hasRx);
    m_edRxTo->setEnabled(hasRx);
    m_edRxFrom->setText(p.params.value("rx_from"));
    m_edRxTo->setText(p.params.value("rx_to"));

    int transIdx = 0;
    if (p.postActions.contains(PostAction::TranslateLLM_It)) transIdx = 1;
    else if (p.postActions.contains(PostAction::TranslateLLM_En)) transIdx = 2;
    m_cmbTranslate->setCurrentIndex(transIdx);

    /* Modello traduzione */
    const QString tm = p.translateModel.trimmed();
    if (tm.isEmpty()) {
        m_cmbTransModel->setCurrentIndex(0);
        m_edTransModelCustom->clear();
        m_edTransModelCustom->setEnabled(false);
    } else {
        /* Cerca se è uno dei predefiniti (controlla prima parola = nome modello) */
        int found = -1;
        for (int i = 1; i < m_cmbTransModel->count() - 1; ++i) {
            const QString preset = m_cmbTransModel->itemText(i).section('\xe2', 0, 0).trimmed();
            if (tm == preset) { found = i; break; }
        }
        if (found >= 0) {
            m_cmbTransModel->setCurrentIndex(found);
            m_edTransModelCustom->setEnabled(false);
        } else {
            m_cmbTransModel->setCurrentIndex(m_cmbTransModel->count() - 1);
            m_edTransModelCustom->setText(tm);
            m_edTransModelCustom->setEnabled(true);
        }
    }

    m_loading = false;
}

ModelProfile ModelProfilesTab::profileFromForm() const {
    const int row = currentRow();
    const auto& profiles = ModelProcessor::instance().profiles();

    ModelProfile p;
    if (row >= 0 && row < profiles.size())
        p.id = profiles.at(row).id;
    else
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    p.label       = m_edLabel->text().trimmed();
    p.namePattern = m_edPattern->text().trimmed();
    p.enabled     = m_chkEnabled->isChecked();

    /* Pre-processing azioni */
    const int langIdx = m_cmbLang->currentIndex();
    if (langIdx == 1) p.preActions.append(PreAction::ForceLangIt);
    else if (langIdx == 2) p.preActions.append(PreAction::ForceLangEn);
    else if (langIdx == 3) p.preActions.append(PreAction::ForceLangFr);
    else if (langIdx == 4) p.preActions.append(PreAction::ForceLangZh);
    else if (langIdx == 5) p.preActions.append(PreAction::ForceLangDe);

    if (m_chkPrepSys->isChecked()) {
        p.preActions.append(PreAction::PrependSystem);
        p.params["pre_sys"] = m_edPrepSys->toPlainText().trimmed();
    }
    if (m_chkPrepUser->isChecked()) {
        p.preActions.append(PreAction::PrependUser);
        p.params["pre_user"] = m_edPrepUser->text().trimmed();
    }
    if (m_chkReplSys->isChecked()) {
        p.preActions.append(PreAction::ReplaceSystem);
        p.params["repl_sys"] = m_edReplSys->toPlainText().trimmed();
    }

    /* Post-processing azioni */
    if (m_chkPostThink->isChecked()) p.postActions.append(PostAction::RemoveThinkTags);
    if (m_chkPostMd->isChecked())    p.postActions.append(PostAction::StripMarkdown);
    if (m_chkPostRx->isChecked()) {
        p.postActions.append(PostAction::RegexReplace);
        p.params["rx_from"] = m_edRxFrom->text().trimmed();
        p.params["rx_to"]   = m_edRxTo->text().trimmed();
    }
    const int transIdx = m_cmbTranslate->currentIndex();
    if (transIdx == 1) p.postActions.append(PostAction::TranslateLLM_It);
    else if (transIdx == 2) p.postActions.append(PostAction::TranslateLLM_En);

    /* Modello traduzione: estrai nome puro dalla combo (prima della freccia —) */
    const int tmIdx = m_cmbTransModel->currentIndex();
    if (tmIdx == 0) {
        p.translateModel.clear();
    } else if (tmIdx == m_cmbTransModel->count() - 1) {
        p.translateModel = m_edTransModelCustom->text().trimmed();
    } else {
        /* Es. "aya:8b  — Cohere Aya" → prende solo "aya:8b" */
        p.translateModel = m_cmbTransModel->itemText(tmIdx)
                               .split('\xe2').first().trimmed();  // split su '—' UTF-8 \xe2\x80\x94
    }

    return p;
}

void ModelProfilesTab::onSaveClicked() {
    const int row = currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Nessun profilo"), tr("Seleziona un profilo dalla lista."));
        return;
    }
    if (m_edPattern->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Pattern vuoto"), tr("Il pattern del modello non può essere vuoto."));
        return;
    }
    /* Valida regex */
    QRegularExpression re(m_edPattern->text().trimmed(),
                          QRegularExpression::CaseInsensitiveOption);
    if (!re.isValid()) {
        QMessageBox::warning(this, tr("Regex non valida"),
            "Il pattern non è una regex valida:\n" + re.errorString());
        return;
    }

    ModelProfile p = profileFromForm();
    auto list = ModelProcessor::instance().profiles();
    if (row < list.size())
        list[row] = p;
    else
        list.append(p);
    ModelProcessor::instance().setProfiles(list);
    refreshList();
    m_list->setCurrentRow(row);
}
