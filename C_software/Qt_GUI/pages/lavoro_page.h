#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include "../ai_client.h"
#include "lavoro_data.h"

class LavoroPage : public QWidget {
    Q_OBJECT
public:
    explicit LavoroPage(AiClient* ai, QWidget* parent = nullptr);
private:
    AiClient*    m_ai;
    QTextEdit*   m_lavoroLog     = nullptr;
    QLabel*      m_cvStatus      = nullptr;
    QLineEdit*   m_cvPath        = nullptr;
    QString      m_cvText;
    QComboBox*   m_filtroTipo    = nullptr;
    QComboBox*   m_filtroLivello = nullptr;
    QListWidget* m_offerteLista  = nullptr;
    QComboBox*   m_cmbModello    = nullptr;
    QLabel*      m_modelloLbl    = nullptr;
    quint64      m_myReqId      = 0;    ///< ID dell'ultima chat() avviata da questa pagina

    void applicaFiltri();
    void caricaCV(const QString& path);
    void popolaModelli(const QStringList& models);
};
