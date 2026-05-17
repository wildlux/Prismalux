/* impostazioni_page_slots.cpp
 * Implementazioni degli slot nominati di ImpostazioniPage.
 * Generato recuperando i corpi lambda rimossi da impostazioni_page_ai.cpp
 * durante la conversione lambda→slot (commit 65b4bc7).
 */
#include "impostazioni_page.h"
#include "../widgets/toggle_switch.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include "../app_config.h"
#include "../ai_client.h"

#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QDateTime>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QVector>
#include <QTextEdit>
#include <QDoubleSpinBox>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QGuiApplication>
#include <QClipboard>
#include <QTableWidget>
#include <QStackedWidget>
#include <functional>
#include <memory>

/* ══════════════════════════════════════════════════════════════
   buildAiLocaleTab — slot
   ══════════════════════════════════════════════════════════════ */

void ImpostazioniPage::onThinkModeIdClicked(int id)
{
    AiChatParams p = AiChatParams::load();
    p.thinkMode = id;
    AiChatParams::save(p);
    if (m_ai) m_ai->setChatParams(p);
    const bool active = (id != 1);
    if (m_thinkBudgetSlider) m_thinkBudgetSlider->setEnabled(active);
    if (m_thinkBudgetValLbl) m_thinkBudgetValLbl->setEnabled(active);
    if (m_thinkRamWarnLbl) {
        const qint64 ramTot = P::totalRamBytes();
        if (ramTot > 0 && ramTot < 12LL * 1024 * 1024 * 1024)
            m_thinkRamWarnLbl->setVisible(id != 1);
    }
}

void ImpostazioniPage::onThinkBudgetChanged(int v)
{
    if (m_thinkBudgetValLbl)
        m_thinkBudgetValLbl->setText(QString::number(v) + "\xc3\x97");
    AiChatParams p = AiChatParams::load();
    p.thinkBudget = v;
    AiChatParams::save(p);
    if (m_ai) m_ai->setChatParams(p);
}

void ImpostazioniPage::onKnowledgeInjectToggled(bool checked)
{
    AppConfig::s().setValue(P::SK::kInjectUserKnowledge, checked);
    P::invalidateKnowledgeCache();
}

void ImpostazioniPage::onOpenKnowledgeFileClicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(P::userKnowledgePath()));
}

void ImpostazioniPage::populateOllamaModels()
{
    if (!m_aiModelList) return;
    m_aiModelList->clear();
    if (m_aiHintLbl)   m_aiHintLbl->setText("Caricamento modelli Ollama...");
    if (m_aiStatusLbl) m_aiStatusLbl->setText("\xe2\x8f\xb3 Ollama...");
    if (m_aiUseBtn)    m_aiUseBtn->setEnabled(false);

    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, holder,
            [this, holder](const QStringList& list) {
        holder->deleteLater();
        if (!m_aiModelList) return;
        m_aiModelList->clear();
        if (list.isEmpty()) {
            if (m_aiStatusLbl) m_aiStatusLbl->setText("\xe2\x9d\x8c Nessun modello");
            if (m_aiHintLbl)   m_aiHintLbl->setText(
                "Ollama non raggiungibile o nessun modello installato.\n"
                "Avvia Ollama e riprova.");
            m_aiModelList->addItem(new QListWidgetItem("(Ollama non raggiungibile)"));
        } else {
            if (m_aiStatusLbl)
                m_aiStatusLbl->setText(QString("\xe2\x9c\x85 %1 modelli").arg(list.size()));
            if (m_aiHintLbl)
                m_aiHintLbl->setText(
                    QString("%1 modelli Ollama. Doppio clic o 'Usa modello' per attivare.")
                    .arg(list.size()));
            for (const QString& m : list) {
                auto* item = new QListWidgetItem(m);
                item->setData(Qt::UserRole, m);
                if (m_ai && m == m_ai->model())
                    item->setForeground(QColor("#4ade80"));
                m_aiModelList->addItem(item);
            }
        }
    });
    if (m_ai) m_ai->fetchModels();
}

void ImpostazioniPage::populateLlamaModels()
{
    if (!m_aiModelList) return;
    m_aiModelList->clear();
    if (m_aiUseBtn) m_aiUseBtn->setEnabled(false);

    const QStringList files = PrismaluxPaths::scanGgufFiles();
    if (files.isEmpty()) {
        if (m_aiStatusLbl) m_aiStatusLbl->setText("0 modelli");
        if (m_aiHintLbl)   m_aiHintLbl->setText(
            "Nessun file .gguf trovato.\n"
            "Scarica un modello dalla scheda LLM.");
        m_aiModelList->addItem(
            new QListWidgetItem("(Nessun modello GGUF nella cartella models/)"));
    } else {
        if (m_aiStatusLbl)
            m_aiStatusLbl->setText(QString("%1 file GGUF").arg(files.size()));
        if (m_aiHintLbl)
            m_aiHintLbl->setText(
                QString("%1 modelli GGUF. Doppio clic o 'Usa modello' per attivare.")
                .arg(files.size()));
        for (const QString& path : files) {
            QFileInfo fi(path);
            double mb = fi.size() / 1024.0 / 1024.0;
            QString szStr = mb >= 1024.0
                ? QString("%1 GB").arg(mb / 1024.0, 0, 'f', 1)
                : QString("%1 MB").arg(mb, 0, 'f', 0);
            auto* item = new QListWidgetItem(
                QString("%1   \xe2\x80\x94  %2").arg(fi.fileName(), szStr));
            item->setData(Qt::UserRole, path);
            if (m_ai && path == m_ai->model())
                item->setForeground(QColor("#4ade80"));
            m_aiModelList->addItem(item);
        }
    }
}

