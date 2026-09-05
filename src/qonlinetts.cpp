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

#include "qonlinetts.h"
#include "bingvoicecatalog.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QMetaEnum>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QtAlgorithms>
#include <algorithm>

const QMap<QOnlineTts::Emotion, QString> QOnlineTts::s_emotionCodes = {
    {Neutral, QStringLiteral("neutral")},
    {Good, QStringLiteral("good")},
    {Evil, QStringLiteral("evil")}};

const QMap<QOnlineTts::Voice, QString> QOnlineTts::s_voiceCodes = {
    {Zahar, QStringLiteral("zahar")},
    {Ermil, QStringLiteral("ermil")},
    {Jane, QStringLiteral("jane")},
    {Oksana, QStringLiteral("oksana")},
    {Alyss, QStringLiteral("alyss")},
    {Omazh, QStringLiteral("omazh")}};

const QMap<QPair<QOnlineTranslator::Language, QLocale::Country>, QString> QOnlineTts::s_regionCodes = {
    {{QOnlineTranslator::Bengali, QLocale::Bangladesh}, QStringLiteral("bn-BD")},
    {{QOnlineTranslator::Bengali, QLocale::India}, QStringLiteral("bn-IN")},
    {{QOnlineTranslator::SimplifiedChinese, QLocale::China}, QStringLiteral("cmn-Hans-CN")},
    {{QOnlineTranslator::English, QLocale::Australia}, QStringLiteral("en-AU")},
    {{QOnlineTranslator::English, QLocale::India}, QStringLiteral("en-IN")},
    {{QOnlineTranslator::English, QLocale::UnitedKingdom}, QStringLiteral("en-GB")},
    {{QOnlineTranslator::English, QLocale::UnitedStates}, QStringLiteral("en-US")},
    {{QOnlineTranslator::French, QLocale::Canada}, QStringLiteral("fr-CA")},
    {{QOnlineTranslator::French, QLocale::France}, QStringLiteral("fr-FR")},
    {{QOnlineTranslator::German, QLocale::Germany}, QStringLiteral("de-DE")},
    {{QOnlineTranslator::Portuguese, QLocale::Brazil}, QStringLiteral("pt-BR")},
    {{QOnlineTranslator::Spanish, QLocale::Spain}, QStringLiteral("es-ES")},
    {{QOnlineTranslator::Spanish, QLocale::UnitedStates}, QStringLiteral("es-US")},
    {{QOnlineTranslator::Tamil, QLocale::India}, QStringLiteral("ta-IN")}};

const QMap<QOnlineTranslator::Language, QList<QLocale::Country>> QOnlineTts::s_validRegions = {
    {QOnlineTranslator::Bengali, {QLocale::Bangladesh, QLocale::India}},
    {QOnlineTranslator::SimplifiedChinese, {QLocale::China}},
    {QOnlineTranslator::English, {QLocale::Australia, QLocale::India, QLocale::UnitedKingdom, QLocale::UnitedStates}},
    {QOnlineTranslator::French, {QLocale::Canada, QLocale::France}},
    {QOnlineTranslator::German, {QLocale::Germany}},
    {QOnlineTranslator::Portuguese, {QLocale::Brazil}},
    {QOnlineTranslator::Spanish, {QLocale::Spain, QLocale::UnitedStates}},
    {QOnlineTranslator::Tamil, {QLocale::India}}};

QOnlineTts::QOnlineTts(QObject *parent)
    : QObject(parent)
{
}

