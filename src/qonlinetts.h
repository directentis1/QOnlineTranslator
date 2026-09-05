/*
 *  Copyright © 2018-2023 Hennadii Chernyshchyk <genaloner@gmail.com>
 *
 *  This file is part of QOnlineTranslator.
 *
 *  QOnlineTranslator is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  QOnlineTranslator is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with QOnlineTranslator. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef QONLINETTS_H
#define QONLINETTS_H

#include "qonlinetranslator.h"

#include <QLocale>
#include <QMediaContent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QTemporaryFile>

/**
 * @brief Provides TTS URL generation
 *
 * Example:
 * @code
 * QMediaPlayer *player = new QMediaPlayer(this);
 * QMediaPlaylist *playlist = new QMediaPlaylist(player);
 * QOnlineTts tts;
 *
 * playlist->addMedia(tts.generateUrls("Hello World!", QOnlineTranslator::Google););
 * player->setPlaylist(playlist);
 *
 * player->play(); // Plays "Hello World!"
 * @endcode
 */
class QOnlineTts : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(QOnlineTts)

public:
    /**
     * @brief Defines voice to use
     *
     * Used only by Yandex.
     */
    enum Voice {
        // All
        NoVoice = -1,

        // Yandex
        Zahar,
        Ermil,
        Jane,
        Oksana,
        Alyss,
        Omazh
    };
    Q_ENUM(Voice)

    /**
     * @brief Defines emotion to use
     *
     * Used only by Yandex.
     */
    enum Emotion {
        // All
        NoEmotion = -1,

        // Yandex
        Neutral,
        Good,
        Evil
    };
    Q_ENUM(Emotion)

    /**
     * @brief Indicates all possible error conditions found during the processing of the URLs generation
     */
    enum TtsError {
        /** No error condition */
        NoError,
        /** Specified engine does not support TTS */
        UnsupportedEngine,
        /** Unsupported language by specified engine */
        UnsupportedLanguage,
        /** Unsupported voice by specified engine */
        UnsupportedVoice,
        /** Unsupported emotion by specified engine */
        UnsupportedEmotion,
        /** Network error while talking to a TTS backend that needs live requests (currently only Bing) */
        NetworkError,
        /** The TTS backend returned a response that could not be understood (e.g. Bing changed its page/API) */
        ServiceError
    };

    /**
     * @brief Create object
     *
     * Constructs an object with empty data and with parent.
     * You can use generateUrls() to create URLs for use in QMediaPlayer.
     *
     * @param parent parent object
     */
    explicit QOnlineTts(QObject *parent = nullptr);

    /**
     * @brief Create TTS urls
     *
     * Splits text into parts (engines have a limited number of characters per request) and returns list with the generated API URLs to play.
     *
     * @param text text to speak
     * @param engine online translation engine
     * @param lang text language
     * @param voice voice to use (used only by Yandex)
     * @param emotion emotion to use (used only by Yandex)
     */
    void generateUrls(const QString &text, QOnlineTranslator::Engine engine, QOnlineTranslator::Language lang, Voice voice = NoVoice, Emotion emotion = NoEmotion);

    /**
     * @brief Generated media
     *
     * @return List of generated URLs
     */
    QList<QMediaContent> media() const;

    /**
     * @brief Last error
     *
     * Error that was found during the generating tts.
     * If no error was found, returns TtsError::NoError.
     * The text of the error can be obtained by errorString().
     *
     * @return last error
     */
    TtsError error() const;

    /**
     * @brief Last error string
     *
     * A human-readable description of the last tts URL generation error that occurred.
     *
     * @return last error string
     */
    QString errorString() const;

    /**
     * @brief Code of the voice
     *
     * @param voice voice
     * @return code for voice
     */
    static QString voiceCode(Voice voice);

    /**
     * @brief Code of the emotion
     *
     * Used only by Yandex.
     *
     * @param emotion emotion
     * @return code for emotion
     */
    static QString emotionCode(Emotion emotion);

    /**
     * @brief code of the regional language (voice only)
     *
     * Used only by Google
     *
     * @param language language
     * @param region region
     * @return code for language in region, or a region-neutral language code if region is not supported
     */
    static QString regionCode(QOnlineTranslator::Language language, QLocale::Country region);

    /**
     * @brief Emotion from code
     *
     * Used only by Yandex.
     *
     * @param emotionCode emotion code
     * @return corresponding emotion
     */
    static Emotion emotion(const QString &emotionCode);

    /**
     * @brief Voice from code
     *
     * Used only by Yandex.
     *
     * @param voiceCode voice code
     * @return corresponding voice
     */
    static Voice voice(const QString &voiceCode);

    /**
     * @brief Voice region from code
     *
     * Used only by Google
     *
     * @param regionCode region code
     * @return corresponding region code
     */
    static QPair<QOnlineTranslator::Language, QLocale::Country> region(const QString &regionCode);

    /**
     * @brief valid and supported regions for languages
     * @return a map, with key being language enum and value a list of valid regions enum
     */
    static const QMap<QOnlineTranslator::Language, QList<QLocale::Country>> &validRegions();

    /**
     * @brief Check if an engine supports TTS at all
     *
     * Translation-only engines (Bing, LibreTranslate, Lingva, DeepLX, DeepLXFree, ...) never
     * generate audio; this lets UI code filter which engines are offered as a "speech engine"
     * independently of whichever engine is currently selected for translation.
     *
     * @param engine engine
     * @return `true` if generateUrls() can produce audio for this engine
     */
    static bool isSupportTts(QOnlineTranslator::Engine engine);

    /**
     * @brief region preferences
     * @return region preferences
     */
    const QMap<QOnlineTranslator::Language, QLocale::Country> &regions() const;

    /**
     * @brief set region preferences
     * @param newRegionPreferences new region preferences
     */
    void setRegions(const QMap<QOnlineTranslator::Language, QLocale::Country> &newRegionPreferences);

    /**
     * @brief Per-language Bing voice name preferences (e.g. "en-US-AriaNeural")
     *
     * Populated from BingVoiceCatalog; a language with no entry (or an entry naming a voice the
     * catalog no longer recognizes) falls back to BingVoiceCatalog::defaultVoiceName() when
     * generateUrls() is used with the Bing engine.
     *
     * @return Bing voice preferences
     */
    const QMap<QOnlineTranslator::Language, QString> &bingVoicePreferences() const;

    /**
     * @brief set Bing voice name preferences
     * @param newVoicePreferences new Bing voice preferences
     */
    void setBingVoicePreferences(const QMap<QOnlineTranslator::Language, QString> &newVoicePreferences);

