#pragma once
/*
 * ble_crypto.h — Cifratura AES-256-GCM per payload BLE/BT
 *
 * Dipendenze:
 *   - Con HAVE_OPENSSL_CRYPTO  : OpenSSL 3 EVP (Android + desktop con OpenSSL)
 *   - Senza HAVE_OPENSSL_CRYPTO: fallback deterministico a HMAC-SHA256-XOR
 *     (NON sicuro come GCM, ma almeno offusca il plaintext sui canali Classic BT
 *      privi di link-layer encryption; da sostituire quando OpenSSL è disponibile)
 *
 * Formato wireframe (con OpenSSL GCM, 12+16 byte overhead):
 *   [ IV (12 byte) ][ TAG (16 byte) ][ ciphertext (N byte) ]
 *
 * Formato wireframe (fallback HMAC-XOR, 32 byte overhead):
 *   [ SALT (16 byte) ][ HMAC (16 byte trunc) ][ xor-ciphertext (N byte) ]
 *
 * La chiave è 32 byte (256 bit) in entrambi i casi.
 * Viene generata random una volta, poi salvata in QSettings.
 * Il tag 0x1B introduce ogni frame cifrato sul wire (per distinguerlo
 * da messaggi in chiaro inviati da peer non-Prismalux).
 */

#include <QByteArray>
#include <QString>
#include <QSettings>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QRandomGenerator>

#ifdef HAVE_OPENSSL_CRYPTO
#  include <openssl/evp.h>
#  include <openssl/rand.h>
#endif

namespace BleCrypto {

/* ─── Costanti ──────────────────────────────────────────────────────── */
static constexpr int  kKeyLen    = 32;   /* AES-256: 32 byte */
static constexpr int  kIvLen     = 12;   /* GCM nonce standard */
static constexpr int  kTagLen    = 16;   /* GCM auth-tag */
static constexpr int  kSaltLen   = 16;   /* fallback salt */
static constexpr int  kHmacTrunc = 16;   /* HMAC troncato nel fallback */
static constexpr quint8 kFrameTag = 0x1B; /* primo byte di ogni frame cifrato */

/* ─── Genera chiave 256 bit (crittograficamente random) ─────────────── */
inline QByteArray generateKey()
{
    QByteArray key(kKeyLen, Qt::Uninitialized);
#ifdef HAVE_OPENSSL_CRYPTO
    RAND_bytes(reinterpret_cast<unsigned char*>(key.data()), kKeyLen);
#else
    /* AUDIT-FIX 2026-06-03: usa securelySeeded() — crittograficamente sicuro */
    auto rng = QRandomGenerator::securelySeeded();
    for (int i = 0; i < kKeyLen; i += 4) {
        const quint32 r = rng.generate();
        key[i + 0] = static_cast<char>((r >>  0) & 0xFF);
        key[i + 1] = static_cast<char>((r >>  8) & 0xFF);
        key[i + 2] = static_cast<char>((r >> 16) & 0xFF);
        key[i + 3] = static_cast<char>((r >> 24) & 0xFF);
    }
#endif
    return key;
}

/* ─── Carica/crea chiave da QSettings ───────────────────────────────── */
/* AUDIT-FIX 2026-06-03: usa org/app name espliciti per coerenza con il
   resto dell'app (QSettings("Prismalux","Mobile")) ed evitare che la
   chiave finisca in un file di settings globale leggibile da altre app. */
inline QByteArray loadOrCreateKey(const QString& settingsGroup = "BleCrypto")
{
    QSettings s("Prismalux", "Mobile");
    s.beginGroup(settingsGroup);
    QByteArray key = QByteArray::fromBase64(s.value("session_key", "").toByteArray());
    if (key.size() != kKeyLen) {
        key = generateKey();
        s.setValue("session_key", key.toBase64());
    }
    s.endGroup();
    return key;
}

/* ─── Salva chiave in QSettings (per condivisione via QR/PIN) ──────── */
inline void saveKey(const QByteArray& key,
                    const QString& settingsGroup = "BleCrypto")
{
    QSettings s("Prismalux", "Mobile");  /* AUDIT-FIX 2026-06-03 */
    s.beginGroup(settingsGroup);
    s.setValue("session_key", key.toBase64());
    s.endGroup();
}

/* ════════════════════════════════════════════════════════════════════
   CIFRATURA
   ════════════════════════════════════════════════════════════════════ */

#ifdef HAVE_OPENSSL_CRYPTO
/*
 * encrypt_gcm — AES-256-GCM tramite OpenSSL EVP
 * Formato output: [kFrameTag][IV 12B][TAG 16B][ciphertext NB]
 */
inline QByteArray encrypt(const QByteArray& plaintext, const QByteArray& key)
{
    if (key.size() != kKeyLen) return {};

    /* IV random */
    unsigned char iv[kIvLen];
    if (RAND_bytes(iv, kIvLen) != 1) return {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray result;
    result.reserve(1 + kIvLen + kTagLen + plaintext.size());
    result.append(static_cast<char>(kFrameTag));
    result.append(reinterpret_cast<const char*>(iv), kIvLen);
    result.append(kTagLen, '\0');   /* placeholder tag — verrà scritto dopo */
    result.append(plaintext.size(), '\0');

    int len = 0;
    bool ok = true;

    ok = ok && EVP_EncryptInit_ex2(ctx,
                                    EVP_aes_256_gcm(),
                                    reinterpret_cast<const unsigned char*>(key.constData()),
                                    iv, nullptr) == 1;
    ok = ok && EVP_EncryptUpdate(ctx,
                                  reinterpret_cast<unsigned char*>(result.data() + 1 + kIvLen + kTagLen),
                                  &len,
                                  reinterpret_cast<const unsigned char*>(plaintext.constData()),
                                  plaintext.size()) == 1;
    ok = ok && EVP_EncryptFinal_ex(ctx, nullptr, &len) == 1;

    unsigned char tag[kTagLen];
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen, tag) == 1;

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) return {};

