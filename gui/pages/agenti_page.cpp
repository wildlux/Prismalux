#include "agenti_page.h"
#include "agenti_page_p.h"
#include "agents_config_dialog.h"
#include "../prismalux_paths.h"
#include "../widgets/tts_speak.h"
#include "../widgets/stt_whisper.h"
#include "../widgets/chart_widget.h"
#include "../widgets/formula_parser.h"
#include "../theme_manager.h"
#include <QApplication>
#include <QTime>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <cmath>
#include <cstdlib>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QLabel>
#include <QFrame>
#include <QUrl>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFile>
#include <QTemporaryFile>
#include <QDateTime>
#include <QDir>
#include <QScrollArea>
#include <QScrollBar>
#include <QMessageBox>
#include <algorithm>
#include <QFileDialog>
#include <QTextStream>
#include <QImage>
#include <QBuffer>
#include <QSet>
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QTimer>
#include <QSettings>
#include <QToolTip>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QTextDocument>
namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Costruttore
   ══════════════════════════════════════════════════════════════ */
AgentiPage::AgentiPage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    m_namAuto = new QNetworkAccessManager(this);

    /* Dialog configurazione agenti — non-modal, creato una volta */
    m_cfgDlg = new AgentsConfigDialog(this);

    setupUI();

    connect(m_ai, &AiClient::token,       this, &AgentiPage::onToken);
    connect(m_ai, &AiClient::finished,    this, &AgentiPage::onFinished);
    connect(m_ai, &AiClient::error,       this, &AgentiPage::onError);
    connect(m_ai, &AiClient::modelsReady, this, &AgentiPage::onModelsReady);

    /* Quando il modello cambia da Impostazioni (o altra pagina), aggiorna
       la combo SOLO se l'utente non ha scelto esplicitamente un modello
       per questa scheda (m_pageModel vuoto = nessuna preferenza privata).
       blockSignals previene il loop currentIndexChanged → setBackend. */
    connect(m_ai, &AiClient::modelChanged, this, [this](const QString& newModel) {
        if (!m_cmbLLM || !m_pageModel.isEmpty()) return;
        const int idx = m_cmbLLM->findText(newModel);
        if (idx >= 0 && idx != m_cmbLLM->currentIndex()) {
            m_cmbLLM->blockSignals(true);
            m_cmbLLM->setCurrentIndex(idx);
            m_cmbLLM->blockSignals(false);
        }
    });

    m_ai->fetchModels();
}

