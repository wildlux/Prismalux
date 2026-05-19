#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QSettings>
#include <QVector>
#include <QPushButton>

/* --------------------------------------------------------------
   McpAddonsPage — gestione add-on MCP (abilita/disabilita).

   Mostra una lista di server MCP noti con toggle on/off.
   Le preferenze sono salvate in QSettings.
   -------------------------------------------------------------- */
class McpAddonsPage : public QWidget {
    Q_OBJECT
public:
    explicit McpAddonsPage(QWidget* parent = nullptr);

private slots:
    void onToggled();
    void onAddCustom();

private:
    struct McpEntry {
        QString name;
        QString desc;
        QString url;
        QCheckBox* chk = nullptr;
    };
    QVector<McpEntry> m_entries;
    QVBoxLayout*      m_dynLayout = nullptr;
};