void ImpostazioniPage::applySelectedModel()
{
    if (!m_aiModelList || !m_ai) return;
    auto* cur = m_aiModelList->currentItem();
    if (!cur) return;
    const QString model = cur->data(Qt::UserRole).toString();
    if (model.isEmpty() || model.startsWith("(")) return;
    const AiClient::Backend bk = (m_btnOllamaRadio && m_btnOllamaRadio->isChecked())
        ? AiClient::Ollama
        : AiClient::LlamaServer;
    m_ai->setBackend(bk, m_ai->host(),
                     bk == AiClient::Ollama
                         ? PrismaluxPaths::kOllamaPort
                         : PrismaluxPaths::kLlamaServerPort,
                     model);
    auto& cfg = AppConfig::s();
    cfg.setValue(PrismaluxPaths::SK::kActiveModel,   model);
    cfg.setValue(PrismaluxPaths::SK::kActiveBackend, static_cast<int>(bk));
    if (m_aiActiveLbl)
        m_aiActiveLbl->setText(
            QString("\xf0\x9f\x9f\xa2  Modello attivo: <b>%1</b>").arg(model));
    if (m_aiStatusLbl)
        m_aiStatusLbl->setText(
            QString("\xe2\x9c\x85 Attivo: %1").arg(QFileInfo(model).fileName()));
}

void ImpostazioniPage::onOllamaRadioToggled(bool checked)
{
    if (!checked) return;
    populateOllamaModels();
}

void ImpostazioniPage::onLlamaRadioToggled(bool checked)
{
    if (!checked) return;
    populateLlamaModels();
}

void ImpostazioniPage::onAiLocalRefreshClicked()
{
    if (m_btnOllamaRadio && m_btnOllamaRadio->isChecked())
        populateOllamaModels();
    else
        populateLlamaModels();
}

void ImpostazioniPage::onAiModelListItemChanged(QListWidgetItem* cur, QListWidgetItem*)
{
    if (!m_aiUseBtn) return;
    if (!cur) { m_aiUseBtn->setEnabled(false); return; }
    const QString m = cur->data(Qt::UserRole).toString();
    m_aiUseBtn->setEnabled(!m.isEmpty() && !m.startsWith("("));
}

void ImpostazioniPage::onAiModelListDblClicked(QListWidgetItem*)
{
    applySelectedModel();
}

void ImpostazioniPage::onAiModelsReadyUpdateLabel(const QStringList&)
{
    if (!m_aiActiveLbl || !m_ai) return;
    m_aiActiveLbl->setText(
        QString("\xf0\x9f\x9f\xa2  Modello attivo: <b>%1</b>").arg(
            m_ai->model().isEmpty() ? "(nessuno)" : m_ai->model()));
}

void ImpostazioniPage::onAiLocalTabInit()
{
    if (!m_ai) return;
    if (m_ai->backend() == AiClient::LlamaServer ||
        m_ai->backend() == AiClient::LlamaLocal)
    {
        if (m_btnLlamaRadio) {
            m_btnLlamaRadio->blockSignals(true);
            m_btnLlamaRadio->setChecked(true);
            m_btnLlamaRadio->blockSignals(false);
        }
        if (m_btnOllamaRadio) m_btnOllamaRadio->setChecked(false);
        populateLlamaModels();
    } else {
        populateOllamaModels();
    }
}

