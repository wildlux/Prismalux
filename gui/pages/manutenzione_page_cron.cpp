/* ══════════════════════════════════════════════════════════════
   manutenzione_page_cron.cpp — Sistema Cron integrato Prismalux

   Pianifica prompt AI con cadenza giornaliera, oraria, a
   intervallo fisso o una tantum. I job sono salvati in
   ~/.prismalux/cron_jobs.json e sopravvivono al riavvio.
   ══════════════════════════════════════════════════════════════ */
#include "manutenzione_page.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextBrowser>
#include <QTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QTimeEdit>
#include <QDateTimeEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QDateTime>
#include <QFrame>
#include <QScrollArea>

/* ── Colonne tabella ── */
enum CronCol { CC_En=0, CC_Name, CC_Schedule, CC_Model, CC_LastRun, CC_Next, CC_COUNT };

/* ──────────────────────────────────────────────────────────────
   Helpers schedule
   ────────────────────────────────────────────────────────────── */
bool ManutenzioneePage::cronShouldRun(const QString& schedule, const QString& lastRun) const
{
    const QDateTime now = QDateTime::currentDateTime();

    if (schedule.startsWith("daily@")) {
        const QTime t = QTime::fromString(schedule.mid(6), "HH:mm");
        if (!t.isValid()) return false;
        const QDateTime todayRun(now.date(), t);
        if (now < todayRun) return false;
        if (lastRun.isEmpty()) return true;
        const QDateTime lr = QDateTime::fromString(lastRun, Qt::ISODate);
        return lr < todayRun;
    }
    if (schedule.startsWith("hourly@")) {
        const int mm = schedule.mid(7).toInt();
        if (now.time().minute() < mm) return false;
        const QDateTime thisHour(now.date(), QTime(now.time().hour(), mm));
        if (lastRun.isEmpty()) return true;
        const QDateTime lr = QDateTime::fromString(lastRun, Qt::ISODate);
        return lr < thisHour;
    }
    if (schedule.startsWith("interval@")) {
        const int mins = schedule.mid(9).toInt();
        if (mins <= 0) return false;
        if (lastRun.isEmpty()) return true;
        const QDateTime lr = QDateTime::fromString(lastRun, Qt::ISODate);
        return lr.secsTo(now) >= mins * 60LL;
    }
    if (schedule.startsWith("once@")) {
        const QDateTime t = QDateTime::fromString(schedule.mid(5), Qt::ISODate);
        if (lastRun.isEmpty() && now >= t) return true;
        return false;
    }
    return false;
}

QString ManutenzioneePage::cronNextRun(const QString& schedule) const
{
    const QDateTime now = QDateTime::currentDateTime();
    if (schedule.startsWith("daily@")) {
        const QTime t = QTime::fromString(schedule.mid(6), "HH:mm");
        QDateTime next(now.date(), t);
        if (next <= now) next = next.addDays(1);
        return next.toString("dd/MM  HH:mm");
    }
    if (schedule.startsWith("hourly@")) {
        const int mm = schedule.mid(7).toInt();
        QDateTime next(now.date(), QTime(now.time().hour(), mm));
        if (next <= now) next = next.addSecs(3600);
        return next.toString("HH:mm");
    }
    if (schedule.startsWith("interval@")) {
        const int mins = schedule.mid(9).toInt();
        return QString("ogni %1 min").arg(mins);
    }
    if (schedule.startsWith("once@")) {
        const QDateTime t = QDateTime::fromString(schedule.mid(5), Qt::ISODate);
        return t.toString("dd/MM  HH:mm");
    }
    return "-";
}

/* ── Label leggibile per il tipo di schedule ── */
static QString scheduleLabel(const QString& s)
{
    if (s.startsWith("daily@"))    return "🕐 " + s.mid(6);
    if (s.startsWith("hourly@"))   return "🔁 al min " + s.mid(7);
    if (s.startsWith("interval@")) return "⏱ ogni " + s.mid(9) + " min";
    if (s.startsWith("once@")) {
        const QDateTime t = QDateTime::fromString(s.mid(5), Qt::ISODate);
        return "1️⃣  " + t.toString("dd/MM HH:mm");
    }
    return s;
}

