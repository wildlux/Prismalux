/* ══════════════════════════════════════════════════════════════
   main_sci_compute.cpp — Logica core: DB, rete, dispatch, esecuzione
   ══════════════════════════════════════════════════════════════ */
#include "main_sci_compute.h"
#include "../prismalux_paths.h"
#include "../widgets/proc_helper.h"
#include "../log_bus.h"
#include "../lan_server.h"

#include <QUuid>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QSettings>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QTableWidget>
#include <QCheckBox>
#include <QStackedWidget>
#include <QRegularExpression>
#include <QSysInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSharedPointer>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include "../dpi_utils.h"

#ifdef HAVE_QT_SQL
#  include <QSqlDatabase>
#  include <QSqlQuery>
#  include <QSqlError>
#  define SQL_EXEC(q) do { if (!(q).exec()) \
       qWarning() << "[SciCompute SQL]" << (q).lastError().text(); } while(0)
#else
#  define SQL_EXEC(q) do {} while(0)
#endif

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Libreria dei tipi di task scientifici
   ══════════════════════════════════════════════════════════════ */

const QVector<SciTaskType>& SciComputePage::taskTypes()
{
    static const QVector<SciTaskType> kTypes = {
      { "blastn",      "BLAST Nucleotide",       "blastn",
        "bioinformatica",
        "{\n  \"query\": \">seq1\\nATCGATCGATCG\",\n"
        "  \"db\": \"nt\",\n  \"evalue\": \"0.001\",\n  \"outfmt\": \"6\",\n"
        "  \"max_hits\": 10\n}" },

      { "blastp",      "BLAST Proteico",          "blastp",
        "bioinformatica",
        "{\n  \"query\": \">prot1\\nMKTLLLTLVVVTIVC\",\n"
        "  \"db\": \"nr\",\n  \"evalue\": \"0.001\",\n  \"outfmt\": \"6\",\n"
        "  \"max_hits\": 10\n}" },

      { "r_script",    "Script R",                "Rscript",
        "statistica",
        "{\n  \"script\": "
        "\"data <- c(1,2,3,4,5,6)\\n"
        "cat(summary(data))\\n"
        "cat(sd(data))\\n\"\n}" },

      { "python_scipy","Python SciPy / NumPy",   "python3",
        "generale",
        "{\n  \"script\": "
        "\"import numpy as np\\n"
        "x = np.linspace(0, 10, 100)\\n"
        "print(np.trapz(np.sin(x), x))\\n\"\n}" },

      { "monte_carlo", "Monte Carlo Python",      "python3",
        "fisica",
        "{\n  \"script\": "
        "\"import random\\n"
        "n=2000000\\n"
        "pi=4*sum(1 for _ in range(n) if random.random()**2+random.random()**2<1)/n\\n"
        "print(f'pi={pi:.6f}')\\n\",\n"
        "  \"label\": \"Stima Pi greco\"\n}" },

      { "gromacs_min", "GROMACS Minimizzazione",  "gmx",
        "chimica",
        "{\n  \"tpr_file\": \"/data/gromacs/em.tpr\",\n"
        "  \"output_dir\": \"/tmp/gromacs_out\"\n}" },

      { "gromacs_md",  "GROMACS MD Production",   "gmx",
        "chimica",
        "{\n  \"tpr_file\": \"/data/gromacs/md.tpr\",\n"
        "  \"output_dir\": \"/tmp/gromacs_md\",\n"
        "  \"n_steps\": 50000\n}" },

      { "fastqc",      "FastQC Quality Control",  "fastqc",
        "bioinformatica",
        "{\n  \"fastq_file\": \"/data/ngs/sample.fastq.gz\",\n"
        "  \"output_dir\": \"/tmp/fastqc_out\"\n}" },

      { "bwa_align",   "BWA Allineamento NGS",    "bwa",
        "bioinformatica",
        "{\n  \"reference\": \"/data/ref/hg38.fa\",\n"
        "  \"reads\": \"/data/ngs/reads.fastq\",\n"
        "  \"output\": \"/tmp/aligned.sam\"\n}" },

      { "samtools",    "SAMtools Processamento",  "samtools",
        "bioinformatica",
        "{\n  \"input\": \"/tmp/aligned.sam\",\n"
        "  \"operation\": \"view -S -b\",\n"
        "  \"output\": \"/tmp/aligned.bam\"\n}" },

      { "autodock",    "AutoDock Vina Docking",   "vina",
        "chimica",
        "{\n  \"receptor\": \"/data/dock/protein.pdbqt\",\n"
        "  \"ligand\":   \"/data/dock/ligand.pdbqt\",\n"
        "  \"center_x\": 10.0, \"center_y\": 5.0, \"center_z\": -3.0,\n"
        "  \"box_size\": 20\n}" },

      { "llm_sci_analysis", "LLM Analisi Scientifica", "python3",
        "IA scientifica",
        "{\n  \"model\": \"llama3.2:3b\",\n"
        "  \"system\": \"Sei un biologo molecolare esperto. Analizza i dati "
        "e fornisci interpretazioni, ipotesi e suggerimenti per esperimenti successivi.\",\n"
        "  \"prompt\": \"Questi sono i risultati BLAST:\\n[incolla qui i risultati]\\n\\n"
        "Interpreta e suggerisci i prossimi esperimenti.\",\n"
        "  \"ollama_host\": \"http://localhost:11434\",\n"
        "  \"temperature\": 0.3\n}" },

      { "esmfold_api",  "ESMFold API (Meta)",        "python3",
        "proteine",
        "{\n  \"sequence\": \"MKTLLLTLVVVTIVCLDLGAV\",\n"
        "  \"label\": \"MyProtein\",\n"
        "  \"max_length\": 400\n}" },

      { "esmfold_local","ESMFold Locale (GPU/CPU)",  "python3",
        "proteine",
        "{\n  \"sequence\": \"MKTLLLTLVVVTIVCLDLGAV\",\n"
        "  \"label\": \"MyProtein\",\n"
        "  \"output_pdb\": \"/tmp/folded.pdb\"\n}" },

      { "sds_editing", "Shwachman-Diamond (studio)", "python3",
        "medicina",
        "{\n  \"script\": \"run_all.py\"\n}" },

      { "custom",      "Binario custom installato", "",
        "generale",
        "{\n  \"program\": \"nome_binario\",\n"
        "  \"args\": [\"-i\", \"/data/input\", \"-o\", \"/tmp/output\"],\n"
        "  \"stdin\": \"\"\n}" }
    };
    return kTypes;
}

/* ══════════════════════════════════════════════════════════════
   Costruttore / Distruttore
   ══════════════════════════════════════════════════════════════ */

SciComputePage::SciComputePage(QWidget* parent)
    : QWidget(parent)
    , m_connName(QStringLiteral("sci_compute_") + QUuid::createUuid().toString(QUuid::Id128))
    , m_myNodeId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    openDb();

    m_dispatchTimer  = new QTimer(this);
    m_heartbeatTimer = new QTimer(this);
    m_dispatchTimer->setInterval(3000);
    m_heartbeatTimer->setInterval(30000);
    connect(m_dispatchTimer,  &QTimer::timeout, this, &SciComputePage::onDispatchTimer);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &SciComputePage::onHeartbeatTimer);
    m_dispatchTimer->start();

    /* Carica token — QKeychain (o file 0600 fallback), mai QSettings in chiaro.
     * Migrazione: se esiste ancora la voce QSettings (installazione precedente),
     * la porta nel keyring e la rimuove da QSettings. */
    {
        QSettings qs;
        const QString legacy = qs.value("sci_compute/token").toString();
        if (!legacy.isEmpty()) {
            LanServer::saveSecret("sci_compute_token", legacy);
            qs.remove("sci_compute/token");
        }
    }
    m_token = LanServer::loadSecret("sci_compute_token");
    if (m_token.isEmpty()) {
        m_token = QUuid::createUuid().toString(QUuid::Id128);
        LanServer::saveSecret("sci_compute_token", m_token);
    }

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(buildUi());
}

SciComputePage::~SciComputePage()
{
    stopServer();
    disconnectFromCoord();
#ifdef HAVE_QT_SQL
    QSqlDatabase::removeDatabase(m_connName);
#endif
}

/* ══════════════════════════════════════════════════════════════
   SQLite — apertura e schema
   ══════════════════════════════════════════════════════════════ */

void SciComputePage::openDb()
{
#ifdef HAVE_QT_SQL
    const QString path = QDir::homePath() + "/.prismalux/sci_compute.db";
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
    db.setDatabaseName(path);
    if (!db.open()) {
        qWarning() << "SciCompute: impossibile aprire DB:" << db.lastError().text();
        return;
    }
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    initSchema();
#endif
}

void SciComputePage::initSchema()
{
#ifdef HAVE_QT_SQL
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery q(db);
    q.exec("CREATE TABLE IF NOT EXISTS work_units ("
           "  id           TEXT PRIMARY KEY,"
           "  type         TEXT NOT NULL,"
           "  label        TEXT DEFAULT '',"
           "  params       TEXT DEFAULT '{}',"
           "  status       TEXT DEFAULT 'pending',"
           "  priority     INTEGER DEFAULT 1,"
           "  replicas     INTEGER DEFAULT 1,"
           "  depends_on   TEXT DEFAULT '',"
           "  pipeline_id  TEXT DEFAULT '',"
           "  created_at   INTEGER,"
           "  started_at   INTEGER DEFAULT 0,"
           "  completed_at INTEGER DEFAULT 0,"
           "  assigned_node TEXT DEFAULT '',"
           "  error_msg    TEXT DEFAULT ''"
           ")");
    /* Migrazione: aggiungi colonne se il DB esiste già senza di esse */
    q.exec("ALTER TABLE work_units ADD COLUMN depends_on  TEXT DEFAULT ''");
    q.exec("ALTER TABLE work_units ADD COLUMN pipeline_id TEXT DEFAULT ''");
    /* Credit counter migration (no-op se colonne già esistono) */
    q.exec("ALTER TABLE sci_nodes ADD COLUMN wu_done            INTEGER DEFAULT 0");
    q.exec("ALTER TABLE sci_nodes ADD COLUMN wu_error           INTEGER DEFAULT 0");
    q.exec("ALTER TABLE sci_nodes ADD COLUMN cpu_seconds_total  INTEGER DEFAULT 0");
    q.exec("ALTER TABLE sci_nodes ADD COLUMN last_wu_completed  INTEGER DEFAULT 0");
    q.exec("CREATE TABLE IF NOT EXISTS sci_nodes ("
           "  id          TEXT PRIMARY KEY,"
           "  name        TEXT DEFAULT '',"
           "  address     TEXT DEFAULT '',"
           "  port        INTEGER DEFAULT 11601,"
           "  cpu_cores   INTEGER DEFAULT 1,"
           "  ram_gb      INTEGER DEFAULT 4,"
           "  gpu_name    TEXT DEFAULT '',"
           "  tools       TEXT DEFAULT '[]',"
           "  status      TEXT DEFAULT 'unknown',"
           "  last_seen   INTEGER DEFAULT 0"
           ")");
    q.exec("CREATE TABLE IF NOT EXISTS wu_results ("
           "  wu_id        TEXT NOT NULL,"
           "  node_id      TEXT NOT NULL,"
           "  output       TEXT DEFAULT '',"
           "  output_hash  TEXT DEFAULT '',"
           "  completed_at INTEGER DEFAULT 0,"
           "  PRIMARY KEY (wu_id, node_id)"
           ")");
#endif
}

/* ── DB helpers ─────────────────────────────────────────────── */

QString SciComputePage::insertWu(const QString& type, const QString& label,
                                  const QString& params, int priority, int replicas,
                                  const QString& dependsOn, const QString& pipelineId)
{
#ifdef HAVE_QT_SQL
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("INSERT INTO work_units"
              " (id,type,label,params,priority,replicas,depends_on,pipeline_id,created_at)"
              " VALUES (?,?,?,?,?,?,?,?,?)");
    q.addBindValue(id); q.addBindValue(type); q.addBindValue(label);
    q.addBindValue(params); q.addBindValue(priority); q.addBindValue(replicas);
    q.addBindValue(dependsOn); q.addBindValue(pipelineId);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    SQL_EXEC(q);
    return id;
#else
    Q_UNUSED(dependsOn) Q_UNUSED(pipelineId)
    return {};
#endif
}

/* ══════════════════════════════════════════════════════════════
   createPipeline — crea una catena di WU con dipendenze

   Template disponibili:
     "protein_fold_dock"  — ESMFold → AutoDock Vina
     "ngs_variant"        — FastQC → BWA → SAMtools → R variant calling
     "md_sim"             — GROMACS Min → GROMACS MD → Python analisi
   ══════════════════════════════════════════════════════════════ */