void ImpostazioniPage::onOllamaLanBtnClicked()
{
    if (!m_ollamaLanProc) return;
    if (m_ollamaLanProc->state() != QProcess::NotRunning) {
        m_ollamaLanProc->terminate();
        return;
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("OLLAMA_HOST", "0.0.0.0:11434");
    m_ollamaLanProc->setProcessEnvironment(env);
    if (m_lanBtn) m_lanBtn->setEnabled(false);
    if (m_lanStatusLbl) m_lanStatusLbl->setText("\xe2\x8f\xb3 Avvio in corso...");
    m_ollamaLanProc->start("ollama", {"serve"});
}

void ImpostazioniPage::onOllamaLanStarted()
{
    if (m_lanBtn) {
        m_lanBtn->setEnabled(true);
        m_lanBtn->setText("\xf0\x9f\x94\xb4  Ferma Ollama LAN");
    }
    if (m_lanStatusLbl)
        m_lanStatusLbl->setText("\xe2\x9c\x85 In ascolto su 0.0.0.0:11434");
}

void ImpostazioniPage::onOllamaLanError(QProcess::ProcessError err)
{
    if (m_lanBtn) {
        m_lanBtn->setEnabled(true);
        m_lanBtn->setText("\xf0\x9f\x9f\xa2  Avvia Ollama LAN");
    }
    if (m_lanStatusLbl) {
        if (err == QProcess::FailedToStart)
            m_lanStatusLbl->setText("\xe2\x9d\x8c ollama non trovato nel PATH");
        else
            m_lanStatusLbl->setText("\xe2\x9d\x8c Errore di avvio");
    }
}

void ImpostazioniPage::onOllamaLanFinished(int, QProcess::ExitStatus)
{
    if (m_lanBtn) {
        m_lanBtn->setEnabled(true);
        m_lanBtn->setText("\xf0\x9f\x9f\xa2  Avvia Ollama LAN");
    }
    if (m_lanStatusLbl)
        m_lanStatusLbl->setText("\xe2\x9a\xaa Inattivo");
}

/* ══════════════════════════════════════════════════════════════
   buildRagTab — slot
   ══════════════════════════════════════════════════════════════ */

void ImpostazioniPage::onRagNoSaveToggled(bool on)
{
    m_ragNoSave = on;
    AppConfig::s().setValue(P::SK::kRagNoSave, on);
}

void ImpostazioniPage::onRagJlToggled(bool checked)
{
    AppConfig::s().setValue(P::SK::kRagJlTransform, checked);
}

void ImpostazioniPage::onRagBrowseBtnClicked()
{
    const QString cur = m_ragDirEdit ? m_ragDirEdit->text() : QString();
    QString d = QFileDialog::getExistingDirectory(
        this, "Cartella documenti RAG", cur);
    if (!d.isEmpty() && m_ragDirEdit)
        m_ragDirEdit->setText(d);
}

void ImpostazioniPage::onRagDirChanged(const QString& t)
{
    AppConfig::s().setValue(P::SK::kRagDocsDir, t);
}

void ImpostazioniPage::onRagMaxResultsChanged(int v)
{
    AppConfig::s().setValue(P::SK::kRagMaxResults, v);
}

void ImpostazioniPage::onRagEmbedModelChanged(const QString& v)
{
    AppConfig::s().setValue(P::SK::kRagEmbedModel, v.trimmed());
}

void ImpostazioniPage::refreshRagStatus()
{
    if (!m_ragStatusLbl) return;
    auto& cfg = AppConfig::s();
    int cnt = (m_rag.chunkCount() > 0)
              ? m_rag.chunkCount()
              : cfg.value(P::SK::kRagDocCount, 0).toInt();
    const QString lastIdx = cfg.value(P::SK::kRagLastIndexed, "Mai").toString();
    if (cnt == 0 && lastIdx != "Mai") {
        m_ragStatusLbl->setText(
            QString("\xe2\x9a\xa0  Documenti indicizzati: <b>0</b>"
                    "&nbsp;&nbsp;&nbsp;Ultima indicizzazione: <b>%1</b>"
                    "&nbsp;&nbsp;&mdash;&nbsp;&nbsp;"
                    "<span style='color:#f87171;'>Embedding falliti &mdash; "
                    "installa <code>nomic-embed-text</code> e reindicizza.</span>")
                .arg(lastIdx));
    } else {
        m_ragStatusLbl->setText(
            QString("Documenti indicizzati: <b>%1</b>"
                    "&nbsp;&nbsp;&nbsp;Ultima indicizzazione: <b>%2</b>")
                .arg(cnt)
                .arg(lastIdx));
    }
}

void ImpostazioniPage::onRagDownloadBtnClicked()
{
    const QString ragDir = QDir::homePath() + "/prismalux_rag_docs";
    if (!QDir().mkpath(ragDir)) {
        if (m_ragFeedbackLbl) {
            m_ragFeedbackLbl->setText(
                "\xe2\x9a\xa0  Impossibile creare la cartella: " + ragDir);
            m_ragFeedbackLbl->setVisible(true);
        }
        return;
    }

    using DlItem = QPair<QString,QString>;
    auto items = std::make_shared<QVector<DlItem>>(QVector<DlItem>{
        { "https://www.agenziaentrate.gov.it/portale/documents/20143/9764684/"
          "730_+istruzioni_2026.pdf/2ac8d27a-fa3d-ed9e-ffc1-9bf61457661e",
          "730_istruzioni_2026.pdf" },
        { "https://www.agenziaentrate.gov.it/portale/documents/d/guest/"
          "pf2_2026_istruzioni_bozza-internet",
          "fascicolo2_persone_fisiche_2026.pdf" },
    });

    if (m_ragDownloadBtn) m_ragDownloadBtn->setEnabled(false);
    if (m_ragFeedbackLbl) {
        m_ragFeedbackLbl->setText(
            QString("\xe2\x8f\xb3  Download 1/%1: %2")
            .arg(items->size()).arg((*items)[0].second));
        m_ragFeedbackLbl->setVisible(true);
    }

    auto* nam  = new QNetworkAccessManager(this);
    auto idx   = std::make_shared<int>(0);
    auto errN  = std::make_shared<int>(0);

    auto dlNext = std::make_shared<std::function<void()>>();
    *dlNext = [=, this]() {
        if (*idx >= items->size()) {
            if (*errN == 0) {
                if (m_ragDirEdit) {
                    m_ragDirEdit->setText(ragDir);
                    AppConfig::s().setValue(P::SK::kRagDocsDir, ragDir);
                }
                if (m_ragFeedbackLbl)
                    m_ragFeedbackLbl->setText(
                        "\xe2\x9c\x85  Download completato! "
                        "Cartella: <code>" + ragDir + "</code><br>"
                        "Clicca <b>Reindicizza ora</b> per costruire il RAG.");
            } else {
                if (m_ragFeedbackLbl)
                    m_ragFeedbackLbl->setText(
                        QString("\xe2\x9a\xa0  Download completato con %1 errori. "
                                "Controlla la connessione e riprova.").arg(*errN));
            }
            if (m_ragDownloadBtn) m_ragDownloadBtn->setEnabled(true);
            nam->deleteLater();
            return;
        }

        const QString url   = (*items)[*idx].first;
        const QString fname = (*items)[*idx].second;
        if (m_ragFeedbackLbl)
            m_ragFeedbackLbl->setText(
                QString("\xe2\x8f\xb3  Download %1/%2: %3")
                .arg(*idx + 1).arg(items->size()).arg(fname));

        QNetworkRequest req{QUrl(url)};
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 Prismalux/2.2 (Qt)");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

        auto* reply = nam->get(req);
        connect(reply, &QNetworkReply::finished, reply,
            [=, this]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                QFile f(ragDir + "/" + fname);
                if (f.open(QIODevice::WriteOnly))
                    f.write(reply->readAll());
                else
                    ++(*errN);
            } else {
                ++(*errN);
                if (m_ragFeedbackLbl)
                    m_ragFeedbackLbl->setText(
                        "\xe2\x9a\xa0  Errore: " + reply->errorString()
                        + " \xe2\x80\x94 " + fname);
            }
            ++(*idx);
            (*dlNext)();
        });
    };

    (*dlNext)();
}

