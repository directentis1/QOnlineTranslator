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

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QMetaEnum>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QtAlgorithms>

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

// clang-format off
// Ported from TranslateWebPage's textToSpeech.js (BingHelper.getLanguageData()). Only languages
// Bing's TTS actually supports appear here; anything else falls back to UnsupportedLanguage.
// Note this is keyed by QOnlineTranslator::Language rather than the raw code strings the JS
// source used, so no separate "which code alias means which language" table is needed here -
// QOnlineTranslator::languageApiCode(Bing, ...) already resolves that ambiguity for us.
const QMap<QOnlineTranslator::Language, QOnlineTts::BingVoiceData> QOnlineTts::s_bingVoices = {
    {QOnlineTranslator::Afrikaans, {QStringLiteral("af-ZA"), QStringLiteral("Female"), QStringLiteral("af-ZA-AdriNeural")}},
    {QOnlineTranslator::Amharic, {QStringLiteral("am-ET"), QStringLiteral("Female"), QStringLiteral("am-ET-MekdesNeural")}},
    {QOnlineTranslator::Arabic, {QStringLiteral("ar-SA"), QStringLiteral("Male"), QStringLiteral("ar-SA-HamedNeural")}},
    {QOnlineTranslator::Bengali, {QStringLiteral("bn-IN"), QStringLiteral("Female"), QStringLiteral("bn-IN-TanishaaNeural")}},
    {QOnlineTranslator::Bulgarian, {QStringLiteral("bg-BG"), QStringLiteral("Male"), QStringLiteral("bg-BG-BorislavNeural")}},
    {QOnlineTranslator::Catalan, {QStringLiteral("ca-ES"), QStringLiteral("Female"), QStringLiteral("ca-ES-JoanaNeural")}},
    {QOnlineTranslator::Czech, {QStringLiteral("cs-CZ"), QStringLiteral("Male"), QStringLiteral("cs-CZ-AntoninNeural")}},
    {QOnlineTranslator::Welsh, {QStringLiteral("cy-GB"), QStringLiteral("Female"), QStringLiteral("cy-GB-NiaNeural")}},
    {QOnlineTranslator::Danish, {QStringLiteral("da-DK"), QStringLiteral("Female"), QStringLiteral("da-DK-ChristelNeural")}},
    {QOnlineTranslator::German, {QStringLiteral("de-DE"), QStringLiteral("Female"), QStringLiteral("de-DE-KatjaNeural")}},
    {QOnlineTranslator::Greek, {QStringLiteral("el-GR"), QStringLiteral("Male"), QStringLiteral("el-GR-NestorasNeural")}},
    {QOnlineTranslator::English, {QStringLiteral("en-US"), QStringLiteral("Female"), QStringLiteral("en-US-AriaNeural")}},
    {QOnlineTranslator::Spanish, {QStringLiteral("es-ES"), QStringLiteral("Female"), QStringLiteral("es-ES-ElviraNeural")}},
    {QOnlineTranslator::Estonian, {QStringLiteral("et-EE"), QStringLiteral("Female"), QStringLiteral("et-EE-AnuNeural")}},
    {QOnlineTranslator::Persian, {QStringLiteral("fa-IR"), QStringLiteral("Female"), QStringLiteral("fa-IR-DilaraNeural")}},
    {QOnlineTranslator::Finnish, {QStringLiteral("fi-FI"), QStringLiteral("Female"), QStringLiteral("fi-FI-NooraNeural")}},
    {QOnlineTranslator::French, {QStringLiteral("fr-FR"), QStringLiteral("Female"), QStringLiteral("fr-FR-DeniseNeural")}},
    {QOnlineTranslator::Irish, {QStringLiteral("ga-IE"), QStringLiteral("Female"), QStringLiteral("ga-IE-OrlaNeural")}},
    {QOnlineTranslator::Gujarati, {QStringLiteral("gu-IN"), QStringLiteral("Female"), QStringLiteral("gu-IN-DhwaniNeural")}},
    {QOnlineTranslator::Hebrew, {QStringLiteral("he-IL"), QStringLiteral("Male"), QStringLiteral("he-IL-AvriNeural")}},
    {QOnlineTranslator::Hindi, {QStringLiteral("hi-IN"), QStringLiteral("Female"), QStringLiteral("hi-IN-SwaraNeural")}},
    {QOnlineTranslator::Croatian, {QStringLiteral("hr-HR"), QStringLiteral("Male"), QStringLiteral("hr-HR-SreckoNeural")}},
    {QOnlineTranslator::Hungarian, {QStringLiteral("hu-HU"), QStringLiteral("Male"), QStringLiteral("hu-HU-TamasNeural")}},
    {QOnlineTranslator::Indonesian, {QStringLiteral("id-ID"), QStringLiteral("Male"), QStringLiteral("id-ID-ArdiNeural")}},
    {QOnlineTranslator::Icelandic, {QStringLiteral("is-IS"), QStringLiteral("Female"), QStringLiteral("is-IS-GudrunNeural")}},
    {QOnlineTranslator::Italian, {QStringLiteral("it-IT"), QStringLiteral("Male"), QStringLiteral("it-IT-DiegoNeural")}},
    {QOnlineTranslator::Japanese, {QStringLiteral("ja-JP"), QStringLiteral("Female"), QStringLiteral("ja-JP-NanamiNeural")}},
    {QOnlineTranslator::Kazakh, {QStringLiteral("kk-KZ"), QStringLiteral("Female"), QStringLiteral("kk-KZ-AigulNeural")}},
    {QOnlineTranslator::Khmer, {QStringLiteral("km-KH"), QStringLiteral("Female"), QStringLiteral("km-KH-SreymomNeural")}},
    {QOnlineTranslator::Kannada, {QStringLiteral("kn-IN"), QStringLiteral("Female"), QStringLiteral("kn-IN-SapnaNeural")}},
    {QOnlineTranslator::Korean, {QStringLiteral("ko-KR"), QStringLiteral("Female"), QStringLiteral("ko-KR-SunHiNeural")}},
    {QOnlineTranslator::Lao, {QStringLiteral("lo-LA"), QStringLiteral("Female"), QStringLiteral("lo-LA-KeomanyNeural")}},
    {QOnlineTranslator::Latvian, {QStringLiteral("lv-LV"), QStringLiteral("Female"), QStringLiteral("lv-LV-EveritaNeural")}},
    {QOnlineTranslator::Lithuanian, {QStringLiteral("lt-LT"), QStringLiteral("Female"), QStringLiteral("lt-LT-OnaNeural")}},
    {QOnlineTranslator::Macedonian, {QStringLiteral("mk-MK"), QStringLiteral("Female"), QStringLiteral("mk-MK-MarijaNeural")}},
    {QOnlineTranslator::Malayalam, {QStringLiteral("ml-IN"), QStringLiteral("Female"), QStringLiteral("ml-IN-SobhanaNeural")}},
    {QOnlineTranslator::Marathi, {QStringLiteral("mr-IN"), QStringLiteral("Female"), QStringLiteral("mr-IN-AarohiNeural")}},
    {QOnlineTranslator::Malay, {QStringLiteral("ms-MY"), QStringLiteral("Male"), QStringLiteral("ms-MY-OsmanNeural")}},
    {QOnlineTranslator::Maltese, {QStringLiteral("mt-MT"), QStringLiteral("Female"), QStringLiteral("mt-MT-GraceNeural")}},
    {QOnlineTranslator::Myanmar, {QStringLiteral("my-MM"), QStringLiteral("Female"), QStringLiteral("my-MM-NilarNeural")}},
    {QOnlineTranslator::Dutch, {QStringLiteral("nl-NL"), QStringLiteral("Female"), QStringLiteral("nl-NL-ColetteNeural")}},
    {QOnlineTranslator::Norwegian, {QStringLiteral("nb-NO"), QStringLiteral("Female"), QStringLiteral("nb-NO-PernilleNeural")}},
    {QOnlineTranslator::Polish, {QStringLiteral("pl-PL"), QStringLiteral("Female"), QStringLiteral("pl-PL-ZofiaNeural")}},
    {QOnlineTranslator::Pashto, {QStringLiteral("ps-AF"), QStringLiteral("Female"), QStringLiteral("ps-AF-LatifaNeural")}},
    {QOnlineTranslator::Portuguese, {QStringLiteral("pt-BR"), QStringLiteral("Female"), QStringLiteral("pt-BR-FranciscaNeural")}},
    {QOnlineTranslator::Romanian, {QStringLiteral("ro-RO"), QStringLiteral("Male"), QStringLiteral("ro-RO-EmilNeural")}},
    {QOnlineTranslator::Russian, {QStringLiteral("ru-RU"), QStringLiteral("Female"), QStringLiteral("ru-RU-DariyaNeural")}},
    {QOnlineTranslator::Slovak, {QStringLiteral("sk-SK"), QStringLiteral("Male"), QStringLiteral("sk-SK-LukasNeural")}},
    {QOnlineTranslator::Slovenian, {QStringLiteral("sl-SI"), QStringLiteral("Male"), QStringLiteral("sl-SI-RokNeural")}},
    {QOnlineTranslator::SerbianCyrillic, {QStringLiteral("sr-RS"), QStringLiteral("Female"), QStringLiteral("sr-RS-SophieNeural")}},
    {QOnlineTranslator::Swedish, {QStringLiteral("sv-SE"), QStringLiteral("Female"), QStringLiteral("sv-SE-SofieNeural")}},
    {QOnlineTranslator::Tamil, {QStringLiteral("ta-IN"), QStringLiteral("Female"), QStringLiteral("ta-IN-PallaviNeural")}},
    {QOnlineTranslator::Telugu, {QStringLiteral("te-IN"), QStringLiteral("Male"), QStringLiteral("te-IN-ShrutiNeural")}},
    {QOnlineTranslator::Thai, {QStringLiteral("th-TH"), QStringLiteral("Male"), QStringLiteral("th-TH-NiwatNeural")}},
    {QOnlineTranslator::Turkish, {QStringLiteral("tr-TR"), QStringLiteral("Female"), QStringLiteral("tr-TR-EmelNeural")}},
    {QOnlineTranslator::Ukrainian, {QStringLiteral("uk-UA"), QStringLiteral("Female"), QStringLiteral("uk-UA-PolinaNeural")}},
    {QOnlineTranslator::Urdu, {QStringLiteral("ur-IN"), QStringLiteral("Female"), QStringLiteral("ur-IN-GulNeural")}},
    {QOnlineTranslator::Uzbek, {QStringLiteral("uz-UZ"), QStringLiteral("Female"), QStringLiteral("uz-UZ-MadinaNeural")}},
    {QOnlineTranslator::Vietnamese, {QStringLiteral("vi-VN"), QStringLiteral("Male"), QStringLiteral("vi-VN-NamMinhNeural")}},
    {QOnlineTranslator::SimplifiedChinese, {QStringLiteral("zh-CN"), QStringLiteral("Female"), QStringLiteral("zh-CN-XiaoxiaoNeural")}},
    {QOnlineTranslator::TraditionalChinese, {QStringLiteral("zh-CN"), QStringLiteral("Female"), QStringLiteral("zh-CN-XiaoxiaoNeural")}},
    {QOnlineTranslator::Cantonese, {QStringLiteral("zh-HK"), QStringLiteral("Female"), QStringLiteral("zh-HK-HiuGaaiNeural")}},
};
// clang-format on