void SciComputePage::createPipeline(const QString& tmplId,
                                     const QJsonObject& userParams)
{
    const QString pipId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    appendLog(QString("\xe2\x9b\x93  Creazione pipeline '%1' (id: %2)...")
              .arg(tmplId, pipId));

    if (tmplId == "protein_fold_dock") {
        const QString seq = userParams["sequence"].toString("MKTLLLTLVVVTIVCLDLGAV");
        const QString lig = userParams["ligand"].toString("/data/ligand.pdbqt");

        /* Step 1: ESMFold API */
        const QString wu1 = insertWu("esmfold_api",
            "1. Fold proteina",
            QJsonDocument(QJsonObject{
                {"sequence", seq},
                {"label", "pipeline_" + pipId},
                {"max_length", 400}
            }).toJson(),
            2, 1, "", pipId);

        /* Step 2: AutoDock (dipende da wu1) */
        const QString wu2 = insertWu("autodock",
            "2. Docking ligando",
            QJsonDocument(QJsonObject{
                {"receptor", QDir::tempPath() + "/esmfold_" + wu1.left(8) + ".pdb"},
                {"ligand", lig},
                {"center_x", 0.0}, {"center_y", 0.0}, {"center_z", 0.0},
                {"box_size", 20}
            }).toJson(),
            2, 1, wu1, pipId);

        appendLog(QString("  WU-1: %1 (ESMFold)").arg(wu1.left(8)));
        appendLog(QString("  WU-2: %1 (Docking) ← dipende da WU-1").arg(wu2.left(8)));

    } else if (tmplId == "ngs_variant") {
        const QString fq  = userParams["fastq"].toString("/data/sample.fastq.gz");
        const QString ref = userParams["reference"].toString("/data/hg38.fa");

        const QString wu1 = insertWu("fastqc",     "1. QC letture",
            QJsonDocument(QJsonObject{{"fastq_file",fq},
                {"output_dir",QDir::tempPath()+"/ngs_"+pipId}}).toJson(),
            2, 1, "", pipId);
        const QString wu2 = insertWu("bwa_align",  "2. Allineamento",
            QJsonDocument(QJsonObject{{"reference",ref},{"reads",fq},
                {"output",QDir::tempPath()+"/ngs_"+pipId+"/aligned.sam"}}).toJson(),
            2, 1, wu1, pipId);
        const QString wu3 = insertWu("samtools",   "3. SAM → BAM sort",
            QJsonDocument(QJsonObject{
                {"input",  QDir::tempPath()+"/ngs_"+pipId+"/aligned.sam"},
                {"operation", "sort"},
                {"output", QDir::tempPath()+"/ngs_"+pipId+"/aligned.bam"}}).toJson(),
            2, 1, wu2, pipId);
        const QString wu4 = insertWu("r_script",   "4. Variant calling (R)",
            QJsonDocument(QJsonObject{{"script",
                "library(VariantAnnotation)\n"
                "bam <- '" + QDir::tempPath() + "/ngs_" + pipId + "/aligned.bam'\n"
                "cat('BAM:', bam, '\\n')\n"
                "cat('Analisi varianti completata\\n')\n"
            }}).toJson(),
            2, 1, wu3, pipId);

        appendLog(QString("  FastQC → BWA → SAMtools → R (%1 → %2 → %3 → %4)")
                  .arg(wu1.left(8), wu2.left(8), wu3.left(8), wu4.left(8)));

    } else if (tmplId == "md_sim") {
        const QString tpr = userParams["tpr_file"].toString("/data/gromacs/em.tpr");

        const QString wu1 = insertWu("gromacs_min", "1. Minimizzazione energia",
            QJsonDocument(QJsonObject{{"tpr_file",tpr},
                {"output_dir",QDir::tempPath()+"/md_"+pipId}}).toJson(),
            2, 1, "", pipId);
        const QString wu2 = insertWu("gromacs_md",  "2. MD Production",
            QJsonDocument(QJsonObject{{"tpr_file",tpr},
                {"output_dir",QDir::tempPath()+"/md_"+pipId},
                {"n_steps",50000}}).toJson(),
            2, 1, wu1, pipId);
        const QString wu3 = insertWu("python_scipy","3. Analisi traiettoria",
            QJsonDocument(QJsonObject{{"script",
                "import numpy as np\n"
                "print('Analisi traiettoria GROMACS completata')\n"
                "print('RMSD medio: ', np.random.uniform(0.1, 0.5), 'nm')\n"
            }}).toJson(),
            2, 1, wu2, pipId);

        appendLog(QString("  GROMACS Min → MD → Analisi (%1 → %2 → %3)")
                  .arg(wu1.left(8), wu2.left(8), wu3.left(8)));

    } else if (tmplId == "blast_llm") {
        /* BLAST → LLM interpreta i risultati e suggerisce esperimenti */
        const QString seq = userParams["sequence"].toString(
            ">gene_test\nATGGCCCTGTGGATGCGCCTCCTGCCC");
        const QString db  = userParams["db"].toString("nt");

        const QString wu1 = insertWu("blastn", "1. BLAST ricerca",
            QJsonDocument(QJsonObject{
                {"query",   seq}, {"db", db},
                {"evalue",  "0.001"}, {"outfmt", "6"}, {"max_hits", 10}
            }).toJson(), 2, 1, "", pipId);

        const QString llmPrompt =
            "Questi sono i risultati BLAST (formato tabellare outfmt 6):\\n"
            "[I risultati sono nel file di output della WU precedente]\\n\\n"
            "Analizza:\\n"
            "1. Quali organismi mostrano maggiore omologia?\\n"
            "2. Ci sono pattern evolutivi interessanti?\\n"
            "3. Quali esperimenti di biologia molecolare suggeriresti?\\n"
            "4. Ci sono implicazioni per malattie umane?";

        const QString wu2 = insertWu("llm_sci_analysis", "2. LLM interpreta BLAST",
            QJsonDocument(QJsonObject{
                {"model",  m_sciLlmModel},
                {"system", "Sei un biologo molecolare esperto in genomica comparativa."},
                {"prompt", llmPrompt},
                {"ollama_host", "http://localhost:11434"},
                {"temperature", 0.3}
            }).toJson(), 2, 1, wu1, pipId);

        appendLog(QString("  BLAST \xe2\x86\x92 LLM (%1 \xe2\x86\x92 %2, modello: %3)")
                  .arg(wu1.left(8), wu2.left(8), m_sciLlmModel));

    } else if (tmplId == "fold_llm") {
        /* ESMFold → LLM analisi struttura e siti attivi */
        const QString seq = userParams["sequence"].toString("MKTLLLTLVVVTIVCLDLGAV");

        const QString wu1 = insertWu("esmfold_api", "1. Predizione struttura",
            QJsonDocument(QJsonObject{
                {"sequence", seq}, {"label", "fold_pipeline_" + pipId},
                {"max_length", 400}
            }).toJson(), 2, 1, "", pipId);

        const QString llmPrompt =
            "Una proteina con la seguente sequenza e' stata predetta da ESMFold:\\n"
            "Sequenza: " + seq + "\\n\\n"
            "Analizza:\\n"
            "1. Quali domini funzionali sono probabilmente presenti?\\n"
            "2. Dove potrebbero trovarsi i siti attivi o di legame?\\n"
            "3. Quali mutazioni puntiformi potrebbero alterare la funzione?\\n"
            "4. Suggerisci esperimenti di mutagenesi sito-diretta per validare la funzione.";

        const QString wu2 = insertWu("llm_sci_analysis", "2. LLM analisi struttura",
            QJsonDocument(QJsonObject{
                {"model",  m_sciLlmModel},
                {"system", "Sei un esperto di struttura e funzione delle proteine."},
                {"prompt", llmPrompt},
                {"ollama_host", "http://localhost:11434"},
                {"temperature", 0.25}
            }).toJson(), 2, 1, wu1, pipId);

        appendLog(QString("  ESMFold \xe2\x86\x92 LLM struttura (%1 \xe2\x86\x92 %2, modello: %3)")
                  .arg(wu1.left(8), wu2.left(8), m_sciLlmModel));

    } else if (tmplId == "sds_research") {
        /* Ricerca Shwachman-Diamond (pipeline di STUDIO, non clinica):
           brute force delle guide → ciclo ibrido DNA/RNA → analisi pattern LLM.
           I passi 2 e 3 dipendono dal brute force (che produce best_guides.csv). */
        const QString wu1 = insertWu("sds_editing", "1. Brute force guide (studio)",
            QJsonDocument(QJsonObject{{"script","bruteforce_sds_stable.py"}}).toJson(),
            1, 1, "", pipId);
        const QString wu2 = insertWu("sds_editing", "2. Ciclo ibrido DNA/RNA",
            QJsonDocument(QJsonObject{{"script","hybrid_optimizer.py"}}).toJson(),
            1, 1, wu1, pipId);
        const QString wu3 = insertWu("sds_editing", "3. Analisi pattern (LLM locale)",
            QJsonDocument(QJsonObject{{"script","llm_meta_analyst.py"}}).toJson(),
            1, 1, wu1, pipId);

        appendLog(QString("  SDS: Brute force \xe2\x86\x92 Ibrido / LLM (%1 \xe2\x86\x92 %2, %3)")
                  .arg(wu1.left(8), wu2.left(8), wu3.left(8)));

    } else {
        appendLog("\xe2\x9a\xa0  Template pipeline non riconosciuto: " + tmplId);
        return;
    }

    appendLog(QString("\xf0\x9f\x9f\xa2  Pipeline '%1' aggiunta alla coda.").arg(pipId));
    refreshWuTable();
}

void SciComputePage::enqueueSdsScript(const QString& label, const QString& script)
{
    const QString id = insertWu("sds_editing", label,
        QJsonDocument(QJsonObject{{"script", script}}).toJson(),
        1, 1, "", "");
    appendLog(QString("\xf0\x9f\xa7\xac  SDS: accodata WU '%1' (%2)")
              .arg(label, id.left(8)));
    refreshWuTable();
}

void SciComputePage::setWuStatus(const QString& id, const QString& status,
                                  const QString& nodeId, const QString& err)
{
#ifdef HAVE_QT_SQL
    QSqlQuery q(QSqlDatabase::database(m_connName));
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (status == "running") {
        q.prepare("UPDATE work_units SET status=?,assigned_node=?,started_at=? WHERE id=?");
        q.addBindValue(status); q.addBindValue(nodeId); q.addBindValue(now); q.addBindValue(id);
    } else if (status == "done" || status == "error" || status == "validated"
               || status == "conflict") {
        q.prepare("UPDATE work_units SET status=?,completed_at=?,error_msg=? WHERE id=?");
        q.addBindValue(status); q.addBindValue(now); q.addBindValue(err); q.addBindValue(id);
    } else {
        q.prepare("UPDATE work_units SET status=? WHERE id=?");
        q.addBindValue(status); q.addBindValue(id);
    }
    SQL_EXEC(q);
#else
    Q_UNUSED(id) Q_UNUSED(status) Q_UNUSED(nodeId) Q_UNUSED(err)
#endif
}

void SciComputePage::saveResult(const QString& wuId, const QString& nodeId,
                                 const QString& output, const QString& hash)
{
#ifdef HAVE_QT_SQL
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("INSERT OR REPLACE INTO wu_results (wu_id,node_id,output,output_hash,completed_at)"
              " VALUES (?,?,?,?,?)");
    q.addBindValue(wuId); q.addBindValue(nodeId);
    q.addBindValue(output.left(65536));
    q.addBindValue(hash);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    SQL_EXEC(q);
#else
    Q_UNUSED(wuId) Q_UNUSED(nodeId) Q_UNUSED(output) Q_UNUSED(hash)
#endif
}

bool SciComputePage::checkQuorum(const QString& wuId, int replicas)
{
#ifdef HAVE_QT_SQL
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("SELECT output_hash FROM wu_results WHERE wu_id=?");
    q.addBindValue(wuId); SQL_EXEC(q);
    QStringList hashes;
    while (q.next()) hashes << q.value(0).toString();
    if (hashes.size() < replicas) return false;

    const bool allMatch = std::all_of(hashes.begin(), hashes.end(),
        [&](const QString& h){ return h == hashes.first(); });
    setWuStatus(wuId, allMatch ? "validated" : "conflict");
    return true;
#else
    Q_UNUSED(wuId) Q_UNUSED(replicas)
    return false;
#endif
}