void QOnlineTts::generateUrls(const QString &text, QOnlineTranslator::Engine engine, QOnlineTranslator::Language lang, Voice voice, Emotion emotion)
{
    // Drop whatever the previous call put on the playlist - but NOT the Bing audio cache, which
    // is deliberately kept across calls (see the m_bingAudioCache comment in the header).
    m_media.clear();
    setError(NoError, {});

    // Get speech
    QString unparsedText = text;
    switch (engine) {
    case QOnlineTranslator::Google: {
        if (voice != NoVoice) {
            setError(UnsupportedVoice, tr("Selected engine %1 does not support voice settings").arg(QMetaEnum::fromType<QOnlineTranslator::Engine>().valueToKey(engine)));
            return;
        }

        if (emotion != NoEmotion) {
            setError(UnsupportedEmotion, tr("Selected engine %1 does not support emotion settings").arg(QMetaEnum::fromType<QOnlineTranslator::Engine>().valueToKey(engine)));
            return;
        }

        const QString langString = languageApiCode(engine, lang);
        if (langString.isNull())
            return;

        // Google has a limit of characters per tts request. If the query is larger, then it should be splited into several
        while (!unparsedText.isEmpty()) {
            const int splitIndex = QOnlineTranslator::getSplitIndex(unparsedText, s_googleTtsLimit); // Split the part by special symbol

            // Generate URL API for add it to the playlist
            QUrl apiUrl(QStringLiteral("https://translate.googleapis.com/translate_tts"));
            const QString query = QStringLiteral("ie=UTF-8&client=gtx&tl=%1&q=%2").arg(langString, QString(QUrl::toPercentEncoding(unparsedText.left(splitIndex))));
            apiUrl.setQuery(query);
            m_media.append(apiUrl);

            // Remove the said part from the next saying
            unparsedText = unparsedText.mid(splitIndex);
        }
        break;
    }
    case QOnlineTranslator::Yandex: {
        const QString langString = languageApiCode(engine, lang);
        if (langString.isNull())
            return;

        const QString voiceString = voiceApiCode(engine, voice);
        if (voiceString.isNull())
            return;

        const QString emotionString = emotionApiCode(engine, emotion);
        if (emotionString.isNull())
            return;

        // Yandex has a limit of characters per tts request. If the query is larger, then it should be splited into several
        while (!unparsedText.isEmpty()) {
            const int splitIndex = QOnlineTranslator::getSplitIndex(unparsedText, s_yandexTtsLimit); // Split the part by special symbol

            // Generate URL API for add it to the playlist
            QUrl apiUrl(QStringLiteral("https://tts.voicetech.yandex.net/tts"));
            const QString query = QStringLiteral("text=%1&lang=%2&speaker=%3&emotion=%4&format=mp3")
                                      .arg(QUrl::toPercentEncoding(unparsedText.left(splitIndex)), langString, voiceString, emotionString);
            apiUrl.setQuery(query);
            m_media.append(apiUrl);

            // Remove the said part from the next saying
            unparsedText = unparsedText.mid(splitIndex);
        }
        break;
    }
    case QOnlineTranslator::Bing: {
        if (voice != NoVoice) {
            setError(UnsupportedVoice, tr("Selected engine %1 does not support voice settings").arg(QMetaEnum::fromType<QOnlineTranslator::Engine>().valueToKey(engine)));
            return;
        }

        if (emotion != NoEmotion) {
            setError(UnsupportedEmotion, tr("Selected engine %1 does not support emotion settings").arg(QMetaEnum::fromType<QOnlineTranslator::Engine>().valueToKey(engine)));
            return;
        }

        generateBingUrls(text, lang);
        break;
    }
    case QOnlineTranslator::LibreTranslate:
    case QOnlineTranslator::Lingva:
    case QOnlineTranslator::DeepLX:
    case QOnlineTranslator::DeepLXFree:
        // NOTE:
        // Lingva returns audio in strange format, use placeholder, until we'll figure it out
        //
        // Example: https://lingva.garudalinux.org/api/v1/audio/en/Hello%20World!
        // Will return json with uint bytes array, according to documentation
        // See: https://github.com/TheDavidDelta/lingva-translate#public-apis
        setError(UnsupportedEngine, tr("%1 engine does not support TTS").arg(QMetaEnum::fromType<QOnlineTranslator::Engine>().valueToKey(engine)));
        break;
    }
}

