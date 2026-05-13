#pragma once
/*
 * ai_error_widget.h — Pannello errore AI riutilizzabile
 * ======================================================
 * QFrame che mostra messaggi di errore da AiClient::error() con
 * suggerimenti contestuali e bottoni di azione rapida.
 *
 * Uso:
 *   m_aiErr = new AiErrorWidget(this);
 *   lay->addWidget(m_aiErr);   // inizialmente hidden
 *
 *   // nel gestore AiClient::error():
 *   connect(m_ai, &AiClient::error, m_aiErr,
 *       [this](const QString& msg){ m_aiErr->showError(msg, [this]{ runAi(); }); });
 *
 * Bottoni generati:
 *   "🔄 Riprova"       → chiama il callback onRetry (se fornito) + si nasconde
 *   "⚙ Impostazioni"  → apre ImpostazioniPage tramite MainWindow::openSettingsDialog()
 *   "✕"               → si nasconde
 */

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <functional>

class AiErrorWidget : public QFrame {
    Q_OBJECT
public:
    explicit AiErrorWidget(QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName("aiErrorWidget");
        setStyleSheet(
            "QFrame#aiErrorWidget {"
            "  background:#fef2f2;"
            "  border:1px solid #fca5a5;"
            "  border-radius:6px;"
            "}"
        );
        setVisible(false);

        auto* vlay = new QVBoxLayout(this);
        vlay->setContentsMargins(10, 8, 10, 8);
        vlay->setSpacing(4);

        /* ── riga icona + testo errore ── */
        auto* errRow = new QHBoxLayout;
        errRow->setSpacing(6);

        auto* iconLbl = new QLabel("\xe2\x9d\x8c", this);   // ❌
        iconLbl->setFixedWidth(20);
        errRow->addWidget(iconLbl);

        m_errLbl = new QLabel(this);
        m_errLbl->setWordWrap(true);
        m_errLbl->setStyleSheet("color:#b91c1c; font-weight:600;");
        errRow->addWidget(m_errLbl, 1);

        auto* closeBtn = new QPushButton("\xe2\x9c\x95", this);   // ✕
        closeBtn->setFixedSize(20, 20);
        closeBtn->setFlat(true);
        closeBtn->setToolTip("Chiudi");
        closeBtn->setStyleSheet("QPushButton{ color:#6b7280; border:none; font-weight:bold; }"
                                "QPushButton:hover{ color:#111827; }");
        connect(closeBtn, &QPushButton::clicked, this, &QWidget::hide);
        errRow->addWidget(closeBtn);

        vlay->addLayout(errRow);

        /* ── suggerimento (riga sotto, opzionale) ── */
        m_hintLbl = new QLabel(this);
        m_hintLbl->setWordWrap(true);
        m_hintLbl->setStyleSheet("color:#7c3aed; font-size:12px;");
        m_hintLbl->hide();
        vlay->addWidget(m_hintLbl);

        /* ── bottoni azione ── */
        auto* btnRow = new QHBoxLayout;
        btnRow->setContentsMargins(0, 4, 0, 0);
        btnRow->setSpacing(8);

        m_retryBtn = new QPushButton("\xf0\x9f\x94\x84  Riprova", this);   // 🔄
        m_retryBtn->setFixedHeight(26);
        m_retryBtn->hide();
        connect(m_retryBtn, &QPushButton::clicked, this, [this]{
            hide();
            if (m_retryFn) m_retryFn();
            emit retryRequested();
        });
        btnRow->addWidget(m_retryBtn);

        auto* settBtn = new QPushButton("\xe2\x9a\x99  Impostazioni AI", this);   // ⚙
        settBtn->setFixedHeight(26);
        settBtn->setToolTip("Apri Impostazioni → AI Locale → Parametri AI");
        connect(settBtn, &QPushButton::clicked, this, [this]{
            /* Trova la MainWindow risalendo la catena dei parent */
            QWidget* win = window();
            if (win) QMetaObject::invokeMethod(win, "openSettingsDialog");
        });
        btnRow->addWidget(settBtn);

        btnRow->addStretch();
        vlay->addLayout(btnRow);
    }

    /* Mostra il pannello con il messaggio di errore.
     * onRetry: callback per il bottone "🔄 Riprova" (nullptr = bottone nascosto) */
    void showError(const QString& msg, std::function<void()> onRetry = nullptr)
    {
        m_retryFn = onRetry;

        /* Separa testo principale dal suggerimento sulla seconda riga */
        const int nl = msg.indexOf('\n');
        if (nl > 0) {
            m_errLbl->setText(msg.left(nl).trimmed());
            const QString hint = msg.mid(nl + 1).trimmed();
            if (!hint.isEmpty()) {
                m_hintLbl->setText(hint);
                m_hintLbl->show();
            } else {
                m_hintLbl->hide();
            }
        } else {
            m_errLbl->setText(msg);
            m_hintLbl->hide();
        }

        m_retryBtn->setVisible(onRetry != nullptr);
        show();
    }

signals:
    void retryRequested();

private:
    QLabel*      m_errLbl   = nullptr;
    QLabel*      m_hintLbl  = nullptr;
    QPushButton* m_retryBtn = nullptr;
    std::function<void()> m_retryFn;
};
