#include "onnx_embedder.h"
#include "../prismalux_paths.h"

#include <QFile>
#include <QTextStream>
#include <QHash>
#include <QStringList>
#include <QRunnable>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>

namespace P = PrismaluxPaths;

#ifdef HAVE_ONNX
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>
#endif

/* ══════════════════════════════════════════════════════════════
   WordPieceTokenizer — tokenizer BERT minimal per all-MiniLM-L6-v2.
   Carica vocab.txt (una riga = un token, riga N = id N).
   ══════════════════════════════════════════════════════════════ */
struct WordPieceTokenizer {
    static constexpr int kCLS = 101;
    static constexpr int kSEP = 102;
    static constexpr int kPAD = 0;
    static constexpr int kUNK = 100;
    static constexpr int kMaxSeq = 128;

    QHash<QString, int> vocab;
    bool loaded = false;

    bool load(const QString& vocabPath) {
        QFile f(vocabPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        QTextStream ts(&f);
        int id = 0;
        while (!ts.atEnd()) {
            const QString tok = ts.readLine().trimmed();
            if (!tok.isEmpty())
                vocab.insert(tok, id);
            ++id;
        }
        loaded = !vocab.isEmpty();
        return loaded;
    }

    /* Normalizza: lowercase, strip accenti (solo ASCII safe — sufficiente per EN) */
    static QString normalize(const QString& s) {
        return s.toLower().trimmed();
    }

    /* Tokenizza una parola con greedy WordPiece longest-match.
       prefix = "" per la prima sotto-parola, "##" per le successive. */
    QVector<int> tokenizeWord(const QString& word, bool first = true) const {
        QVector<int> ids;
        QString remain = word;
        bool isFirst = first;
        while (!remain.isEmpty()) {
            bool found = false;
            for (int len = remain.length(); len > 0; --len) {
                const QString candidate = (isFirst ? "" : "##") + remain.left(len);
                auto it = vocab.find(candidate);
                if (it != vocab.end()) {
                    ids.append(it.value());
                    remain = remain.mid(len);
                    isFirst = false;
                    found = true;
                    break;
                }
            }
            if (!found) {
                ids.append(kUNK);
                break;
            }
        }
        return ids;
    }

    /* Divide il testo in "parole" (whitespace + punteggiatura singola) */
    static QStringList splitWords(const QString& text) {
        QStringList words;
        QString cur;
        for (const QChar& c : text) {
            if (c.isSpace()) {
                if (!cur.isEmpty()) { words.append(cur); cur.clear(); }
            } else if (c.isPunct()) {
                if (!cur.isEmpty()) { words.append(cur); cur.clear(); }
                words.append(QString(c));
            } else {
                cur += c;
            }
        }
        if (!cur.isEmpty()) words.append(cur);
        return words;
    }

    /* encode(text) → { inputIds, attentionMask, tokenTypeIds }
       tutti di lunghezza kMaxSeq con padding a destra. */
    struct Encoded {
        std::vector<int64_t> inputIds;
        std::vector<int64_t> attentionMask;
        std::vector<int64_t> tokenTypeIds;
    };

    Encoded encode(const QString& text) const {
        const QStringList words = splitWords(normalize(text));

        QVector<int> tokens;
        tokens.reserve(kMaxSeq);
        tokens.append(kCLS);

        for (const QString& w : words) {
            if (tokens.size() >= kMaxSeq - 1)
                break;
            const QVector<int> wTokens = tokenizeWord(w);
            for (int t : wTokens) {
                if (tokens.size() >= kMaxSeq - 1)
                    break;
                tokens.append(t);
            }
        }
        tokens.append(kSEP);

        const int realLen = tokens.size();

        Encoded enc;
        enc.inputIds.resize(kMaxSeq, kPAD);
        enc.attentionMask.resize(kMaxSeq, 0);
        enc.tokenTypeIds.resize(kMaxSeq, 0);

        for (int i = 0; i < realLen; ++i) {
            enc.inputIds[i]     = tokens[i];
            enc.attentionMask[i] = 1;
        }
        return enc;
    }
};

/* ══════════════════════════════════════════════════════════════
   OnnxEmbedderPrivate
   ══════════════════════════════════════════════════════════════ */
struct OnnxEmbedderPrivate {
    WordPieceTokenizer tokenizer;
    bool ready = false;
    int  dims  = 0;

#ifdef HAVE_ONNX
    Ort::Env            env{ORT_LOGGING_LEVEL_WARNING, "onnx_embedder"};
    Ort::SessionOptions opts;
    std::unique_ptr<Ort::Session> session;
#endif

