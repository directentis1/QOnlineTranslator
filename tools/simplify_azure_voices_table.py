#!/usr/bin/env python3
"""
Convert Azure TTS voices JSON (azure_voices_tables.json) into a simplified
JSON containing only: name, locale, country, language, gender.

Usage:
    python simplify_azure_voices.py azure_voices_tables.json azure_voices_simplified.json

Requires:
    pip install babel
"""

import json
import sys
from babel import Locale, UnknownLocaleError

# Manual overrides for locales Babel can't resolve or names you want to
# customize (e.g. macrolanguage codes, script variants, etc.)
LOCALE_OVERRIDES = {
    # "example-Locale": {"country": "Some Country", "language": "Some Language"},
    "iu-Cans-CA": {"country": "Canada", "language": "Inuktitut (Canadian Aboriginal Syllabics)"},
    "iu-Latn-CA": {"country": "Canada", "language": "Inuktitut (Latin)"},
    "wuu-CN": {"country": "China", "language": "Wu Chinese"},
}

# English locale used to look up display names, so results are always in
# English regardless of the locale being resolved (e.g. "South Africa" not
# "Suid-Afrika", "Chinese" not "中文").
_EN = Locale("en")

# Cache so we only resolve each locale once
_locale_cache = {}


def resolve_locale(locale_str):
    """Return (country, language) English display names for a BCP-47 locale string."""
    if locale_str in _locale_cache:
        return _locale_cache[locale_str]

    if locale_str in LOCALE_OVERRIDES:
        override = LOCALE_OVERRIDES[locale_str]
        result = (override.get("country"), override.get("language"))
        _locale_cache[locale_str] = result
        return result

    country, language = None, None
    try:
        # Babel expects underscores, e.g. "af_ZA" instead of "af-ZA"
        babel_locale = Locale.parse(locale_str.replace("-", "_"))
        language = _EN.languages.get(babel_locale.language)
        if babel_locale.territory:
            country = _EN.territories.get(babel_locale.territory)
    except (UnknownLocaleError, ValueError):
        pass

    _locale_cache[locale_str] = (country, language)
    return country, language


def simplify_voices(voices):
    simplified = []
    unresolved = []

    for voice in voices:
        short_name = voice.get("shortName", "")
        locale = voice.get("locale", "")
        gender = voice.get("properties", {}).get("Gender", "")

        country, language = resolve_locale(locale)
        if country is None or language is None:
            unresolved.append(locale)

        simplified.append({
            "name": short_name,
            "locale": locale,
            "country": country,
            "language": language,
            "gender": gender,
        })

    if unresolved:
        uniq = sorted(set(unresolved))
        print(f"Warning: could not fully resolve {len(uniq)} locale(s): {uniq}",
              file=sys.stderr)
        print("Add entries for these to LOCALE_OVERRIDES in this script if needed.",
              file=sys.stderr)

    return simplified


def main():
    if len(sys.argv) != 3:
        print("Usage: python simplify_azure_voices.py <input.json> <output.json>",
              file=sys.stderr)
        sys.exit(1)

    input_path, output_path = sys.argv[1], sys.argv[2]

    with open(input_path, "r", encoding="utf-8") as f:
        voices = json.load(f)

    simplified = simplify_voices(voices)

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(simplified, f, indent=2, ensure_ascii=False)

    print(f"Wrote {len(simplified)} voices to {output_path}")


if __name__ == "__main__":
    main()
