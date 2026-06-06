#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

class OnnxEmbedderPrivate;

/**
 * OnnxEmbedder — embedding locale via ONNX Runtime (all-MiniLM-L6-v2).
 *
 * Usa PIMPL per mantenere gli header ORT fuori dall'interfaccia pubblica.
 * Senza HAVE_ONNX la classe compila ma isReady() restituisce sempre false.
 *
 * Modelli attesi in Frameworks/onnx/models/:
 *   all-MiniLM-L6-v2.onnx   (~22 MB)
 *   vocab.txt                (~226 KB, tokenizer WordPiece BERT)
 *
 * Output: QVector<float> di 384 valori normalizzati L2.
 */
class OnnxEmbedder : public QObject {
    Q_OBJECT
public:
    explicit OnnxEmbedder(QObject* parent = nullptr);
    ~OnnxEmbedder() override;

    /** Carica modello .onnx e vocab.txt. Restituisce true se tutto ok. */
    bool loadModel(const QString& modelPath, const QString& vocabPath);

    /** true se il modello è caricato e pronto per l'inferenza. */
    bool isReady() const;

    /** Dimensione output (384 per all-MiniLM-L6-v2). */
    int dims() const;

    /** Inferenza sincrona — usa solo da thread non-UI (o QThreadPool). */
    QVector<float> embedSync(const QString& text) const;

    /** Inferenza asincrona — emette embeddingReady o embeddingError. */
    void embedAsync(const QString& text);

    /* ── Path default ────────────────────────────────────── */
    static QString defaultModelPath();
    static QString defaultVocabPath();
    static bool    defaultModelExists();

signals:
    void embeddingReady(const QVector<float>& vec);
    void embeddingError(const QString& msg);
    void modelLoaded(int dims);

private:
    std::unique_ptr<OnnxEmbedderPrivate> d;
};