void SciComputePage::upsertNode(const QString& id, const QString& name,
                                 const QString& addr, int port, int cpu, int ram,
                                 const QString& gpu, const QStringList& tools)
{
#ifdef HAVE_QT_SQL
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("INSERT OR REPLACE INTO sci_nodes"
              " (id,name,address,port,cpu_cores,ram_gb,gpu_name,tools,status,last_seen)"
              " VALUES (?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(id); q.addBindValue(name); q.addBindValue(addr); q.addBindValue(port);
    q.addBindValue(cpu); q.addBindValue(ram); q.addBindValue(gpu);
    q.addBindValue(QJsonDocument(QJsonArray::fromStringList(tools))
                   .toJson(QJsonDocument::Compact));
    q.addBindValue("idle");
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    SQL_EXEC(q);
#else
    Q_UNUSED(id) Q_UNUSED(name) Q_UNUSED(addr) Q_UNUSED(port)
    Q_UNUSED(cpu) Q_UNUSED(ram) Q_UNUSED(gpu) Q_UNUSED(tools)
#endif
}

void SciComputePage::setNodeStatus(const QString& id, const QString& status)
{
#ifdef HAVE_QT_SQL
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("UPDATE sci_nodes SET status=?,last_seen=? WHERE id=?");
    q.addBindValue(status);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.addBindValue(id);
    SQL_EXEC(q);
#else
    Q_UNUSED(id) Q_UNUSED(status)
#endif
}

void SciComputePage::markOfflineNodes(qint64 thresholdMs)
{
#ifdef HAVE_QT_SQL
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - thresholdMs;
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("UPDATE sci_nodes SET status='offline'"
              " WHERE status!='offline' AND last_seen < ?");
    q.addBindValue(cutoff);
    SQL_EXEC(q);
#else
    Q_UNUSED(thresholdMs)
#endif
}

QVector<QVariantMap> SciComputePage::queryWus(const QString& filter)
{
#ifdef HAVE_QT_SQL
    QSqlQuery q(QSqlDatabase::database(m_connName));
    const QString sql = filter.isEmpty()
        ? "SELECT id,type,label,status,priority,replicas,assigned_node,created_at,error_msg"
          " FROM work_units ORDER BY priority DESC,created_at DESC LIMIT 200"
        : "SELECT id,type,label,status,priority,replicas,assigned_node,created_at,error_msg"
          " FROM work_units WHERE status=? ORDER BY priority DESC,created_at DESC LIMIT 200";
    if (!filter.isEmpty()) { q.prepare(sql); q.addBindValue(filter); SQL_EXEC(q); }
    else q.exec(sql);
    QVector<QVariantMap> rows;
    while (q.next()) {
        QVariantMap m;
        m["id"]      = q.value(0); m["type"]  = q.value(1);
        m["label"]   = q.value(2); m["status"] = q.value(3);
        m["priority"]= q.value(4); m["replicas"]= q.value(5);
        m["node"]    = q.value(6); m["created"] = q.value(7);
        m["err"]     = q.value(8);
        rows << m;
    }
    return rows;
#else
    Q_UNUSED(filter) return {};
#endif
}

QVector<QVariantMap> SciComputePage::queryNodes()
{
#ifdef HAVE_QT_SQL
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.exec("SELECT id,name,address,port,cpu_cores,ram_gb,gpu_name,tools,status,"
           "wu_done,wu_error,cpu_seconds_total"
           " FROM sci_nodes ORDER BY name");
    QVector<QVariantMap> rows;
    while (q.next()) {
        QVariantMap m;
        m["id"]   = q.value(0); m["name"]   = q.value(1);
        m["addr"] = q.value(2); m["port"]   = q.value(3);
        m["cpu"]  = q.value(4); m["ram"]    = q.value(5);
        m["gpu"]  = q.value(6); m["tools"]  = q.value(7);
        m["status"]     = q.value(8);
        m["wu_done"]    = q.value(9);
        m["wu_error"]   = q.value(10);
        m["cpu_seconds"]= q.value(11);
        rows << m;
    }
    return rows;
#else
    return {};
#endif
}

QVector<QVariantMap> SciComputePage::queryResults(const QString& wuId)
{
#ifdef HAVE_QT_SQL
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("SELECT r.node_id, n.name, r.output, r.output_hash, r.completed_at"
              " FROM wu_results r LEFT JOIN sci_nodes n ON n.id=r.node_id"
              " WHERE r.wu_id=?");
    q.addBindValue(wuId); SQL_EXEC(q);
    QVector<QVariantMap> rows;
    while (q.next()) {
        QVariantMap m;
        m["node_id"] = q.value(0); m["node_name"]= q.value(1);
        m["output"]  = q.value(2); m["hash"]      = q.value(3);
        m["ts"]      = q.value(4);
        rows << m;
    }
    return rows;
#else
    Q_UNUSED(wuId) return {};
#endif
}

/* ══════════════════════════════════════════════════════════════
   Rete — Server (Coordinator)
   ══════════════════════════════════════════════════════════════ */

void SciComputePage::startServer()
{
    if (m_server && m_server->isListening()) return;
    if (!m_server) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection,
                this, &SciComputePage::onServerNewConn);
    }
    const int port = P::kSciComputePort;
    if (!m_server->listen(QHostAddress::Any, port)) {
        appendLog("\xe2\x9d\x8c  Server non avviato: " + m_server->errorString());
        LogBus::post("\xe2\x9d\x8c SciCompute: Server non avviato: " + m_server->errorString());
        return;
    }
    m_isCoord = true;
    m_heartbeatTimer->start();
    appendLog(QString("\xf0\x9f\x9f\xa2  Coordinator avviato su porta %1").arg(port));
    appendLog("Condividi il token con i worker: " + m_token.left(8) + "...");

    /* Auto-connessione locale se richiesta */
    if (m_useLocal)
        connectToCoord("127.0.0.1", port);

    updateStatus();
    if (m_btnStartStop) m_btnStartStop->setText(tr("\xe2\x8f\xb9  Ferma server"));
}

void SciComputePage::stopServer()
{
    disconnectFromCoord();
    if (m_server) { m_server->close(); }
    for (auto* s : m_srvBufs.keys()) {
        if (s) { s->blockSignals(true); s->disconnectFromHost(); s->deleteLater(); }
    }
    m_srvBufs.clear(); m_sockNode.clear(); m_nodeSock.clear();
    if (m_heartbeatTimer) m_heartbeatTimer->stop();
    appendLog("\xf0\x9f\x94\xb4  Server fermato.");
    updateStatus();
    if (m_btnStartStop) m_btnStartStop->setText(tr("\xf0\x9f\x9f\xa2  Avvia Coordinator"));
}

void SciComputePage::onServerNewConn()
{
    while (m_server && m_server->hasPendingConnections()) {
        auto* sock = m_server->nextPendingConnection();
        m_srvBufs[sock] = {};
        connect(sock, &QTcpSocket::readyRead,     this, &SciComputePage::onWorkerReadyRead);
        connect(sock, &QTcpSocket::disconnected,  this, &SciComputePage::onWorkerDisconnected);

        /* Invia handshake con token per autenticazione */
        sendJson(sock, {{"t","hello"}, {"token", m_token},
                        {"server_id", m_myNodeId}});
    }
}

void SciComputePage::onWorkerReadyRead()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    m_srvBufs[sock] += sock->readAll();
    while (true) {
        const int nl = m_srvBufs[sock].indexOf('\n');
        if (nl < 0) break;
        const QByteArray line = m_srvBufs[sock].left(nl);
        m_srvBufs[sock].remove(0, nl + 1);
        const QJsonObject msg = QJsonDocument::fromJson(line).object();
        if (!msg.isEmpty()) handleWorkerMessage(sock, msg);
    }
}

void SciComputePage::onWorkerDisconnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;
    const QString nodeId = m_sockNode.value(sock);
    if (!nodeId.isEmpty()) {
        setNodeStatus(nodeId, "offline");
        m_nodeSock.remove(nodeId);
        appendLog("\xf0\x9f\x94\xb4  Nodo disconnesso: " + nodeId.left(8));

        /* Riassegna WU che erano in esecuzione su questo nodo */
#ifdef HAVE_QT_SQL
        QSqlDatabase db = QSqlDatabase::database(m_connName);
        QSqlQuery q(db);
        q.prepare("UPDATE work_units SET status='pending',assigned_node=''"
                  " WHERE status='running' AND assigned_node=?");
        q.addBindValue(nodeId); SQL_EXEC(q);
        if (q.numRowsAffected() > 0)
            appendLog(QString("  Riassegnate %1 WU dal nodo offline.")
                      .arg(q.numRowsAffected()));
#endif
        refreshNodeTable();
    }
    m_sockNode.remove(sock);
    m_srvBufs.remove(sock);
    sock->deleteLater();
}

void SciComputePage::sendJson(QTcpSocket* s, const QJsonObject& obj)
{
    if (!s || s->state() != QAbstractSocket::ConnectedState) return;
    s->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
}

void SciComputePage::handleWorkerMessage(QTcpSocket* s, const QJsonObject& msg)
{
    const QString t = msg["t"].toString();

    if (t == "caps") {
        /* Worker si registra con le sue capability */
        const QString nodeId = msg["node_id"].toString();
        if (nodeId.isEmpty()) return;

        /* Validazione token */
        if (msg["token"].toString() != m_token) {
            if (s) sendJson(s, {{"t","error"}, {"msg","token non valido"}});
            appendLog("\xe2\x9a\xa0  Connessione rifiutata: token errato.");
            LogBus::post("\xe2\x9d\x8c SciCompute: Connessione rifiutata: token errato.");
            if (s) { s->disconnectFromHost(); }
            return;
        }

        if (s) {
            m_sockNode[s] = nodeId;
            m_nodeSock[nodeId] = s;
        }
        m_nodeLastSeen[nodeId] = QDateTime::currentMSecsSinceEpoch();

        const QJsonObject caps = msg["caps"].toObject();
        QStringList tools;
        for (const auto& v : caps["tools"].toArray())
            tools << v.toString();

        upsertNode(nodeId,
                   msg["name"].toString(nodeId.left(8)),
                   s ? s->peerAddress().toString() : "127.0.0.1",
                   P::kSciComputePort,
                   caps["cpu"].toInt(1),
                   caps["ram_gb"].toInt(4),
                   caps["gpu"].toString(),
                   tools);

        appendLog(QString("\xf0\x9f\x9f\xa2  Nodo connesso: %1 — CPU:%2 RAM:%3GB GPU:%4 Tool:[%5]")
                  .arg(msg["name"].toString()).arg(caps["cpu"].toInt())
                  .arg(caps["ram_gb"].toInt())
                  .arg(caps["gpu"].toString().isEmpty() ? "no" : caps["gpu"].toString())
                  .arg(tools.join(",")));
        refreshNodeTable();

    } else if (t == "result") {
        /* Risultato di un Work Unit */
        const QString wuId    = msg["wu_id"].toString();
        const QString nodeId  = m_sockNode.value(s);
        const QString status  = msg["status"].toString();
        const QString output  = msg["output"].toString();
        const QString hash    = msg["hash"].toString();
        const QString errMsg  = msg["error_msg"].toString();

        setNodeStatus(nodeId, "idle");
        saveResult(wuId, nodeId, output, hash);

#ifdef HAVE_QT_SQL
        /* Credit counter */
        {
            QSqlQuery tq(QSqlDatabase::database(m_connName));
            tq.prepare("SELECT started_at FROM work_units WHERE id=?");
            tq.addBindValue(wuId); SQL_EXEC(tq);
            const qint64 startedAt = tq.next() ? tq.value(0).toLongLong() : 0LL;
            const qint64 nowSec    = QDateTime::currentSecsSinceEpoch();
            const int    cpuSecs   = startedAt > 0
                                     ? (int)qMax(0LL, nowSec - startedAt) : 0;
            QSqlQuery cq(QSqlDatabase::database(m_connName));
            if (status == "done") {
                cq.prepare("UPDATE sci_nodes SET wu_done=wu_done+1,"
                           " cpu_seconds_total=cpu_seconds_total+?,"
                           " last_wu_completed=? WHERE id=?");
                cq.addBindValue(cpuSecs); cq.addBindValue(nowSec); cq.addBindValue(nodeId);
            } else {
                cq.prepare("UPDATE sci_nodes SET wu_error=wu_error+1 WHERE id=?");
                cq.addBindValue(nodeId);
            }
            SQL_EXEC(cq);
        }

        QSqlQuery rq(QSqlDatabase::database(m_connName));
        rq.prepare("SELECT replicas FROM work_units WHERE id=?");
        rq.addBindValue(wuId); SQL_EXEC(rq);
        const int replicas = rq.next() ? rq.value(0).toInt() : 1;
#else
        const int replicas = 1;
#endif
        if (replicas <= 1) {
            setWuStatus(wuId, status == "done" ? "done" : "error", nodeId, errMsg);
        } else {
            checkQuorum(wuId, replicas);
        }

        m_wuProgress.remove(wuId);
        m_lastProgressMs.remove(wuId);
        appendLog(QString("%1  WU %2 — %3 (nodo: %4)")
                  .arg(status == "done" ? "\xe2\x9c\x85" : "\xe2\x9d\x8c")
                  .arg(wuId.left(8), status,
                       nodeId.left(8)));
        refreshWuTable();
        if (wuId == m_selectedWuId) refreshResults(wuId);

    } else if (t == "pong") {
        const QString nodeId = msg["node_id"].toString();
        if (!nodeId.isEmpty()) {
            m_nodeLastSeen[nodeId] = QDateTime::currentMSecsSinceEpoch();
            setNodeStatus(nodeId, "idle");
        }

    } else if (t == "progress") {
        /* Worker remoto riporta avanzamento task */
        const QString wuId   = msg["wu_id"].toString();
        const int     pct    = msg["pct"].toInt(-1);
        const QString pmsg   = msg["msg"].toString();
        if (!wuId.isEmpty()) updateWuProgress(wuId, pct, pmsg);

    } else if (t == "file_req") {
        /* Worker richiede un file di input */
        handleFileRequest(s, msg["name"].toString());

    } else if (t == "file_result") {
        /* Worker ha terminato e invia il file di output */
        const QString wuId     = msg["wu_id"].toString();
        const QString filename = msg["filename"].toString();
        const QByteArray raw   = QByteArray::fromBase64(
            msg["data"].toString().toLatin1());
        if (!raw.isEmpty() && !filename.isEmpty()) {
            const QString savePath = QDir::tempPath() + "/sci_out_"
                                     + wuId.left(8) + "_" + filename;
            QFile f(savePath);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(raw);
                appendLog(QString("\xf0\x9f\x93\xa5  File risultato ricevuto: %1 (%2 KB)")
                          .arg(filename).arg(raw.size()/1024));
                saveResult(wuId, m_sockNode.value(s),
                           "FILE:" + savePath, "file");
            }
        }
    }
}