QOnlineTts::QOnlineTts(QObject *parent)
    : QObject(parent)
{
}

void QOnlineTts::generateUrls(const QString &text, QOnlineTranslator::Engine engine, QOnlineTranslator::Language lang, Voice voice, Emotion emotion)
{
    // Drop whatever the previous call left behind. Needed because callers are expected to reuse
    // one QOnlineTts instance across many speak requests rather than making a new one each time
    // (Bing's temporary audio files must outlive the call that creates them - see the
    // m_bingAudioFiles comment in the header - so a fresh instance per call would have its files
    // deleted before playback finishes).
    m_media.clear();
    qDeleteAll(m_bingAudioFiles);
    m_bingAudioFiles.clear();
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

bool QOnlineTts::bingVoiceData(QOnlineTranslator::Language lang, BingVoiceData &voice)
{
    const auto it = s_bingVoices.constFind(lang);
    if (it == s_bingVoices.constEnd())
        return false;

    voice = it.value();
    return true;
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
    // Plain string formatting rather than QDomDocument, so this doesn't need the Qt Xml module.
    // voice.locale/gender/name only ever come from s_bingVoices (our own static table), so only
    // `text` (the only piece of untrusted/free-form content, and the only one that lands in
    // element content rather than an attribute) needs escaping.
    static const QString ssmlTemplate = QStringLiteral(
        "<speak version='1.0' xml:lang='%1'>"
        "<voice xml:lang='%1' xml:gender='%2' name='%3'>"
        "<prosody rate='+0.00%'>%4</prosody>"
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
    request.setHeader(QNetworkRequest::UserAgentHeader, QCoreApplication::applicationName() + '/' + QCoreApplication::applicationVersion());

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
    QUrl url(QStringLiteral("https://www.bing.com/tfettts"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("isVertical"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("IG"), s_bingIg);
    // Bing's own frontend increments the trailing counter for every request made in the same
    // page session; mirror that instead of hardcoding ".1" for every chunk.
    query.addQueryItem(QStringLiteral("IID"), QStringLiteral("%1.%2").arg(s_bingIid).arg(++s_bingRequestCounter));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::UserAgentHeader, QCoreApplication::applicationName() + '/' + QCoreApplication::applicationVersion());

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("ssml"), QString::fromUtf8(requestBody));
    form.addQueryItem(QStringLiteral("token"), QString::fromUtf8(s_bingToken));
    form.addQueryItem(QStringLiteral("key"), QString::fromUtf8(s_bingKey));

    QNetworkReply *reply = m_networkManager->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
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

    if (!ensureBingCredentials())
        return; // setError() was already called

    if (!m_networkManager)
        m_networkManager = new QNetworkAccessManager(this);

    for (const QString &chunk : splitTextForBing(text)) {
        const QByteArray ssml = buildBingSsml(chunk, voice);
        const QByteArray audio = postBingSpeech(ssml);
        if (audio.isEmpty())
            return; // setError() was already called by postBingSpeech()

        // Parented to `this` - see the m_bingAudioFiles comment in the header for the lifetime
        // implications of that.
        auto *file = new QTemporaryFile(this);
        file->setFileTemplate(QDir::tempPath() + QStringLiteral("/crow-translate-bing-tts-XXXXXX.mp3"));
        if (!file->open() || file->write(audio) != audio.size()) {
            setError(ServiceError, tr("Error: Unable to write Bing TTS audio to a temporary file"));
            delete file;
            return;
        }
        file->close();

        m_bingAudioFiles.append(file);
        m_media.append(QUrl::fromLocalFile(file->fileName()));
    }
}
