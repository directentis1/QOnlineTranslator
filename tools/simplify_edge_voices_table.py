#!/usr/bin/env python3
"""
Convert edge_voices_tables.json (full Edge TTS voice list) into a simplified
JSON list with just: name, locale, country, language, gender.

Usage:
    python simplify_voices.py input.json output.json
"""

import json
import re
import sys


def parse_locale_name(locale_name: str):
    """Split 'Afrikaans (South Africa)' into ('Afrikaans', 'South Africa')."""
    match = re.match(r"^(.*?)\s*\((.*)\)\s*$", locale_name.strip())
    if match:
        language, country = match.group(1).strip(), match.group(2).strip()
    else:
        # Fallback: no parentheses found, treat whole string as language
        language, country = locale_name.strip(), ""
    return language, country


def simplify_voices(voices: list) -> list:
    simplified = []
    for voice in voices:
        language, country = parse_locale_name(voice.get("LocaleName", ""))
        simplified.append({
            "name": voice.get("ShortName", ""),
            "locale": voice.get("Locale", ""),
            "country": country,
            "language": language,
            "gender": voice.get("Gender", ""),
        })
    return simplified


def main():
    if len(sys.argv) != 3:
        print("Usage: python simplify_voices.py <input.json> <output.json>")
        sys.exit(1)

    input_path, output_path = sys.argv[1], sys.argv[2]

    with open(input_path, "r", encoding="utf-8") as f:
        voices = json.load(f)

    simplified = simplify_voices(voices)

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(simplified, f, ensure_ascii=False, indent=2)

    print(f"Converted {len(simplified)} voices -> {output_path}")


if __name__ == "__main__":
    main()