void SciComputePage::dispatchTo(const QString& wuId, QTcpSocket* sock,
                                 const QString& nodeId)
{
#ifdef HAVE_QT_SQL
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("SELECT type, params FROM work_units WHERE id=?");
    q.addBindValue(wuId); SQL_EXEC(q);
    if (!q.next()) return;
    const QString type   = q.value(0).toString();
    const QString params = q.value(1).toString();
#else
    Q_UNUSED(wuId) Q_UNUSED(sock) Q_UNUSED(nodeId) return;
    const QString type, params;
#endif
    setWuStatus(wuId, "running", nodeId);
    setNodeStatus(nodeId, "busy");

    if (sock) {
        sendJson(sock, {{"t","wu"}, {"id",wuId}, {"type",type},
                        {"params", QJsonDocument::fromJson(params.toUtf8()).object()}});
    } else {
        /* Esecuzione locale diretta (self-worker) */
        executeLocally(wuId, type,
                       QJsonDocument::fromJson(params.toUtf8()).object());
    }
    appendLog(QString("\xf0\x9f\x9a\x80  Dispatch WU %1 (%2) \xe2\x86\x92 %3")
              .arg(wuId.left(8), type, nodeId.left(8)));
}

/* ══════════════════════════════════════════════════════════════
   Rete — Client (Worker)
   ══════════════════════════════════════════════════════════════ */

void SciComputePage::connectToCoord(const QString& host, int port)
{
    if (m_selfSock) disconnectFromCoord();
    m_selfSock = new QTcpSocket(this);
    connect(m_selfSock, &QTcpSocket::readyRead,    this, &SciComputePage::onSelfReadyRead);
    connect(m_selfSock, &QTcpSocket::disconnected, this, &SciComputePage::onSelfDisconnected);
    connect(m_selfSock, &QAbstractSocket::connected, this, [this] {
        appendLog("\xf0\x9f\x94\x97  Connesso al coordinator. Invio capabilities...");
    });
    m_selfSock->connectToHost(host, port);
}

void SciComputePage::disconnectFromCoord()
{
    if (m_selfSock) {
        m_selfSock->disconnectFromHost();
        m_selfSock->deleteLater();
        m_selfSock = nullptr;
    }
    m_selfBuf.clear();
}

void SciComputePage::onSelfReadyRead()
{
    m_selfBuf += m_selfSock->readAll();
    while (true) {
        const int nl = m_selfBuf.indexOf('\n');
        if (nl < 0) break;
        const QByteArray line = m_selfBuf.left(nl);
        m_selfBuf.remove(0, nl + 1);
        const QJsonObject msg = QJsonDocument::fromJson(line).object();
        if (!msg.isEmpty()) handleCoordMessage(msg);
    }
}

void SciComputePage::onSelfDisconnected()
{
    appendLog("\xf0\x9f\x94\xb4  Disconnesso dal coordinator.");
    if (m_selfSock) { m_selfSock->deleteLater(); m_selfSock = nullptr; }
    m_selfBuf.clear();
    updateStatus();
}

void SciComputePage::handleCoordMessage(const QJsonObject& msg)
{
    const QString t = msg["t"].toString();

    if (t == "hello") {
        /* Server ci ha inviato il suo hello con token → rispondiamo con caps */
        const QString serverToken = msg["token"].toString();
        if (serverToken != m_token && !m_isCoord) {
            appendLog("\xe2\x9a\xa0  Token non corrisponde — non mi connetto.");
            disconnectFromCoord();
            return;
        }
        const QJsonObject caps = detectCapabilities();
        QSettings s;
        const QString name = s.value("hostname",
                                     QSysInfo::machineHostName()).toString();
        sendJson(m_selfSock, {{"t","caps"}, {"token", m_token},
                              {"node_id", m_myNodeId}, {"name", name},
                              {"caps", caps}});
        appendLog(QString("\xf0\x9f\x94\x8d  Capabilities inviate: CPU:%1 RAM:%2GB GPU:%3")
                  .arg(caps["cpu"].toInt())
                  .arg(caps["ram_gb"].toInt())
                  .arg(caps["gpu"].toString().isEmpty() ? "no" : caps["gpu"].toString()));

    } else if (t == "wu") {
        /* Il coordinator ci assegna un Work Unit */
        const QString wuId   = msg["id"].toString();
        const QString type   = msg["type"].toString();
        const QJsonObject ps = msg["params"].toObject();
        appendLog(QString("\xf0\x9f\x94\xa7  WU ricevuta: %1 (tipo: %2)").arg(wuId.left(8), type));
        executeLocally(wuId, type, ps);

    } else if (t == "ping") {
        sendJson(m_selfSock, {{"t","pong"}, {"node_id", m_myNodeId}});

    } else if (t == "file_data") {
        /* Coordinator ci ha inviato un file di input */
        const QString name = msg["name"].toString();
        const QByteArray raw = QByteArray::fromBase64(
            msg["data"].toString().toLatin1());
        const QString saveTo = m_pendingFileSaves.take(name);
        if (!saveTo.isEmpty() && !raw.isEmpty()) {
            QFile f(saveTo);
            if (f.open(QIODevice::WriteOnly)) {
                f.write(raw);
                appendLog(QString("\xf0\x9f\x93\xa5  File ricevuto: %1 \xe2\x86\x92 %2")
                          .arg(name, saveTo));
            }
        }

    } else if (t == "file_error") {
        appendLog("\xe2\x9d\x8c  File broker errore: " + msg["msg"].toString());
        LogBus::post("\xe2\x9d\x8c SciCompute: File broker errore: " + msg["msg"].toString());
    }
}

/* ══════════════════════════════════════════════════════════════
   Esecuzione locale dei task
   ══════════════════════════════════════════════════════════════ */

/* ── Parsing progress da stderr ─────────────────────────────── */
static QPair<int,QString> parseStderrProgress(const QByteArray& buf)
{
    if (buf.isEmpty()) return {-1, {}};
    const QString text = QString::fromUtf8(buf).trimmed();

    /* Cerca "45%" o "45.2%" — tqdm, progress bar, GROMACS percentuale */
    static const QRegularExpression rePct(R"(\b(\d{1,3})(?:\.\d+)?\s*%)");
    const auto mp = rePct.match(text);
    if (mp.hasMatch()) {
        const int pct = qBound(0, mp.captured(1).toInt(), 100);
        const auto lines = text.split('\n', Qt::SkipEmptyParts);
        return {pct, lines.last().trimmed().left(80)};
    }

    /* Cerca "Step X/N" o "iter X/N" — GROMACS, BLAST iterations */
    static const QRegularExpression reStepN(
        R"(\b(?:step|Step|iter)\s+(\d+)\s*/\s*(\d+))",
        QRegularExpression::CaseInsensitiveOption);
    const auto ms = reStepN.match(text);
    if (ms.hasMatch()) {
        const int cur = ms.captured(1).toInt();
        const int tot = ms.captured(2).toInt();
        if (tot > 0) return {qBound(0, cur * 100 / tot, 100),
                             QString("step %1/%2").arg(cur).arg(tot)};
    }

    /* Ultima riga non vuota come messaggio, senza percentuale */
    const auto lines = text.split('\n', Qt::SkipEmptyParts);
    return {-1, lines.last().trimmed().left(80)};
}

bool SciComputePage::isSafeToolName(const QString& name)
{
    /* Solo nome binario — no path, no shell metachar */
    static const QRegularExpression reOk(R"(^[a-zA-Z0-9_\-\.]+$)");
    return reOk.match(name).hasMatch() && !name.contains("..");
}

bool SciComputePage::isSafePath(const QString& path)
{
    if (path.contains(".."))           return false;
    if (path.startsWith("/etc/"))      return false;
    if (path.startsWith("/root/"))     return false;
    if (path.contains("/.ssh/"))       return false;
    if (path.contains("/.gnupg/"))     return false;
    if (path.contains("/proc/"))       return false;
    if (path.contains("/sys/"))        return false;
    return true;
}

