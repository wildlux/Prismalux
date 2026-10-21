#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

class DecisionOnnxPrivate;

/* ══════════════════════════════════════════════════════════════
   DecisionOnnx — backend decisionale locale via ONNX Runtime

   Non è un LLM: due modelli lineari piccolissimi (pochi KB) che
   ricevono feature hashing del testo e restituiscono una scelta.

     decision_classifier.onnx : testo    -> A | B | C | D
     decision_verifier.onnx   : Q + A    -> N | Y

   Le feature sono calcolate qui, non da un tokenizer esterno. Il
   classificatore usa 512 bucket di testo; il verificatore riserva i
   primi 64 bucket a segnali strutturati (tipo domanda, cifre, nomi
   propri, procedure, rifiuti) e usa i restanti 448 per il testo.
   Dopo il modello, `verify()` applica guardie deterministiche per i
   casi ovvi; non è un fact-checker.

   La stessa funzione è riprodotta in `gui/tools/gen_decision_onnx.py`:
   se una delle due cambia, vanno rigenerati i modelli e rieseguito
   `test_decision_onnx` (verifica di parità feature + guardie).

   Senza HAVE_ONNX la classe compila ma load() fallisce con un
   errore esplicito: nessun fallback silenzioso su Ollama.
   ══════════════════════════════════════════════════════════════ */
class DecisionOnnx
{
public:
    static constexpr int kFeatureDim = 512;
    static constexpr int kClassifierLabels = 4;
    static constexpr int kVerifierLabels   = 2;

    struct Result {
        bool    ok = false;
        int     label = 0;       ///< indice: 0..3 classificatore, 0..1 verificatore
        double  confidence = 0.0;
        QString error;
        QString reason;      ///< spiegazione della guardia deterministica, se applicata
    };

    DecisionOnnx();
    ~DecisionOnnx();

    /** Carica entrambi i modelli. Idempotente: se è già ready()
     *  ritorna true senza rileggere i file. */
    bool load();
    bool isReady() const;
    QString lastError() const;

    Result classify(const QString& text) const;
    Result verify(const QString& question, const QString& answer) const;

    /* ── Feature (esposte per i test di parità col generatore) ── */
    static QVector<float> featuresText(const QString& text);
    static QVector<float> featuresPair(const QString& question,
                                       const QString& answer);

    /* ── Percorsi / disponibilità ────────────────────────────── */
    static QString defaultDir();          ///< PRISMALUX_DECISION_ONNX_DIR o Frameworks/onnx/models
    static QString classifierPath();
    static QString verifierPath();
    static bool    modelsPresent();       ///< entrambi i file esistono (non implica che siano validi)
    static bool    runtimeAvailable();    ///< compilato con HAVE_ONNX

    static const QStringList& classifierLabels();  ///< {"A","B","C","D"}
    static const QStringList& verifierLabels();    ///< {"N","Y"}

private:
    std::unique_ptr<DecisionOnnxPrivate> d;
};