void ImpostazioniPage::onStopIndexClicked()
{
    m_indexAborted = true;
    if (m_btnStopIndex) m_btnStopIndex->setEnabled(false);
    if (m_ragFeedbackLbl) {
        m_ragFeedbackLbl->setText(
            "\xe2\x8f\xb3  Interruzione in corso dopo il chunk corrente...");
        m_ragFeedbackLbl->setVisible(true);
    }
}

void ImpostazioniPage::onReindexBtnClicked()
{
    if (!m_ragDirEdit || !m_ai) return;
    const QString dir = m_ragDirEdit->text().trimmed();

    if (dir.isEmpty() || !QDir(dir).exists()) {
        if (m_ragFeedbackLbl) {
            m_ragFeedbackLbl->setText(
                "\xe2\x9a\xa0  Cartella non valida o inesistente. "
                "Specifica un percorso esistente.");
            m_ragFeedbackLbl->setVisible(true);
        }
        return;
    }
    if (m_ai->backend() == AiClient::LlamaLocal) {
        if (m_ragFeedbackLbl) {
            m_ragFeedbackLbl->setText(
                "\xe2\x9a\xa0  Embedding disponibile solo con Ollama o llama-server.");
            m_ragFeedbackLbl->setVisible(true);
        }
        return;
    }
    m_indexAborted = false;

    auto addChunks = [this](const QString& content, const QString& fileName) {
        for (int i = 0; i < content.size(); i += 400) {
            QString chunk = content.mid(i, 500).simplified();
            if (chunk.size() >= 30) {
                m_ragQueue       << chunk;
                m_ragQueueSource << fileName;
            }
        }
    };

    m_ragQueue.clear();
    m_ragQueueSource.clear();
    m_ragQueuePos = 0;
    m_rag.clear();

    QStringList filters{"*.txt","*.md","*.csv","*.rst","*.py","*.cpp","*.h","*.c"};
    QDirIterator it(dir, filters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString fp = it.next();
        QFile f(fp);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        addChunks(QString::fromUtf8(f.readAll()), QFileInfo(fp).fileName());
    }

    const QString pdfToText = QStandardPaths::findExecutable("pdftotext");
    if (!pdfToText.isEmpty()) {
        QDirIterator pdfIt(dir, QStringList{"*.pdf"}, QDir::Files,
                           QDirIterator::Subdirectories);
        while (pdfIt.hasNext()) {
            const QString pdfPath = pdfIt.next();
            QProcess p;
            p.start(pdfToText, {pdfPath, "-"});
            if (!p.waitForFinished(60000)) continue;
            addChunks(QString::fromUtf8(p.readAllStandardOutput()),
                      QFileInfo(pdfPath).fileName());
        }
    }

    if (m_ragQueue.isEmpty()) {
        if (m_ragFeedbackLbl) {
            m_ragFeedbackLbl->setText(
                "\xf0\x9f\x8c\xab  Nessun contenuto trovato nella cartella.");
            m_ragFeedbackLbl->setVisible(true);
        }
        return;
    }

    if (m_ragReindexBtn) m_ragReindexBtn->setEnabled(false);
    if (m_btnStopIndex)  m_btnStopIndex->setEnabled(true);
    if (m_ragFeedbackLbl) {
        m_ragFeedbackLbl->setText(
            QString("\xe2\x8f\xb3  Indicizzazione: 0 / %1 chunk...").arg(m_ragQueue.size()));
        m_ragFeedbackLbl->setVisible(true);
    }
    emit indexingProgress(0, m_ragQueue.size());

    auto errCount  = std::make_shared<int>(0);
    auto indexNext = std::make_shared<std::function<void()>>();
    *indexNext = [this, errCount, dir, indexNext]() {
        const bool done = (m_ragQueuePos >= m_ragQueue.size());
        if (done || m_indexAborted) {
            const QString path = QDir::homePath() + "/.prismalux_rag.json";
            int n = m_rag.chunkCount();
            auto& cfg = AppConfig::s();
            if (n > 0) {
                if (!m_ragNoSave)
                    m_rag.save(path);
                cfg.setValue(P::SK::kRagDocCount, n);
                cfg.setValue(P::SK::kRagLastIndexed,
                    QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm"));
            }
            refreshRagStatus();

            const bool wasAborted = m_indexAborted;
            m_indexAborted = false;

            if (m_ragFeedbackLbl) {
                if (wasAborted) {
                    m_ragFeedbackLbl->setText(
                        QString("\xe2\x8f\xb9  Indicizzazione interrotta &mdash; "
                                "<b>%1</b> chunk salvati su %2 totali.")
                            .arg(n).arg(m_ragQueue.size()));
                } else if (n == 0) {
                    const QString usedModel =
                        cfg.value(P::SK::kRagEmbedModel, "").toString();
                    m_ragFeedbackLbl->setText(
                        QString("\xe2\x9d\x8c  <b>Embedding falliti</b> (%1 errori). "
                                "Il modello <b>%2</b> non supporta <code>/api/embeddings</code>.<br>"
                                "Installa il modello embedding: "
                                "<code>ollama pull nomic-embed-text</code><br>"
                                "Poi verifica il campo <b>Modello embedding</b> qui sopra "
                                "e ripeti l'indicizzazione.")
                            .arg(*errCount)
                            .arg(usedModel.isEmpty() ? "corrente" : usedModel));
                } else {
                    m_ragFeedbackLbl->setText(
                        QString("\xe2\x9c\x85  Indicizzati <b>%1</b> chunk da <code>%2</code>"
                                "%3. %4")
                            .arg(n).arg(dir)
                            .arg(*errCount > 0
                                ? QString(" (%1 chunk saltati per errore)").arg(*errCount)
                                : QString())
                            .arg(m_ragNoSave
                                ? "Indice <b>solo in RAM</b> (non salvato su disco)."
                                : "Indice salvato in <code>~/.prismalux_rag.json</code>."));
                }
            }
            if (m_btnStopIndex)  m_btnStopIndex->setEnabled(false);
            if (m_ragReindexBtn) m_ragReindexBtn->setEnabled(true);
            emit indexingFinished(n, wasAborted);
            return;
        }

        const QString srcName = (m_ragQueuePos < m_ragQueueSource.size())
            ? m_ragQueueSource[m_ragQueuePos] : QString();
        if (m_ragFeedbackLbl)
            m_ragFeedbackLbl->setText(
                QString("\xf0\x9f\x93\x84  chunk %1 / %2%3")
                    .arg(m_ragQueuePos + 1).arg(m_ragQueue.size())
                    .arg(srcName.isEmpty()
                        ? "..."
                        : " da <b>" + srcName + "</b>"));
        emit indexingProgress(m_ragQueuePos, m_ragQueue.size());

        const QString chunk = m_ragQueue[m_ragQueuePos++];

        auto conn    = std::make_shared<QMetaObject::Connection>();
        auto connErr = std::make_shared<QMetaObject::Connection>();

        *conn = connect(m_ai, &AiClient::embeddingReady, this,
            [this, chunk, indexNext, conn, connErr](const QVector<float>& vec) {
                disconnect(*conn);
                disconnect(*connErr);
                m_rag.addChunk(chunk, vec);
                (*indexNext)();
            }, Qt::SingleShotConnection);

        *connErr = connect(m_ai, &AiClient::embeddingError, this,
            [this, indexNext, errCount, conn, connErr](const QString&) {
                disconnect(*connErr);
                disconnect(*conn);
                ++(*errCount);
                (*indexNext)();
            }, Qt::SingleShotConnection);

        m_ai->fetchEmbedding(chunk);
    };

    (*indexNext)();
}

/* ══════════════════════════════════════════════════════════════
   buildAiParamsTab — slot
   ══════════════════════════════════════════════════════════════ */

void ImpostazioniPage::onCtxSpinChanged(int ctx)
{
    /* Aggiorna l'hint KV-cache dinamicamente */
    if (!m_ctxHint) return;
    const double kvGb = ctx * 0.00033;
    QString level, advice;
    if (kvGb < 1.0) {
        level  = "\xe2\x9c\x85";
        advice = "modello 4B quasi interamente su GPU 4 GB";
    } else if (kvGb < 2.0) {
        level  = "\xf0\x9f\x9f\xa1";
        advice = "Misto GPU+CPU consigliato";
    } else if (kvGb < 4.0) {
        level  = "\xf0\x9f\x9f\xa0";
        advice = "Misto obbligatorio, parte significativa su CPU/RAM";
    } else {
        level  = "\xf0\x9f\x94\xb4";
        advice = "GPU 4 GB insufficiente \xe2\x80\x94 quasi tutto su CPU";
    }
    m_ctxHint->setText(QString("%1  KV-cache stimata: ~%2 GB  \xe2\x80\x94  %3")
                       .arg(level).arg(kvGb, 0, 'f', 1).arg(advice));
}

void ImpostazioniPage::onPersonaBtnClicked(QAbstractButton* btn)
{
    if (!btn) return;
    const QString key = btn->property("persona_key").toString();
    AppConfig::s().setValue(P::SK::kAiPersonality, key);
}

void ImpostazioniPage::onMlockToggled(bool on)
{
    AppConfig::s().setValue(P::SK::kMlockModel, on);
}

void ImpostazioniPage::onAiParamsSave()
{
    if (!m_tempSpin || !m_topPSpin || !m_topKSpin || !m_repSpin ||
        !m_predSpin || !m_ctxSpin || !m_honestyCb || !m_cavemanToggle || !m_flashCb)
        return;

    AiChatParams p;
    p.temperature    = m_tempSpin->value();
    p.top_p          = m_topPSpin->value();
    p.top_k          = m_topKSpin->value();
    p.repeat_penalty = m_repSpin->value();
    p.num_predict    = m_predSpin->value();
    p.num_ctx        = m_ctxSpin->value();
    p.honesty_prefix = m_honestyCb->isChecked();
    p.caveman_mode   = m_cavemanToggle->isChecked();
    p.flash_attn     = m_flashCb->isChecked();
    AiChatParams::save(p);
    if (m_ai) m_ai->setChatParams(p);
    if (m_saveStatus) {
        m_saveStatus->setText("\xe2\x9c\x85  Salvato in " + AiChatParams::filePath());
        QTimer::singleShot(3000, this,
                           &ImpostazioniPage::onAiParamsSaveStatusClear);
    }
}

void ImpostazioniPage::onAiParamsSaveStatusClear()
{
    if (m_saveStatus) m_saveStatus->setText("");
}

void ImpostazioniPage::onAiParamsReset()
{
    if (!m_tempSpin || !m_topPSpin || !m_topKSpin || !m_repSpin ||
        !m_predSpin || !m_ctxSpin || !m_honestyCb || !m_cavemanToggle || !m_flashCb)
        return;

    m_tempSpin->setValue(0.05);
    m_topPSpin->setValue(0.85);
    m_topKSpin->setValue(20);
    m_repSpin->setValue(1.20);
    m_predSpin->setValue(2048);
    m_ctxSpin->setValue(8192);
    m_honestyCb->setChecked(true);
    m_cavemanToggle->setChecked(false);
    m_flashCb->setChecked(false);
    onAiParamsSave();
}

void ImpostazioniPage::onAiPreset8GbClicked()
{
    if (!m_ctxSpin || !m_predSpin || !m_tempSpin || !m_flashCb) return;
    m_ctxSpin->setValue(4096);
    m_predSpin->setValue(1024);
    m_tempSpin->setValue(0.05);
    m_flashCb->setChecked(true);
    onAiParamsSave();
}

void ImpostazioniPage::onAiPresetLongClicked()
{
    if (!m_ctxSpin || !m_flashCb) return;
    const qint64 ram = PrismaluxPaths::totalRamBytes();
    const int ctx = (ram > 0 && ram < 16LL * 1024 * 1024 * 1024) ? 8192 : 16384;
    m_ctxSpin->setValue(ctx);
    m_flashCb->setChecked(true);
    onAiParamsSave();
}

void ImpostazioniPage::onCavemanToggled(bool on)
{
    if (m_cavemanBadge) {
        m_cavemanBadge->setText(on ? "  ON " : "  OFF");
        m_cavemanBadge->setObjectName(on ? "badgeActive" : "badgeInactive");
        P::repolish(m_cavemanBadge);
    }
    onAiParamsSave();
}

/* ══════════════════════════════════════════════════════════════
   buildSandboxTab — slot
   ══════════════════════════════════════════════════════════════ */

void ImpostazioniPage::onSandboxEnabledToggled(bool on)
{
    AppConfig::s().setValue(P::SK::kSandboxEnabled, on);
}

void ImpostazioniPage::onSandboxImageEditFinished()
{
    if (!m_sandboxImgEdit) return;
    const QString val = m_sandboxImgEdit->text().trimmed();
    AppConfig::s().setValue(P::SK::kSandboxImage,
        val.isEmpty() ? "python:3.11-slim" : val);
}

void ImpostazioniPage::onSandboxMemSpinChanged(int v)
{
    AppConfig::s().setValue(P::SK::kSandboxMemory, v);
}

void ImpostazioniPage::onSandboxPullBtnClicked()
{
    if (!m_sandboxPullBtn || !m_sandboxPullStatus) return;
    const QString img = AppConfig::s()
        .value(P::SK::kSandboxImage, "python:3.11-slim").toString();
    m_sandboxPullBtn->setEnabled(false);
    m_sandboxPullStatus->setText("\xf0\x9f\x94\x84  Pull in corso...");

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ImpostazioniPage::onSandboxPullProcFinished);
    proc->start(P::findDocker(), {"pull", img});
}