    /* Inserisce il tag nella posizione riservata */
    memcpy(result.data() + 1 + kIvLen, tag, kTagLen);
    return result;
}

/*
 * decrypt_gcm — verifica tag GCM e decifra
 * Input atteso: [kFrameTag][IV 12B][TAG 16B][ciphertext NB]
 */
inline QByteArray decrypt(const QByteArray& frame, const QByteArray& key)
{
    if (key.size() != kKeyLen) return {};
    if (frame.size() < 1 + kIvLen + kTagLen) return {};
    if (static_cast<unsigned char>(frame[0]) != kFrameTag) {
        /* Frame non cifrato — restituisce così com'è (compatibilità peer legacy) */
        return frame;
    }

    const unsigned char* iv  = reinterpret_cast<const unsigned char*>(frame.constData() + 1);
    const unsigned char* tag = reinterpret_cast<const unsigned char*>(frame.constData() + 1 + kIvLen);
    const unsigned char* ct  = reinterpret_cast<const unsigned char*>(frame.constData() + 1 + kIvLen + kTagLen);
    const int ctLen = frame.size() - 1 - kIvLen - kTagLen;
    if (ctLen < 0) return {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray plaintext(ctLen, '\0');
    int len = 0;
    bool ok = true;

    ok = ok && EVP_DecryptInit_ex2(ctx,
                                    EVP_aes_256_gcm(),
                                    reinterpret_cast<const unsigned char*>(key.constData()),
                                    iv, nullptr) == 1;
    ok = ok && EVP_DecryptUpdate(ctx,
                                  reinterpret_cast<unsigned char*>(plaintext.data()),
                                  &len,
                                  ct, ctLen) == 1;

    /* Imposta il tag prima del Final per la verifica GCM */
    ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen,
                                    const_cast<unsigned char*>(tag)) == 1;

    int finalLen = 0;
    const int finalRet = EVP_DecryptFinal_ex(ctx,
        reinterpret_cast<unsigned char*>(plaintext.data()) + len,
        &finalLen);
    ok = ok && (finalRet == 1);

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) return {};   /* tag authentication failure */
    return plaintext;
}

#else  /* ────────────── FALLBACK: HMAC-SHA256-XOR ────────────────── */
/*
 * Fallback per build desktop senza OpenSSL.
 * Sicurezza: BASSA — offuscamento + integrità HMAC ma senza
 * confidenzialità robusta. Adeguato solo per sviluppo/test.
 * Formato: [kFrameTag][SALT 16B][HMAC_TRUNC 16B][xor-cipher NB]
 */
inline QByteArray encrypt(const QByteArray& plaintext, const QByteArray& key)
{
    if (key.size() != kKeyLen) return {};

    /* Salt random — AUDIT-FIX 2026-06-03: usa securelySeeded() */
    QByteArray salt(kSaltLen, Qt::Uninitialized);
    {
        auto rng = QRandomGenerator::securelySeeded();
        for (int i = 0; i < kSaltLen; i += 4) {
            const quint32 r = rng.generate();
            salt[i + 0] = static_cast<char>((r >>  0) & 0xFF);
            salt[i + 1] = static_cast<char>((r >>  8) & 0xFF);
            salt[i + 2] = static_cast<char>((r >> 16) & 0xFF);
            salt[i + 3] = static_cast<char>((r >> 24) & 0xFF);
        }
    }

    /* Keystream = SHA256(key + salt) ripetuto */
    QByteArray keystream;
    keystream.reserve(plaintext.size());
    QByteArray seed = key + salt;
    while (keystream.size() < plaintext.size()) {
        keystream += QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
        seed = QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
    }
    keystream.truncate(plaintext.size());

    /* XOR */
    QByteArray cipher(plaintext.size(), '\0');
    for (int i = 0; i < plaintext.size(); ++i)
        cipher[i] = static_cast<char>(
            static_cast<unsigned char>(plaintext[i]) ^
            static_cast<unsigned char>(keystream[i]));

    /* HMAC-SHA256 troncato su (salt + cipher) */
    const QByteArray mac = QMessageAuthenticationCode::hash(
        salt + cipher, key, QCryptographicHash::Sha256).left(kHmacTrunc);

    QByteArray frame;
    frame.reserve(1 + kSaltLen + kHmacTrunc + cipher.size());
    frame.append(static_cast<char>(kFrameTag));
    frame.append(salt);
    frame.append(mac);
    frame.append(cipher);
    return frame;
}