/* ──────────────────────────────────────────────────────────────
   Persistenza JSON
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::cronLoadJobs()
{
    const QString path = P::cronFile();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    m_cronJobs.clear();
    for (const QJsonValue& v : doc.object()["jobs"].toArray()) {
        const QJsonObject o = v.toObject();
        CronJob j;
        j.id         = o["id"].toString();
        j.name       = o["name"].toString();
        j.prompt     = o["prompt"].toString();
        j.model      = o["model"].toString();
        j.schedule   = o["schedule"].toString();
        j.enabled    = o["enabled"].toBool(true);
        j.lastRun    = o["last_run"].toString();
        j.lastResult = o["last_result"].toString();
        if (!j.id.isEmpty() && !j.name.isEmpty()) m_cronJobs.append(j);
    }
}

void ManutenzioneePage::cronSaveJobs()
{
    const QString path = P::cronFile();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray arr;
    for (const CronJob& j : m_cronJobs) {
        QJsonObject o;
        o["id"]          = j.id;
        o["name"]        = j.name;
        o["prompt"]      = j.prompt;
        o["model"]       = j.model;
        o["schedule"]    = j.schedule;
        o["enabled"]     = j.enabled;
        o["last_run"]    = j.lastRun;
        o["last_result"] = j.lastResult;
        arr.append(o);
    }
    QJsonObject root;
    root["jobs"] = arr;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson());
}

/* ──────────────────────────────────────────────────────────────
   Refresh tabella
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::cronRefreshTable()
{
    if (!m_cronTable) return;
    m_cronTable->setRowCount(0);
    for (int i = 0; i < m_cronJobs.size(); ++i) {
        const CronJob& j = m_cronJobs[i];
        m_cronTable->insertRow(i);

        auto* enItem = new QTableWidgetItem;
        enItem->setCheckState(j.enabled ? Qt::Checked : Qt::Unchecked);
        enItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        m_cronTable->setItem(i, CC_En, enItem);

        m_cronTable->setItem(i, CC_Name,     new QTableWidgetItem(j.name));
        m_cronTable->setItem(i, CC_Schedule, new QTableWidgetItem(scheduleLabel(j.schedule)));
        m_cronTable->setItem(i, CC_Model,    new QTableWidgetItem(j.model.isEmpty() ? "(corrente)" : j.model));

        const QString lr = j.lastRun.isEmpty() ? "mai" :
            QDateTime::fromString(j.lastRun, Qt::ISODate).toString("dd/MM  HH:mm");
        m_cronTable->setItem(i, CC_LastRun, new QTableWidgetItem(lr));
        m_cronTable->setItem(i, CC_Next,    new QTableWidgetItem(
            j.enabled ? cronNextRun(j.schedule) : "-"));

        for (int c = CC_Name; c < CC_COUNT; ++c) {
            if (auto* it = m_cronTable->item(i, c))
                it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        }
    }
}

/* ──────────────────────────────────────────────────────────────
   Esegui singolo job
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::cronRunJob(int idx)
{
    if (idx < 0 || idx >= m_cronJobs.size()) return;
    if (m_ai->busy()) {
        if (m_cronLog) m_cronLog->append(
            QString("<span style='color:#f0c060'>[%1] ⏳ AI occupata — job <b>%2</b> saltato.</span>")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
            .arg(m_cronJobs[idx].name.toHtmlEscaped()));
        return;
    }

    CronJob& j = m_cronJobs[idx];
    j.lastRun  = QDateTime::currentDateTime().toString(Qt::ISODate);
    cronSaveJobs();
    cronRefreshTable();

    const QString jobName  = j.name;
    const QString jobModel = j.model.isEmpty() ? m_ai->model() : j.model;
    const QString prevModel = m_ai->model();
    const QString prompt   = j.prompt;

    if (m_cronLog) m_cronLog->append(
        QString("<span style='color:#6ac'>▶ [%1] Job <b>%2</b> avviato con <i>%3</i>…</span>")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
        .arg(jobName.toHtmlEscaped())
        .arg(jobModel.toHtmlEscaped()));

    if (jobModel != prevModel)
        m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), jobModel);

    auto* h = new QObject(this);
    connect(m_ai, &AiClient::finished, h, [this, h, idx, jobName, prevModel, jobModel](const QString& resp){
        h->deleteLater();
        if (jobModel != prevModel)
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), prevModel);
        if (idx < m_cronJobs.size()) {
            m_cronJobs[idx].lastResult = resp.left(500);
            cronSaveJobs();
        }
        if (m_cronLog) m_cronLog->append(
            QString("<span style='color:#8c8'>[%1] ✅ <b>%2</b> completato. Risposta: %3</span>")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
            .arg(jobName.toHtmlEscaped())
            .arg(resp.left(200).toHtmlEscaped()));
    });
    connect(m_ai, &AiClient::error, h, [this, h, jobName, prevModel, jobModel](const QString& err){
        h->deleteLater();
        if (jobModel != prevModel)
            m_ai->setBackend(m_ai->backend(), m_ai->host(), m_ai->port(), prevModel);
        if (m_cronLog) m_cronLog->append(
            QString("<span style='color:#f66'>[%1] ❌ <b>%2</b> errore: %3</span>")
            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
            .arg(jobName.toHtmlEscaped())
            .arg(err.toHtmlEscaped()));
    });

    m_ai->chat("Sei un assistente Prismalux. Rispondi in modo conciso.", prompt);
}

/* ──────────────────────────────────────────────────────────────
   Tick del timer (ogni 60s)
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::cronTick()
{
    if (m_cronPaused) return;
    for (int i = 0; i < m_cronJobs.size(); ++i) {
        const CronJob& j = m_cronJobs[i];
        if (!j.enabled) continue;
        if (cronShouldRun(j.schedule, j.lastRun))
            cronRunJob(i);
    }
}

/* ──────────────────────────────────────────────────────────────
   Dialog aggiunta / modifica job
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::cronAddOrEdit(int idx)
{
    const bool isEdit = (idx >= 0 && idx < m_cronJobs.size());
    const CronJob& src = isEdit ? m_cronJobs[idx] : CronJob{};

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(isEdit ? "Modifica job" : "Nuovo job");
    dlg->setMinimumWidth(420);

    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(10);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    auto* edName = new QLineEdit(src.name, dlg);
    edName->setPlaceholderText("Es. Report mattutino");
    form->addRow("Nome:", edName);

    auto* edPrompt = new QTextEdit(dlg);
    edPrompt->setPlainText(src.prompt);
    edPrompt->setPlaceholderText("Prompt inviato all'AI…");
    edPrompt->setFixedHeight(90);
    form->addRow("Prompt:", edPrompt);

    auto* cmbModel = new QComboBox(dlg);
    cmbModel->setEditable(true);
    cmbModel->addItem("(modello corrente)", "");
    for (const QString& m : m_ai->models()) cmbModel->addItem(m, m);
    if (!src.model.isEmpty()) {
        const int fi = cmbModel->findData(src.model);
        if (fi >= 0) cmbModel->setCurrentIndex(fi);
        else { cmbModel->addItem(src.model, src.model); cmbModel->setCurrentIndex(cmbModel->count()-1); }
    }
    form->addRow("Modello:", cmbModel);

    lay->addLayout(form);

    /* ── Tipo schedule ── */
    auto* schedGrp = new QGroupBox("Pianificazione", dlg);
    auto* schedLay = new QVBoxLayout(schedGrp);
    schedLay->setSpacing(6);

    auto* bg = new QButtonGroup(dlg);
    auto* rdDaily    = new QRadioButton("Giornaliero — alle ore:", schedGrp);
    auto* rdHourly   = new QRadioButton("Ogni ora — al minuto:", schedGrp);
    auto* rdInterval = new QRadioButton("Ogni N minuti:", schedGrp);
    auto* rdOnce     = new QRadioButton("Una volta — il:", schedGrp);
    bg->addButton(rdDaily,    0);
    bg->addButton(rdHourly,   1);
    bg->addButton(rdInterval, 2);
    bg->addButton(rdOnce,     3);

    auto* teDaily    = new QTimeEdit(QTime(9,0), schedGrp);
    auto* spHourly   = new QSpinBox(schedGrp);
    spHourly->setRange(0, 59); spHourly->setSuffix(" min");
    auto* spInterval = new QSpinBox(schedGrp);
    spInterval->setRange(1, 1440); spInterval->setValue(30); spInterval->setSuffix(" min");
    auto* dtOnce     = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600), schedGrp);
    dtOnce->setDisplayFormat("dd/MM/yyyy  HH:mm");
    dtOnce->setCalendarPopup(true);

    auto addSchedRow = [&](QRadioButton* rb, QWidget* ctrl) {
        auto* row = new QHBoxLayout;
        row->addWidget(rb);
        row->addWidget(ctrl);
        schedLay->addLayout(row);
    };
    addSchedRow(rdDaily,    teDaily);
    addSchedRow(rdHourly,   spHourly);
    addSchedRow(rdInterval, spInterval);
    addSchedRow(rdOnce,     dtOnce);

    /* Pre-carica valori esistenti */
    auto enableSchedWidgets = [=](){
        teDaily->setEnabled(rdDaily->isChecked());
        spHourly->setEnabled(rdHourly->isChecked());
        spInterval->setEnabled(rdInterval->isChecked());
        dtOnce->setEnabled(rdOnce->isChecked());
    };
    connect(bg, &QButtonGroup::idClicked, dlg, [enableSchedWidgets](int){ enableSchedWidgets(); });

    if (src.schedule.startsWith("daily@")) {
        rdDaily->setChecked(true);
        teDaily->setTime(QTime::fromString(src.schedule.mid(6), "HH:mm"));
    } else if (src.schedule.startsWith("hourly@")) {
        rdHourly->setChecked(true);
        spHourly->setValue(src.schedule.mid(7).toInt());
    } else if (src.schedule.startsWith("interval@")) {
        rdInterval->setChecked(true);
        spInterval->setValue(src.schedule.mid(9).toInt());
    } else if (src.schedule.startsWith("once@")) {
        rdOnce->setChecked(true);
        dtOnce->setDateTime(QDateTime::fromString(src.schedule.mid(5), Qt::ISODate));
    } else {
        rdDaily->setChecked(true);
    }
    enableSchedWidgets();

    lay->addWidget(schedGrp);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    bb->button(QDialogButtonBox::Ok)->setText(isEdit ? "Salva" : "Aggiungi");
    lay->addWidget(bb);

    connect(bb, &QDialogButtonBox::accepted, dlg, [=](){
        if (edName->text().trimmed().isEmpty() || edPrompt->toPlainText().trimmed().isEmpty()) return;

        CronJob j;
        j.id      = isEdit ? src.id : QUuid::createUuid().toString(QUuid::WithoutBraces);
        j.name    = edName->text().trimmed();
        j.prompt  = edPrompt->toPlainText().trimmed();
        j.model   = cmbModel->currentData().toString();
        j.enabled = isEdit ? src.enabled : true;
        j.lastRun = isEdit ? src.lastRun : QString();
        j.lastResult = isEdit ? src.lastResult : QString();

        if (rdDaily->isChecked())
            j.schedule = "daily@" + teDaily->time().toString("HH:mm");
        else if (rdHourly->isChecked())
            j.schedule = "hourly@" + QString::number(spHourly->value());
        else if (rdInterval->isChecked())
            j.schedule = "interval@" + QString::number(spInterval->value());
        else
            j.schedule = "once@" + dtOnce->dateTime().toString(Qt::ISODate);

        if (isEdit) m_cronJobs[idx] = j;
        else        m_cronJobs.append(j);

        cronSaveJobs();
        cronRefreshTable();
        dlg->accept();
    });
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    dlg->exec();
    dlg->deleteLater();
}