void ImpostazioniPage::onSandboxPullProcFinished(int code, QProcess::ExitStatus)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    const QString img = AppConfig::s()
        .value(P::SK::kSandboxImage, "python:3.11-slim").toString();
    const QString out = QString::fromUtf8(proc->readAll()).trimmed();
    proc->deleteLater();
    if (m_sandboxPullBtn) m_sandboxPullBtn->setEnabled(true);
    if (m_sandboxPullStatus) {
        if (code == 0)
            m_sandboxPullStatus->setText(
                QString("\xe2\x9c\x85  %1 scaricata con successo.").arg(img));
        else
            m_sandboxPullStatus->setText(
                QString("\xe2\x9d\x8c  Errore: %1").arg(out.left(120)));
    }
}

/* ══════════════════════════════════════════════════════════════
   buildTestTab — slot
   (m_testProc, m_testRunOut, m_testBtnBuild, m_testBtnRun,
    m_testBtnStop, m_testBuildDir, m_testQtGuiDir)
   ══════════════════════════════════════════════════════════════ */

void ImpostazioniPage::onTestProcReadyRead()
{
    if (!m_testProc || !m_testRunOut) return;
    m_testRunOut->append(QString::fromUtf8(m_testProc->readAll()).trimmed());
}

void ImpostazioniPage::onTestProcFinished(int code, QProcess::ExitStatus)
{
    if (m_testBtnBuild) m_testBtnBuild->setEnabled(true);
    if (m_testBtnRun)   m_testBtnRun->setEnabled(true);
    if (m_testBtnStop)  m_testBtnStop->setEnabled(false);
    if (m_testRunOut)
        m_testRunOut->append(code == 0
            ? "\xe2\x9c\x85  Completato con successo."
            : "\xe2\x9d\x8c  Terminato con codice " + QString::number(code));
}