QList<QMediaContent> QOnlineTts::media() const
{
    return m_media;
}

QString QOnlineTts::errorString() const
{
    return m_errorString;
}

QOnlineTts::TtsError QOnlineTts::error() const
{
    return m_error;
}

QString QOnlineTts::voiceCode(Voice voice)
{
    return s_voiceCodes.value(voice);
}

QString QOnlineTts::regionCode(QOnlineTranslator::Language language, QLocale::Country region)
{
    return s_regionCodes.value({language, region}, QOnlineTranslator::languageApiCode(QOnlineTranslator::Google, language));
}

QString QOnlineTts::emotionCode(Emotion emotion)
{
    return s_emotionCodes.value(emotion);
}

QOnlineTts::Emotion QOnlineTts::emotion(const QString &emotionCode)
{
    return s_emotionCodes.key(emotionCode, NoEmotion);
}

QOnlineTts::Voice QOnlineTts::voice(const QString &voiceCode)
{
    return s_voiceCodes.key(voiceCode, NoVoice);
}

QPair<QOnlineTranslator::Language, QLocale::Country> QOnlineTts::region(const QString &regionCode)
{
    return s_regionCodes.key(regionCode, {QOnlineTranslator::NoLanguage, QLocale::AnyCountry});
}

const QMap<QOnlineTranslator::Language, QList<QLocale::Country>> &QOnlineTts::validRegions()
{
    return s_validRegions;
}

bool QOnlineTts::isSupportTts(QOnlineTranslator::Engine engine)
{
    switch (engine) {
    case QOnlineTranslator::Google:
    case QOnlineTranslator::Yandex:
    case QOnlineTranslator::Bing:
        return true;
    case QOnlineTranslator::LibreTranslate:
    case QOnlineTranslator::Lingva:
    case QOnlineTranslator::DeepLX:
    case QOnlineTranslator::DeepLXFree:
        return false;
    }

    return false;
}

void QOnlineTts::setError(TtsError error, const QString &errorString)
{
    m_error = error;
    m_errorString = errorString;
}

// Returns engine-specific language code for tts
QString QOnlineTts::languageApiCode(QOnlineTranslator::Engine engine, QOnlineTranslator::Language lang)
{
    switch (engine) {
    case QOnlineTranslator::Google:
    case QOnlineTranslator::Lingva: // Lingva is a frontend to Google Translate
        if (lang != QOnlineTranslator::Auto)
            return regionCode(lang, m_regionPreferences.value(lang)); // Google use the same codes for tts (except 'auto')
        break;
    case QOnlineTranslator::Yandex:
        switch (lang) {
        case QOnlineTranslator::Russian:
            return QStringLiteral("ru_RU");
        case QOnlineTranslator::Tatar:
            return QStringLiteral("tr_TR");
        case QOnlineTranslator::English:
            return QStringLiteral("en_GB");
        default:
            break;
        }
        break;
    default:
        break;
    }

    setError(UnsupportedLanguage, tr("Selected language %1 is not supported for %2").arg(QMetaEnum::fromType<QOnlineTranslator::Language>().valueToKey(lang), QMetaEnum::fromType<QOnlineTranslator::Engine>().valueToKey(engine)));
    return {};
}

QString QOnlineTts::voiceApiCode(QOnlineTranslator::Engine engine, Voice voice)
{
    if (engine == QOnlineTranslator::Yandex) {
        if (voice == NoVoice)
            return voiceCode(Zahar);
        return voiceCode(voice);
    }

    setError(UnsupportedVoice, tr("Selected voice %1 is not supported for %2").arg(QMetaEnum::fromType<Voice>().valueToKey(voice), QMetaEnum::fromType<QOnlineTranslator::Engine>().valueToKey(engine)));
    return {};
}

