# Bing TTS voice picker - integration notes

This adds a full language -> region -> gender -> voice picker for the Bing speech engine to
Settings -> Speech synthesis, backed by a generated, easy-to-regenerate voice catalog instead of
hand-maintained C++ tables.

## Files

```
tools/
  generate_bing_voice_catalog.py   Converts bing-tts-voice-table.json -> bingvoicecatalog_data.h
  sample-bing-tts-voice-table.json Small sample input (mainstream languages + a few others) used
                                    to test the generator - NOT a full voice list, see below.

src/qonlinetranslator/
  bingvoicecatalog_data.h  GENERATED from the sample JSON above. Regenerate this from your real,
                            full bing-tts-voice-table.json before building (see below).
  bingvoicecatalog.h/.cpp  Hand-written. Turns the flat generated table into the
                            language -> region -> gender -> voice hierarchy the settings dialog
                            uses. This is the only place that needs to change if Bing's naming
                            grows a new edge case - never the generated data file.
  qonlinetts.h/.cpp        Modified: the old hardcoded ~70-entry s_bingVoices map is gone.
                            QOnlineTts now takes a per-language voice-name preference map
                            (bingVoicePreferences()/setBingVoicePreferences()) and resolves the
                            actual voice through BingVoiceCatalog, falling back to the catalog's
                            default voice for a language if nothing was configured.

src/
  speakbuttons.h/.cpp      Modified: SpeakButtons (used by both the settings dialog's test-speech
                            widget and, potentially, other TTS UI) grew the same
                            bingVoicePreferences()/setBingVoicePreferences() pair, mirroring the
                            existing regions() mechanism for Google, and passes it to QOnlineTts.

src/settings/
  appsettings.h/.cpp       Modified: persists the per-language Bing voice map under
                            "TTS/BingVoices", the same way Google's per-language region map is
                            stored under "TTS/GoogleRegions".
  settingsdialog.h/.cpp/.ui  Modified: adds the "Bing" group box next to the existing Yandex/Google
                            ones, with Language / Region / Gender / Voice combo boxes plus a test-
                            speech row, matching the existing widgets' look and wiring style.
```

## Regenerating the real catalog

The sample JSON only has a couple dozen entries (enough to prove the pipeline end-to-end). Before
building for real, run the generator against your actual, full `bing-tts-voice-table.json`:

```bash
python3 tools/generate_bing_voice_catalog.py bing-tts-voice-table.json \
    -o src/qonlinetranslator/bingvoicecatalog_data.h
```

Re-run this any time Microsoft's voice list changes - nothing else needs to be touched.

## CMakeLists changes needed

I don't have `src/qonlinetranslator/CMakeLists.txt` (the submodule's own build file) to edit
directly, so add these two lines to its source list yourself:

```cmake
add_library(QOnlineTranslator ...
    ...
    bingvoicecatalog.cpp
    bingvoicecatalog.h        # optional to list, but nice for IDEs; no Q_OBJECT, so no moc needed
    ...
)
```

`bingvoicecatalog_data.h` doesn't need to be listed (it's only `#include`d by bingvoicecatalog.cpp),
but you may want it there too for IDE visibility.

## How the language dropdown's "Other language..." works

`bingLanguageComboBox` is seeded once (in the constructor) with `BingVoiceCatalog::mainstreamLanguages()`
(English, French, Spanish, German, Italian, Vietnamese - filtered defensively against whatever the
generated table actually contains), then a separator, then a sentinel "Other language..." row.

Picking that row opens a small ad-hoc search dialog (`SettingsDialog::pickOtherBingLanguage()`,
built directly in code rather than as a `.ui` - it's just a `QLineEdit` filter over a `QListWidget`)
listing every language `BingVoiceCatalog::supportedLanguages()` has a voice for. Picking one there
inserts it into the main combo box (just above the separator) so it stays reachable without
searching again, and any language you'd previously configured a voice for is restored into the
combo the same way when settings are loaded.

## Known simplifications / things worth double-checking

- **Cantonese / Traditional Chinese**: `QLocale` can't tell `zh-HK`/`zh-TW` apart from plain
  Chinese, so `bingvoicecatalog.cpp` has a small manual override table for just those codes
  (`resolveLanguage()`). If Bing ever adds more genuinely ambiguous locale codes, that's the one
  place to extend - never the generated data file.
- **No Traditional Chinese voice**: if your real voice list has no `zh-TW` entry (Microsoft's
  Neural list currently doesn't), `countryVariants(TraditionalChinese)` transparently falls back to
  `SimplifiedChinese`'s variants, mirroring the old hardcoded behavior of sharing the zh-CN voice.
- **Voice display names**: the voice combo box strips the leading `xx-XX-` locale prefix from each
  voice name for readability (e.g. `en-US-AriaNeural` -> `AriaNeural`). Adjust
  `onBingGenderSelectionChanged()` in `settingsdialog.cpp` if you'd rather show the full name.
- **Compile verification**: I test-compiled `bingvoicecatalog.{h,cpp}`, `qonlinetts.{h,cpp}`, and
  `speakbuttons.{h,cpp}` standalone against your real `qonlinetranslator.h` with Qt 5.15 headers
  (stubbing only the two unrelated submodule headers, `qexample.h`/`qoption.h`, that weren't
  uploaded) - all clean. `settingsdialog.cpp` has ~80 unrelated widgets from the rest of the dialog
  that I didn't have headers for (`ocr.h`, `shortcutsmodel/*.h`, etc.), so I wasn't able to do a
  full mock-compile of that file; instead I cross-checked every new widget name and slot signature
  against the `.ui` file by hand (see the grep output in-conversation) and traced each new function
  against the existing Google/DeepL cascade code it's modeled on. Please do a real build early to
  catch anything a stub couldn't.