private:
    /**
     * @brief Per-language voice data needed to build a Bing/Azure Speech SSML request
     */
    struct BingVoiceData {
        QString locale; // e.g. "en-US"
        QString gender; // "Male" or "Female"
        QString name; // e.g. "en-US-AriaNeural"
    };

    void setError(TtsError error, const QString &errorString);

    QString languageApiCode(QOnlineTranslator::Engine engine, QOnlineTranslator::Language lang);
    QString voiceApiCode(QOnlineTranslator::Engine engine, Voice voice);
    QString emotionApiCode(QOnlineTranslator::Engine engine, Emotion emotion);

    // Bing (Microsoft Translator's "tfettts" endpoint, the same one the web version of
    // Bing Translator uses - there is no official/documented public API for it)
    void generateBingUrls(const QString &text, QOnlineTranslator::Language lang);
    bool ensureBingCredentials();
    QByteArray postBingSpeech(const QByteArray &requestBody);
    static QVector<QString> splitTextForBing(const QString &text);
    static QByteArray buildBingSsml(const QString &text, const BingVoiceData &voice);

    // Resolves the voice to actually use for `lang`: m_bingVoicePreferences's choice if set and
    // still recognized by BingVoiceCatalog, otherwise BingVoiceCatalog::defaultVoiceName(). Not
    // static (unlike before) since it now depends on this instance's preferences.
    bool bingVoiceData(QOnlineTranslator::Language lang, BingVoiceData &voice);
    static void setBingBrowserHeaders(QNetworkRequest &request);

    static const QMap<Emotion, QString> s_emotionCodes;
    static const QMap<Voice, QString> s_voiceCodes;
    static const QMap<QPair<QOnlineTranslator::Language, QLocale::Country>, QString> s_regionCodes;
    static const QMap<QOnlineTranslator::Language, QList<QLocale::Country>> s_validRegions;

    // Credentials scraped from the Bing Translator webpage, shared by every QOnlineTts instance
    // in the process (mirrors how QOnlineTranslator caches its own copy of the same credentials
    // for text translation). Kept independent from QOnlineTranslator's copy so this class has no
    // dependency on a QOnlineTranslator instance/internals, at the cost of a second, short-lived
    // fetch the first time each is used.
    static inline QByteArray s_bingKey;
    static inline QByteArray s_bingToken;
    static inline QString s_bingIg;
    static inline QString s_bingIid;
    static inline qint64 s_bingCredentialsTimestamp = 0;
    static inline int s_bingRequestCounter = 0;

    QMap<QOnlineTranslator::Language, QLocale::Country> m_regionPreferences;
    QMap<QOnlineTranslator::Language, QString> m_bingVoicePreferences;

    // Lazily created; only Bing needs to talk to the network directly (Google/Yandex just hand
    // back plain GET URLs for the media player to fetch itself, see generateUrls())
    QNetworkAccessManager *m_networkManager = nullptr;

    // Bing has no public streamable URL - audio has to be fetched via a POST request and handed
    // to the player as a local file. Unlike Google/Yandex (where generateUrls() just builds URLs
    // and repeat playback is a cheap GET the media player does itself), each Bing chunk costs a
    // real network round-trip, so fetched audio is cached here keyed by (language, chunk text)
    // and reused on the next identical request instead of being re-fetched. Entries older than
    // s_bingAudioCacheTtlMs are purged the next time generateUrls() runs for Bing (there's no
    // background timer - this is a lazy sweep, so a cache entry can outlive its TTL by however
    // long the user goes without triggering Bing TTS again, but never gets *reused* past it), and
    // s_bingAudioCacheLimit is a secondary hard cap (oldest-first) in case a lot of distinct text
    // gets spoken inside one TTL window. Files are parented to `this` so they're cleaned up
    // automatically when purged/evicted or when the QOnlineTts instance itself is destroyed.
    //
    // IMPORTANT: this means whoever calls generateUrls() for Bing must reuse the same QOnlineTts
    // instance across calls to get any caching benefit at all, and must keep it alive for as long
    // as playback can last - a fresh, short-lived QOnlineTts per speak request would both defeat
    // the cache and have its temp files deleted before QMediaPlayer gets a chance to play them.
    // SpeakButtons keeps one QOnlineTts per SpeakButtons instance for exactly this reason.
    struct BingCacheEntry {
        QTemporaryFile *file = nullptr;
        qint64 cachedAtMs = 0; // QDateTime::currentMSecsSinceEpoch() when this entry was fetched
    };
    QMap<QString, BingCacheEntry> m_bingAudioCache;
    QVector<QString> m_bingAudioCacheOrder; // insertion order, oldest first, for FIFO eviction
    static constexpr int s_bingAudioCacheLimit = 200;
    static constexpr qint64 s_bingAudioCacheTtlMs = 30 * 60 * 1000; // 30 minutes

    static QString bingCacheKey(QOnlineTranslator::Language lang, const QString &chunkText);
    void cacheBingAudio(const QString &key, QTemporaryFile *file);
    void purgeExpiredBingAudio();

    static constexpr int s_googleTtsLimit = 200;
    static constexpr int s_yandexTtsLimit = 1400;

    // Bing's web frontend keeps each chunk under ~170 characters, splitting only on word
    // boundaries (falling back to a hard split for single words longer than that). Ported from
    // TranslateWebPage's textToSpeech.js so behavior matches the reference implementation.
    static constexpr int s_bingTtsSoftLimit = 170;
    static constexpr int s_bingTtsHardWordLimit = 160;

    QList<QMediaContent> m_media;
    QString m_errorString;
    TtsError m_error = NoError;
};

#endif // QONLINETTS_H
