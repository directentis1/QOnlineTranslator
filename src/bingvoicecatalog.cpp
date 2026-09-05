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

#include "bingvoicecatalog.h"
#include "bingvoicecatalog_data.h"

#include <QMap>
#include <algorithm>

namespace
{
// Locale codes where QLocale can't disambiguate the way QOnlineTranslator::Language needs -
// Cantonese and Traditional Chinese both parse to plain QLocale::Chinese, same as Simplified
// Chinese. Anything not listed here falls through to QOnlineTranslator::language(QLocale).
QOnlineTranslator::Language resolveLanguage(const QString &localeCode)
{
    static const QMap<QString, QOnlineTranslator::Language> overrides = {
        {QStringLiteral("zh-HK"), QOnlineTranslator::Cantonese},
        {QStringLiteral("zh-TW"), QOnlineTranslator::TraditionalChinese},
        {QStringLiteral("zh-MO"), QOnlineTranslator::Cantonese},
    };

    const auto overrideIt = overrides.constFind(localeCode);
    if (overrideIt != overrides.constEnd())
        return overrideIt.value();

    QString name = localeCode;
    name.replace(QLatin1Char('-'), QLatin1Char('_'));
    return QOnlineTranslator::language(QLocale(name));
}

struct Index {
    QMap<QOnlineTranslator::Language, QVector<BingVoiceCatalog::CountryVariant>> byLanguage;
    QVector<QOnlineTranslator::Language> allLanguages; // sorted by display name
};

const Index &index()
{
    static const Index index = [] {
        Index built;

        for (const auto &row : BingVoiceCatalogData::kVoices) {
            const QOnlineTranslator::Language lang = resolveLanguage(QString::fromUtf8(row.locale));
            if (lang == QOnlineTranslator::NoLanguage)
                continue; // Not a language QOnlineTranslator knows about - nothing sensible to key it by

            QVector<BingVoiceCatalog::CountryVariant> &variants = built.byLanguage[lang];
            const QString locale = QString::fromUtf8(row.locale);

            auto variantIt = std::find_if(variants.begin(), variants.end(), [&locale](const auto &variant) {
                return variant.locale == locale;
            });
            if (variantIt == variants.end()) {
                QString localeName = locale;
                localeName.replace(QLatin1Char('-'), QLatin1Char('_'));
                variants.append({QLocale(localeName).country(), QString::fromUtf8(row.country), locale, {}});
                variantIt = variants.end() - 1;
            }

            const QString voiceName = QString::fromUtf8(row.name);
            const bool alreadyPresent = std::any_of(variantIt->voices.cbegin(), variantIt->voices.cend(), [&voiceName](const auto &voice) {
                return voice.name == voiceName;
            });
            if (!alreadyPresent)
                variantIt->voices.append({voiceName, QString::fromUtf8(row.gender)});
        }

        // Deterministic, friendly ordering: country variants alphabetically by display name, and
        // within each variant, Female voices first (matches defaultVoiceName()'s preference) then
        // alphabetically by name.
        for (auto it = built.byLanguage.begin(); it != built.byLanguage.end(); ++it) {
            std::sort(it->begin(), it->end(), [](const auto &a, const auto &b) {
                return a.countryName < b.countryName;
            });
            for (auto &variant : *it) {
                std::sort(variant.voices.begin(), variant.voices.end(), [](const auto &a, const auto &b) {
                    if (a.gender != b.gender)
                        return a.gender == QLatin1String("Female");
                    return a.name < b.name;
                });
            }
        }

        built.allLanguages = built.byLanguage.keys().toVector();
        std::sort(built.allLanguages.begin(), built.allLanguages.end(), [](auto a, auto b) {
            return QOnlineTranslator::languageName(a) < QOnlineTranslator::languageName(b);
        });

        return built;
    }();

    return index;
}
} // namespace

const QVector<QOnlineTranslator::Language> &BingVoiceCatalog::supportedLanguages()
{
    return index().allLanguages;
}

const QVector<QOnlineTranslator::Language> &BingVoiceCatalog::mainstreamLanguages()
{
    static const QVector<QOnlineTranslator::Language> mainstream = [] {
        QVector<QOnlineTranslator::Language> candidates = {
            QOnlineTranslator::English,
            QOnlineTranslator::French,
            QOnlineTranslator::Spanish,
            QOnlineTranslator::German,
            QOnlineTranslator::Italian,
            QOnlineTranslator::Vietnamese,
        };

        // Defensive: only keep languages the generated table actually has voices for, in case a
        // future regeneration drops one of these.
        const QVector<QOnlineTranslator::Language> &supported = supportedLanguages();
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                         [&supported](QOnlineTranslator::Language lang) {
                                             return !supported.contains(lang);
                                         }),
                         candidates.end());
        return candidates;
    }();

    return mainstream;
}

const QVector<BingVoiceCatalog::CountryVariant> &BingVoiceCatalog::countryVariants(QOnlineTranslator::Language lang)
{
    static const QVector<CountryVariant> empty;

    const auto &byLanguage = index().byLanguage;
    auto it = byLanguage.constFind(lang);
    if (it == byLanguage.constEnd() && lang == QOnlineTranslator::TraditionalChinese) {
        // Bing has no dedicated Traditional Chinese voice; reuse Simplified Chinese's, matching
        // the previous hardcoded behavior.
        it = byLanguage.constFind(QOnlineTranslator::SimplifiedChinese);
    }

    return it != byLanguage.constEnd() ? it.value() : empty;
}

QString BingVoiceCatalog::defaultVoiceName(QOnlineTranslator::Language lang)
{
    const QVector<CountryVariant> &variants = countryVariants(lang);
    if (variants.isEmpty() || variants.first().voices.isEmpty())
        return {};

    // Voices are pre-sorted Female-first, so the first entry of the first variant is the pick.
    return variants.first().voices.first().name;
}

bool BingVoiceCatalog::voiceInfo(QOnlineTranslator::Language lang, const QString &voiceName, QString &locale, QString &gender)
{
    for (const CountryVariant &variant : countryVariants(lang)) {
        for (const VoiceInfo &voice : variant.voices) {
            if (voice.name == voiceName) {
                locale = variant.locale;
                gender = voice.gender;
                return true;
            }
        }
    }

    return false;
}