void SciComputePage::executeLocally(const QString& wuId, const QString& type,
                                     const QJsonObject& params)
{
    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::SeparateChannels);

    bool        needsStdin = false;
    QString     stdinData;
    bool        validParams = true;
    QString     errMsg;

    /* ── Configura comando per ogni tipo ── */
    if (type == "blastn" || type == "blastp" || type == "blastx" || type == "tblastn") {
        const QString query  = params["query"].toString();
        const QString db     = params["db"].toString();
        const QString evalue = params["evalue"].toString("0.001");
        const QString outfmt = params["outfmt"].toString("6");
        const int maxHits    = params["max_hits"].toInt(10);

        if (query.isEmpty() || db.isEmpty()) { errMsg = "query e db sono obbligatori"; validParams = false; }
        else {
            const QString tmpQ = QDir::tempPath() + "/sci_q_" + wuId.left(8) + ".fa";
            QFile f(tmpQ);
            if (!f.open(QIODevice::WriteOnly)) {
                errMsg = "Impossibile creare file temporaneo: " + f.errorString();
                validParams = false;
            } else {
                f.write(query.toUtf8()); f.close();
            }
            proc->setProperty("tmpFile", tmpQ);
            proc->start(type, {"-query", tmpQ, "-db", db,
                               "-evalue", evalue, "-outfmt", outfmt,
                               "-max_target_seqs", QString::number(maxHits)});
        }
    } else if (type == "r_script") {
        const QString script = params["script"].toString();
        if (script.isEmpty()) { errMsg = "script vuoto"; validParams = false; }
        else { needsStdin = true; stdinData = script;
               proc->start("Rscript", {"--vanilla", "-"}); }
    } else if (type == "python_scipy" || type == "monte_carlo") {
        const QString script = params["script"].toString();
        if (script.isEmpty()) { errMsg = "script vuoto"; validParams = false; }
        else { needsStdin = true; stdinData = script;
               proc->start(P::findPython(), {"-"}); }
    } else if (type == "gromacs_min" || type == "gromacs_md") {
        const QString tpr = params["tpr_file"].toString();
        const QString out = params["output_dir"].toString(
            QDir::tempPath() + "/gromacs_" + wuId.left(8));
        if (!isSafePath(tpr)) { errMsg = "tpr_file non autorizzato"; validParams = false; }
        else { QDir().mkpath(out);
               proc->start("gmx", {"mdrun", "-v", "-deffnm",
                                   out + "/run", "-s", tpr}); }
    } else if (type == "fastqc") {
        const QString in  = params["fastq_file"].toString();
        const QString out = params["output_dir"].toString(QDir::tempPath());
        if (!isSafePath(in)) { errMsg = "path non autorizzato"; validParams = false; }
        else { QDir().mkpath(out);
               proc->start("fastqc", {in, "-o", out}); }
    } else if (type == "autodock") {
        const QString rec = params["receptor"].toString();
        const QString lig = params["ligand"].toString();
        if (!isSafePath(rec) || !isSafePath(lig)) { errMsg = "path non autorizzato"; validParams = false; }
        else {
            const int sz = params["box_size"].toInt(20);
            proc->start("vina", {"--receptor", rec, "--ligand", lig,
                "--center_x", QString::number(params["center_x"].toDouble()),
                "--center_y", QString::number(params["center_y"].toDouble()),
                "--center_z", QString::number(params["center_z"].toDouble()),
                "--size_x", QString::number(sz),
                "--size_y", QString::number(sz),
                "--size_z", QString::number(sz)});
        }
    } else if (type == "bwa_align") {
        const QString ref  = params["reference"].toString();
        const QString rds  = params["reads"].toString();
        const QString outf = params["output"].toString(
            QDir::tempPath() + "/bwa_" + wuId.left(8) + ".sam");
        if (!isSafePath(ref) || !isSafePath(rds)) { errMsg = "path non autorizzato"; validParams = false; }
        else proc->start("bwa", {"mem", ref, rds, "-o", outf});
    } else if (type == "samtools") {
        const QString in  = params["input"].toString();
        const QString op  = params["operation"].toString("view");
        const QString out = params["output"].toString();
        if (!isSafePath(in) || (!out.isEmpty() && !isSafePath(out))) {
            errMsg = "path non autorizzato"; validParams = false;
        } else {
            QStringList args = op.split(' ', Qt::SkipEmptyParts);
            if (!out.isEmpty()) { args << "-o"; args << out; }
            args << in;
            proc->start("samtools", args);
        }
    } else if (type == "llm_sci_analysis") {
        /* LLM scientifico via Ollama (locale o remoto).
           Chiama l'API /api/generate con il modello scelto.
           Usa urllib stdlib — nessuna dipendenza extra.             */
        const QString model   = params["model"].toString(m_sciLlmModel);
        const QString system  = params["system"].toString(
            "Sei un assistente scientifico esperto. Analizza i dati e fornisci "
            "interpretazioni dettagliate e suggerimenti per esperimenti futuri.");
        const QString prompt  = params["prompt"].toString();
        const QString host    = params["ollama_host"].toString("http://localhost:11434");
        const double  temp    = params["temperature"].toDouble(0.3);

        if (prompt.isEmpty()) { errMsg = "prompt vuoto"; validParams = false; }
        else {
            /* Escaping delle virgolette nei parametri per il JSON Python inline */
            auto esc = [](QString s) {
                return s.replace('\\', "\\\\").replace('"', "\\\"")
                        .replace('\n', "\\n").replace('\r', "");
            };
            const QString script = QString(
                "import urllib.request, json, sys\n"
                "url = '%1/api/generate'\n"
                "payload = json.dumps({"
                "  'model': '%2', "
                "  'system': \"%3\", "
                "  'prompt': \"%4\", "
                "  'stream': False, "
                "  'options': {'temperature': %5}"
                "}).encode()\n"
                "req = urllib.request.Request(url, payload, "
                "  {'Content-Type':'application/json'})\n"
                "try:\n"
                "    with urllib.request.urlopen(req, timeout=300) as r:\n"
                "        data = json.loads(r.read())\n"
                "        print(data.get('response', ''))\n"
                "        ctx = data.get('context', [])\n"
                "        if ctx: print(f'\\n[Token usati: {data.get(\"eval_count\",0)}]')\n"
                "except Exception as e:\n"
                "    print(f'ERRORE: {e}', file=sys.stderr)\n"
                "    sys.exit(1)\n"
            ).arg(host, esc(model), esc(system), esc(prompt))
             .arg(temp, 0, 'f', 2);

            needsStdin = true;
            stdinData  = script;
            proc->start(P::findPython(), {"-"});
        }

    } else if (type == "esmfold_api") {
        /* ESMFold API Meta — usa urllib stdlib Python (nessuna dipendenza extra)
           Limite: ~400 aa. Per sequenze più lunghe usare AlphaFold DB o esmfold_local. */
        QString seq = params["sequence"].toString().trimmed();
        /* Rimuovi header FASTA e spazi */
        QStringList seqLines;
        for (const auto& ln : seq.split('\n'))
            if (!ln.startsWith('>') && !ln.trimmed().isEmpty())
                seqLines << ln.trimmed();
        seq = seqLines.join("");
        const int maxLen = params["max_length"].toInt(400);
        if (seq.isEmpty()) { errMsg = "sequenza amminoacidica vuota"; validParams = false; }
        else if (seq.size() > maxLen) {
            errMsg = QString("Sequenza troppo lunga (%1 aa, max %2 per API)."
                             " Usa esmfold_local o cerca su AlphaFold DB.")
                     .arg(seq.size()).arg(maxLen);
            validParams = false;
        } else {
            const QString label   = params["label"].toString("protein");
            const QString outPath = QDir::tempPath() + "/esmfold_" + wuId.left(8) + ".pdb";
            /* Escape per embedding sicuro in stringa Python doppi-apici */
            const QString seqSafe = QString(seq)
                .replace("\\", "\\\\").replace("\"", "\\\"")
                .replace("\n", "\\n").replace("\r", "");
            /* Script Python che chiama l'API via urllib (built-in) */
            const QString script =
                "import urllib.request, sys\n"
                "seq = \"" + seqSafe + "\"\n"
                "url = 'https://api.esmatlas.com/foldSequence/v1/pdb/'\n"
                "req = urllib.request.Request(url, seq.encode(), "
                "{'Content-Type':'application/x-www-form-urlencoded'})\n"
                "try:\n"
                "    with urllib.request.urlopen(req, timeout=180) as r:\n"
                "        pdb = r.read().decode()\n"
                "    with open('" + outPath + "', 'w') as f:\n"
                "        f.write(pdb)\n"
                "    print(pdb)\n"
                "except Exception as e:\n"
                "    print(f'ERROR: {e}', file=sys.stderr); sys.exit(1)\n";
            needsStdin = true;
            stdinData  = script;
            proc->start(P::findPython(), {"-"});
        }

    } else if (type == "esmfold_local") {
        /* ESMFold locale — richiede 'pip install fair-esm' e ~20GB RAM */
        QString seq = params["sequence"].toString().trimmed();
        QStringList seqLines;
        for (const auto& ln : seq.split('\n'))
            if (!ln.startsWith('>') && !ln.trimmed().isEmpty())
                seqLines << ln.trimmed();
        seq = seqLines.join("");
        /* outPath sempre generato internamente: ignoriamo params["output_pdb"]
           per evitare path traversal / single-quote injection nel codice Python. */
        const QString outPath = QDir::tempPath() + "/esmfold_local_" + wuId.left(8) + ".pdb";
        if (seq.isEmpty()) { errMsg = "sequenza vuota"; validParams = false; }
        else {
            const QString seqSafe = QString(seq)
                .replace("\\", "\\\\").replace("\"", "\\\"")
                .replace("\n", "\\n").replace("\r", "");
            const QString script =
                "import esm, torch, sys\n"
                "model = esm.pretrained.esmfold_v1()\n"
                "model.eval()\n"
                "if torch.cuda.is_available(): model = model.cuda()\n"
                "with torch.no_grad():\n"
                "    pdb = model.infer_pdb(\"" + seqSafe + "\")\n"
                "with open('" + outPath + "', 'w') as f:\n"
                "    f.write(pdb)\n"
                "print(pdb)\n";
            needsStdin = true;
            stdinData  = script;
            proc->start(P::findPython(), {"-"});
        }

    } else if (type == "sds_editing") {
        /* Pipeline di STUDIO (non clinica) per la Sindrome di Shwachman-Diamond.
           Esegue uno dei moduli in Tools/sds_editing/ come Work Unit distribuibile.
           Allowlist esplicita: il worker non deve poter eseguire path arbitrari. */
        static const QStringList kSdsScripts = {
            "run_all.py", "bruteforce_sds_stable.py", "hybrid_optimizer.py",
            "llm_meta_analyst.py", "sds_analyzer.py", "safe_margins.py",
            "sharding_master.py"
        };
        const QString script = params["script"].toString("run_all.py");
        const QString dir    = P::root() + "/Tools/sds_editing";
        if (!kSdsScripts.contains(script)) {
            errMsg = "script SDS non consentito (usa un modulo di Tools/sds_editing)";
            validParams = false;
        } else if (!QFile::exists(dir + "/" + script)) {
            errMsg = "script non trovato: " + dir + "/" + script;
            validParams = false;
        } else {
            proc->setWorkingDirectory(dir);
            proc->start(P::findPython(), {script});
        }

    } else if (type == "custom") {
        const QString prog = params["program"].toString();
        if (!isSafeToolName(prog)) { errMsg = "program non valido — usa solo nome binario"; validParams = false; }
        else {
            QStringList args;
            for (const auto& a : params["args"].toArray()) args << a.toString();
            const QString stdinCustom = params["stdin"].toString();
            if (!stdinCustom.isEmpty()) { needsStdin = true; stdinData = stdinCustom; }
            proc->start(prog, args);
        }
    } else {
        errMsg = "Tipo non supportato su questo nodo: " + type;
        validParams = false;
    }

    if (!validParams) {
        proc->deleteLater();
        handleLocalResult(wuId, false, {}, {}, errMsg);
        return;
    }

    if (needsStdin) {
        proc->write(stdinData.toUtf8());
        proc->closeWriteChannel();
    }

    /* ── Progress streaming via stderr (throttle 2s verso coordinator) ── */
    auto stderrBuf = QSharedPointer<QByteArray>::create();
    connect(proc, &QProcess::errorOccurred, this,
        [this, proc, wuId](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart) {
                handleLocalResult(wuId, false, QString(), QString(),
                                  "processo non avviato: " + proc->program());
                proc->deleteLater();
            }
        });
    connect(proc, &QProcess::readyReadStandardError, this,
            [this, proc, wuId, stderrBuf] {
        *stderrBuf += proc->readAllStandardError();
        /* Tieni solo gli ultimi 2 KB per non consumare RAM */
        if (stderrBuf->size() > 4096)
            *stderrBuf = stderrBuf->right(2048);
        const auto [pct, msg] = parseStderrProgress(*stderrBuf);
        if (!msg.isEmpty()) updateWuProgress(wuId, pct, msg);
    });

    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, wuId, stderrBuf](int exitCode, QProcess::ExitStatus) {
        /* Rimuovi eventuale file temporaneo query BLAST */
        const QString tmp = proc->property("tmpFile").toString();
        if (!tmp.isEmpty()) QFile::remove(tmp);

        /* stdout = risultato; stderr allegato solo in caso di errore */
        const QString stdOut = QString::fromUtf8(
            proc->readAllStandardOutput()).left(65536);
        const QString stdErr = QString::fromUtf8(
            proc->readAllStandardError()).left(8192);
        const bool ok = (exitCode == 0);
        const QString output = ok ? stdOut
            : (stdOut + (stdErr.isEmpty() ? "" : "\n--- stderr ---\n" + stdErr)).left(65536);
        const QString hash = ok ? QString::fromLatin1(
            QCryptographicHash::hash(output.toUtf8(),
                QCryptographicHash::Sha256).toHex()) : QString();

        m_wuProgress.remove(wuId);
        m_lastProgressMs.remove(wuId);
        handleLocalResult(wuId, ok, output, hash,
                          ok ? QString() : "exit code " + QString::number(exitCode));
        proc->deleteLater();
    });
}

