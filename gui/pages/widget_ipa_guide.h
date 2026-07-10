#pragma once
#include <QWidget>
#include <QUrl>
#include <QStringList>
#include <QProcess>

class QTextBrowser;
class QPushButton;
class QHBoxLayout;
class QVBoxLayout;

/* ════════════════════════════════════════════════════════════════════════════
   IpaGuideWidget — Guida IPA (International Phonetic Alphabet)
   Tutorial "Parlare e scrivere in Inglese ed altre lingue" — tabella fonemica
   interattiva (ogni suono testabile con un click, sintesi via espeak-ng
   mnemonic "[[...]]") + le 8 fasi di apprendimento di una lingua straniera
   + barra "Componi una parola" (accoda i fonemi cliccati come tessere,
   riproducibili in sequenza con un pulsante play/pausa).
   ════════════════════════════════════════════════════════════════════════════ */
class IpaGuideWidget : public QWidget {
    Q_OBJECT
public:
    explicit IpaGuideWidget(QWidget* parent = nullptr);
    ~IpaGuideWidget() override;

private slots:
    void onAnchorClicked(const QUrl& url);
    void onPhonemeTileClicked();
    void onPlayPauseComposeClicked();
    void onClearComposeClicked();
    void onComposePlaybackFinished(int exitCode, QProcess::ExitStatus status);

private:
    QTextBrowser* m_view = nullptr;
    QWidget* m_tileContainer = nullptr;
    QHBoxLayout* m_tileLayout = nullptr;
    QPushButton* m_btnPlayCompose = nullptr;
    QPushButton* m_btnClearCompose = nullptr;

    QStringList m_composedSymbols;    /* tessere mostrate, es. "iː" */
    QStringList m_composedMnemonics;  /* mnemonic espeak-ng per la riproduzione */
    QProcess* m_playProc = nullptr;

    void buildUi();
    void buildComposeBar(QVBoxLayout* lay);
    void updateContent();
    void addComposedPhoneme(const QString& symbol, const QString& mnemonic);
    void rebuildTileRow();
    void startComposePlayback();
    void stopComposePlayback();
};