    bool load(const QString& modelPath, const QString& vocabPath) {
        if (!tokenizer.load(vocabPath))
            return false;
#ifdef HAVE_ONNX
        opts.SetIntraOpNumThreads(2);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        try {
            session = std::make_unique<Ort::Session>(
                env,
#ifdef Q_OS_WIN
                modelPath.toStdWString().c_str(),
#else
                modelPath.toStdString().c_str(),
#endif
                opts
            );
        } catch (const Ort::Exception& e) {
            Q_UNUSED(e)
            return false;
        }
        /* Ricava la dimensione output dall'ultimo asse: (1, seq, dims) */
        Ort::AllocatorWithDefaultOptions alloc;
        const auto outInfo = session->GetOutputTypeInfo(0);
        const auto shape   = outInfo.GetTensorTypeAndShapeInfo().GetShape();
        dims  = (shape.size() >= 3) ? (int)shape[2] : 384;
        ready = true;
        return true;
#else
        Q_UNUSED(modelPath)
        return false;
#endif
    }

    QVector<float> infer(const QString& text) const {
#ifdef HAVE_ONNX
        if (!ready || !session)
            return {};

        const auto enc = tokenizer.encode(text);

        auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::array<int64_t, 2> shape = {1, WordPieceTokenizer::kMaxSeq};

        auto mkTensor = [&](const std::vector<int64_t>& data) {
            return Ort::Value::CreateTensor<int64_t>(
                memInfo,
                const_cast<int64_t*>(data.data()),
                data.size(),
                shape.data(), 2);
        };

        std::array<Ort::Value, 3> inputs = {
            mkTensor(enc.inputIds),
            mkTensor(enc.attentionMask),
            mkTensor(enc.tokenTypeIds)
        };

        const char* inNames[]  = {"input_ids", "attention_mask", "token_type_ids"};
        const char* outNames[] = {"last_hidden_state"};

        std::vector<Ort::Value> outputs;
        try {
            outputs = session->Run(
                Ort::RunOptions{nullptr},
                inNames,  inputs.data(),  3,
                outNames, 1
            );
        } catch (const Ort::Exception&) {
            return {};
        }

        const float* raw = outputs[0].GetTensorData<float>();
        /* Shape: [1, kMaxSeq, dims] — mean-pool sui token reali */
        const int seq = WordPieceTokenizer::kMaxSeq;
        QVector<float> pooled(dims, 0.0f);
        int realTokens = 0;
        for (int s = 0; s < seq; ++s) {
            if (enc.attentionMask[s] == 0) break;
            ++realTokens;
            for (int d = 0; d < dims; ++d)
                pooled[d] += raw[s * dims + d];
        }
        if (realTokens > 0)
            for (float& v : pooled) v /= (float)realTokens;

        /* L2 normalize */
        float norm = 0.0f;
        for (float v : pooled) norm += v * v;
        norm = std::sqrt(norm);
        if (norm > 1e-9f)
            for (float& v : pooled) v /= norm;

        return pooled;
#else
        Q_UNUSED(text)
        return {};
#endif
    }
};

/* ══════════════════════════════════════════════════════════════
   OnnxEmbedder — parte pubblica
   ══════════════════════════════════════════════════════════════ */
OnnxEmbedder::OnnxEmbedder(QObject* parent)
    : QObject(parent)
    , d(std::make_unique<OnnxEmbedderPrivate>())
{}

OnnxEmbedder::~OnnxEmbedder() = default;

bool OnnxEmbedder::loadModel(const QString& modelPath, const QString& vocabPath)
{
    const bool ok = d->load(modelPath, vocabPath);
    if (ok)
        emit modelLoaded(d->dims);
    return ok;
}

bool OnnxEmbedder::isReady() const { return d->ready; }
int  OnnxEmbedder::dims()    const { return d->dims;  }

QVector<float> OnnxEmbedder::embedSync(const QString& text) const
{
    return d->infer(text);
}

void OnnxEmbedder::embedAsync(const QString& text)
{
    QFuture<QVector<float>> fut = QtConcurrent::run([this, text]() -> QVector<float> {
        return d->infer(text);
    });
    auto* watcher = new QFutureWatcher<QVector<float>>(this);
    connect(watcher, &QFutureWatcher<QVector<float>>::finished, this,
            [this, watcher]() {
                watcher->deleteLater();
                const QVector<float> result = watcher->result();
                if (result.isEmpty())
                    emit embeddingError("ONNX inference fallita");
                else
                    emit embeddingReady(result);
            });
    watcher->setFuture(fut);
}

QString OnnxEmbedder::defaultModelPath()
{
    return P::onnxDir() + "/models/all-MiniLM-L6-v2.onnx";
}

QString OnnxEmbedder::defaultVocabPath()
{
    return P::onnxDir() + "/models/vocab.txt";
}

bool OnnxEmbedder::defaultModelExists()
{
    return QFile::exists(defaultModelPath()) && QFile::exists(defaultVocabPath());
}