QString QOnlineTts::emotionApiCode(QOnlineTranslator::Engine engine, Emotion emotion)
{
    if (engine == QOnlineTranslator::Yandex) {
        if (emotion == NoEmotion)
            return emotionCode(Neutral);
        return emotionCode(emotion);
    }

    setError(UnsupportedEmotion, tr("Selected emotion %1 is not supported by %2").arg(QMetaEnum::fromType<Emotion>().valueToKey(emotion), QMetaEnum::fromType<QOnlineTranslator::Engine>().valueToKey(engine)));
    return {};
}

const QMap<QOnlineTranslator::Language, QLocale::Country> &QOnlineTts::regions() const
{
    return m_regionPreferences;
}

void QOnlineTts::setRegions(const QMap<QOnlineTranslator::Language, QLocale::Country> &newRegionPreferences)
{
    m_regionPreferences = newRegionPreferences;
}

const QMap<QOnlineTranslator::Language, QString> &QOnlineTts::bingVoicePreferences() const
{
    return m_bingVoicePreferences;
}

void QOnlineTts::setBingVoicePreferences(const QMap<QOnlineTranslator::Language, QString> &newVoicePreferences)
{
    m_bingVoicePreferences = newVoicePreferences;
}

bool QOnlineTts::bingVoiceData(QOnlineTranslator::Language lang, BingVoiceData &voice)
{
    QString locale;
    QString gender;
    QString voiceName = m_bingVoicePreferences.value(lang);

    // Fall back to the catalog default if nothing was configured, or if the configured voice name
    // is no longer recognized (e.g. Bing renamed/removed it since the user picked it).
    if (voiceName.isEmpty() || !BingVoiceCatalog::voiceInfo(lang, voiceName, locale, gender)) {
        voiceName = BingVoiceCatalog::defaultVoiceName(lang);
        if (voiceName.isEmpty() || !BingVoiceCatalog::voiceInfo(lang, voiceName, locale, gender))
            return false; // Bing has no voice at all for this language
    }

    voice = {locale, gender, voiceName};
    return true;
}

