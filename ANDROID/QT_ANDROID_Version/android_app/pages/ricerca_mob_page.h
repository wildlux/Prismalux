#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#ifdef HAVE_POSITIONING
#  include <QGeoPositionInfoSource>
#  include <QGeoPositionInfo>
#  include <QPermission>
#endif
#include "../ai_client.h"

/* Ricerca + Grafo RAG semplificato (text-only, no Graphviz) per Android. */
class RicercaMobPage : public QWidget {
    Q_OBJECT
public:
    explicit RicercaMobPage(AiClient* ai, QWidget* parent = nullptr);

private slots:
    void onSearchClicked();
    void onRagQueryClicked();
    void onToken(const QString& t);
    void onFinished(const QString& full);
    void onAiError(const QString& msg);
    void onAborted();
    void setBusy(bool busy);
    void onModeChanged(int index);
#ifdef HAVE_POSITIONING
    /* C5 — GPS Carta Astrale */
    void onGpsClicked();
    void onPositionUpdated(const QGeoPositionInfo& info);
    void onGpsError(QGeoPositionInfoSource::Error err);
    void onGpsPermissionResult(const QPermission& perm);
#endif

private:
    AiClient*    m_ai        = nullptr;
    QComboBox*   m_modeCmb   = nullptr;
    QLineEdit*   m_queryEdit = nullptr;
    QComboBox*   m_srcCmb    = nullptr;
    QPushButton* m_searchBtn = nullptr;
    QPushButton* m_stopBtn   = nullptr;
    QTextEdit*   m_output    = nullptr;
    QLabel*      m_status    = nullptr;

    /* C5 — campi GPS per Carta Astrale (sempre presenti; GPS opzionale) */
    QWidget*     m_gpsRow    = nullptr;
    QLineEdit*   m_latEdit   = nullptr;
    QLineEdit*   m_lonEdit   = nullptr;
    QPushButton* m_gpsBtn    = nullptr;
#ifdef HAVE_POSITIONING
    QGeoPositionInfoSource* m_geoSrc = nullptr;
#endif
};