/* ──────────────────────────────────────────────────────────────
   buildCronTab — widget completo per il tab ⏰ Cron
   ────────────────────────────────────────────────────────────── */
QWidget* ManutenzioneePage::buildCronTab()
{
    cronLoadJobs();

    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(14, 12, 14, 10);
    lay->setSpacing(8);

    /* Descrizione */
    auto* desc = new QLabel(
        "\xe2\x8f\xb0  <b>Cron Prismalux</b> \xe2\x80\x94 "
        "Pianifica prompt AI che vengono eseguiti automaticamente "
        "mentre Prismalux \xc3\xa8 aperto. I risultati vengono mostrati nel log.", w);
    desc->setObjectName("hintLabel");
    desc->setWordWrap(true);
    desc->setTextFormat(Qt::RichText);
    lay->addWidget(desc);

    /* Toolbar */
    auto* tb = new QWidget(w);
    auto* tbLay = new QHBoxLayout(tb);
    tbLay->setContentsMargins(0, 0, 0, 0);
    tbLay->setSpacing(6);

    auto* btnAdd  = new QPushButton("+ Aggiungi", tb);
    auto* btnEdit = new QPushButton("\xe2\x9c\x8f  Modifica", tb);
    auto* btnDel  = new QPushButton("\xf0\x9f\x97\x91  Rimuovi", tb);
    auto* btnRun  = new QPushButton("\xe2\x96\xb6  Esegui ora", tb);
    btnAdd->setObjectName("actionBtn");
    btnEdit->setObjectName("actionBtn");
    btnDel->setObjectName("actionBtn");
    btnRun->setObjectName("actionBtn");

    m_btnCronPause = new QPushButton("\xe2\x8f\xb8  Pausa tutto", tb);
    m_btnCronPause->setObjectName("actionBtn");
    m_btnCronPause->setCheckable(true);

    tbLay->addWidget(btnAdd);
    tbLay->addWidget(btnEdit);
    tbLay->addWidget(btnDel);
    tbLay->addWidget(btnRun);
    tbLay->addStretch(1);
    tbLay->addWidget(m_btnCronPause);
    lay->addWidget(tb);

    /* Tabella */
    m_cronTable = new QTableWidget(0, CC_COUNT, w);
    m_cronTable->setHorizontalHeaderLabels({"On", "Nome", "Schedule", "Modello", "Ultima esec.", "Prossima"});
    m_cronTable->horizontalHeader()->setSectionResizeMode(CC_Name, QHeaderView::Stretch);
    m_cronTable->horizontalHeader()->setSectionResizeMode(CC_Schedule, QHeaderView::ResizeToContents);
    m_cronTable->horizontalHeader()->setSectionResizeMode(CC_Model,    QHeaderView::ResizeToContents);
    m_cronTable->horizontalHeader()->setSectionResizeMode(CC_LastRun,  QHeaderView::ResizeToContents);
    m_cronTable->horizontalHeader()->setSectionResizeMode(CC_Next,     QHeaderView::ResizeToContents);
    m_cronTable->setColumnWidth(CC_En, 32);
    m_cronTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cronTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_cronTable->setAlternatingRowColors(true);
    m_cronTable->verticalHeader()->setVisible(false);
    lay->addWidget(m_cronTable, 1);

    /* Log */
    auto* logTitle = new QLabel("\xf0\x9f\x93\x9c  Log esecuzioni:", w);
    logTitle->setObjectName("formLabel");
    lay->addWidget(logTitle);

    m_cronLog = new QTextBrowser(w);
    m_cronLog->setFixedHeight(120);
    m_cronLog->setOpenLinks(false);
    lay->addWidget(m_cronLog);

    cronRefreshTable();

    /* ── Toggle abilitato dal checkbox ── */
    connect(m_cronTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item){
        if (item->column() != CC_En) return;
        const int row = item->row();
        if (row < 0 || row >= m_cronJobs.size()) return;
        m_cronJobs[row].enabled = (item->checkState() == Qt::Checked);
        cronSaveJobs();
        if (auto* nxt = m_cronTable->item(row, CC_Next))
            nxt->setText(m_cronJobs[row].enabled ? cronNextRun(m_cronJobs[row].schedule) : "-");
    });

    /* ── Doppio click = modifica ── */
    connect(m_cronTable, &QTableWidget::doubleClicked, this, [this](const QModelIndex& idx){
        cronAddOrEdit(idx.row());
    });

    /* ── Pulsanti toolbar ── */
    connect(btnAdd,  &QPushButton::clicked, this, [this](){ cronAddOrEdit(-1); });
    connect(btnEdit, &QPushButton::clicked, this, [this](){
        const int row = m_cronTable->currentRow();
        if (row >= 0) cronAddOrEdit(row);
    });
    connect(btnDel, &QPushButton::clicked, this, [this](){
        const int row = m_cronTable->currentRow();
        if (row < 0 || row >= m_cronJobs.size()) return;
        m_cronJobs.removeAt(row);
        cronSaveJobs();
        cronRefreshTable();
    });
    connect(btnRun, &QPushButton::clicked, this, [this](){
        const int row = m_cronTable->currentRow();
        if (row >= 0) cronRunJob(row);
    });
    connect(m_btnCronPause, &QPushButton::toggled, this, [this](bool on){
        m_cronPaused = on;
        m_btnCronPause->setText(on ? "\xe2\x96\xb6  Riprendi tutto" : "\xe2\x8f\xb8  Pausa tutto");
        if (m_cronLog) m_cronLog->append(
            on ? "<span style='color:#f0c060'>⏸ Cron in pausa.</span>"
               : "<span style='color:#8c8'>▶ Cron ripreso.</span>");
    });

    /* ── Timer principale (ogni 60s) ── */
    m_cronTimer = new QTimer(this);
    m_cronTimer->setInterval(60'000);
    connect(m_cronTimer, &QTimer::timeout, this, &ManutenzioneePage::cronTick);
    m_cronTimer->start();

    /* Tick immediato al primo avvio (controlla job scaduti) */
    QTimer::singleShot(3000, this, &ManutenzioneePage::cronTick);

    return w;
}