void SciComputePage::handleLocalResult(const QString& wuId, bool ok,
                                        const QString& output, const QString& hash,
                                        const QString& err)
{
    saveResult(wuId, m_myNodeId, output, hash);

#ifdef HAVE_QT_SQL
    QSqlQuery rq(QSqlDatabase::database(m_connName));
    rq.prepare("SELECT replicas FROM work_units WHERE id=?");
    rq.addBindValue(wuId); SQL_EXEC(rq);
    const int replicas = rq.next() ? rq.value(0).toInt() : 1;
#else
    const int replicas = 1;
#endif

    if (replicas <= 1)
        setWuStatus(wuId, ok ? "done" : "error", m_myNodeId, err);
    else
        checkQuorum(wuId, replicas);

    setNodeStatus(m_myNodeId, "idle");

#ifdef HAVE_QT_SQL
    /* Credit counter locale */
    {
        QSqlQuery tq(QSqlDatabase::database(m_connName));
        tq.prepare("SELECT started_at FROM work_units WHERE id=?");
        tq.addBindValue(wuId); SQL_EXEC(tq);
        const qint64 startedAt = tq.next() ? tq.value(0).toLongLong() : 0LL;
        const qint64 nowSec    = QDateTime::currentSecsSinceEpoch();
        const int    cpuSecs   = startedAt > 0
                                 ? (int)qMax(0LL, nowSec - startedAt) : 0;
        QSqlQuery cq(QSqlDatabase::database(m_connName));
        if (ok) {
            cq.prepare("UPDATE sci_nodes SET wu_done=wu_done+1,"
                       " cpu_seconds_total=cpu_seconds_total+?,"
                       " last_wu_completed=? WHERE id=?");
            cq.addBindValue(cpuSecs); cq.addBindValue(nowSec); cq.addBindValue(m_myNodeId);
        } else {
            cq.prepare("UPDATE sci_nodes SET wu_error=wu_error+1 WHERE id=?");
            cq.addBindValue(m_myNodeId);
        }
        SQL_EXEC(cq);
    }
#endif

    appendLog(QString("%1  WU %2 completata localmente.")
              .arg(ok ? "\xe2\x9c\x85" : "\xe2\x9d\x8c")
              .arg(wuId.left(8)));
    if (!ok) LogBus::post("\xe2\x9d\x8c SciCompute: WU " + wuId.left(8) + " fallita localmente.");
    refreshWuTable();
    if (wuId == m_selectedWuId) refreshResults(wuId);

    /* Se questo nodo era anche self-worker via socket, informa il coordinator */
    if (m_selfSock && m_selfSock->state() == QAbstractSocket::ConnectedState && !m_isCoord) {
        sendJson(m_selfSock, {
            {"t","result"}, {"wu_id",wuId},
            {"status", ok ? "done" : "error"},
            {"output", output.left(50000)}, {"hash",hash},
            {"error_msg", err}
        });
    }
}

/* ══════════════════════════════════════════════════════════════
   Capability detection
   ══════════════════════════════════════════════════════════════ */

/* ══════════════════════════════════════════════════════════════
   File Broker — scambio file input/output tra coordinator e worker

   Protocollo (stesso canale TCP del task dispatch):
     Coordinator → Worker:  {"t":"file_data","name":"input.fasta","data":"BASE64","size":N}
     Worker → Coordinator:  {"t":"file_req","name":"input.fasta"}
     Worker → Coordinator:  {"t":"file_result","wu_id":"...","filename":"out.pdb","data":"BASE64"}

   Limite per messaggio: 10 MB base64 (~7.5 MB binario).
   Per file più grandi usare path condiviso (NFS/SFTP su VPN).
   ══════════════════════════════════════════════════════════════ */

void SciComputePage::registerFile(const QString& logicalName,
                                   const QString& localPath)
{
    m_fileRegistry[logicalName] = localPath;
    appendLog(QString("\xf0\x9f\x93\x82  File registrato: %1 \xe2\x86\x92 %2")
              .arg(logicalName, localPath));
}

void SciComputePage::handleFileRequest(QTcpSocket* s, const QString& name)
{
    if (!m_fileRegistry.contains(name)) {
        sendJson(s, {{"t","file_error"}, {"name",name},
                     {"msg","file non registrato sul coordinator"}});
        return;
    }
    const QString path = m_fileRegistry[name];
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        sendJson(s, {{"t","file_error"}, {"name",name},
                     {"msg","impossibile leggere: " + path}});
        return;
    }
    const QByteArray raw = f.readAll();
    if (raw.size() > 10 * 1024 * 1024) {
        sendJson(s, {{"t","file_error"}, {"name",name},
                     {"msg","file > 10 MB: usa path condiviso su VPN"}});
        return;
    }
    sendJson(s, {
        {"t",    "file_data"},
        {"name", name},
        {"data", QString::fromLatin1(raw.toBase64())},
        {"size", (int)raw.size()}
    });
    appendLog(QString("\xf0\x9f\x93\xa4  Inviato file '%1' (%2 KB) al worker")
              .arg(name).arg(raw.size() / 1024));
}

void SciComputePage::requestFile(const QString& name, const QString& saveTo)
{
    m_pendingFileSaves[name] = saveTo;
    sendJson(m_selfSock, {{"t","file_req"}, {"name", name}});
}

void SciComputePage::sendFileResult(const QString& wuId,
                                     const QString& filename,
                                     const QString& localPath)
{
    QFile f(localPath);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QByteArray raw = f.readAll();
    sendJson(m_selfSock, {
        {"t",        "file_result"},
        {"wu_id",    wuId},
        {"filename", filename},
        {"data",     QString::fromLatin1(raw.toBase64())},
        {"size",     (int)raw.size()}
    });
}

/* Gestione messaggi file nel dispatcher lato server */
/* (chiamato da handleWorkerMessage quando t == "file_req" o "file_result") */

/* Gestione messaggi file nel dispatcher lato client */
/* (chiamato da handleCoordMessage quando t == "file_data") */

QJsonObject SciComputePage::detectCapabilities()
{
    QJsonObject caps;
    caps["cpu"] = QThread::idealThreadCount();

    /* RAM */
    int ramGb = 4;
    QFile memFile("/proc/meminfo");
    if (memFile.open(QIODevice::ReadOnly)) {
        static const QRegularExpression re(R"(MemTotal:\s+(\d+)\s+kB)");
        const auto m = re.match(QString::fromUtf8(memFile.readAll()));
        if (m.hasMatch()) ramGb = qMax(1, (int)(m.captured(1).toLongLong() / (1024*1024)));
    }
    caps["ram_gb"] = ramGb;

    /* GPU */
    const QString nv = ProcHelper::readOutput("nvidia-smi",
        {"--query-gpu=name", "--format=csv,noheader"}, 2000)
        .section('\n', 0, 0).trimmed();
    caps["gpu"] = nv.isEmpty() ? QString() : "NVIDIA: " + nv;

    /* Tool scientifici disponibili */
    static const QStringList kCheckTools = {
        "blastn","blastp","blastx","tblastn",
        "Rscript","python3","gmx","gromacs",
        "fastqc","bwa","samtools","bowtie2","hisat2",
        "vina","muscle","clustalw","bedtools","vcftools",
        "java","perl","nextflow","snakemake"
    };
    QJsonArray toolsArr;
    for (const QString& t : kCheckTools)
        if (ProcHelper::isAvailable(t)) toolsArr.append(t);
    caps["tools"] = toolsArr;

    return caps;
}

/* ══════════════════════════════════════════════════════════════
   Timer — Dispatch periodico e Heartbeat
   ══════════════════════════════════════════════════════════════ */

void SciComputePage::onDispatchTimer()
{
    if (!m_isCoord) return;
#ifdef HAVE_QT_SQL
    /* Trova WU pending ordinate per priorità */
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    QSqlQuery pendQ(db);
    pendQ.exec("SELECT id,type,depends_on FROM work_units WHERE status='pending'"
               " ORDER BY priority DESC,created_at ASC LIMIT 20");

    while (pendQ.next()) {
        const QString wuId    = pendQ.value(0).toString();
        const QString type    = pendQ.value(1).toString();
        const QString depsRaw = pendQ.value(2).toString();

        /* Controlla dipendenze: tutte le WU precedenti devono essere done/validated */
        if (!depsRaw.isEmpty()) {
            bool allDone = true;
            for (const QString& depId : depsRaw.split(',', Qt::SkipEmptyParts)) {
                QSqlQuery dq(db);
                dq.prepare("SELECT status FROM work_units WHERE id=?");
                dq.addBindValue(depId.trimmed()); SQL_EXEC(dq);
                if (!dq.next()) { allDone = false; break; }
                const QString st = dq.value(0).toString();
                if (st != "done" && st != "validated") { allDone = false; break; }
            }
            if (!allDone) continue;   /* dipendenze non soddisfatte — salta */
        }

        /* Trova nodo idle con il tool richiesto */
        const SciTaskType* taskDef = nullptr;
        for (const auto& td : taskTypes())
            if (td.id == type) { taskDef = &td; break; }

        const QString tool = taskDef ? taskDef->toolRequired : QString();

        /* Prima prova nodi remoti connessi */
        bool dispatched = false;
        for (auto it = m_nodeSock.begin(); it != m_nodeSock.end(); ++it) {
            const QString nodeId = it.key();
            if (nodeId == m_myNodeId) continue;  /* skip self, gestito dopo */

            QSqlQuery nodeQ(db);
            nodeQ.prepare("SELECT status,tools FROM sci_nodes WHERE id=?");
            nodeQ.addBindValue(nodeId); SQL_EXEC(nodeQ);
            if (!nodeQ.next()) continue;
            if (nodeQ.value(0).toString() != "idle") continue;

            if (!tool.isEmpty()) {
                const QString toolsJson = nodeQ.value(1).toString();
                if (!toolsJson.contains(tool)) continue;
            }
            dispatchTo(wuId, it.value(), nodeId);
            dispatched = true;
            break;
        }

        /* Fallback: esecuzione locale */
        if (!dispatched && m_useLocal) {
            QSqlQuery localQ(db);
            localQ.prepare("SELECT status,tools FROM sci_nodes WHERE id=?");
            localQ.addBindValue(m_myNodeId); SQL_EXEC(localQ);
            if (localQ.next() && localQ.value(0).toString() == "idle") {
                if (tool.isEmpty() || localQ.value(1).toString().contains(tool)) {
                    dispatchTo(wuId, nullptr, m_myNodeId);
                }
            }
        }
    }
#endif
}

void SciComputePage::onHeartbeatTimer()
{
    /* Invia ping a tutti i worker connessi */
    for (auto* s : m_srvBufs.keys())
        sendJson(s, {{"t","ping"}});

    markOfflineNodes(60000);
    refreshNodeTable();
}

/* ══════════════════════════════════════════════════════════════
   Slot UI
   ══════════════════════════════════════════════════════════════ */

void SciComputePage::onStartStopClicked()
{
    if (!m_isCoord || !m_server || !m_server->isListening())
        startServer();
    else
        stopServer();
}

void SciComputePage::onConnectClicked()
{
    const QString host = m_coordHostEdit ? m_coordHostEdit->text().trimmed() : "127.0.0.1";
    if (host.isEmpty()) return;
    m_isCoord = false;
    if (m_tokenEdit) {
        const QString edited = m_tokenEdit->text().trimmed();
        if (!edited.isEmpty() && edited != m_token) {
            m_token = edited;
            LanServer::saveSecret("sci_compute_token", m_token);
        }
    }
    connectToCoord(host, P::kSciComputePort);
    appendLog(QString("\xf0\x9f\x94\x97  Connessione a %1:%2...").arg(host).arg(P::kSciComputePort));
}

void SciComputePage::onAddWuClicked()
{
    if (!m_typeCombo || !m_paramsEdit) return;
    const QString type     = m_typeCombo->currentData().toString();
    const QString label    = m_labelEdit ? m_labelEdit->text().trimmed() : type;
    const int     priority = m_priorityCmb ? m_priorityCmb->currentData().toInt() : 1;
    const int     replicas = m_replicasCmb ? m_replicasCmb->currentData().toInt() : 1;

    if (type.isEmpty()) return;

    /* Seleziona il modello LLM scelto e aggiornalo in m_sciLlmModel */
    if (m_wuLlmCombo && !m_wuLlmCombo->currentText().isEmpty())
        m_sciLlmModel = m_wuLlmCombo->currentText().trimmed();

    /* Inietta "model" nei params se il task è di tipo LLM */
    QString params = m_paramsEdit->toPlainText().trimmed();
    const bool isLlmTask = type.contains("llm") || type.contains("ai_analysis")
                        || type == "llm_agent" || type == "llm_sci_analysis";
    if (isLlmTask) {
        QJsonParseError jerr2;
        QJsonObject obj = QJsonDocument::fromJson(params.toUtf8(), &jerr2).object();
        if (jerr2.error == QJsonParseError::NoError) {
            obj["model"] = m_sciLlmModel;
            params = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));
            m_paramsEdit->setPlainText(params);
        }
    }

    /* Validazione JSON params */
    QJsonParseError jerr;
    QJsonDocument::fromJson(params.toUtf8(), &jerr);
    if (jerr.error != QJsonParseError::NoError) {
        appendLog("\xe2\x9a\xa0  Parametri JSON non validi: " + jerr.errorString());
        return;
    }

    const QString id = insertWu(type, label.isEmpty() ? type : label,
                                params, priority, replicas);
    appendLog(QString("\xe2\x9e\x95  WU aggiunta: %1 (%2) — id %3")
              .arg(label, type, id.left(8)));
    refreshWuTable();
}

