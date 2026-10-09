#pragma once

#include <JuceHeader.h>
#include <map>
#include "Speech.h"

// Der Song-Generator: baut aus Zufallszahl (Seed) und den Reglern Länge, Intensität, Nähe zum Original
// und Abwechslung einen kompletten Elektro-Pop-Song für sieben Spuren, inklusive Roboterstimme.
// Alles wird nach Stilregeln der deutschen Elektronik der Siebziger und Achtziger neu erfunden; es werden keine
// Melodien, Akkordfolgen, Texte oder Klänge aus Originalaufnahmen übernommen. "Nähe zum Original" steuert nur,
// wie oft der Generator zu den typischen Stilmitteln greift (Sequenzer-Ostinato, Oktav-Bass, minimale Akkorde,
// elektronische Percussion, Maschinengeräusche, gesprochene Technikwörter und Refrain-Wort) statt frei zu würfeln.
// Die eingebauten Wörter sind neutrale Technikbegriffe, Zahlen oder Lautmalerei; eigener Text wird so gesprochen,
// wie er eingetippt wurde.
namespace Kt
{
    enum Lane { Pad, Seq, Bass, Lead, Vocal, Drums, Fx, numLanes };
    enum DrumVoice { Kick, Snare, HatClosed, HatOpen, Clap, Clank, Tom, Blip, numDrums };
    enum FxType { Static, Swell, Train, Car, Geiger, Morse, Data, numFx };
    enum SectionType { Intro, Verse, PreChorus, Chorus, Interlude, Bridge, Break, Outro, numSectionTypes };
    enum Era { Motorik74, Radio75, Rails77, Machines78, Computer81, Digital86, numEras };
    enum TextStyle { Technik, Numbers, Onomatopoeia, OwnText, numTextStyles };

    inline juce::String sectionName (int type)
    {
        static const char* names[] { "Intro", "Strophe", "Überleitung", "Refrain", "Thema", "Bridge", "Break", "Outro" };
        return juce::String::fromUTF8 (names[juce::jlimit (0, (int) numSectionTypes - 1, type)]);
    }

    // Großschreibung mit Umlauten (toUpperCase kennt sie je nach System nicht)
    inline juce::String upper (const juce::String& text)
    {
        return text.toUpperCase().replace (juce::String::fromUTF8 ("ä"), juce::String::fromUTF8 ("Ä"))
                                 .replace (juce::String::fromUTF8 ("ö"), juce::String::fromUTF8 ("Ö"))
                                 .replace (juce::String::fromUTF8 ("ü"), juce::String::fromUTF8 ("Ü"));
    }

    inline juce::String laneName (int lane)
    {
        static const char* names[] { "Flächen", "Sequenz", "Bass", "Melodie", "Stimme", "Rhythmus", "Geräusche" };
        return juce::String::fromUTF8 (names[juce::jlimit (0, (int) numLanes - 1, lane)]);
    }

    inline juce::String fxName (int type)
    {
        static const char* names[] { "Funkrauschen", "Anlauf", "Zug", "Autobahn", "Zählrohr", "Morsezeichen", "Datenpiepser" };
        return juce::String::fromUTF8 (names[juce::jlimit (0, (int) numFx - 1, type)]);
    }

    // Kurzbeschreibung der Epochen (für Steckbrief und Handbuch)
    inline juce::String eraDescription (int era)
    {
        static const char* text[] {
            "unterwegs: hüpfender Oktav-Bass, Flöte und Orgel, sanfte Elektro-Drums, Autos ziehen vorbei",
            "Funkwellen: Chorflächen, langsame Pulse, Zählrohr-Knacken und Morsezeichen",
            "auf Schienen: Metall-Percussion im Fahrrhythmus, Streicherflächen, Zuggeräusche",
            "Maschinenpark: Sechzehntel-Sequenzen, Syndrum-Toms, Vocoder",
            "Rechenzentrum: Piepser-Melodien, Sprachchip, Datengeräusche, knackige Snare",
            "digital: harte Drums mit Klatschen und Metall, Glocken, Sprechrhythmus",
        };
        return juce::String::fromUTF8 (text[juce::jlimit (0, (int) numEras - 1, era)]);
    }

    inline constexpr int scaleTable[5][7] {
        { 0, 2, 3, 5, 7, 8, 10 },   // Moll (äolisch)
        { 0, 2, 3, 5, 7, 9, 10 },   // Dorisch
        { 0, 1, 3, 5, 7, 8, 10 },   // Phrygisch
        { 0, 2, 4, 5, 7, 9, 11 },   // Dur
        { 0, 2, 4, 5, 7, 9, 10 },   // Mixolydisch
    };

    // Noten in Vierteln (Beats). Bei Rhythmus = Instrument, bei Geräuschen = FxType (syllable = Grundton).
    // Stimme: syllable = Index in Song::syllables, harmony = Zweitstimme
    struct Note
    {
        double start = 0.0, length = 0.0;
        int pitch = 0;
        float velocity = 1.0f;
        int syllable = -1;
        bool harmony = false;
        bool wordEnd = false;   // letzte Silbe eines Worts (für die Textanzeige)
    };

    struct Settings
    {
        juce::uint32 seed = 1, noteSeed = 1;
        float lengthMinutes = 4.0f, intensity = 0.6f, closeness = 0.8f, variety = 0.5f;
        int era = Machines78, key = 9, scale = 0;  // scale: 0 = Auto, sonst 1..5
        int textStyle = Technik;
        juce::String ownText;                      // eigener Text (bei textStyle == OwnText)
        float tempo = 120.0f;
        std::vector<float> intensityOverride;     // je Abschnitt, < 0 = automatisch
        std::vector<int> sectionSalt;             // je Abschnitt, > 0 = neu gewürfelt
    };

    struct Section
    {
        int type = Intro, startBar = 0, bars = 8;
        int theme = 0, variant = 0;
        float intensity = 0.5f;
        bool overridden = false, rerolled = false;
        std::array<bool, numLanes> lanes {};
        std::array<int, 4> progression {};
        int chordBars = 2;
        int transpose = 0;
        float filterFrom = 0.5f, filterTo = 0.5f;   // Sequenz-Filterfahrt (0..1)
        float fadeFrom = 1.0f, fadeTo = 1.0f;       // Gesamtlautstärke (Ein- und Ausblenden)

        double startBeat() const { return startBar * 4.0; }
        double endBeat() const   { return (startBar + bars) * 4.0; }
    };

    struct Song
    {
        Settings settings;
        double tempo = 120.0;
        int root = 0, scaleIndex = 0;
        std::array<int, 7> scale {};
        int totalBars = 0;
        juce::String title, hookWord;
        std::vector<Section> sections;
        std::array<std::vector<Note>, numLanes> notes;
        std::vector<Phon::Syl> syllables;          // Laute jeder gesprochenen Silbe
        std::vector<juce::String> syllableText;    // Schreibweise jeder Silbe
        float typicality = 0.0f;    // Anteil der stiltypischen Entscheidungen (0..1)

        double totalBeats() const { return totalBars * 4.0; }

        int sectionIndexAt (double beat) const
        {
            for (int i = (int) sections.size(); --i >= 0;)
                if (beat >= sections[(size_t) i].startBeat())
                    return i;
            return 0;
        }

        juce::String syllableAt (int index) const
        {
            return index >= 0 && index < (int) syllableText.size() ? syllableText[(size_t) index] : juce::String();
        }

        // Silbe eintragen (gleiche Laute und Schreibweise = gleicher Index)
        int addSyllable (const Phon::SylText& s)
        {
            for (size_t i = 0; i < syllables.size(); ++i)
                if (syllables[i] == s.syl && syllableText[i] == s.text)
                    return (int) i;
            syllables.push_back (s.syl);
            syllableText.push_back (s.text);
            return (int) syllables.size() - 1;
        }

        // Filterfahrt bzw. Blende am Ort 'beat'
        float automation (double beat, bool filter) const
        {
            if (sections.empty())
                return filter ? 0.5f : 1.0f;
            const auto& s = sections[(size_t) sectionIndexAt (beat)];
            const float t = (float) juce::jlimit (0.0, 1.0, (beat - s.startBeat()) / juce::jmax (1.0, s.endBeat() - s.startBeat()));
            return filter ? s.filterFrom + (s.filterTo - s.filterFrom) * t
                          : s.fadeFrom + (s.fadeTo - s.fadeFrom) * t;
        }

