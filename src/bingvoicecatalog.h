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

#ifndef BINGVOICECATALOG_H
#define BINGVOICECATALOG_H

#include "qonlinetranslator.h"

#include <QLocale>
#include <QString>
#include <QVector>

/**
 * @brief Read-only, lazily-built index over the generated Bing TTS voice table.
 *
 * bingvoicecatalog_data.h (produced by tools/generate_bing_voice_catalog.py from Microsoft's
 * published voice list) is a flat array of {name, locale, country, language, gender} rows with no
 * structure at all. This class groups those rows into the
 * language -> country/regional variant -> voice hierarchy the settings dialog needs, and is the
 * single place that knows about naming edge cases the flat table can't represent on its own
 * (Cantonese and Traditional Chinese both share a QLocale/QOnlineTranslator::Language ambiguity -
 * see resolveLanguage() in the .cpp). None of this needs to change when the data file is
 * regenerated from a fresh Microsoft export.
 *
 * The index is built once, on first use, and cached for the lifetime of the process.
 */
class BingVoiceCatalog
{
public:
    struct VoiceInfo {
        QString name; // e.g. "en-US-AriaNeural"
        QString gender; // "Male" or "Female", as published by Microsoft
    };

    struct CountryVariant {
        QLocale::Country country = QLocale::AnyCountry;
        QString countryName; // Display string from the table, e.g. "South Africa"
        QString locale; // BCP-47 code for this variant, e.g. "af-ZA"
        QVector<VoiceInfo> voices; // Every voice offered for this language + country, table order
    };

    /**
     * @brief Every language Bing TTS has at least one voice for, sorted by display name.
     */
    static const QVector<QOnlineTranslator::Language> &supportedLanguages();

    /**
     * @brief Small curated subset shown directly in the language dropdown. Everything else is
     * reached through the dropdown's "Other language..." entry, which searches supportedLanguages().
     */
    static const QVector<QOnlineTranslator::Language> &mainstreamLanguages();

    /**
     * @brief Regional/country variants available for a language, in table order.
     *
     * Falls back from TraditionalChinese to SimplifiedChinese's variants when Bing has no
     * Traditional-Chinese-specific voice (matching the previous hardcoded behavior, where both
     * shared the zh-CN voice) - see the .cpp for details.
     */
    static const QVector<CountryVariant> &countryVariants(QOnlineTranslator::Language lang);

    /**
     * @brief Best-effort default voice name for a language: first Female voice in table order, or
     * the first voice at all if the language has no Female voice. Used both as the built-in
     * fallback when the user hasn't picked anything yet, and to populate "reset to default".
     */
    static QString defaultVoiceName(QOnlineTranslator::Language lang);

    /**
     * @brief Locale + gender for a specific voice name, as needed to build the Bing SSML request.
     * @return `false` if the voice name is unknown for this language (e.g. it was renamed/removed
     * in a later Bing voice list than the one the catalog was generated from), letting the caller
     * fall back to defaultVoiceName() instead of failing outright.
     */
    static bool voiceInfo(QOnlineTranslator::Language lang, const QString &voiceName, QString &locale, QString &gender);

private:
    BingVoiceCatalog() = default;
};

#endif // BINGVOICECATALOG_H