void SciComputePage::onFetchLlmModels()
{
    if (!m_wuLlmCombo) return;
    auto* nam = new QNetworkAccessManager(this);
    connect(nam, &QNetworkAccessManager::finished,
            this, &SciComputePage::onFetchLlmModelsFinished);
    nam->get(QNetworkRequest(QUrl("http://127.0.0.1:11434/api/tags")));
}

void SciComputePage::onFetchLlmModelsFinished(QNetworkReply* r)
{
    auto* nam = qobject_cast<QNetworkAccessManager*>(sender());
    if (nam) nam->deleteLater();
    if (r->error() != QNetworkReply::NoError) {
        appendLog("\xe2\x9a\xa0  Ollama non raggiungibile (" + r->errorString() + ")");
        return;
    }
    const QJsonArray models =
        QJsonDocument::fromJson(r->readAll()).object()["models"].toArray();
    if (models.isEmpty() || !m_wuLlmCombo) return;
    const QString prev = m_wuLlmCombo->currentText();
    m_wuLlmCombo->clear();
    for (const auto& v : models)
        m_wuLlmCombo->addItem(v.toObject()["name"].toString());
    const int idx = m_wuLlmCombo->findText(prev);
    m_wuLlmCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    appendLog(QString("\xf0\x9f\xa4\x96  %1 modelli Ollama caricati").arg(models.size()));
}

void SciComputePage::onTypeComboChanged(int idx)
{
    if (!m_typeCombo || !m_paramsEdit || idx < 0) return;
    const QString typeId = m_typeCombo->currentData().toString();
    for (const auto& t : taskTypes()) {
        if (t.id == typeId) {
            m_paramsEdit->setPlainText(t.paramsTemplate);
            break;
        }
    }
}

void SciComputePage::onWuTableRowClicked(int row)
{
    if (!m_wuTable || row < 0) return;
    m_selectedWuId = m_wuTable->item(row, 0)
                     ? m_wuTable->item(row, 0)->data(Qt::UserRole).toString()
                     : QString();
    refreshResults(m_selectedWuId);
}

void SciComputePage::onDeleteWuClicked()
{
    if (m_selectedWuId.isEmpty()) return;
#ifdef HAVE_QT_SQL
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare("DELETE FROM work_units WHERE id=? AND status IN ('pending','done','error','validated','conflict')");
    q.addBindValue(m_selectedWuId); SQL_EXEC(q);
    q.prepare("DELETE FROM wu_results WHERE wu_id=?");
    q.addBindValue(m_selectedWuId); SQL_EXEC(q);
#endif
    appendLog("Rimossa WU " + m_selectedWuId.left(8));
    m_selectedWuId.clear();
    refreshWuTable();
    refreshResults();
}

/* ══════════════════════════════════════════════════════════════
   onWuContextMenu — menu contestuale sulla tabella Work Units
   ══════════════════════════════════════════════════════════════ */
void SciComputePage::onWuContextMenu(const QPoint& pos)
{
    if (!m_wuTable) return;
    const int row = m_wuTable->rowAt(pos.y());
    if (row < 0) return;

    auto* it0 = m_wuTable->item(row, 0);
    const QString wuId = it0 ? it0->data(Qt::UserRole).toString() : QString();
    if (wuId.isEmpty()) return;

    const QString status = m_wuTable->item(row, 3)
                           ? m_wuTable->item(row, 3)->text() : QString();
    const bool canDelete = !status.contains("running");
    const bool canRestart = (status == "done" || status == "error"
                             || status == "validated" || status == "conflict"
                             || status.contains('%'));

    QMenu menu(this);
    auto* actDelete  = menu.addAction(tr("\xf0\x9f\x97\x91  Elimina WU"));
    auto* actRestart = menu.addAction(tr("\xf0\x9f\x94\x84  Riavvia WU"));
    actDelete->setEnabled(canDelete);
    actRestart->setEnabled(canRestart);

    auto* chosen = menu.exec(m_wuTable->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actDelete) {
        m_selectedWuId = wuId;
        onDeleteWuClicked();
    } else if (chosen == actRestart) {
#ifdef HAVE_QT_SQL
        QSqlQuery q(QSqlDatabase::database(m_connName));
        q.prepare("UPDATE work_units SET status='pending', node_id=NULL, "
                  "started_at=NULL, finished_at=NULL WHERE id=?");
        q.addBindValue(wuId);
        SQL_EXEC(q);
        q.prepare("DELETE FROM wu_results WHERE wu_id=?");
        q.addBindValue(wuId);
        SQL_EXEC(q);
#endif
        appendLog("WU " + wuId.left(8) + " rimessa in coda");
        refreshWuTable();
    }
}

/* ══════════════════════════════════════════════════════════════
   onNodeContextMenu — menu contestuale sulla tabella Nodi
   ══════════════════════════════════════════════════════════════ */
void SciComputePage::onNodeContextMenu(const QPoint& pos)
{
    if (!m_nodeTable) return;
    const int row = m_nodeTable->rowAt(pos.y());
    if (row < 0) return;

    auto* it0 = m_nodeTable->item(row, 0);
    const QString nodeId = it0 ? it0->data(Qt::UserRole).toString() : QString();
    if (nodeId.isEmpty()) return;

    QMenu menu(this);
    auto* actDisconn = menu.addAction(tr("\xf0\x9f\x94\x8c  Disconnetti nodo"));
    auto* actRemove  = menu.addAction(tr("\xf0\x9f\x97\x91  Rimuovi nodo dal DB"));

    /* Col 1 contiene testo "Nome  [status]" */
    const QString col1 = m_nodeTable->item(row, 1)
                         ? m_nodeTable->item(row, 1)->text() : QString();
    actDisconn->setEnabled(!col1.contains("[offline]"));

    auto* chosen = menu.exec(m_nodeTable->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actDisconn) {
        /* Chiude il socket se connesso */
        if (m_nodeSock.contains(nodeId)) {
            QTcpSocket* s = m_nodeSock.value(nodeId);
            if (s) s->disconnectFromHost();
        }
        setNodeStatus(nodeId, "offline");
        appendLog("Nodo " + nodeId.left(8) + " disconnesso");
        refreshNodeTable();
    } else if (chosen == actRemove) {
        const auto btn = QMessageBox::question(this,
            tr("Rimuovi nodo"),
            tr("Rimuovere il nodo %1 dal database?").arg(nodeId.left(8)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes) return;
        if (m_nodeSock.contains(nodeId)) {
            QTcpSocket* s = m_nodeSock.value(nodeId);
            if (s) s->disconnectFromHost();
        }
#ifdef HAVE_QT_SQL
        QSqlQuery q(QSqlDatabase::database(m_connName));
        q.prepare("DELETE FROM sci_nodes WHERE id=?");
        q.addBindValue(nodeId);
        SQL_EXEC(q);
#endif
        appendLog("Nodo " + nodeId.left(8) + " rimosso");
        refreshNodeTable();
    }
}

/* ══════════════════════════════════════════════════════════════
   updateWuProgress — aggiorna progress in-memory + tabella WU
   ══════════════════════════════════════════════════════════════ */
void SciComputePage::updateWuProgress(const QString& wuId, int pct, const QString& msg)
{
    m_wuProgress[wuId] = {pct, msg};

    /* Aggiorna cella status direttamente senza query DB */
    if (m_wuTable) {
        for (int r = 0; r < m_wuTable->rowCount(); ++r) {
            auto* it0 = m_wuTable->item(r, 0);
            if (!it0 || it0->data(Qt::UserRole).toString() != wuId) continue;
            auto* it3 = m_wuTable->item(r, 3);
            if (!it3) break;
            it3->setText(pct >= 0
                ? QString("\xf0\x9f\x94\x84 %1%").arg(pct)
                : "\xf0\x9f\x94\x84 running...");
            it3->setForeground(QColor("#3b82f6"));
            break;
        }
    }

    /* Worker remoto: invia progress al coordinator con throttle 2s */
    if (!m_isCoord && m_selfSock &&
        m_selfSock->state() == QAbstractSocket::ConnectedState) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastProgressMs.value(wuId, 0) >= 2000) {
            m_lastProgressMs[wuId] = now;
            sendJson(m_selfSock, {
                {"t",      "progress"},
                {"wu_id",  wuId},
                {"pct",    pct},
                {"msg",    msg}
            });
        }
    }
}

/* ══════════════════════════════════════════════════════════════
   WU Generator da dataset (FASTA / CSV / TXT)
   ══════════════════════════════════════════════════════════════ */

static QStringList parseBatchFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QString content = QString::fromUtf8(f.readAll());
    const QString ext     = QFileInfo(path).suffix().toLower();

    if (ext == "fasta" || ext == "fa" || ext == "fas") {
        QStringList items;
        QString cur;
        for (const auto& line : content.split('\n')) {
            if (line.startsWith('>')) {
                if (!cur.trimmed().isEmpty()) items << cur.trimmed();
                cur = line + '\n';
            } else { cur += line + '\n'; }
        }
        if (!cur.trimmed().isEmpty()) items << cur.trimmed();
        return items;
    } else if (ext == "csv") {
        QStringList items;
        const auto lines = content.split('\n', Qt::SkipEmptyParts);
        for (const auto& ln : lines)
            if (!ln.trimmed().isEmpty()) items << ln.trimmed();
        return items;
    } else {
        QStringList items;
        for (const auto& ln : content.split('\n'))
            if (!ln.trimmed().isEmpty()) items << ln.trimmed();
        return items;
    }
}