// Bing's tfettts endpoint returned "500 Internal Server Error" for requests that were otherwise
// well-formed but only carried a plain custom User-Agent - a request captured from an actual
// Chrome tab (via Charles Proxy) with these headers succeeded. None of the values below need to
// match a real, current Chrome install exactly - Bing doesn't appear to validate the version
// number itself, just that the general shape of a browser request (Fetch Metadata headers,
// Client Hints, a real-looking Accept/Accept-Language) is present.
void QOnlineTts::setBingBrowserHeaders(QNetworkRequest &request)
{
    request.setHeader(QNetworkRequest::UserAgentHeader,
                       QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"));
    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.9");
    request.setRawHeader("sec-ch-ua", R"("Chromium";v="124", "Google Chrome";v="124", "Not-A.Brand";v="99")");
    request.setRawHeader("sec-ch-ua-mobile", "?0");
    request.setRawHeader("sec-ch-ua-platform", "\"Windows\"");
    request.setRawHeader("sec-fetch-dest", "empty");
    request.setRawHeader("sec-fetch-mode", "cors");
    request.setRawHeader("sec-fetch-site", "none");
    request.setRawHeader("sec-fetch-storage-access", "active");
    request.setRawHeader("priority", "u=1, i");
}

// Splits `text` the same way TranslateWebPage's textToSpeech.js does for Bing: collect words,
// then greedily pack them into chunks kept under s_bingTtsSoftLimit characters, only ever
// breaking a single word if that word alone is longer than s_bingTtsHardWordLimit.
QVector<QString> QOnlineTts::splitTextForBing(const QString &text)
{
    QVector<QString> words;
    for (QString word : text.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        while (word.size() > s_bingTtsHardWordLimit) {
            words.append(word.left(s_bingTtsHardWordLimit));
            word = word.mid(s_bingTtsHardWordLimit);
        }
        if (!word.trimmed().isEmpty())
            words.append(word);
    }

    QVector<QString> chunks;
    QString chunk;
    for (QString word : words) {
        word += QLatin1Char(' ');
        if (chunk.size() + word.size() < s_bingTtsSoftLimit) {
            chunk += word;
        } else {
            chunks.append(chunk);
            chunk = word;
        }
    }
    if (!chunk.trimmed().isEmpty())
        chunks.append(chunk);

    return chunks;
}

QByteArray QOnlineTts::buildBingSsml(const QString &text, const BingVoiceData &voice)
{
    // Deliberately mirrors the exact SSML shape Bing's own web frontend sends - double-quoted
    // attributes, and the voice name stuffed into a nonstandard `xml:name` attribute alongside an
    // *empty* standard `name` attribute - rather than "corrected" spec-compliant SSML with the
    // voice in `name` directly. That corrected version reliably got a 500 back from the tfettts
    // endpoint (confirmed via Charles Proxy against a real browser request); whatever Bing's
    // server does when it sees a populated `name` attribute, it doesn't like it, so don't give it
    // one - just replicate the working request byte-for-byte instead of guessing why.
    static const QString ssmlTemplate = QStringLiteral(
        "<speak version=\"1.0\" xml:lang=\"%1\">"
        "<voice xml:lang=\"%1\" xml:gender=\"%2\" name=\"\" xml:name=\"%3\">"
        "<prosody rate=\"+0.00%\">%4</prosody>"
        "</voice>"
        "</speak>");

    return ssmlTemplate.arg(voice.locale, voice.gender, voice.name, text.toHtmlEscaped()).toUtf8();
}

// Scrapes the IG/IID/key/token quadruplet that the Bing Translator webpage embeds in its own
// HTML and that its JS then attaches to every translate/tts request as a lightweight anti-abuse
// check. This is the exact same trick QOnlineTranslator::parseBingCredentials() uses for text
// translation, kept as an independent copy here (see the comment above s_bingKey in the header
// for why). Blocks with a nested event loop, matching the rest of this class's synchronous API.
bool QOnlineTts::ensureBingCredentials()
{
    // Bing's own frontend treats these as valid for up to ~30 minutes; refetch a bit earlier
    // to be safe.
    constexpr qint64 credentialsTtlMs = 25 * 60 * 1000;
    if (!s_bingKey.isEmpty() && !s_bingToken.isEmpty() && QDateTime::currentMSecsSinceEpoch() - s_bingCredentialsTimestamp < credentialsTtlMs)
        return true;

    if (!m_networkManager)
        m_networkManager = new QNetworkAccessManager(this);

    QNetworkRequest request{QUrl(QStringLiteral("https://www.bing.com/translator"))};
    setBingBrowserHeaders(request);

    QNetworkReply *reply = m_networkManager->get(request);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setError(NetworkError, reply->errorString());
        return false;
    }

    const QByteArray webSiteData = reply->readAll();

    const QByteArray abuseBeginString = "var params_AbusePreventionHelper = [";
    const int credentialsBeginPos = webSiteData.indexOf(abuseBeginString);
    if (credentialsBeginPos == -1) {
        setError(ServiceError, tr("Error: Unable to find Bing credentials in web version."));
        return false;
    }

    const int keyBeginPos = credentialsBeginPos + abuseBeginString.size();
    const int keyEndPos = webSiteData.indexOf(',', keyBeginPos);
    if (keyEndPos == -1) {
        setError(ServiceError, tr("Error: Unable to extract Bing key from web version."));
        return false;
    }
    const QByteArray key = webSiteData.mid(keyBeginPos, keyEndPos - keyBeginPos);

    const int tokenBeginPos = keyEndPos + 2; // Skip two symbols instead of one because the value is enclosed in quotes
    const int tokenEndPos = webSiteData.indexOf('"', tokenBeginPos);
    if (tokenEndPos == -1) {
        setError(ServiceError, tr("Error: Unable to extract Bing token from web version."));
        return false;
    }
    const QByteArray token = webSiteData.mid(tokenBeginPos, tokenEndPos - tokenBeginPos);

    const QByteArray igString = "IG:\"";
    const int igBeginPos = webSiteData.indexOf(igString);
    const int igEndPos = webSiteData.indexOf('"', igBeginPos + igString.size());
    if (igBeginPos == -1 || igEndPos == -1) {
        setError(ServiceError, tr("Error: Unable to extract additional Bing information from web version."));
        return false;
    }
    const QString ig = QString::fromUtf8(webSiteData.mid(igBeginPos + igString.size(), igEndPos - (igBeginPos + igString.size())));

    const QByteArray iidString = "data-iid=\"";
    const int iidBeginPos = webSiteData.indexOf(iidString);
    const int iidEndPos = webSiteData.indexOf('"', iidBeginPos + iidString.size());
    if (iidBeginPos == -1 || iidEndPos == -1) {
        setError(ServiceError, tr("Error: Unable to extract additional Bing information from web version."));
        return false;
    }
    const QString iid = QString::fromUtf8(webSiteData.mid(iidBeginPos + iidString.size(), iidEndPos - (iidBeginPos + iidString.size())));

    s_bingKey = key;
    s_bingToken = token;
    s_bingIg = ig;
    s_bingIid = iid;
    s_bingCredentialsTimestamp = QDateTime::currentMSecsSinceEpoch();
    return true;
}