void ImpostazioniPage::onTestBuildClicked()
{
    if (!m_testProc || m_testProc->state() != QProcess::NotRunning) return;
    if (m_testBtnBuild) m_testBtnBuild->setEnabled(false);
    if (m_testBtnRun)   m_testBtnRun->setEnabled(false);
    if (m_testBtnStop)  m_testBtnStop->setEnabled(true);
    if (m_testRunOut) {
        m_testRunOut->clear();
        m_testRunOut->append("\xf0\x9f\x94\xa8  Compilazione build_tests...");
    }
    m_testProc->start("/bin/bash", {"-c",
        QString("cmake -B \"%1\" -S \"%2\" -DBUILD_TESTS=ON "
                "&& cmake --build \"%1\" -j$(nproc) 2>&1")
        .arg(m_testBuildDir, m_testQtGuiDir)});
}

void ImpostazioniPage::onTestRunClicked()
{
    if (!m_testProc || m_testProc->state() != QProcess::NotRunning) return;
    if (m_testBtnBuild) m_testBtnBuild->setEnabled(false);
    if (m_testBtnRun)   m_testBtnRun->setEnabled(false);
    if (m_testBtnStop)  m_testBtnStop->setEnabled(true);
    if (m_testRunOut) {
        m_testRunOut->clear();
        m_testRunOut->append("\xf0\x9f\x94\x84  Esecuzione test suite...");
    }
    m_testProc->start("/bin/bash", {"-c",
        QString("ctest --test-dir \"%1\" -j4 --output-on-failure 2>&1")
        .arg(m_testBuildDir)});
}