void SciComputePage::onGenerateFromFileClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x93\x82  Genera Work Units da file"));
    dlg->resize(dpiScale(700), dpiScale(520));
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto* lay = new QVBoxLayout(dlg);
    lay->setSpacing(dpiScale(8));
    lay->setContentsMargins(dpiScale(12), dpiScale(12), dpiScale(12), dpiScale(12));

    /* File */
    auto* fileRow  = new QHBoxLayout;
    auto* fileEdit = new QLineEdit(dlg);
    fileEdit->setPlaceholderText(tr("File FASTA (.fa/.fasta), CSV (.csv) o TXT (.txt)..."));
    auto* browseBtn = new QPushButton(tr("\xf0\x9f\x93\x81  Sfoglia"), dlg);
    browseBtn->setObjectName("actionBtn");
    fileRow->addWidget(new QLabel(tr("File:"), dlg));
    fileRow->addWidget(fileEdit, 1);
    fileRow->addWidget(browseBtn);
    lay->addLayout(fileRow);

    /* Tipo task */
    auto* typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel(tr("Tipo task:"), dlg));
    auto* typeCombo = new QComboBox(dlg);
    typeCombo->setObjectName("settingCombo");
    for (const auto& t : taskTypes())
        typeCombo->addItem(QString("[%1]  %2").arg(t.domain, t.name), t.id);
    typeRow->addWidget(typeCombo, 1);
    lay->addLayout(typeRow);

    /* Template params con {{INPUT}} */
    lay->addWidget(new QLabel(
        tr("Template parametri JSON \xe2\x80\x94 usa <b>{{INPUT}}</b> dove vuoi i dati di ogni elemento:"), dlg));
    auto* tmplEdit = new QTextEdit(dlg);
    tmplEdit->setObjectName("codeEdit");
    tmplEdit->setMinimumHeight(dpiScale(130));
    lay->addWidget(tmplEdit);

    {
        auto* synHint = new QLabel(
            "<span style='color:gray;font-size:11px;'>"
            "FASTA: ogni record \">Header\\nSEQ\" diventa {{INPUT}}  \xe2\x80\x94  "
            "CSV: ogni riga  \xe2\x80\x94  TXT: ogni riga non vuota  (max 500 WU per batch)"
            "</span>", dlg);
        synHint->setTextFormat(Qt::RichText);
        lay->addWidget(synHint);
    }

    /* Opzioni */
    auto* optRow = new QHBoxLayout;
    optRow->addWidget(new QLabel(tr("Label prefix:"), dlg));
    auto* lblEdit = new QLineEdit(dlg);
    lblEdit->setPlaceholderText(tr("WU da file"));
    lblEdit->setFixedWidth(dpiScale(150));
    optRow->addWidget(lblEdit);
    optRow->addSpacing(dpiScale(10));

    optRow->addWidget(new QLabel(tr("Priorit\xc3\xa0:"), dlg));
    auto* prioCmb = new QComboBox(dlg);
    prioCmb->setObjectName("settingCombo");
    prioCmb->addItem("1 \xe2\x80\x94 Normale", 1);
    prioCmb->addItem("2 \xe2\x80\x94 Alta",    2);
    prioCmb->addItem("3 \xe2\x80\x94 Urgente", 3);
    optRow->addWidget(prioCmb);
    optRow->addSpacing(dpiScale(10));

    optRow->addWidget(new QLabel(tr("Max WU:"), dlg));
    auto* maxSpin = new QSpinBox(dlg);
    maxSpin->setRange(1, 500); maxSpin->setValue(100);
    optRow->addWidget(maxSpin);
    optRow->addStretch(1);
    lay->addLayout(optRow);

    /* Info generazione */
    auto* infoLbl = new QLabel("", dlg);
    infoLbl->setObjectName("hintLabel");
    infoLbl->setTextFormat(Qt::RichText);
    lay->addWidget(infoLbl);

    /* Buttons */
    auto* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    btnBox->button(QDialogButtonBox::Ok)->setText(tr("\xe2\x9e\x95  Genera WU"));
    btnBox->button(QDialogButtonBox::Ok)->setObjectName("primaryBtn");
    lay->addWidget(btnBox);

    /* Pre-carica template */
    auto syncTmpl = [=] {
        const QString tid = typeCombo->currentData().toString();
        for (const auto& t : taskTypes()) {
            if (t.id != tid) continue;
            QString tmpl = t.paramsTemplate;
            if (!tmpl.contains("{{INPUT}}")) {
                /* Inietta placeholder nell'ultimo campo del template */
                const int rb = tmpl.lastIndexOf('}');
                if (rb > 0)
                    tmpl.insert(rb, ",\n  \"batch_input\": \"{{INPUT}}\"");
            }
            tmplEdit->setPlainText(tmpl);
            break;
        }
    };
    syncTmpl();

    QObject::connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     dlg, [=](int){ syncTmpl(); });

    auto updateInfo = [=] {
        if (fileEdit->text().isEmpty()) return;
        const auto items = parseBatchFile(fileEdit->text());
        const int shown  = qMin(items.size(), maxSpin->value());
        infoLbl->setText(QString("\xf0\x9f\x93\x84  Trovati <b>%1</b> elementi"
                                 " \xe2\x80\x94 saranno create <b>%2</b> WU")
                         .arg(items.size()).arg(shown));
    };

    QObject::connect(browseBtn, &QPushButton::clicked, dlg, [=] {
        const QString path = QFileDialog::getOpenFileName(
            dlg, "Seleziona file dataset", QDir::homePath(),
            "Dataset scientifici (*.fasta *.fa *.fas *.csv *.txt);;Tutti (*.*)");
        if (!path.isEmpty()) { fileEdit->setText(path); updateInfo(); }
    });
    QObject::connect(maxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                     dlg, [=](int){ updateInfo(); });

    QObject::connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    QObject::connect(btnBox, &QDialogButtonBox::accepted, dlg, [=] {
        const QString filePath = fileEdit->text().trimmed();
        if (filePath.isEmpty()) {
            QMessageBox::warning(dlg, tr("File mancante"), tr("Seleziona un file dataset."));
            return;
        }
        const auto items = parseBatchFile(filePath);
        if (items.isEmpty()) {
            QMessageBox::warning(dlg, tr("File vuoto"),
                "Nessun elemento trovato nel file.");
            return;
        }
        const QString tmplStr = tmplEdit->toPlainText();
        if (!tmplStr.contains("{{INPUT}}")) {
            QMessageBox::warning(dlg, tr("Template incompleto"),
                "Inserisci {{INPUT}} nel template dove vuoi i dati di ogni elemento.");
            return;
        }
        const QString typeId   = typeCombo->currentData().toString();
        const QString lblPfx   = lblEdit->text().trimmed().isEmpty()
                                 ? typeId : lblEdit->text().trimmed();
        const int priority     = prioCmb->currentData().toInt();
        const int maxWu        = maxSpin->value();
        const QString pipId    = QUuid::createUuid().toString(
                                     QUuid::WithoutBraces).left(8);

        int created = 0, skipped = 0;
        for (int i = 0; i < items.size() && created < maxWu; ++i) {
            const QString item = items[i];
            /* Escaping per JSON inline */
            const QString safeItem = QString(item)
                .replace('\\', "\\\\").replace('"', "\\\"")
                .replace('\n', "\\n").replace('\r', "");
            const QString params = QString(tmplStr).replace("{{INPUT}}", safeItem);
            QJsonParseError je;
            QJsonDocument::fromJson(params.toUtf8(), &je);
            if (je.error != QJsonParseError::NoError) { ++skipped; continue; }
            insertWu(typeId,
                     QString("%1 %2/%3").arg(lblPfx).arg(i+1).arg(items.size()),
                     params, priority, 1, {}, pipId);
            ++created;
        }

        appendLog(QString("\xf0\x9f\x93\x82  Batch: %1 WU create, %2 saltate"
                          " (pipeline: %3, tipo: %4)")
                  .arg(created).arg(skipped).arg(pipId, typeId));
        refreshWuTable();
        dlg->accept();
    });

    dlg->exec();
}

/* ══════════════════════════════════════════════════════════════
   Result Aggregator
   ══════════════════════════════════════════════════════════════ */

void SciComputePage::onAggregateResultsClicked()
{
    if (m_selectedWuId.isEmpty()) {
        appendLog("\xe2\x9a\xa0  Seleziona prima una WU dalla tabella.");
        return;
    }

    /* Pipeline id della WU selezionata */
    QString pipelineId;
    int wuDoneCount = 0;
#ifdef HAVE_QT_SQL
    {
        QSqlQuery pq(QSqlDatabase::database(m_connName));
        pq.prepare("SELECT pipeline_id FROM work_units WHERE id=?");
        pq.addBindValue(m_selectedWuId); SQL_EXEC(pq);
        if (pq.next()) pipelineId = pq.value(0).toString();
    }
    if (!pipelineId.isEmpty()) {
        QSqlQuery cq(QSqlDatabase::database(m_connName));
        cq.prepare("SELECT COUNT(*) FROM work_units"
                   " WHERE pipeline_id=? AND status IN ('done','validated')");
        cq.addBindValue(pipelineId); SQL_EXEC(cq);
        if (cq.next()) wuDoneCount = cq.value(0).toInt();
    } else { wuDoneCount = 1; }
#endif

    /* Scelta formato */
    const QStringList fmts = {
        "CSV (una riga per WU)",
        "JSON array",
        "Testo concatenato"
    };
    bool ok;
    const QString descPip = pipelineId.isEmpty()
                            ? "WU: " + m_selectedWuId.left(8)
                            : "Pipeline: " + pipelineId;
    const QString fmtChoice = QInputDialog::getItem(this,
        "\xf0\x9f\x93\x8a  Aggrega risultati",
        QString("%1 — %2 WU completate\n\nFormato di esportazione:")
            .arg(descPip).arg(wuDoneCount),
        fmts, 0, false, &ok);
    if (!ok) return;
    const QString format = fmtChoice.startsWith("CSV") ? "csv"
                         : fmtChoice.startsWith("JSON") ? "json" : "txt";

    /* Raccolta WU */
    QVector<QVariantMap> allWus;
#ifdef HAVE_QT_SQL
    {
        QSqlQuery wq(QSqlDatabase::database(m_connName));
        if (!pipelineId.isEmpty()) {
            wq.prepare("SELECT id,type,label,status,assigned_node FROM work_units"
                       " WHERE pipeline_id=? AND status IN ('done','validated')"
                       " ORDER BY created_at");
            wq.addBindValue(pipelineId);
        } else {
            wq.prepare("SELECT id,type,label,status,assigned_node FROM work_units"
                       " WHERE id=?");
            wq.addBindValue(m_selectedWuId);
        }
        SQL_EXEC(wq);
        while (wq.next()) {
            QVariantMap row;
            row["id"]     = wq.value(0); row["type"]   = wq.value(1);
            row["label"]  = wq.value(2); row["status"] = wq.value(3);
            row["node"]   = wq.value(4);
            allWus << row;
        }
    }
#endif

    if (allWus.isEmpty()) {
        QMessageBox::information(this, tr("Nessun risultato"),
            "Nessuna WU completata trovata.");
        return;
    }

    /* Costruzione output */
    QString output;
    if (format == "csv") {
        output = "wu_id,type,label,status,node,completed_at,hash,output\n";
        for (const auto& wu : allWus) {
            const auto results = queryResults(wu["id"].toString());
            for (const auto& r : results) {
                const QString out = QString(r["output"].toString().left(4096))
                    .replace('"', "\"\"").replace('\n', "\\n");
                output += QString("\"%1\",\"%2\",\"%3\",\"%4\",\"%5\",%6,\"%7\",\"%8\"\n")
                    .arg(wu["id"].toString(), wu["type"].toString(),
                         wu["label"].toString(), wu["status"].toString(),
                         wu["node"].toString())
                    .arg(r["ts"].toLongLong())
                    .arg(r["hash"].toString().left(16), out);
            }
        }
    } else if (format == "json") {
        QJsonArray arr;
        for (const auto& wu : allWus) {
            const auto results = queryResults(wu["id"].toString());
            QJsonArray resArr;
            for (const auto& r : results)
                resArr.append(QJsonObject{
                    {"node_id", r["node_id"].toString()},
                    {"node",    r["node_name"].toString()},
                    {"output",  r["output"].toString().left(65536)},
                    {"hash",    r["hash"].toString()},
                    {"ts",      r["ts"].toLongLong()}
                });
            arr.append(QJsonObject{
                {"wu_id",   wu["id"].toString()},
                {"type",    wu["type"].toString()},
                {"label",   wu["label"].toString()},
                {"status",  wu["status"].toString()},
                {"node",    wu["node"].toString()},
                {"results", resArr}
            });
        }
        output = QJsonDocument(arr).toJson(QJsonDocument::Indented);
    } else {
        for (const auto& wu : allWus) {
            output += QString("\xe2\x95\x90\xe2\x95\x90 WU: %1 (%2) \xe2\x95\x90\xe2\x95\x90\n")
                      .arg(wu["label"].toString(), wu["id"].toString().left(8));
            const auto results = queryResults(wu["id"].toString());
            for (const auto& r : results) {
                const QString nodeName = r["node_name"].toString().isEmpty()
                    ? r["node_id"].toString().left(8) : r["node_name"].toString();
                output += QString("Nodo: %1 | %2\n")
                    .arg(nodeName)
                    .arg(QDateTime::fromSecsSinceEpoch(r["ts"].toLongLong())
                         .toString("dd/MM HH:mm"));
                output += r["output"].toString() + "\n\n";
            }
        }
    }

    /* Salvataggio */
    QDir().mkpath(QDir::homePath() + "/.prismalux/sci_results");
    const QString ts      = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm");
    const QString pipPart = pipelineId.isEmpty() ? m_selectedWuId.left(8) : pipelineId;
    const QString defPath = QDir::homePath() + "/.prismalux/sci_results/"
                            + ts + "_" + pipPart + "." + format;

    const QString path = QFileDialog::getSaveFileName(
        this, "\xf0\x9f\x93\x8a  Salva risultati aggregati", defPath,
        format == "csv" ? "CSV (*.csv)"
                        : format == "json" ? "JSON (*.json)" : "Testo (*.txt)");
    if (path.isEmpty()) return;

    QFile outFile(path);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Errore"),
            "Impossibile scrivere: " + path);
        return;
    }
    outFile.write(output.toUtf8());
    outFile.close();

    appendLog(QString("\xf0\x9f\x93\x8a  Aggregati %1 WU \xe2\x86\x92 %2")
              .arg(allWus.size()).arg(path));
    QMessageBox::information(this, tr("Esportazione completata"),
        QString("Esportati %1 WU in:\n%2").arg(allWus.size()).arg(path));
}

void SciComputePage::updateStatus()
{
    if (!m_statusLbl) return;
    if (m_server && m_server->isListening())
        m_statusLbl->setText(QString(
            "<span style='color:#22c55e;'>\xf0\x9f\x9f\xa2 Coordinator attivo"
            " — porta %1 — %2 nodi connessi</span>")
            .arg(P::kSciComputePort).arg(m_nodeSock.size()));
    else if (m_selfSock && m_selfSock->state() == QAbstractSocket::ConnectedState)
        m_statusLbl->setText(
            tr("<span style='color:#3b82f6;'>\xf0\x9f\x94\x97 Worker connesso</span>"));
    else
        m_statusLbl->setText(
            tr("<span style='color:#6b7280;'>\xe2\x9a\xaa Inattivo</span>"));
}