// Blocking POST to Bing's TTS endpoint. Returns the raw audio bytes, or an empty array on error
// (with setError() already called).
QByteArray QOnlineTts::postBingSpeech(const QByteArray &requestBody)
{
    // Built as a plain string rather than through QUrlQuery: a request captured from Bing's own
    // web frontend has a literal "isVertical=1&&IG=..." (double ampersand, i.e. an empty query
    // item between the two) - QUrlQuery would normalize that away, so this mirrors it exactly
    // rather than risk the server caring about it.
    const QUrl url(QStringLiteral("https://www.bing.com/tfettts?isVertical=1&&IG=%1&IID=%2.%3")
                       .arg(s_bingIg, s_bingIid)
                       // Bing's own frontend increments the trailing counter for every request
                       // made in the same page session; mirror that instead of hardcoding ".1"
                       // for every chunk.
                       .arg(++s_bingRequestCounter));

    QNetworkRequest request(url);
    setBingBrowserHeaders(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    // application/x-www-form-urlencoded uses '+' for spaces (what a real browser's
    // URLSearchParams/fetch() produces); QUrlQuery's own percent-encoding would leave spaces as
    // %20 instead, so encode the three fields by hand to match the working reference exactly.
    const auto formEncode = [](const QByteArray &value) {
        QByteArray encoded = QUrl::toPercentEncoding(QString::fromUtf8(value));
        encoded.replace("%20", "+");
        return encoded;
    };
    const QByteArray body = "ssml=" + formEncode(requestBody) + "&token=" + formEncode(s_bingToken) + "&key=" + formEncode(s_bingKey);

    QNetworkReply *reply = m_networkManager->post(request, body);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setError(NetworkError, reply->errorString());
        return {};
    }

    const QByteArray contentType = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
    const QByteArray data = reply->readAll();

    // A successful request returns raw "audio/mpeg" bytes; an expired/invalid key+token pair (or
    // Bing throttling us) comes back as a small JSON/text error body instead.
    if (!contentType.startsWith("audio/") || data.isEmpty()) {
        // The credentials may have just expired server-side (independent of our local TTL guess)
        // - drop them so the *next* call to generateUrls() re-fetches instead of reusing dead ones.
        s_bingKey.clear();
        s_bingToken.clear();
        setError(ServiceError, tr("Error: Bing TTS returned an unexpected response (%1)").arg(QString::fromUtf8(data.left(200))));
        return {};
    }

    return data;
}