inline QByteArray decrypt(const QByteArray& frame, const QByteArray& key)
{
    if (key.size() != kKeyLen) return {};
    /* AUDIT-FIX 2026-06-03: controlla isEmpty() prima di accedere a frame[0] */
    if (frame.isEmpty()) return {};
    if (static_cast<unsigned char>(frame[0]) != kFrameTag) {
        /* Frame non cifrato — compatibilità peer legacy */
        return frame;
    }
    if (frame.size() < 1 + kSaltLen + kHmacTrunc) return {};

    const QByteArray salt   = frame.mid(1, kSaltLen);
    const QByteArray hmacRx = frame.mid(1 + kSaltLen, kHmacTrunc);
    const QByteArray cipher = frame.mid(1 + kSaltLen + kHmacTrunc);

    /* Verifica HMAC */
    const QByteArray hmacCalc = QMessageAuthenticationCode::hash(
        salt + cipher, key, QCryptographicHash::Sha256).left(kHmacTrunc);
    if (hmacCalc != hmacRx) return {};   /* integrity failure */

    /* Keystream */
    QByteArray keystream;
    keystream.reserve(cipher.size());
    QByteArray seed = key + salt;
    while (keystream.size() < cipher.size()) {
        keystream += QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
        seed = QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
    }
    keystream.truncate(cipher.size());

    QByteArray plain(cipher.size(), '\0');
    for (int i = 0; i < cipher.size(); ++i)
        plain[i] = static_cast<char>(
            static_cast<unsigned char>(cipher[i]) ^
            static_cast<unsigned char>(keystream[i]));

    return plain;
}
#endif /* HAVE_OPENSSL_CRYPTO */

/* ─── Utilità wire ─────────────────────────────────────────────────── */

/* Restituisce true se il frame ha il tag Prismalux (frame cifrato) */
inline bool isCiphered(const QByteArray& frame)
{
    return !frame.isEmpty() &&
           static_cast<unsigned char>(frame[0]) == kFrameTag;
}

/*
 * Cifra un messaggio testuale e lo serializza in Base64 per il wire.
 * Il Base64 garantisce che il frame sia una stringa stampabile e
 * sopravviva alla serializzazione UTF-8 di QBluetoothSocket.
 * Il separatore di linea '\n' rimane sul wire (readLine() funziona).
 */
inline QByteArray encryptToWire(const QString& msg, const QByteArray& key)
{
    const QByteArray cipher = encrypt(msg.toUtf8(), key);
    if (cipher.isEmpty()) return {};
    return cipher.toBase64();
}

/*
 * Decifra un frame letto dal wire.
 *
 * Protocollo di auto-rilevamento:
 *   1. Prova a decodificare wireData come Base64.
 *   2. Se il primo byte del risultato è kFrameTag → frame cifrato Prismalux →
 *      decifra e restituisce il plaintext.
 *   3. Altrimenti (Base64 decode non ha prodotto kFrameTag, oppure non era
 *      Base64 valido) → frame plain-text da peer legacy → restituisce
 *      wireData.trimmed() interpretato come UTF-8.
 *
 * Restituisce QString() (null) solo in caso di errore autentico
 * (tag GCM non valido o HMAC errato = messaggio alterato/chiave sbagliata).
 */
inline QString decryptFromWire(const QByteArray& wireData, const QByteArray& key)
{
    const QByteArray trimmed = wireData.trimmed();
    if (trimmed.isEmpty()) return QString("");

    /* Prova il decode Base64 */
    QByteArray::FromBase64Result res =
        QByteArray::fromBase64Encoding(trimmed, QByteArray::AbortOnBase64DecodingErrors);

    if (res.decodingStatus == QByteArray::Base64DecodingStatus::Ok &&
        !res.decoded.isEmpty() &&
        isCiphered(res.decoded))
    {
        /* Frame cifrato Prismalux */
        const QByteArray plain = decrypt(res.decoded, key);
        if (plain.isEmpty()) return QString();  /* null = authentication failure */
        return QString::fromUtf8(plain);
    }

    /* Frame plain-text (peer legacy o messaggio non cifrato) */
    return QString::fromUtf8(trimmed);
}

} /* namespace BleCrypto */