        int pitchClassOffset (double beat) const   // Tonart + Rückung des Abschnitts
        {
            return sections.empty() ? root : root + sections[(size_t) sectionIndexAt (beat)].transpose;
        }

        size_t noteCount() const
        {
            size_t n = 0;
            for (auto& v : notes) n += v.size();
            return n;
        }

        // Prüfsumme über alle Noten (für Selbsttests: gleicher Seed = gleicher Song)
        juce::uint64 fingerprint() const
        {
            juce::uint64 h = 1469598103934665603ull;
            auto mix = [&h] (juce::uint64 v) { h = (h ^ v) * 1099511628211ull; };
            for (int l = 0; l < numLanes; ++l)
                for (auto& n : notes[(size_t) l])
                {
                    mix ((juce::uint64) l);
                    mix ((juce::uint64) juce::roundToInt (n.start * 960.0));
                    mix ((juce::uint64) juce::roundToInt (n.length * 960.0));
                    mix ((juce::uint64) (n.pitch + 1000));
                    mix ((juce::uint64) juce::roundToInt (n.velocity * 1000.0f));
                    mix ((juce::uint64) (n.syllable + 5000 + (n.harmony ? 100000 : 0)));
                }
            return h;
        }
    };

    //==========================================================================
    namespace detail
    {
        inline juce::uint64 splitmix (juce::uint64 x)
        {
            x += 0x9E3779B97F4A7C15ull;
            x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
            x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
            return x ^ (x >> 31);
        }

        inline juce::uint64 hash (juce::uint64 a, juce::uint64 b = 0, juce::uint64 c = 0, juce::uint64 d = 0)
        {
            return splitmix (splitmix (splitmix (splitmix (a) ^ b) ^ c) ^ d);
        }

        struct Rng
        {
            explicit Rng (juce::uint64 seed) : state (seed) {}
            float next()               { state = splitmix (state); return (float) ((state >> 40) & 0xffffff) / 16777216.0f; }
            int nextInt (int n)        { return juce::jlimit (0, juce::jmax (0, n - 1), (int) (next() * (float) n)); }
            bool chance (float p)      { return next() < p; }

            // gewichtete Auswahl
            int weighted (std::initializer_list<float> w)
            {
                float sum = 0.0f;
                for (auto x : w) sum += x;
                float r = next() * sum;
                int i = 0;
                for (auto x : w)
                {
                    if (r < x) return i;
                    r -= x;
                    ++i;
                }
                return (int) w.size() - 1;
            }

            juce::uint64 state;
        };

