#include "main_programming.h"
#include "main_distillazione.h"
#include "widget_code_interpreter.h"
#include "widget_coding_lab.h"
#include "widget_webdev.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSet>
#include <QTabWidget>
#include <QTextStream>

/* ══════════════════════════════════════════════════════════════
   isIntentionalError — rileva errori volutamente creati dall'utente.

   Regola: se l'output contiene "SyntaxError" E il sorgente contiene
   un'istruzione `raise SyntaxError(...)` → errore custom intenzionale.
   Il loop si ferma: l'utente vuole quel comportamento nel codice.

   Esteso anche ad altri pattern comuni di errori custom intenzionali:
   - raise ValueError / raise TypeError / raise CustomException
     quando la classe è definita nel sorgente stesso.
   ══════════════════════════════════════════════════════════════ */
bool ProgrammazionePage::isIntentionalError(const QString& errorOut,
                                             const QString& source)
{
    /* Pattern 1: SyntaxError nell'output + raise SyntaxError nel sorgente */
    if (errorOut.contains("SyntaxError")) {
        static const QRegularExpression reRaiseSyntax(
            R"(\braise\s+SyntaxError\b)");
        if (reRaiseSyntax.match(source).hasMatch())
            return true;
    }

    /* Pattern 2: qualsiasi "raise XxxError" dove la classe è definita nel sorgente
       (es. class MyError(Exception): pass; raise MyError()) */
    {
        static const QRegularExpression reRaise(
            R"(\braise\s+(\w+)\s*[(\n])");
        static const QRegularExpression reClass(
            R"(\bclass\s+(\w+)\s*\()");

        /* Raccogli le classi definite nel sorgente */
        QSet<QString> definedClasses;
        auto mClass = reClass.globalMatch(source);
        while (mClass.hasNext())
            definedClasses.insert(mClass.next().captured(1));

        /* Controlla se una raise usa una di quelle classi */
        auto mRaise = reRaise.globalMatch(source);
        while (mRaise.hasNext()) {
            const QString raised = mRaise.next().captured(1);
            if (definedClasses.contains(raised) && errorOut.contains(raised))
                return true;
        }
    }

    return false;
}

/* ══════════════════════════════════════════════════════════════
   ProgrammazionePage — costruttore
   ══════════════════════════════════════════════════════════════ */
ProgrammazionePage::~ProgrammazionePage()
{
    if (m_replProc) {
        m_replProc->disconnect();
        m_replProc->kill();
    }
}

void ProgrammazionePage::addExternalTab(QWidget* w, const QString& label)
{
    if (m_innerTabs && w) m_innerTabs->addTab(w, label);
}

ProgrammazionePage::ProgrammazionePage(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(12, 12, 12, 12);
    mainLay->setSpacing(8);

    m_innerTabs = new QTabWidget(this);
    m_innerTabs->setObjectName("innerTabs");
    mainLay->addWidget(m_innerTabs, 1);

    buildInnerTabs();
    m_editor->setPlainText(currentTemplate());
}

/* ── Livello 1: crea tutti i tab interni ── */
void ProgrammazionePage::buildInnerTabs()
{
    /* Titolo nella tab bar */
    auto* titleCorner = new QLabel(
        "\xf0\x9f\x92\xbb  Codifica", m_innerTabs);
    titleCorner->setObjectName("pageTitle");
    titleCorner->setContentsMargins(4, 0, 16, 0);
    m_innerTabs->setCornerWidget(titleCorner, Qt::TopLeftCorner);

    m_innerTabs->addTab(buildCodingTab(m_innerTabs),
        "\xf0\x9f\x92\xbb  Programmazione");
    m_innerTabs->addTab(buildAgentica(m_innerTabs),
        "\xf0\x9f\x8f\x97\xef\xb8\x8f  Architetta Software");
    m_innerTabs->addTab(buildTranslitter(m_innerTabs),
        "\xf0\x9f\x94\x80  Converti Codice");
    m_innerTabs->addTab(buildReverseEngineering(m_innerTabs),
        "\xf0\x9f\x94\x8d  Reverse Eng.");
    m_innerTabs->addTab(buildGitMcp(m_innerTabs),
        "\xf0\x9f\x94\xa7  Git");
    m_innerTabs->addTab(buildPythonRepl(m_innerTabs),
        "\xf0\x9f\x90\x8d  REPL");
    m_innerTabs->addTab(new CodeInterpreterWidget(m_ai, m_innerTabs),
        "\xf0\x9f\xa7\xaa  Interpreter");

    m_innerTabs->addTab(buildDriverKernelTab(m_innerTabs),
        "\xf0\x9f\x94\xa7  Driver & Kernel");
    m_innerTabs->addTab(new CodingLabWidget(m_ai, m_innerTabs),
        "\xf0\x9f\xa7\xaa  Lab Coding");
    m_innerTabs->addTab(new DistillazionePage(m_ai, m_innerTabs),
        "\xf0\x9f\xa7\xac  Distillazione");
    m_innerTabs->addTab(new WebDevWidget(m_ai, m_innerTabs),
        "\xf0\x9f\x8c\x90  Web Dev");
}

bool ProgrammazionePage::hasUnsavedWork() const {
    return m_editor && m_editor->document() && m_editor->document()->isModified();
}

void ProgrammazionePage::saveCurrentFile() {
    if (!m_editor) return;
    const QString path = m_currentFilePath.isEmpty()
        ? QFileDialog::getSaveFileName(this, "Salva codice",
              QDir::homePath() + "/codice.py",
              "Python (*.py);;C++ (*.cpp);;Testo (*.txt);;Tutti (*.*)")
        : m_currentFilePath;
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QTextStream(&f) << m_editor->toPlainText();
    m_editor->document()->setModified(false);
}