void QOnlineTts::generateBingUrls(const QString &text, QOnlineTranslator::Language lang)
{
    BingVoiceData voice;
    if (lang == QOnlineTranslator::Auto || !bingVoiceData(lang, voice)) {
        setError(UnsupportedLanguage, tr("Selected language %1 is not supported for %2").arg(QMetaEnum::fromType<QOnlineTranslator::Language>().valueToKey(lang), QMetaEnum::fromType<QOnlineTranslator::Engine>().valueToKey(QOnlineTranslator::Bing)));
        return;
    }

    const QVector<QString> chunks = splitTextForBing(text);

    purgeExpiredBingAudio();

    // Skip the credentials fetch entirely if every chunk is already cached for this exact voice
    // (e.g. the user just hit "speak" again on text they already played with the same voice) - no
    // need to talk to Bing at all.
    const bool allCached = std::all_of(chunks.cbegin(), chunks.cend(), [this, lang, &voice](const QString &chunk) {
        return m_bingAudioCache.contains(bingCacheKey(lang, voice.name, chunk));
    });
    if (!allCached && !ensureBingCredentials())
        return; // setError() was already called

    if (!m_networkManager)
        m_networkManager = new QNetworkAccessManager(this);

    for (const QString &chunk : chunks) {
        const QString cacheKey = bingCacheKey(lang, voice.name, chunk);
        QTemporaryFile *file = m_bingAudioCache.value(cacheKey).file;

        if (!file) {
            const QByteArray ssml = buildBingSsml(chunk, voice);
            const QByteArray audio = postBingSpeech(ssml);
            if (audio.isEmpty())
                return; // setError() was already called by postBingSpeech()

            // Parented to `this` - see the m_bingAudioCache comment in the header for the
            // lifetime implications of that.
            file = new QTemporaryFile(this);
            file->setFileTemplate(QDir::tempPath() + QStringLiteral("/crow-translate-bing-tts-XXXXXX.mp3"));
            if (!file->open() || file->write(audio) != audio.size()) {
                setError(ServiceError, tr("Error: Unable to write Bing TTS audio to a temporary file"));
                delete file;
                return;
            }
            file->close();

            cacheBingAudio(cacheKey, file);
        }

        m_media.append(QUrl::fromLocalFile(file->fileName()));
    }
}

QString QOnlineTts::bingCacheKey(QOnlineTranslator::Language lang, const QString &voiceName, const QString &chunkText)
{
    return QString::number(lang) + QLatin1Char('|') + voiceName + QLatin1Char('|') + chunkText;
}

void QOnlineTts::cacheBingAudio(const QString &key, QTemporaryFile *file)
{
    m_bingAudioCache.insert(key, {file, QDateTime::currentMSecsSinceEpoch()});
    m_bingAudioCacheOrder.append(key);

    if (m_bingAudioCacheOrder.size() > s_bingAudioCacheLimit) {
        const QString oldestKey = m_bingAudioCacheOrder.takeFirst();
        delete m_bingAudioCache.take(oldestKey).file;
    }
}

// Lazy TTL sweep: called at the start of every Bing generateUrls() so entries don't linger
// (and keep eating disk space) indefinitely just because the user hasn't spoken anything new.
void QOnlineTts::purgeExpiredBingAudio()
{
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - s_bingAudioCacheTtlMs;

    // Walked back-to-front purely so removeAt() below doesn't invalidate indices still to be
    // visited - every entry is still checked regardless of position, this isn't an early-exit.
    for (int i = m_bingAudioCacheOrder.size() - 1; i >= 0; --i) {
        const QString &key = m_bingAudioCacheOrder.at(i);
        const auto it = m_bingAudioCache.find(key);
        if (it == m_bingAudioCache.end() || it.value().cachedAtMs > cutoff)
            continue; // not expired (or already gone)

        delete it.value().file;
        m_bingAudioCache.erase(it);
        m_bingAudioCacheOrder.removeAt(i);
    }
}