        inline int floorDiv (int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
        inline int mod (int a, int b)      { return ((a % b) + b) % b; }

        // Minimale Akkordfolgen (Stufen 0..6 der Skala): oft nur zwei Akkorde im Wechsel oder ein langer Grundton
        inline constexpr int majorTemplates[][4] {
            { 0, 0, 3, 3 }, { 0, 3, 0, 4 }, { 0, 4, 0, 4 }, { 0, 0, 4, 4 }, { 0, 5, 3, 4 },
            { 0, 3, 4, 0 }, { 0, 1, 0, 1 }, { 3, 3, 0, 0 }, { 0, 0, 0, 3 }, { 0, 6, 0, 6 },
        };
        inline constexpr int majorChorusTemplates[][4] {
            { 3, 4, 0, 0 }, { 0, 4, 3, 0 }, { 5, 3, 0, 4 }, { 0, 3, 0, 4 }, { 3, 0, 3, 4 },
        };
        inline constexpr int minorTemplates[][4] {
            { 0, 0, 5, 5 }, { 0, 5, 0, 5 }, { 0, 6, 0, 6 }, { 0, 3, 0, 3 }, { 0, 0, 3, 4 },
            { 0, 5, 6, 0 }, { 0, 2, 5, 6 }, { 0, 0, 0, 6 }, { 0, 3, 5, 6 }, { 5, 6, 0, 0 },
        };
        inline constexpr int minorChorusTemplates[][4] {
            { 5, 6, 0, 0 }, { 0, 6, 5, 6 }, { 5, 5, 0, 0 }, { 3, 4, 0, 0 }, { 0, 5, 3, 6 },
        };

        // Sequenzer-Figuren: 0..3 = Grundton, Terz, Quinte, Oktave; 4..7 = eine Oktave höher; '.' = Pause
        inline const char* const riffTemplates[] {
            "0.4.0.4.0.4.0.4.",   // 0 Oktav-Hüpfer
            "0246024602460246",   // 1 Arpeggio aufwärts
            "0.0.4.0.2.0.4.0.",   // 2 Fahrt-Ostinato
            "0..0..4.0..0..2.",   // 3 Synkope
            "6420642064206420",   // 4 Arpeggio abwärts
            "0.2.4.6.4.2.0.2.",   // 5 Pendel
            "0404040404040404",   // 6 Sechzehntel-Oktaven
            "0..2..4..6..4..2",   // 7 Dreier-Gruppen
            "0.4.2.6.0.4.2.6.",   // 8 Rechner-Arpeggio
            "0...4...2...4...",   // 9 langsame Pulse
        };
        inline constexpr int riffByEra[numEras][4] { { 0, 9, 5, 2 }, { 9, 5, 3, 0 }, { 2, 3, 8, 0 }, { 1, 6, 4, 7 }, { 8, 1, 4, 6 }, { 3, 7, 8, 2 } };

        // Bass in Sechzehnteln: 0 Grundton, 1 Oktave, 5 Quinte, '-' gehalten, '.' Pause
        inline const char* const bassTemplates[] {
            "0.5.1.5.0.5.1.5.",   // 0 Hüpfer (Grundton, Quinte, Oktave)
            "0.0.0.0.0.0.0.0.",   // 1 gerade Achtel
            "0101010101010101",   // 2 Sechzehntel-Oktaven
            "0..0..0.0..0..0.",   // 3 Fahrrhythmus
            "0-------0-------",   // 4 lange Töne
            "0.0.1.0.5.0.1.0.",   // 5 Wechselbass
            "0..0..1.0..0..5.",   // 6 Synkope
            "0-0-1-0-0-0-1-5-",   // 7 Achtel mit Oktave
        };
        inline constexpr int bassByEra[numEras][3] { { 0, 5, 1 }, { 4, 1, 4 }, { 3, 1, 6 }, { 2, 6, 1 }, { 1, 7, 5 }, { 6, 3, 7 } };

        // Akkord-Stabs (Digital 1986): x = Anschlag
        inline const char* const stabTemplates[] {
            "x..x..x...x.....", "x.....x...x.....", "..x...x...x...x.", "x...x...x...x...",
        };

        // Elektronische Percussion (16 Schritte): Bassdrum, Snare, Hihat zu, Hihat offen, Klatschen, Metall, Tom, Piepser
        // x = voll, o = leise, . = nichts
        struct DrumTemplate { const char* rows[numDrums]; };
        inline const DrumTemplate drumTemplates[] {
            { { "x...x...x...x...", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", "................", "................", "................", "................" } }, // 0 Motorik
            { { "x.....x.x.......", "....x.......x...", "xoxoxoxoxoxoxoxo", "................", "................", "................", "............o...", "................" } }, // 1 Motorik locker
            { { "x.......x.......", "................", "o...o...o...o...", "................", "................", "................", "................", "..o.....o....o.." } }, // 2 Funk-Puls
            { { "x...x...x...x...", "........x.......", "................", "..o...o...o...o.", "................", "................", "............x...", "................" } }, // 3 Funk-Schritt
            { { "x..x....x..x....", "....x.......x...", "xxx.xxx.xxx.xxx.", "................", "................", "......x.......x.", "................", "................" } }, // 4 Schienen
            { { "x...x...x...x...", "....x.......x...", "xox.xox.xox.xox.", "................", "................", "..x...x...x...x.", "................", "................" } }, // 5 Schienen schnell
            { { "x...x...x...x...", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", "................", "................", "..........x.x.x.", "................" } }, // 6 Maschinen
            { { "x.....x...x.....", "....x.......x..o", "oooooooooooooooo", "................", "................", "................", "...x.......x....", "................" } }, // 7 Maschinen synkopiert
            { { "x...x...x...x...", "....x.......x...", "oxoxoxoxoxoxoxox", "................", "....x.......x...", "................", "................", ".x...x.....x..x." } }, // 8 Rechner
            { { "x..x..x...x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", "................", "................", "................", "...x...x.x...x.." } }, // 9 Rechner synkopiert
            { { "x..x..x...x.x...", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", "....x.......x...", "..x.......x.....", ".......x......x.", "................" } }, // 10 Digital
            { { "x...x...x..xx...", "....x.......x...", "oooooooooooooooo", "..x...x...x...x.", "............x...", "..........x.....", "................", "................" } }, // 11 Digital gerade
        };
        inline constexpr int drumByEra[numEras][3] { { 0, 1, 0 }, { 2, 3, 2 }, { 4, 5, 4 }, { 6, 7, 6 }, { 8, 9, 8 }, { 10, 11, 10 } };

        struct RiffStep { int tone = -1, octave = 0; bool accent = false; };
        struct BassStep { int kind = -1; bool tie = false; };   // kind: 0 Grundton, 1 Oktave, 2 Quinte
        struct MelNote  { double beat = 0.0, length = 1.0; int degree = 0; };
        struct VocNote  { double beat = 0.0, length = 1.0; int degree = 0; int syllable = -1; bool wordEnd = false; };

        //----------------------------------------------------------------------
        // Wortschatz der Stimme: neutrale Technikwörter, Zahlen, Lautmalerei oder eigener Text
        inline const char* const technikWords[] {
            "Strom", "Takt", "Daten", "Signal", "Impuls", "Kontakt", "System", "Energie", "Motor", "Maschine",
            "Programm", "Kabel", "Sender", "Kanal", "Netz", "Funk", "Gleis", "Modul", "Sensor", "Schaltung",
            "Speicher", "Schalter", "Welle", "Volt", "Watt", "Kilohertz", "Elektrik", "Diode", "Dynamo", "Matrix",
            "Logik", "Tempo", "Plus", "Minus", "Start", "Stopp", "Fabrik", "Station", "Turbine", "Sektor",
            "Rotor", "Kristall", "Prozessor", "Monitor", "Zentrale", "Kontrolle", "Analog", "Digital", "Elektron", "Laser",
            "Sonar", "Radar", "Silizium", "Phase", "Spannung", "Frequenz", "Ampere", "Schaltkreis", "Taktgeber", "Rechner",
        };
        inline const char* const numberWords[] { "null", "eins", "zwei", "drei", "vier", "fünf", "sechs", "sieben", "acht", "neun", "zehn", "elf", "zwölf" };
        inline const char* const soundWords[] { "bum", "tschak", "boing", "ping", "zisch", "tick", "tack", "dong", "bip", "klick" };

        struct Lexicon
        {
            std::vector<std::vector<int>> words;     // Silben-Indizes in Song::syllables
            std::vector<int> hook;                    // Refrain-Wort (Silben)
            std::vector<int> sequence;                // eigener Text: alle Silben der Reihe nach
            std::vector<bool> sequenceWordEnd;
            int style = Technik;
            juce::String hookText;
        };

        inline std::vector<int> addWord (Song& song, const Phon::Word& w)
        {
            std::vector<int> ids;
            for (auto& s : w) ids.push_back (song.addSyllable (s));
            return ids;
        }

        inline std::vector<int> addText (Song& song, const char* text)
        {
            const auto words = Phon::parseText (juce::String::fromUTF8 (text));
            return words.empty() ? std::vector<int> {} : addWord (song, words[0]);
        }

        inline juce::String joinText (const Song& song, const std::vector<int>& ids)
        {
            juce::String t;
            for (int i : ids) t << song.syllableAt (i);
            return t;
        }

        inline Lexicon makeLexicon (Song& song, juce::uint64 key, int style, const juce::String& ownText)
        {
            Rng r (hash (key, 31337));
            Lexicon lex;
            lex.style = style;
            if (style == OwnText)
            {
                const auto words = Phon::parseText (ownText);
                if (words.empty())
                    style = lex.style = Technik;
                else
                {
                    for (auto& w : words)
                    {
                        const auto ids = addWord (song, w);
                        lex.words.push_back (ids);
                        for (size_t k = 0; k < ids.size(); ++k)
                        {
                            lex.sequence.push_back (ids[k]);
                            lex.sequenceWordEnd.push_back (k + 1 == ids.size());
                        }
                    }
                    // Refrain: die ersten Wörter des Textes (höchstens vier Silben)
                    juce::StringArray hookWords;
                    for (auto& ids : lex.words)
                    {
                        if (! lex.hook.empty() && lex.hook.size() + ids.size() > 4) break;
                        lex.hook.insert (lex.hook.end(), ids.begin(), ids.end());
                        hookWords.add (joinText (song, ids));
                    }
                    lex.hookText = hookWords.joinIntoString (" ");
                    return lex;
                }
            }
            if (style == Onomatopoeia)
            {
                for (auto* w : soundWords) lex.words.push_back (addText (song, w));
                lex.hook = lex.words[(size_t) r.nextInt (3)];
                lex.hookText = joinText (song, lex.hook);
                return lex;
            }
            if (style == Numbers)
            {
                for (auto* w : numberWords) lex.words.push_back (addText (song, w));
                // Refrain: zwei Zahlen hintereinander
                const int a = 1 + r.nextInt (12), b = 1 + r.nextInt (12);
                lex.hook = lex.words[(size_t) a];
                lex.hook.insert (lex.hook.end(), lex.words[(size_t) b].begin(), lex.words[(size_t) b].end());
                lex.hookText = joinText (song, lex.words[(size_t) a]) + " " + joinText (song, lex.words[(size_t) b]);
                return lex;
            }
            // Technik: zwölf Wörter aus der Liste, Refrain-Wort mit zwei bis vier Silben
            std::vector<int> pool;
            for (int i = 0; i < (int) std::size (technikWords); ++i) pool.push_back (i);
            for (int i = 0; i < 12; ++i)
            {
                const int k = i + r.nextInt ((int) pool.size() - i);
                std::swap (pool[(size_t) i], pool[(size_t) k]);
                lex.words.push_back (addText (song, technikWords[pool[(size_t) i]]));
            }
            for (int tries = 0; tries < 40; ++tries)
            {
                const auto& w = lex.words[(size_t) r.nextInt ((int) lex.words.size())];
                if (w.size() >= 2 && w.size() <= 4) { lex.hook = w; break; }
            }
            if (lex.hook.empty())
                lex.hook = addText (song, "Signal");
            lex.hookText = joinText (song, lex.hook);
            return lex;
        }

        // Silben für eine Phrase aus den Wörtern ziehen; im Refrain steht das Refrain-Wort am Anfang
        inline void fillSyllables (std::vector<VocNote>& line, const Lexicon& lex, Rng& r, bool withHook)
        {
            size_t i = 0;
            bool hookPlaced = ! withHook;
            while (i < line.size())
            {
                const bool useHook = ! hookPlaced;
                const auto& w = useHook ? lex.hook : lex.words[(size_t) r.nextInt ((int) lex.words.size())];
                hookPlaced = true;
                for (size_t k = 0; k < w.size() && i < line.size(); ++k, ++i)
                {
                    line[i].syllable = w[k];
                    line[i].wordEnd = k + 1 == w.size();
                }
            }
            if (! line.empty()) line.back().wordEnd = true;
        }

        // Phrase der Stimme über zwei Takte: gesprochen wirkt sie fast monoton (viele Tonwiederholungen),
        // der Refrain singt das Refrain-Wort auf Akkordtönen mit längeren Tönen
        inline std::vector<VocNote> makeVocalLine (Rng& r, int era, int kind, bool typical)
        {
            std::vector<VocNote> line;
            const bool chorus = kind == Chorus;
            double t = typical ? (chorus ? 0.0 : (r.chance (0.6f) ? 0.0 : 1.0)) : r.nextInt (4) * 0.5;
            const double endAt = chorus ? 6.0 : (kind == Bridge ? 7.0 : (r.chance (0.5f) ? 4.0 : 6.0));
            int degree;
            switch (kind)
            {
                case Chorus:    degree = r.chance (0.5f) ? 4 : (r.chance (0.5f) ? 2 : 7); break;
                case PreChorus: degree = r.chance (0.5f) ? 2 : 4; break;
                case Bridge:    degree = r.chance (0.5f) ? 0 : 4; break;
                default:        degree = r.chance (0.7f) ? 0 : 2; break;
            }
            if (! typical) degree = r.nextInt (8) - 1;
            const bool slow = era == Radio75 || era == Motorik74;
            while (t < endAt - 0.01)
            {
                double d;
                if (! typical)
                    d = std::array<double, 5> { 0.25, 0.5, 1.0, 1.5, 2.0 }[(size_t) r.nextInt (5)];
                else if (chorus)
                    d = std::array<double, 3> { 1.0, 2.0, 0.5 }[(size_t) r.weighted ({ 4.0f, 2.0f, 2.0f })];
                else
                    d = std::array<double, 3> { 0.5, 1.0, 0.25 }[(size_t) r.weighted ({ 5.0f, slow ? 4.0f : 2.0f, era == Computer81 || era == Digital86 ? 1.5f : 0.3f })];
                d = juce::jmin (d, endAt - t);
                if (d < 0.24) break;
                line.push_back ({ t, d, degree, -1, false });
                t += d;
                // gelegentlich ein Achtel Atempause
                if (typical && ! chorus && r.chance (0.12f)) t += 0.5;
                int step;
                if (! typical)      step = r.nextInt (7) - 3;
                else if (chorus)    step = r.weighted ({ 1.0f, 2.0f, 3.0f, 2.0f, 1.0f }) - 2;
                else                step = r.weighted ({ 0.6f, 6.0f, 0.8f }) - 1;    // meist derselbe Ton (gesprochen)
                degree += step;
                const int lo = chorus ? 0 : -2, hi = chorus ? 7 : 4;
                degree = juce::jlimit (lo, hi, degree);
            }
            if (line.empty())
                line.push_back ({ 0.0, 2.0, chorus ? 4 : 0, -1, false });
            return line;
        }

        inline std::vector<MelNote> makeMotif (Rng& r, int era, bool typical)
        {
            static const double motorik[]  { 0.5, 1.0, 1.5, 2.0, 0.25 };
            static const double radio[]    { 2.0, 1.0, 4.0, 3.0, 0.5 };
            static const double rails[]    { 1.0, 0.5, 2.0, 1.5, 0.75 };
            static const double machines[] { 0.5, 0.25, 1.0, 0.75, 1.5 };
            static const double computer[] { 0.25, 0.5, 0.25, 1.0, 0.75 };
            static const double digital[]  { 0.5, 0.75, 0.25, 1.0, 2.0 };
            static const double free[]     { 0.25, 0.5, 1.0, 1.5, 2.0, 3.0 };
            const double* table = era == Motorik74 ? motorik : era == Radio75 ? radio : era == Rails77 ? rails
                                : era == Machines78 ? machines : era == Computer81 ? computer : digital;
            std::vector<MelNote> notes;
            double t = 0.0;
            int degree = typical ? (r.chance (0.5f) ? 4 : 7) : r.nextInt (7);
            while (t < 7.99)
            {
                double d = typical ? table[(size_t) r.weighted ({ 3.0f, 3.0f, 2.0f, 1.0f, 1.0f })] : free[(size_t) r.nextInt (6)];
                d = juce::jmin (d, 8.0 - t);
                if (t > 0.0 && r.chance (0.15f))
                {
                    t += juce::jmin (d, 1.0);
                    continue;
                }
                if (typical && std::fmod (t, 2.0) < 0.01)
                {
                    static const int chordTones[] { 0, 2, 4, 7, 9, 11 };
                    int best = 0;
                    for (int c : chordTones)
                        if (std::abs (c - degree) < std::abs (best - degree)) best = c;
                    degree = best;
                }
                notes.push_back ({ t, d, degree });
                t += d;
                // schlichte, fast kindliche Melodien: kleine Schritte und Tonwiederholungen
                static const int steps[] { -1, -1, 1, 1, 0, 2, -2, 0, 1 };
                degree = juce::jlimit (0, 11, degree + steps[r.nextInt (9)]);
            }
            if (notes.empty())
                notes.push_back ({ 0.0, 4.0, 4 });
            return notes;
        }

        // Material eines Themas: kehrt bei jeder Wiederholung wieder (Wiedererkennung)
        struct Material
        {
            std::array<int, 4> progression {};
            int chordBars = 1;
            std::array<RiffStep, 16> riff {};
            bool riffSixteenths = true;
            float riffGate = 0.5f;
            std::array<BassStep, 16> bass {};
            float bassGate = 0.7f;
            std::array<std::array<float, 16>, numDrums> drums {};
            std::array<bool, 16> stabs {};
            bool useStabs = false;
            std::vector<MelNote> motif;
            std::vector<VocNote> lineA, lineB;
            int typical = 0, decisions = 0;
        };

        inline Material makeMaterial (juce::uint64 key, const Settings& s, int kind, const Lexicon& lex, bool major)
        {
            Rng r (key);
            Material m;
            auto typical = [&]
            {
                const bool t = r.next() < 0.03f + 0.97f * s.closeness;
                ++m.decisions;
                if (t) ++m.typical;
                return t;
            };
            const int era = juce::jlimit (0, (int) numEras - 1, s.era);
            const bool chorusLike = kind == Chorus || kind == Intro || kind == Interlude || kind == Outro;

            // Akkordfolge
            if (typical())
            {
                const bool chorusTable = chorusLike && r.chance (0.5f);
                const int* t = major ? (chorusTable ? majorChorusTemplates[r.nextInt ((int) std::size (majorChorusTemplates))]
                                                    : majorTemplates[r.nextInt ((int) std::size (majorTemplates))])
                                     : (chorusTable ? minorChorusTemplates[r.nextInt ((int) std::size (minorChorusTemplates))]
                                                    : minorTemplates[r.nextInt ((int) std::size (minorTemplates))]);
                for (int i = 0; i < 4; ++i) m.progression[(size_t) i] = t[i];
            }
            else
            {
                m.progression[0] = r.chance (0.5f) ? 0 : r.nextInt (7);
                for (int i = 1; i < 4; ++i)
                {
                    int d = r.nextInt (7);
                    if (d == m.progression[(size_t) i - 1]) d = (d + 1 + r.nextInt (5)) % 7;
                    m.progression[(size_t) i] = d;
                }
            }
            if (kind == PreChorus)
            {
                m.progression[3] = 4;
                if (m.progression[2] == 4) m.progression[2] = 3;
            }
            const float twoBarChance = era == Radio75 ? 0.8f : era == Motorik74 ? 0.6f : era == Rails77 ? 0.55f : 0.4f;
            m.chordBars = (kind == PreChorus) ? 1 : (r.chance (twoBarChance) ? 2 : 1);

            // Sequenzer-Figur
            if (typical())
            {
                const char* t = riffTemplates[riffByEra[era][r.nextInt (4)]];
                for (int i = 0; i < 16; ++i)
                {
                    const char c = t[i];
                    if (c == '.') { m.riff[(size_t) i] = {}; continue; }
                    const int v = c - '0';
                    m.riff[(size_t) i] = { v % 4, v / 4, i % 4 == 0 };
                }
            }
            else
            {
                for (int i = 0; i < 16; ++i)
                    m.riff[(size_t) i] = r.chance (0.35f) ? RiffStep {} : RiffStep { r.nextInt (4), r.nextInt (3) - 1, r.chance (0.3f) };
            }
            m.riffSixteenths = era == Machines78 || era == Computer81 || r.chance (0.5f);
            m.riffGate = 0.25f + 0.4f * r.next();

            // Bass
            if (typical())
            {
                const char* t = bassTemplates[bassByEra[era][r.nextInt (3)]];
                for (int i = 0; i < 16; ++i)
                {
                    const char c = t[i];
                    m.bass[(size_t) i] = c == '.' ? BassStep {} : c == '-' ? BassStep { -1, true }
                                       : BassStep { c == '1' ? 1 : c == '5' ? 2 : 0, false };
                }
            }
            else
            {
                for (int i = 0; i < 16; ++i)
                    m.bass[(size_t) i] = (i == 0 || r.chance (0.4f)) ? BassStep { r.weighted ({ 4.0f, 1.0f, 1.0f }), false } : BassStep {};
            }
            m.bassGate = era == Machines78 || era == Computer81 || era == Digital86 ? 0.5f : 0.75f;

            // Akkord-Stabs: typisch für Digital 1986, sonst selten
            {
                const float stabChance = era == Digital86 ? 0.6f : era == Computer81 ? 0.15f : 0.0f;
                if (stabChance > 0.0f && r.chance (stabChance) && typical())
                {
                    const char* t = stabTemplates[r.nextInt ((int) std::size (stabTemplates))];
                    for (int i = 0; i < 16; ++i) m.stabs[(size_t) i] = t[i] == 'x';
                    m.useStabs = kind != Bridge;
                }
            }

            // Percussion
            if (typical())
            {
                const auto& t = drumTemplates[drumByEra[era][r.nextInt (3)]];
                for (int d = 0; d < numDrums; ++d)
                    for (int i = 0; i < 16; ++i)
                        m.drums[(size_t) d][(size_t) i] = t.rows[d][i] == 'x' ? 1.0f : t.rows[d][i] == 'o' ? 0.55f : 0.0f;
            }
            else
            {
                const float density[] { 0.3f, 0.12f, 0.5f, 0.1f, 0.1f, 0.12f, 0.1f, 0.15f };
                for (int d = 0; d < numDrums; ++d)
                    for (int i = 0; i < 16; ++i)
                        m.drums[(size_t) d][(size_t) i] = r.chance (density[d]) ? (r.chance (0.7f) ? 1.0f : 0.55f) : 0.0f;
                m.drums[Kick][0] = 1.0f;
                if (r.chance (0.6f)) m.drums[Snare][4] = m.drums[Snare][12] = 1.0f;
            }

            m.motif = makeMotif (r, era, typical());

            const int vocalKind = (kind == Verse || kind == PreChorus || kind == Chorus || kind == Bridge) ? kind : Verse;
            m.lineA = makeVocalLine (r, era, vocalKind, typical());
            m.lineB = makeVocalLine (r, era, vocalKind, typical());
            Rng words (hash (key, 4711));
            fillSyllables (m.lineA, lex, words, kind == Chorus);
            fillSyllables (m.lineB, lex, words, false);
            return m;
        }

        // Titel aus Wortlisten (frei erfunden) oder das Refrain-Wort
        inline juce::String makeTitle (juce::uint64 seed, float closeness, const Lexicon& lex)
        {
            Rng r (hash (seed, 9001));
            if ((lex.style == Technik && r.chance (0.25f + 0.3f * closeness)) || lex.style == OwnText)
            {
                const auto t = lex.hookText;
                return upper (t.substring (0, 1)) + t.substring (1);
            }
            struct Noun { const char* word; char gender; };
            static const Noun nouns[] { { "Takt", 'm' }, { "Strom", 'm' }, { "Signal", 'n' }, { "Netz", 'n' }, { "Kanal", 'm' },
                                        { "Station", 'f' }, { "Fabrik", 'f' }, { "Frequenz", 'f' }, { "Impuls", 'm' }, { "Motor", 'm' },
                                        { "Gleis", 'n' }, { "Kristall", 'm' }, { "Sender", 'm' }, { "Zentrale", 'f' }, { "Welle", 'f' },
                                        { "Maschine", 'f' }, { "Schaltung", 'f' }, { "Turbine", 'f' }, { "Uhrwerk", 'n' }, { "Labor", 'n' },
                                        { "Magnet", 'm' }, { "Rechner", 'm' }, { "Spannung", 'f' }, { "Dynamo", 'm' } };
            static const char* adjectives[] { "Elektrisch", "Digital", "Automatisch", "Synthetisch", "Magnetisch", "Neu", "Kalt",
                                              "Stählern", "Blau", "Gleich", "Exakt", "Analog", "Optisch", "Metallisch", "Endlos", "Modern" };
            const auto& n = nouns[r.nextInt ((int) std::size (nouns))];
            juce::String adj = juce::String::fromUTF8 (adjectives[r.nextInt ((int) std::size (adjectives))]);
            adj << (n.gender == 'm' ? "er" : n.gender == 'n' ? "es" : "e");
            return adj + " " + juce::String::fromUTF8 (n.word);
        }
    }

    // Vorschläge beim Würfeln: stiltypisches Tempo und Tonart
    inline int suggestTempo (juce::uint32 seed, int era, float closeness)
    {
        detail::Rng r (detail::hash (seed, 77));
        const float ranges[numEras][2] { { 84.0f, 104.0f }, { 72.0f, 96.0f }, { 104.0f, 120.0f }, { 112.0f, 128.0f }, { 112.0f, 132.0f }, { 108.0f, 124.0f } };
        const int e = juce::jlimit (0, (int) numEras - 1, era);
        const float widen = (1.0f - closeness) * 30.0f;
        const float lo = juce::jmax (60.0f, ranges[e][0] - widen);
        const float hi = juce::jmin (170.0f, ranges[e][1] + widen);
        return juce::roundToInt (lo + r.next() * (hi - lo));
    }

    inline int suggestKey (juce::uint32 seed, float closeness)
    {
        detail::Rng r (detail::hash (seed, 78));
        if (r.chance (closeness))
        {
            static const int typicalKeys[] { 9, 4, 0, 2, 7, 5, 11 };
            return typicalKeys[r.weighted ({ 3.0f, 3.0f, 2.0f, 2.0f, 2.0f, 1.0f, 1.0f })];
        }
        return r.nextInt (12);
    }

    // Skalen, die je Epoche als stiltypisch gelten (Index in scaleTable)
    inline bool scaleTypical (int era, int scaleIndex)
    {
        if (era == Motorik74)  return scaleIndex == 3 || scaleIndex == 4;
        if (era == Rails77)    return scaleIndex == 0 || scaleIndex == 2;
        if (era == Computer81) return scaleIndex == 0 || scaleIndex == 3;
        return scaleIndex == 0 || scaleIndex == 1;   // Radio, Maschinen, Digital
    }

    //==========================================================================
    inline Song generate (const Settings& s)
    {
        using namespace detail;
        Song song;
        song.settings = s;
        song.tempo = juce::jlimit (40.0f, 220.0f, s.tempo);
        song.root = detail::mod (s.key, 12);
        const int era = juce::jlimit (0, (int) numEras - 1, s.era);
        const float closeness = juce::jlimit (0.0f, 1.0f, s.closeness);
        const float variety = juce::jlimit (0.0f, 1.0f, s.variety);
        int typical = 0, decisions = 0;
        auto count = [&] (bool t) { ++decisions; if (t) ++typical; return t; };

        Rng form (hash (s.seed, 1));

        // Skala
        if (s.scale >= 1 && s.scale <= 5)
            song.scaleIndex = s.scale - 1;
        else if (count (form.next() < 0.03f + 0.97f * closeness))
        {
            if (era == Motorik74)       song.scaleIndex = std::array<int, 2> { 3, 4 }[(size_t) form.weighted ({ 3.0f, 2.0f })];
            else if (era == Rails77)    song.scaleIndex = std::array<int, 2> { 0, 2 }[(size_t) form.weighted ({ 4.0f, 2.0f })];
            else if (era == Computer81) song.scaleIndex = std::array<int, 2> { 0, 3 }[(size_t) form.weighted ({ 4.0f, 2.0f })];
            else                        song.scaleIndex = std::array<int, 2> { 0, 1 }[(size_t) form.weighted ({ 4.0f, 3.0f })];
        }
        else
            song.scaleIndex = form.nextInt (5);
        for (int i = 0; i < 7; ++i)
            song.scale[(size_t) i] = scaleTable[song.scaleIndex][i];

        song.totalBars = juce::jmax (8, juce::roundToInt (s.lengthMinutes * song.tempo / 4.0));
        const auto lexicon = makeLexicon (song, s.noteSeed, juce::jlimit (0, (int) numTextStyles - 1, s.textStyle), s.ownText);
        song.title = makeTitle (s.seed, closeness, lexicon);
        song.hookWord = lexicon.hookText;

        //------------------------------------------------------------------
        // Form: langes Intro, Strophe und Refrain im Wechsel mit instrumentalen Themen, Bridge, Break, Outro
        {
            const int total = song.totalBars;
            const bool small = total < 48;
            const int introBars = small ? (total >= 16 ? 4 : 2) : (form.chance (0.5f) ? 8 : 16);
            const int outroBars = small ? (total >= 16 ? 4 : 2) : (form.chance (0.5f) ? 8 : 16);
            std::vector<std::pair<int, int>> plan;   // Typ, Takte
            plan.push_back ({ Intro, introBars });
            int remaining = total - introBars - outroBars;
            static const int cycle[] { Verse, Chorus, Interlude, Verse, PreChorus, Chorus, Interlude, Bridge, Break, Chorus, Interlude, Chorus };
            static const int middleTypes[] { Verse, PreChorus, Chorus, Interlude, Bridge, Break };
            const float formChance = 0.03f + 0.97f * std::sqrt (closeness);
            const bool usePre = form.chance (0.2f + 0.4f * closeness);
            int step = 0, previous = Intro;
            while (remaining > 0)
            {
                int type = cycle[step % 12];
                ++step;
                if (type == PreChorus && ! usePre)
                    continue;
                if (! count (form.next() < formChance))
                {
                    type = middleTypes[form.nextInt (6)];
                    if (type == previous) type = middleTypes[(form.nextInt (5) + 1 + type) % 6];
                }
                int len = (type == Verse || type == Chorus || type == Interlude) ? 16 : 8;
                if (type == PreChorus) len = form.chance (0.5f) ? 4 : 8;
                if (form.chance (variety * 0.4f) && type != PreChorus)
                    len = len == 16 ? 8 : 16;
                if (small)
                    len = juce::jmax (2, len / 2);
                len = juce::jmin (len, remaining);
                if (remaining - len < 4)
                    len = remaining;
                plan.push_back ({ type, len });
                previous = type;
                remaining -= len;
            }
            if (plan.size() > 3 && closeness > 0.5f && plan.back().first != Chorus && plan.back().first != Interlude)
                plan.back().first = Interlude;
            plan.push_back ({ Outro, outroBars });

            int bar = 0;
            for (size_t i = 0; i < plan.size(); ++i)
            {
                Section sec;
                sec.type = plan[i].first;
                sec.bars = plan[i].second;
                sec.startBar = bar;
                bar += sec.bars;
                song.sections.push_back (sec);
            }
        }

        //------------------------------------------------------------------
        // Abschnitte ausstatten: Intensität, Spuren, Thema, Filterfahrt, Blenden
        static const float baseIntensity[] { 0.35f, 0.55f, 0.65f, 0.88f, 0.78f, 0.45f, 0.3f, 0.4f };
        int occurrences[numSectionTypes] {};
        for (size_t i = 0; i < song.sections.size(); ++i)
        {
            auto& sec = song.sections[i];
            Rng r (hash (s.seed, 100 + i));
            const float g = juce::jlimit (0.0f, 1.0f, s.intensity);
            float I = juce::jlimit (0.05f, 1.0f, baseIntensity[sec.type] * (0.35f + 1.0f * g));
            if (i < s.intensityOverride.size() && s.intensityOverride[i] >= 0.0f)
            {
                I = juce::jlimit (0.0f, 1.0f, s.intensityOverride[i]);
                sec.overridden = true;
            }
            sec.intensity = I;

            //                         Flächen    Sequenz     Bass        Melodie     Stimme      Rhythmus    Geräusche
            auto& L = sec.lanes;
            switch (sec.type)
            {
                case Intro:     L = { I > 0.3f,  true,       I > 0.45f,  I > 0.4f,   false,      I > 0.45f,  true }; break;
                case Verse:     L = { I > 0.4f,  true,       true,       false,      true,       I > 0.15f,  I > 0.7f }; break;
                case PreChorus: L = { true,      true,       true,       false,      true,       true,       true }; break;
                case Chorus:    L = { true,      true,       true,       I > 0.6f,   true,       true,       I > 0.6f }; break;
                case Interlude: L = { I > 0.3f,  true,       true,       true,       false,      true,       I > 0.5f }; break;
                case Bridge:    L = { true,      I > 0.35f,  I > 0.3f,   false,      true,       I > 0.5f,   true }; break;
                case Break:     L = { true,      false,      false,      I > 0.5f,   false,      I > 0.7f,   true }; break;
                case Outro:     L = { true,      true,       I > 0.5f,   true,       false,      I > 0.35f,  true }; break;
                default: break;
            }
            for (int l = 0; l < numLanes; ++l)
            {
                const bool flip = r.chance ((1.0f - closeness) * 0.3f);
                const bool core = (sec.type == Chorus && (l == Drums || l == Bass || l == Vocal))
                               || (sec.type == Verse && l == Vocal) || (sec.type == Interlude && l == Lead);
                if (flip && ! core)
                    L[(size_t) l] = ! L[(size_t) l];
            }
            if (std::none_of (L.begin(), L.end(), [] (bool b) { return b; }))
                L[Pad] = true;

            // Thema: Intro/Thema/Outro greifen das Instrumentalthema auf
            switch (sec.type)
            {
                case Verse:     sec.theme = 0; break;
                case Chorus:    sec.theme = 1; break;
                case PreChorus: sec.theme = 3; break;
                case Bridge: case Break: sec.theme = 2; break;
                default:        sec.theme = 4; break;
            }
            const int occ = occurrences[sec.type]++;
            sec.variant = (occ > 0 && r.chance (variety)) ? occ : 0;
            if (i < s.sectionSalt.size() && s.sectionSalt[i] > 0)
            {
                sec.theme = 1000 + (int) i * 37 + s.sectionSalt[i];
                sec.rerolled = true;
            }

            const float level = 0.45f + 0.4f * I;
            sec.filterFrom = sec.filterTo = level;
            switch (sec.type)
            {
                case Intro:     sec.fadeFrom = closeness > 0.3f ? 0.35f : 1.0f; sec.filterFrom = 0.15f; sec.filterTo = 0.55f; break;
                case PreChorus: sec.filterFrom = 0.4f;  sec.filterTo = 0.85f; break;
                case Interlude: sec.filterFrom = 0.35f; sec.filterTo = 0.75f + 0.2f * I; break;   // langsam aufgehender Filter
                case Chorus:    sec.filterFrom = sec.filterTo = 0.6f + 0.35f * I; break;
                case Break:     sec.filterFrom = 0.2f;  sec.filterTo = 0.5f; break;
                case Outro:     sec.fadeTo = 0.0f; sec.filterFrom = level; sec.filterTo = 0.15f; break;
                default: break;
            }
        }

        //------------------------------------------------------------------
        // Material je Thema und Variante
        std::map<juce::uint64, Material> materials;
        auto keyFor = [&] (int theme, int variant)
        {
            return hash (s.noteSeed, (juce::uint64) (theme + 7), (juce::uint64) variant, (juce::uint64) era);
        };
        auto kindOfTheme = [] (int theme, int sectionType)
        {
            switch (theme)
            {
                case 0: return (int) Verse;
                case 1: return (int) Chorus;
                case 2: return (int) Bridge;
                case 3: return (int) PreChorus;
                case 4: return (int) Interlude;
                default: return sectionType;
            }
        };
        auto baseMaterial = [&] (int theme, int sectionType) -> const Material&
        {
            const auto key = keyFor (theme, 0);
            auto it = materials.find (key);
            if (it != materials.end())
                return it->second;
            Material m = makeMaterial (key, s, kindOfTheme (theme, sectionType), lexicon, song.scaleIndex >= 3);
            typical += m.typical;
            decisions += m.decisions;
            return materials.emplace (key, std::move (m)).first->second;
        };
        auto materialFor = [&] (int theme, int variant, int sectionType) -> const Material&
        {
            const auto& base = baseMaterial (theme, sectionType);
            if (variant == 0)
                return base;
            const auto key = keyFor (theme, variant);
            auto it = materials.find (key);
            if (it != materials.end())
                return it->second;
            Material m = base;
            Rng r (key);
            m.motif = makeMotif (r, era, r.next() < 0.03f + 0.97f * closeness);
            Rng words (hash (key, 4711));
            if (kindOfTheme (theme, sectionType) == Chorus)
                fillSyllables (m.lineB, lexicon, words, false);
            else
            {
                fillSyllables (m.lineA, lexicon, words, false);
                fillSyllables (m.lineB, lexicon, words, false);
            }
            return materials.emplace (key, std::move (m)).first->second;
        };

        auto pitchOf = [&] (int degree, int base, int transpose)
        {
            return base + song.root + transpose + floorDiv (degree, 7) * 12 + song.scale[(size_t) detail::mod (degree, 7)];
        };
        auto fold = [] (int p, int lo, int hi)
        {
            while (p < lo) p += 12;
            while (p > hi) p -= 12;
            return p;
        };
        auto snapToChord = [] (int degree, int chord)
        {
            int best = degree, bestDist = 99;
            for (int oct = -2; oct <= 2; ++oct)
                for (int t : { 0, 2, 4 })
                {
                    const int c = chord + t + 7 * oct;
                    const int dist = std::abs (c - degree);
                    if (dist < bestDist) { bestDist = dist; best = c; }
                }
            return best;
        };

        // Lautmalerei: Wörter für den Sprechrhythmus
        std::vector<int> wBum, wTschak, wBoing, wTick, wPing;
        if (lexicon.style == Onomatopoeia)
        {
            wBum = lexicon.words[0]; wTschak = lexicon.words[1]; wBoing = lexicon.words[2];
            wPing = lexicon.words[3]; wTick = lexicon.words[5];
        }
        // eigener Text: Strophen lesen den Text der Reihe nach, der Refrain wiederholt die ersten Wörter
        size_t textCursor = 0;

        std::vector<int> lastVoicing, curVoicing;
        for (size_t si = 0; si < song.sections.size(); ++si)
        {
            auto& sec = song.sections[si];
            const auto& m = materialFor (sec.theme, sec.variant, sec.type);
            sec.progression = m.progression;
            int chordBars = m.chordBars;
            if (sec.type == Intro || sec.type == Outro || sec.type == Break)
                chordBars = juce::jmin (4, chordBars * 2);
            sec.chordBars = chordBars;
            const float I = sec.intensity;
            const int tr = sec.transpose;
            Rng r (hash (s.noteSeed, 500 + si, (juce::uint64) juce::jmax (0, sec.theme)));
            const double secEnd = sec.endBeat();
            auto chordAt = [&] (int bar) { return m.progression[(size_t) ((bar / chordBars) % 4)]; };
            curVoicing.clear();
            size_t hookCursor = 0;

            for (int b = 0; b < sec.bars; ++b)
            {
                const double beat0 = (sec.startBar + b) * 4.0;
                const int degree = chordAt (b);

                // Flächen: Akkord mit weicher Stimmführung, bei mehr Intensität mit Grundton darunter
                if (sec.lanes[Pad] && (b % chordBars == 0 || m.useStabs))
                {
                    if (b % chordBars == 0 || curVoicing.empty())
                    {
                        std::vector<int> chord { pitchOf (degree, 48, tr), pitchOf (degree + 2, 48, tr), pitchOf (degree + 4, 48, tr) };
                        if (I > 0.7f && era != Motorik74)
                            chord.push_back (pitchOf (degree + 6, 48, tr));
                        std::vector<int> best;
                        int bestCost = 1 << 30;
                        for (int inv = 0; inv < (int) chord.size(); ++inv)
                            for (int oct = -1; oct <= 2; ++oct)
                            {
                                std::vector<int> v;
                                for (int k = 0; k < (int) chord.size(); ++k)
                                    v.push_back (chord[(size_t) k] + 12 * oct + (k < inv ? 12 : 0));
                                std::sort (v.begin(), v.end());
                                if (v.front() < 53 || v.back() > 79)
                                    continue;
                                int cost = 0;
                                for (size_t k = 0; k < v.size(); ++k)
                                    cost += lastVoicing.empty() ? std::abs (v[k] - 64) : std::abs (v[k] - lastVoicing[juce::jmin (k, lastVoicing.size() - 1)]);
                                if (cost < bestCost) { bestCost = cost; best = v; }
                            }
                        if (best.empty())
                            for (int p : chord) best.push_back (fold (p, 55, 77));
                        lastVoicing = curVoicing = best;
                    }
                    const float vel = 0.5f + 0.35f * I;
                    if (m.useStabs)
                    {
                        for (int st = 0; st < 16; ++st)
                        {
                            if (! m.stabs[(size_t) st]) continue;
                            const double start = beat0 + st * 0.25;
                            if (start >= secEnd) break;
                            for (int p : curVoicing)
                                song.notes[Pad].push_back ({ start, juce::jmin (0.45, secEnd - start), p, vel * (st % 4 == 0 ? 1.0f : 0.88f) });
                        }
                    }
                    else
                    {
                        const double len = juce::jmin ((double) chordBars * 4.0, secEnd - beat0) + 0.1;
                        for (int p : curVoicing)
                            song.notes[Pad].push_back ({ beat0, len, p, vel });
                        if (I > 0.45f)
                            song.notes[Pad].push_back ({ beat0, len, fold (pitchOf (degree, 36, tr), 36, 50), vel * 0.7f });
                    }
                }

                // Sequenzer-Ostinato
                if (sec.lanes[Seq])
                {
                    const int steps = m.riffSixteenths ? 16 : 8;
                    const double stepLen = 4.0 / steps;
                    for (int st = 0; st < steps; ++st)
                    {
                        const auto& a = m.riff[(size_t) (m.riffSixteenths ? st : st * 2)];
                        if (a.tone < 0)
                            continue;
                        const float h = (float) (hash (s.noteSeed, (juce::uint64) st, (juce::uint64) sec.theme) & 0xffff) / 65536.0f;
                        if (! a.accent && h < (1.0f - I) * 0.5f)
                            continue;
                        static const int toneDegrees[] { 0, 2, 4, 7 };
                        const int p = fold (pitchOf (degree + toneDegrees[a.tone], 60, tr) + 12 * a.octave, 52, 84);
                        const float vel = (a.accent ? 1.0f : 0.75f) * (0.7f + 0.3f * I);
                        song.notes[Seq].push_back ({ beat0 + st * stepLen, stepLen * m.riffGate, p, vel });
                    }
                }

                // Bass
                if (sec.lanes[Bass])
                    for (int st = 0; st < 16; ++st)
                    {
                        const auto& bs = m.bass[(size_t) st];
                        if (bs.kind < 0)
                            continue;
                        int ties = 0;
                        while (st + 1 + ties < 16 && m.bass[(size_t) (st + 1 + ties)].tie) ++ties;
                        const int p = fold (pitchOf (degree + (bs.kind == 2 ? 4 : 0), 36, tr), 31, 50) + (bs.kind == 1 ? 12 : 0);
                        const double len = 0.25 * (ties + 1) * (ties > 0 ? 0.95 : m.bassGate);
                        song.notes[Bass].push_back ({ beat0 + st * 0.25, len, p, st % 4 == 0 ? 1.0f : 0.82f });
                    }

                // Percussion
                if (sec.lanes[Drums])
                {
                    const bool fill = b == sec.bars - 1 && si + 1 < song.sections.size() && r.chance (0.3f + 0.5f * I);
                    const bool halfTime = (sec.type == Intro || sec.type == Bridge) && I < 0.5f;
                    for (int d = 0; d < numDrums; ++d)
                        for (int st = 0; st < 16; ++st)
                        {
                            float v = m.drums[(size_t) d][(size_t) st];
                            if (fill && st >= 8)
                            {
                                if (d == Tom) v = (st % 2 == 0) ? 0.9f : 0.0f;   // Syndrum-Lauf
                                if (d == Snare && st >= 12) v = 0.8f;
                            }
                            if (v <= 0.0f)
                                continue;
                            if (halfTime && d == Snare && st != 8 && ! fill)
                                continue;
                            const bool core = d == Kick || d == Snare;
                            const float h = (float) (hash (s.noteSeed, (juce::uint64) (d * 16 + st), (juce::uint64) sec.theme, 3) & 0xffff) / 65536.0f;
                            if (core ? (I < 0.3f && st % 8 != 0) : h > 0.1f + 0.95f * I)
                                continue;
                            song.notes[Drums].push_back ({ beat0 + st * 0.25, 0.25, d, v * (0.75f + 0.25f * I) });
                        }
                }

                // Melodie: Instrumentalthema in Intro/Thema/Outro; mit Stimme nur Antworten in den Pausen
                if (sec.lanes[Lead] && b % 2 == 0)
                {
                    const bool answersOnly = sec.lanes[Vocal];
                    for (size_t k = 0; k < m.motif.size(); ++k)
                    {
                        auto n = m.motif[k];
                        if (answersOnly && n.beat < 6.0)
                            continue;
                        if ((b / 2) % 4 == 3 && k == m.motif.size() - 1)
                            n.degree += (n.degree % 2 == 0) ? 2 : -1;
                        const double start = beat0 + n.beat;
                        if (start >= secEnd)
                            break;
                        const int chordDeg = chordAt (b + (int) (n.beat / 4.0));
                        const int p = fold (pitchOf (chordDeg + n.degree, 60, tr), 60, 86);
                        double len = n.length;
                        if (k + 1 < m.motif.size() && std::abs (m.motif[k + 1].beat - (n.beat + n.length)) < 0.01)
                            len += 0.05;
                        song.notes[Lead].push_back ({ start, juce::jmin (len, secEnd - start), p, (answersOnly ? 0.6f : 0.8f) + 0.2f * I });
                    }
                }

                // Stimme
                if (sec.lanes[Vocal] && lexicon.style == Onomatopoeia)
                {
                    // Sprechrhythmus: bum auf der Bassdrum, tschak auf der Snare, dazwischen boing, tick und ping
                    for (int st = 0; st < 16; ++st)
                    {
                        const std::vector<int>* w = nullptr;
                        int deg = 0;
                        double len = 0.5;
                        if (m.drums[Kick][(size_t) st] > 0.0f)                                                  { w = &wBum; deg = 0; }
                        else if (m.drums[Snare][(size_t) st] > 0.0f || m.drums[Clap][(size_t) st] > 0.0f)        { w = &wTschak; deg = 4; }
                        else if (st % 4 == 2 && r.chance (0.12f + 0.2f * I))                                     { w = &wBoing; deg = 2; len = 0.75; }
                        else if (st % 2 == 1 && r.chance (0.08f * I))                                            { w = r.chance (0.5f) ? &wTick : &wPing; deg = 7; len = 0.25; }
                        if (w == nullptr || w->empty())
                            continue;
                        if (sec.type == Verse && I < 0.3f && st % 8 != 0)
                            continue;
                        const double start = beat0 + st * 0.25;
                        if (start >= secEnd)
                            break;
                        int p = pitchOf (deg, 48, tr);
                        while (p < 55) p += 12;
                        while (p > 76) p -= 12;
                        const float vel = 0.85f + 0.15f * I;
                        for (size_t k = 0; k < w->size(); ++k)
                            song.notes[Vocal].push_back ({ start + (double) k * 0.25, juce::jmin (len, secEnd - start) / (double) w->size(), p, vel,
                                                           (*w)[k], false, k + 1 == w->size() });
                    }
                }
                else if (sec.lanes[Vocal] && b % 2 == 0)
                {
                    // Zwei-Takt-Phrasen. Strophe A A B A', Refrain H H H B, sonst A B im Wechsel
                    const int unit = (b / 2) % 4;
                    const bool useB = sec.type == Verse ? unit == 2 : sec.type == Chorus ? unit == 3 : (unit % 2 == 1);
                    const auto& line = useB ? m.lineB : m.lineA;
                    const bool skip = sec.type == Verse && I < 0.25f && unit % 2 == 1;
                    const bool harmony = sec.type == Chorus && I > 0.5f;
                    const bool own = lexicon.style == OwnText;
                    for (size_t k = 0; k < line.size() && ! skip; ++k)
                    {
                        auto n = line[k];
                        if (unit == 3 && k == line.size() - 1 && ! useB)
                            n.degree += (n.degree % 2 == 0) ? -2 : 1;
                        const double start = beat0 + n.beat;
                        if (start >= secEnd)
                            break;
                        const int chord = chordAt (b + (int) (n.beat / 4.0));
                        int deg = n.degree;
                        if (std::fmod (n.beat, 2.0) < 0.01 || n.length >= 1.5)
                            deg = snapToChord (deg, chord);
                        int p = pitchOf (deg, 60, tr);
                        while (p > 76) p -= 12;
                        while (p < 55) p += 12;
                        const double len = juce::jmin (n.length * 0.92, secEnd - start);
                        const float vel = 0.8f + 0.2f * I - (std::fmod (n.beat, 1.0) > 0.01 ? 0.06f : 0.0f);
                        int syl = n.syllable;
                        bool wordEnd = n.wordEnd;
                        if (own)
                        {
                            if (sec.type == Chorus && ! useB)
                            {
                                syl = lexicon.hook[hookCursor % lexicon.hook.size()];
                                wordEnd = true;
                                ++hookCursor;
                            }
                            else
                            {
                                syl = lexicon.sequence[textCursor % lexicon.sequence.size()];
                                wordEnd = lexicon.sequenceWordEnd[textCursor % lexicon.sequence.size()];
                                ++textCursor;
                            }
                        }
                        song.notes[Vocal].push_back ({ start, len, p, vel, syl, false, wordEnd });
                        if (harmony)
                        {
                            // Zweitstimme: eine Oktave tiefer (wie ein zweiter Vocoder-Kanal)
                            int hp = p - 12;
                            if (hp < 43) hp += 12;
                            song.notes[Vocal].push_back ({ start, len, hp, vel * 0.8f, syl, true, wordEnd });
                        }
                    }
                }
            }

            // Geräusche: Funkrauschen in ruhigen Teilen, das Maschinengeräusch der Epoche, Anlauf vor Refrain und Thema
            const double len = sec.bars * 4.0;
            if (sec.lanes[Fx])
            {
                if (sec.type == Intro || sec.type == Outro || sec.type == Break || sec.type == Bridge)
                    song.notes[Fx].push_back ({ sec.startBeat(), len, Static, 0.6f + 0.3f * closeness });
                // Leitgeräusch der Epoche (bei wenig Nähe zum Original frei gewählt)
                int machine;
                switch (era)
                {
                    case Motorik74:  machine = Car; break;
                    case Radio75:    machine = r.chance (0.5f) ? Geiger : Morse; break;
                    case Rails77:    machine = Train; break;
                    case Machines78: machine = r.chance (0.6f) ? Data : Morse; break;
                    case Computer81: machine = Data; break;
                    default:         machine = r.chance (0.5f) ? Data : Geiger; break;
                }
                if (r.chance ((1.0f - closeness) * 0.6f))
                    machine = std::array<int, 5> { Train, Car, Geiger, Morse, Data }[(size_t) r.nextInt (5)];
                const bool calm = sec.type == Intro || sec.type == Break || sec.type == Bridge || sec.type == Outro || sec.type == Interlude;
                if (calm || r.chance (0.3f * I))
                {
                    double at = sec.startBeat(), dur = len;
                    if (machine == Car || machine == Morse)   // kurze Ereignisse: Vorbeifahrt bzw. Funkspruch
                    {
                        dur = juce::jmin (len, machine == Car ? 16.0 : 8.0);
                        at = sec.startBeat() + (double) (r.nextInt (juce::jmax (1, (int) ((len - dur) / 4.0) + 1)) * 4);
                    }
                    Note fx { at, dur, machine, 0.6f + 0.4f * r.next() };
                    fx.syllable = fold (pitchOf (chordAt (0), 60, tr), 60, 71);
                    song.notes[Fx].push_back (fx);
                }
            }
            if (si + 1 < song.sections.size())
            {
                const int nextType = song.sections[si + 1].type;
                if ((nextType == Chorus || nextType == Interlude) && sec.bars >= 4 && r.chance (0.25f + 0.45f * closeness))
                    song.notes[Fx].push_back ({ secEnd - 4.0, 4.0, Swell, 0.8f });
            }
        }

        for (auto& lane : song.notes)
            std::stable_sort (lane.begin(), lane.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });

        song.typicality = decisions > 0 ? (float) typical / (float) decisions : 0.0f;
        return song;
    }
}