void ImpostazioniPage::onTestStopClicked()
{
    if (!m_testProc) return;
    m_testProc->kill();
    if (m_testBtnBuild) m_testBtnBuild->setEnabled(true);
    if (m_testBtnRun)   m_testBtnRun->setEnabled(true);
    if (m_testBtnStop)  m_testBtnStop->setEnabled(false);
}

/* ══════════════════════════════════════════════════════════════
   buildLlmConsigliatiTab — slot
   Note: questa tab usa lambda locali che catturano dati statici.
   Gli slot nominati operano sui member ptr salvati nel costruttore
   (m_consigliatiList, m_llmConsDetailLbl, m_llmConsInstallBtn, ecc.)
   ma la logica completa (dataset OLLAMA/GGUF) rimane nelle lambda
   locali. Gli slot qui implementano solo le azioni sui member ptr.
   ══════════════════════════════════════════════════════════════ */

void ImpostazioniPage::onLlmConsModelChanged(QListWidgetItem* cur, QListWidgetItem*)
{
    if (!m_llmConsInstallBtn || !m_llmConsDetailLbl) return;
    if (!cur) {
        m_llmConsInstallBtn->setEnabled(false);
        m_llmConsDetailLbl->setText("Seleziona un modello per i dettagli.");
        return;
    }
    /* Abilita il pulsante; il testo di dettaglio viene aggiornato
       dalla lambda locale nella tab (che ha accesso al dataset statico). */
    m_llmConsInstallBtn->setEnabled(true);
}

void ImpostazioniPage::onLlmConsInstallClicked()
{
    /* La logica reale di installazione resta nella lambda locale della tab
       che ha accesso al dataset OLLAMA/GGUF statico.
       Questo slot è uno stub per soddisfare il linker. */
}

void ImpostazioniPage::onLlmConsOllamaToggled(bool on)
{
    if (!on) return;
    onLlmConsPopulate();
}

void ImpostazioniPage::onLlmConsGgufToggled(bool on)
{
    if (!on) return;
    onLlmConsPopulate();
}

void ImpostazioniPage::onLlmConsCatToggled(bool on)
{
    if (!on) return;
    onLlmConsPopulate();
}

void ImpostazioniPage::onLlmConsPopulate()
{
    /* La logica di popolamento reale è nella lambda locale (accesso al
       dataset statico). Questo slot è uno stub per il linker. */
    if (m_consigliatiList) refreshLlmColors();
}

void ImpostazioniPage::onLlmCustomDlClicked()
{
    /* Stub — la logica download URL personalizzato resta nella lambda
       locale che ha accesso a customEdit, logOut, proc, ecc. */
}

/* ══════════════════════════════════════════════════════════════
   buildLlmClassificaTab — slot
   ══════════════════════════════════════════════════════════════ */

void ImpostazioniPage::onLlmRankPopulate()
{
    /* Stub — la logica di popolamento resta nella lambda locale
       che ha accesso al dataset RANK statico. */
    if (m_rankTable) refreshLlmColors();
}

void ImpostazioniPage::onLlmRankCellChanged(int row, int /*col*/, int, int)
{
    if (!m_rankTable || !m_llmRankInstallBtn || !m_llmRankDetailLbl) return;
    if (row < 0 || row >= m_rankTable->rowCount()) {
        m_llmRankInstallBtn->setEnabled(false);
        return;
    }
    m_llmRankInstallBtn->setEnabled(true);
}

void ImpostazioniPage::onLlmRankInstallClicked()
{
    /* Stub — la logica ollama pull resta nella lambda locale
       che ha accesso al dataset RANK statico. */
}

/* ══════════════════════════════════════════════════════════════
   buildTemaTab (visuale) — slot
   Le lambda in buildTemaTab catturano nm.value/ns.value/em.value
   da loop su array statici locali. Questi slot sono stub perché
   le lambda esistenti soddisfano già la logica. Il linker
   richiede solo la definizione del simbolo.
   ══════════════════════════════════════════════════════════════ */

void ImpostazioniPage::onNavTabModeToggled(bool checked)
{
    if (!checked) return;
    auto* rb = qobject_cast<QAbstractButton*>(sender());
    if (!rb) return;
    const QString val = rb->property("modeValue").toString();
    AppConfig::s().setValue(P::SK::kNavTabMode, val);
    emit tabModeChanged(val);
}

void ImpostazioniPage::onNavStyleToggled(bool checked)
{
    if (!checked) return;
    auto* rb = qobject_cast<QAbstractButton*>(sender());
    if (!rb) return;
    const QString val = rb->property("modeValue").toString();
    AppConfig::s().setValue(P::SK::kNavStyle, val);
    emit navStyleChanged(val);
}

void ImpostazioniPage::onExecBtnModeToggled(bool checked)
{
    if (!checked) return;
    auto* rb = qobject_cast<QAbstractButton*>(sender());
    if (!rb) return;
    const QString val = rb->property("modeValue").toString();
    AppConfig::s().setValue(P::SK::kNavExecBtnMode, val);
    emit execBtnModeChanged(val);
}

/* ══════════════════════════════════════════════════════════════
   buildMcpTab (altro) — slot
   ══════════════════════════════════════════════════════════════ */

void ImpostazioniPage::onMcpOpenFileClicked()
{
    /* Apre ~/.claude/settings.json con il file manager di sistema */
    const QString cfgPath = m_mcpClaudeCfgPath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
          + "/.claude/settings.json"
        : m_mcpClaudeCfgPath;
    QProcess::startDetached("xdg-open", {cfgPath});
}

void ImpostazioniPage::onMcpCopyOllamaCmdClicked()
{
    QGuiApplication::clipboard()->setText("npx -y ollama-mcp-server");
}
