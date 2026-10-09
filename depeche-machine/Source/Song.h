#pragma once

#include <JuceHeader.h>
#include <map>

// Der Song-Generator: baut aus Zufallszahl (Seed) und den Reglern Länge, Intensität, Nähe zum Original
// und Abwechslung einen kompletten Song mit Strophen, Refrains und Noten für sieben Spuren, inklusive Gesangslinie.
// Alles wird nach Stilregeln des dunklen Synthpop der Achtziger und frühen Neunziger neu erfunden; es werden keine
// Melodien, Akkordfolgen, Texte oder Klänge aus Originalaufnahmen übernommen. "Nähe zum Original" steuert nur,
// wie oft der Generator zu den typischen Stilmitteln greift (Moll, Strophe/Refrain, Ostinato-Riffs, Sequenzer-Bass,
// Metall-Schläge, tiefe Gesangslinie mit Refrain-Haken) statt frei zu würfeln.
namespace Depeche
{
    enum Lane { Pad, Seq, Bass, Lead, Vocal, Drums, Fx, numLanes };
    enum DrumVoice { Kick, Snare, HatClosed, HatOpen, Clap, Metal, Tom, Tambourine, numDrums };
    enum FxType { Atmo, Swell, Clang, numFx };
    enum SectionType { Intro, Verse, PreChorus, Chorus, Interlude, Bridge, Break, Outro, numSectionTypes };
    enum Era { Synthpop81, Industrial84, Noir87, Blues90, Dark93, numEras };
    enum TextStyle { Words, Vocables, Humming };

    inline juce::String sectionName (int type)
    {
        static const char* names[] { "Intro", "Strophe", "Vorrefrain", "Refrain", "Zwischenspiel", "Bridge", "Break", "Outro" };
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
        static const char* names[] { "Flächen", "Riff", "Bass", "Melodie", "Gesang", "Rhythmus", "Effekte" };
        return juce::String::fromUTF8 (names[juce::jlimit (0, (int) numLanes - 1, lane)]);
    }

    // Kurzbeschreibung der Epochen (für Steckbrief und Handbuch)
    inline juce::String eraDescription (int era)
    {
        static const char* text[] {
            "hell und hüpfend: Pulswellen, Oktav-Bass, einfache Drumcomputer",
            "dunkel und hart: Metallschläge, Hammer-Snare, Sechzehntel-Bass, Chor",
            "groß und düster: weite Hallräume, Glas-Flächen, getragene Linien",
            "Elektro trifft Blues: Slide-Gitarre, treibender Bass, Tamburin",
            "schwer und langsam: tiefer Puls, viel Raum, Gospel-Moll",
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

    //==========================================================================
    // Laute für den Gesang: jede Silbe = Anlaut + Vokal (auch Doppelvokal oder Summen) + Auslaut
    namespace Phon
    {
        enum Onset { ONone, OM, ON, OL, OW, OJ, OR, OS, OSch, OT, OD, OK, OH, OF, numOnsets };
        enum Nucleus { A, E, I, O, U, Ae, Ai, Au, Oi, Hum, numNuclei };
        enum Coda { CNone, CN, CM, CS, CT, numCodas };

        inline int make (int onset, int nucleus, int coda = CNone) { return onset + 16 * nucleus + 256 * coda; }
        inline int onset (int code)   { return code & 15; }
        inline int nucleus (int code) { return (code >> 4) & 15; }
        inline int coda (int code)    { return (code >> 8) & 15; }

        inline juce::String text (int code)
        {
            if (code < 0) return {};
            static const char* on[] { "", "m", "n", "l", "w", "j", "r", "s", "sch", "t", "d", "k", "h", "f" };
            static const char* nu[] { "a", "e", "i", "o", "u", "ä", "ei", "au", "eu", "mm" };
            static const char* co[] { "", "n", "m", "s", "t" };
            const int o = juce::jlimit (0, (int) numOnsets - 1, onset (code));
            const int n = juce::jlimit (0, (int) numNuclei - 1, nucleus (code));
            const int c = juce::jlimit (0, (int) numCodas - 1, coda (code));
            if (n == Hum)
                return o == OH ? "hm" : "mm";
            return juce::String::fromUTF8 (on[o]) + juce::String::fromUTF8 (nu[n]) + co[c];
        }
    }

    // Noten in Vierteln (Beats). Bei Rhythmus = Instrument, bei Effekten = FxType.
    // Gesang: syllable = Lautcode (Phon), harmony = Zweitstimme
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
        int era = Synthpop81, key = 9, scale = 0;  // scale: 0 = Auto, sonst 1..5
        int textStyle = Words;
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
        float filterFrom = 0.5f, filterTo = 0.5f;   // Riff-Filterfahrt (0..1)
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
        float typicality = 0.0f;    // Anteil der stiltypischen Entscheidungen (0..1)

        double totalBeats() const { return totalBars * 4.0; }

        int sectionIndexAt (double beat) const
        {
            for (int i = (int) sections.size(); --i >= 0;)
                if (beat >= sections[(size_t) i].startBeat())
                    return i;
            return 0;
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

        // Stiltypische Akkordfolgen (Stufen 0..6 in der Moll-Skala gedacht): Pendel und fallende Moll-Folgen
        inline constexpr int progressionTemplates[][4] {
            { 0, 5, 2, 6 }, { 0, 3, 5, 4 }, { 0, 6, 5, 6 }, { 0, 0, 5, 3 }, { 0, 3, 0, 4 },
            { 0, 5, 3, 4 }, { 0, 2, 3, 4 }, { 0, 6, 3, 4 }, { 5, 3, 0, 4 }, { 0, 4, 5, 3 },
        };
        // Refrain: öffnet sich gern zur Parallele (Stufe 2) oder zur VI
        inline constexpr int chorusTemplates[][4] {
            { 5, 2, 6, 0 }, { 2, 6, 5, 4 }, { 5, 6, 0, 0 }, { 3, 0, 4, 5 }, { 2, 4, 5, 6 }, { 5, 3, 0, 4 },
        };

        // Riff-Figuren: 0..3 = Grundton, Terz, Quinte, Oktave; 4..7 = eine Oktave höher; '.' = Pause
        inline const char* const riffTemplates[] {
            "0.4.2.4.0.4.2.4.",   // 0 hüpfende Oktaven
            "0.2.4.2.1.2.4.2.",   // 1 Pop-Arpeggio
            "04..04..04..0402",   // 2 Stich-Staccato
            "0..0..0.0..0..2.",   // 3 Metall-Synkope
            "0246024602460246",   // 4 steigendes Arpeggio
            "4.2.0...4.2.0.1.",   // 5 fallende Glocke
            "0.0.4.0.2.0.4.0.",   // 6 Blues-Puls
            "0...2...4...2...",   // 7 langsames Pendel
            "0.00.00.0.00.004",   // 8 Galopp
            "0204020402040204",   // 9 Wechselschlag
        };
        inline constexpr int riffByEra[numEras][4] { { 0, 1, 8, 9 }, { 2, 3, 8, 1 }, { 4, 5, 7, 1 }, { 6, 9, 0, 3 }, { 7, 5, 3, 6 } };

        // Bass in Sechzehnteln: 0 Grundton, 1 Oktave, 5 Quinte, '-' gehalten, '.' Pause
        inline const char* const bassTemplates[] {
            "0.1.0.1.0.1.0.1.",   // 0 Oktav-Hüpfer
            "0-0-0-0-0-0-0-0-",   // 1 gerade Achtel
            "0000000000000000",   // 2 Sechzehntel-Motor
            "0..0..0.0..0..5.",   // 3 Synkope
            "0.000.000.000.00",   // 4 Galopp
            "0-------0---5---",   // 5 lang
            "0..1..0.5..1..0.",   // 6 Sprünge
            "0.0.0.0.0.0.0.0.",   // 7 Achtel staccato
        };
        inline constexpr int bassByEra[numEras][3] { { 0, 7, 1 }, { 2, 3, 4 }, { 1, 6, 3 }, { 2, 4, 7 }, { 5, 3, 1 } };

        // Drumcomputer-Muster (16 Schritte): Bassdrum, Snare, Hihat zu, Hihat offen, Klatschen, Metall, Tom, Tamburin
        // x = voll, o = leise, . = nichts
        struct DrumTemplate { const char* rows[numDrums]; };
        inline const DrumTemplate drumTemplates[] {
            { { "x...x...x...x...", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", "....x.......x...", "................", "................", "................" } }, // 0 Pop gerade
            { { "x.....x.x.......", "....x.......x...", "xoxoxoxoxoxoxoxo", "..............x.", "................", "................", "................", "................" } }, // 1 Pop synkopiert
            { { "x..x..x...x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", "................", "..x.....x.x...x.", "................", "................" } }, // 2 Hammer
            { { "x...x...x...x...", "....x.......x...", "................", "..x...x...x...x.", ".......x.......x", "....x..x....x...", "................", "................" } }, // 3 Fabrik
            { { "x.......x.x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", "................", "................", "...........o.o.o", "....x.......x..." } }, // 4 Groß
            { { "x..x....x.......", "....x.......x...", "x.xxx.xxx.xxx.xx", "................", "....x.......x...", "................", "................", "................" } }, // 5 Noir
            { { "x...x...x...x...", "....x.......x...", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "................", "................", "................", "xoxoxoxoxoxoxoxo" } }, // 6 Vierviertel mit Tamburin
            { { "x..x..x...x..x..", "....x.......x...", "x.xx.xx.xx.xx.x.", "................", "....x.......x...", "................", "................", "..x...x...x...x." } }, // 7 Shuffle
            { { "x.....x...x.....", "....x.......x...", "x..x..x..x..x..x", "................", "................", "........x.......", "..............oo", "....x.......x..." } }, // 8 schwer
            { { "x.......x.......", "....x.......x...", "x...x...x...x...", "................", "................", "................", "................", "o.o.o.o.o.o.o.o." } }, // 9 Ballade
        };
        inline constexpr int drumByEra[numEras][3] { { 0, 1, 0 }, { 2, 3, 2 }, { 4, 5, 4 }, { 6, 7, 6 }, { 8, 9, 7 } };

        struct RiffStep { int tone = -1, octave = 0; bool accent = false; };
        struct BassStep { int kind = -1; bool tie = false; };   // kind: 0 Grundton, 1 Oktave, 2 Quinte
        struct MelNote  { double beat = 0.0, length = 1.0; int degree = 0; };
        struct VocNote  { double beat = 0.0, length = 1.0; int degree = 0; int syllable = -1; bool wordEnd = false; };

        // Erfundene Wörter des Songs (Kunstwörter, Lautmalerei oder Summen)
        struct Lexicon
        {
            std::vector<std::vector<int>> words;
            std::vector<int> hook;   // Refrain-Wort
            int style = Words;
        };

        inline std::vector<int> makeWord (Rng& r, int syllables)
        {
            using namespace Phon;
            // weiche Anlaute häufiger, dunkle Vokale bevorzugt
            static const int onsets[] { OM, ON, OL, OW, OS, OD, OT, OK, OR, OSch, OF, OH, OJ, ONone };
            static const float onsetW[] { 3, 3, 3, 2, 3, 2, 2, 1.5f, 1.5f, 1, 1, 1, 0.8f, 1.5f };
            static const int nuclei[] { A, E, I, O, U, Ae, Ai, Au, Oi };
            static const float nucleusW[] { 4, 2, 2.5f, 3.5f, 2.5f, 1, 1.5f, 1, 0.6f };
            static const int codas[] { CN, CS, CM };
            std::vector<int> w;
            for (int s = 0; s < syllables; ++s)
            {
                float sum = 0.0f;
                for (auto x : onsetW) sum += x;
                float x = r.next() * sum;
                int o = ONone;
                for (int i = 0; i < (int) std::size (onsets); ++i) { if (x < onsetW[i]) { o = onsets[i]; break; } x -= onsetW[i]; }
                sum = 0.0f;
                for (auto y : nucleusW) sum += y;
                x = r.next() * sum;
                int n = A;
                for (int i = 0; i < (int) std::size (nuclei); ++i) { if (x < nucleusW[i]) { n = nuclei[i]; break; } x -= nucleusW[i]; }
                int c = CNone;
                if (s == syllables - 1 && r.chance (0.45f))
                    c = codas[r.nextInt (3)];
                w.push_back (make (o, n, c));
            }
            return w;
        }

        inline Lexicon makeLexicon (juce::uint64 key, int style)
        {
            using namespace Phon;
            Rng r (hash (key, 31337));
            Lexicon lex;
            lex.style = style;
            if (style == Humming)
            {
                lex.words = { { make (OM, Hum) }, { make (OH, Hum) }, { make (OM, U) }, { make (OM, Hum), make (OM, Hum) } };
                lex.hook = { make (ONone, O), make (OM, Hum) };
                return lex;
            }
            if (style == Vocables)
            {
                lex.words = { { make (ONone, O) }, { make (ONone, A) }, { make (ON, A), make (ON, A) }, { make (OL, A) },
                              { make (OD, U) }, { make (OH, O) }, { make (OW, O) }, { make (OJ, E) }, { make (ONone, Ai) },
                              { make (ONone, U), make (ONone, O) } };
                const int pick = r.nextInt (4);
                lex.hook = pick == 0 ? std::vector<int> { make (ONone, O), make (ONone, A) }
                         : pick == 1 ? std::vector<int> { make (ON, A), make (ON, A), make (ON, A) }
                         : pick == 2 ? std::vector<int> { make (OL, A), make (OL, A) }
                                     : std::vector<int> { make (OW, O), make (ONone, O) };
                return lex;
            }
            for (int i = 0; i < 9; ++i)
                lex.words.push_back (makeWord (r, 1 + r.weighted ({ 3.0f, 4.0f, 2.0f })));
            lex.hook = makeWord (r, 2 + r.nextInt (2));
            return lex;
        }

        // Silben für eine Gesangsphrase aus den Wörtern ziehen; im Refrain steht das Haken-Wort am Anfang
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

        // Gesangsphrase über zwei Takte (8 Viertel): Rhythmus und Tonstufen (absolut zur Tonika)
        inline std::vector<VocNote> makeVocalLine (Rng& r, int era, int kind, bool typical)
        {
            std::vector<VocNote> line;
            const bool chorus = kind == Chorus;
            double t;
            if (! typical) t = r.nextInt (4) * 0.5;
            else if (chorus) t = r.chance (0.6f) ? 0.0 : 0.5;
            else t = r.chance (0.5f) ? 0.5 : (r.chance (0.5f) ? 0.0 : 1.0);   // Strophe: oft mit Auftakt-Versatz

            // Wie weit die Phrase reicht, bevor geatmet wird
            const double endAt = chorus ? 6.5 : (kind == Bridge ? 7.0 : 6.0);
            int degree;
            switch (kind)
            {
                case Chorus:    degree = r.chance (0.5f) ? 4 : (r.chance (0.5f) ? 7 : 5); break;
                case PreChorus: degree = r.chance (0.5f) ? 2 : 1; break;
                case Bridge:    degree = r.chance (0.5f) ? 2 : 4; break;
                default:        degree = r.chance (0.6f) ? 0 : (r.chance (0.5f) ? 2 : -1); break;
            }
            if (! typical) degree = r.nextInt (8) - 1;

            const double slowness = era == Dark93 ? 1.5 : era == Noir87 ? 1.2 : era == Synthpop81 ? 0.85 : 1.0;
            while (t < endAt - 0.01)
            {
                double d;
                if (! typical)
                    d = std::array<double, 5> { 0.25, 0.5, 1.0, 1.5, 2.0 }[(size_t) r.nextInt (5)];
                else if (chorus)
                    d = std::array<double, 4> { 0.5, 1.0, 1.5, 2.0 }[(size_t) r.weighted ({ 2.0f, 3.0f, 2.0f, 2.0f })];
                else if (kind == Bridge)
                    d = std::array<double, 3> { 1.0, 2.0, 3.0 }[(size_t) r.weighted ({ 2.0f, 3.0f, 1.0f })];
                else
                    d = std::array<double, 4> { 0.5, 1.0, 0.75, 1.5 }[(size_t) r.weighted ({ 5.0f, 3.0f, era == Blues90 ? 1.5f : 0.4f, 1.0f })];
                if (typical && d < 1.9 && r.chance ((float) (slowness - 0.85)))
                    d *= 2.0;
                d = juce::jmin (d, endAt - t);
                if (d < 0.24) break;
                line.push_back ({ t, d, degree, -1, false });
                t += d;
                // Schritte: Strophe eng und oft wiederholt, Vorrefrain steigend, Refrain eher fallend
                int step;
                if (! typical)               step = r.nextInt (7) - 3;
                else if (kind == PreChorus)  step = r.weighted ({ 1.0f, 2.0f, 3.0f, 2.0f }) - 1;           // -1..2, meist aufwärts
                else if (chorus)             step = r.weighted ({ 1.0f, 3.0f, 2.0f, 2.0f, 1.0f, 0.6f }) - 3; // eher abwärts
                else                         step = r.weighted ({ 1.0f, 2.0f, 4.0f, 2.0f, 0.8f }) - 2;       // eng, viele Tonwiederholungen
                degree += step;
                const int lo = chorus ? 2 : -2, hi = chorus ? 9 : (kind == PreChorus ? 7 : 4);
                degree = juce::jlimit (lo, hi, degree);
            }
            if (line.empty())
                line.push_back ({ 0.0, 2.0, chorus ? 4 : 0, -1, false });
            // letzte Note der Phrase lang ausklingen lassen
            auto& last = line.back();
            if (typical && last.length < 1.5 && last.beat + 1.5 <= 7.5)
                last.length = juce::jmin (2.5, 7.5 - last.beat);
            return line;
        }

        inline std::vector<MelNote> makeMotif (Rng& r, int era, bool typical)
        {
            static const double pop[]   { 0.5, 0.5, 1.0, 0.25, 1.5 };
            static const double ind[]   { 0.25, 0.5, 0.5, 0.75, 1.0 };
            static const double noir[]  { 1.0, 2.0, 1.5, 0.5, 3.0 };
            static const double blues[] { 1.5, 0.5, 2.0, 1.0, 3.0 };
            static const double dark[]  { 2.0, 1.0, 3.0, 4.0, 1.5 };
            static const double free[]  { 0.25, 0.5, 1.0, 1.5, 2.0, 3.0 };
            const double* table = era == Synthpop81 ? pop : era == Industrial84 ? ind : era == Noir87 ? noir : era == Blues90 ? blues : dark;
            std::vector<MelNote> notes;
            double t = 0.0;
            int degree = typical ? (r.chance (0.5f) ? 4 : 7) : r.nextInt (7);
            while (t < 7.99)
            {
                double d = typical ? table[(size_t) r.weighted ({ 3.0f, 3.0f, 2.0f, 1.0f, 1.0f })] : free[(size_t) r.nextInt (6)];
                d = juce::jmin (d, 8.0 - t);
                if (t > 0.0 && r.chance (0.18f))
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
                static const int steps[] { -2, -1, -1, 1, 1, 2, -3, 0, 0 };
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
            std::vector<MelNote> motif;
            std::vector<VocNote> lineA, lineB;
            int typical = 0, decisions = 0;
        };

        inline Material makeMaterial (juce::uint64 key, const Settings& s, int kind, const Lexicon& lex)
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
                if (chorusLike && r.chance (0.6f))
                {
                    const auto& t = chorusTemplates[r.nextInt ((int) std::size (chorusTemplates))];
                    for (int i = 0; i < 4; ++i) m.progression[(size_t) i] = t[i];
                }
                else
                {
                    const auto& t = progressionTemplates[r.nextInt ((int) std::size (progressionTemplates))];
                    for (int i = 0; i < 4; ++i) m.progression[(size_t) i] = t[i];
                }
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
                // Vorrefrain: steigt zur V (Spannung vor dem Refrain)
                m.progression[3] = 4;
                if (m.progression[2] == 4) m.progression[2] = 3;
            }
            const float twoBarChance = era == Dark93 ? 0.7f : era == Noir87 ? 0.55f : 0.35f;
            m.chordBars = (kind == PreChorus) ? 1 : (r.chance (twoBarChance) ? 2 : 1);

            // Riff
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
            m.riffSixteenths = era == Industrial84 || era == Blues90 || r.chance (0.5f);
            m.riffGate = 0.3f + 0.45f * r.next();

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
            m.bassGate = era == Industrial84 || era == Blues90 ? 0.55f : 0.8f;

            // Drumcomputer
            if (typical())
            {
                const auto& t = drumTemplates[drumByEra[era][r.nextInt (3)]];
                for (int d = 0; d < numDrums; ++d)
                    for (int i = 0; i < 16; ++i)
                        m.drums[(size_t) d][(size_t) i] = t.rows[d][i] == 'x' ? 1.0f : t.rows[d][i] == 'o' ? 0.55f : 0.0f;
            }
            else
            {
                const float density[] { 0.3f, 0.12f, 0.5f, 0.1f, 0.12f, 0.15f, 0.1f, 0.2f };
                for (int d = 0; d < numDrums; ++d)
                    for (int i = 0; i < 16; ++i)
                        m.drums[(size_t) d][(size_t) i] = r.chance (density[d]) ? (r.chance (0.7f) ? 1.0f : 0.55f) : 0.0f;
                m.drums[Kick][0] = 1.0f;
                if (r.chance (0.6f)) m.drums[Snare][4] = m.drums[Snare][12] = 1.0f;
            }

            m.motif = makeMotif (r, era, typical());

            // Gesang: Strophe, Vorrefrain, Refrain, Bridge haben eigene Phrasen
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
            if (lex.style == Words && r.chance (0.25f + 0.3f * closeness))
            {
                juce::String t;
                for (int syl : lex.hook) t << Phon::text (syl);
                return t.substring (0, 1).toUpperCase() + t.substring (1);
            }
            struct Noun { const char* word; char gender; };
            static const Noun nouns[] { { "Glas", 'n' }, { "Puls", 'm' }, { "Stunde", 'f' }, { "Spiegel", 'm' }, { "Herz", 'n' },
                                        { "Stadt", 'f' }, { "Nacht", 'f' }, { "Regen", 'm' }, { "Metall", 'n' }, { "Schatten", 'm' },
                                        { "Beton", 'm' }, { "Linie", 'f' }, { "Signal", 'n' }, { "Sehnsucht", 'f' }, { "Fieber", 'n' },
                                        { "Zeuge", 'm' }, { "Ufer", 'n' }, { "Atem", 'm' }, { "Fabrik", 'f' }, { "Gebet", 'n' },
                                        { "Spur", 'f' }, { "Kontakt", 'm' }, { "Strom", 'm' }, { "Grenze", 'f' } };
            static const char* adjectives[] { "Schwarz", "Kalt", "Still", "Leer", "Fremd", "Blass", "Heilig", "Verloren",
                                              "Ewig", "Dunkel", "Eisern", "Gläsern", "Letzt", "Geheim", "Elektrisch", "Zweit" };
            const auto& n = nouns[r.nextInt ((int) std::size (nouns))];
            juce::String adj = juce::String::fromUTF8 (adjectives[r.nextInt ((int) std::size (adjectives))]);
            if (adj.endsWith ("el")) adj = adj.dropLastCharacters (2) + "l";   // dunkel -> dunkler
            adj << (n.gender == 'm' ? "er" : n.gender == 'n' ? "es" : "e");
            return adj + " " + juce::String::fromUTF8 (n.word);
        }
    }

    // Vorschläge beim Würfeln: stiltypisches Tempo und Tonart
    inline int suggestTempo (juce::uint32 seed, int era, float closeness)
    {
        detail::Rng r (detail::hash (seed, 77));
        const float ranges[numEras][2] { { 118.0f, 136.0f }, { 100.0f, 122.0f }, { 94.0f, 116.0f }, { 104.0f, 124.0f }, { 78.0f, 98.0f } };
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
            static const int typicalKeys[] { 9, 2, 4, 7, 0, 5, 6 };
            return typicalKeys[r.weighted ({ 3.0f, 3.0f, 2.0f, 2.0f, 2.0f, 1.0f, 1.0f })];
        }
        return r.nextInt (12);
    }

    // Skalen, die je Epoche als stiltypisch gelten (Index in scaleTable)
    inline bool scaleTypical (int era, int scaleIndex)
    {
        if (era == Synthpop81) return scaleIndex == 0 || scaleIndex == 3 || scaleIndex == 1;
        if (era == Blues90)    return scaleIndex == 0 || scaleIndex == 1 || scaleIndex == 2;
        return scaleIndex <= 2;
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
            if (era == Synthpop81)   song.scaleIndex = std::array<int, 3> { 0, 3, 1 }[(size_t) form.weighted ({ 4.0f, 3.0f, 1.0f })];
            else if (era == Blues90) song.scaleIndex = form.weighted ({ 4.0f, 4.0f, 1.0f });
            else                     song.scaleIndex = form.weighted ({ 6.0f, 2.0f, 2.0f });
        }
        else
            song.scaleIndex = form.nextInt (5);
        for (int i = 0; i < 7; ++i)
            song.scale[(size_t) i] = scaleTable[song.scaleIndex][i];

        song.totalBars = juce::jmax (8, juce::roundToInt (s.lengthMinutes * song.tempo / 4.0));
        const auto lexicon = makeLexicon (s.noteSeed, juce::jlimit (0, 2, s.textStyle));
        song.title = makeTitle (s.seed, closeness, lexicon);
        for (int syl : lexicon.hook) song.hookWord << Phon::text (syl);

        //------------------------------------------------------------------
        // Form: Intro, dann Strophe / Vorrefrain / Refrain im Kreis, Zwischenspiel, Bridge und Break, am Ende Refrain und Outro
        {
            const int total = song.totalBars;
            const bool small = total < 48;
            const int introBars = small ? (total >= 16 ? 4 : 2) : 8;
            const int outroBars = small ? (total >= 16 ? 4 : 2) : (form.chance (0.5f) ? 8 : 16);
            std::vector<std::pair<int, int>> plan;   // Typ, Takte
            plan.push_back ({ Intro, introBars });
            int remaining = total - introBars - outroBars;
            static const int cycle[] { Verse, PreChorus, Chorus, Interlude, Verse, PreChorus, Chorus, Bridge, Break, Chorus, Chorus, Interlude };
            static const int middleTypes[] { Verse, PreChorus, Chorus, Interlude, Bridge, Break };
            const float formChance = 0.03f + 0.97f * std::sqrt (closeness);
            const bool usePre = form.chance (0.4f + 0.4f * closeness);
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
                int len = (type == Verse || type == Chorus) ? 16 : 8;
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
            // vor dem Outro soll der Refrain stehen (typisch), wenn der Song lang genug ist
            if (plan.size() > 3 && closeness > 0.5f && plan.back().first != Chorus)
                plan.back().first = Chorus;
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
        static const float baseIntensity[] { 0.35f, 0.55f, 0.68f, 0.92f, 0.6f, 0.45f, 0.3f, 0.35f };
        int occurrences[numSectionTypes] {};
        int chorusCount = 0;
        for (auto& sec : song.sections) if (sec.type == Chorus) ++chorusCount;
        int chorusSeen = 0;
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

            //                         Flächen    Riff        Bass        Melodie     Gesang      Rhythmus    Effekte
            auto& L = sec.lanes;
            switch (sec.type)
            {
                case Intro:     L = { I > 0.35f, true,       I > 0.45f,  true,       false,      I > 0.4f,   true }; break;
                case Verse:     L = { I > 0.45f, I > 0.25f,  true,       false,      true,       I > 0.15f,  I > 0.65f }; break;
                case PreChorus: L = { true,      true,       true,       false,      true,       true,       true }; break;
                case Chorus:    L = { true,      true,       true,       I > 0.5f,   true,       true,       I > 0.55f }; break;
                case Interlude: L = { I > 0.3f,  true,       true,       true,       false,      true,       I > 0.45f }; break;
                case Bridge:    L = { true,      I > 0.4f,   I > 0.3f,   false,      true,       I > 0.5f,   true }; break;
                case Break:     L = { true,      false,      false,      I > 0.55f,  false,      I > 0.7f,   true }; break;
                case Outro:     L = { true,      true,       I > 0.5f,   true,       false,      I > 0.35f,  true }; break;
                default: break;
            }
            // Wenig Nähe zum Original: Spuren freier ein- und ausschalten
            for (int l = 0; l < numLanes; ++l)
            {
                const bool flip = r.chance ((1.0f - closeness) * 0.3f);
                // Der Refrain behält Rhythmus, Bass und Gesang, die Strophe ihren Gesang
                const bool core = (sec.type == Chorus && (l == Drums || l == Bass || l == Vocal))
                               || (sec.type == Verse && l == Vocal);
                if (flip && ! core)
                    L[(size_t) l] = ! L[(size_t) l];
            }
            if (std::none_of (L.begin(), L.end(), [] (bool b) { return b; }))
                L[Pad] = true;

            // Thema: Intro/Zwischenspiel/Outro greifen den Refrain auf (Riff als Erkennungszeichen)
            switch (sec.type)
            {
                case Verse:     sec.theme = 0; break;
                case PreChorus: sec.theme = 3; break;
                case Bridge: case Break: sec.theme = 2; break;
                default:        sec.theme = 1; break;
            }
            const int occ = occurrences[sec.type]++;
            sec.variant = (occ > 0 && r.chance (variety)) ? occ : 0;
            if (sec.type == Chorus)
            {
                ++chorusSeen;
                // letzter Refrain gelegentlich einen Ganzton höher
                if (chorusSeen == chorusCount && chorusCount > 2 && r.chance (variety * 0.35f))
                    sec.transpose = 2;
            }
            if (i < s.sectionSalt.size() && s.sectionSalt[i] > 0)
            {
                sec.theme = 1000 + (int) i * 37 + s.sectionSalt[i];
                sec.rerolled = true;
            }

            const float level = 0.45f + 0.4f * I;
            sec.filterFrom = sec.filterTo = level;
            switch (sec.type)
            {
                case Intro:     sec.fadeFrom = closeness > 0.3f ? 0.35f : 1.0f; sec.filterFrom = 0.2f; sec.filterTo = 0.55f; break;
                case PreChorus: sec.filterFrom = 0.4f;  sec.filterTo = 0.9f; break;
                case Chorus:    sec.filterFrom = sec.filterTo = 0.65f + 0.35f * I; break;
                case Break:     sec.filterFrom = 0.2f;  sec.filterTo = 0.5f; break;
                case Outro:     sec.fadeTo = 0.0f; sec.filterFrom = level; sec.filterTo = 0.2f; break;
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
                default: return sectionType;
            }
        };
        auto baseMaterial = [&] (int theme, int sectionType) -> const Material&
        {
            const auto key = keyFor (theme, 0);
            auto it = materials.find (key);
            if (it != materials.end())
                return it->second;
            Material m = makeMaterial (key, s, kindOfTheme (theme, sectionType), lexicon);
            typical += m.typical;
            decisions += m.decisions;
            return materials.emplace (key, std::move (m)).first->second;
        };
        // Varianten behalten Akkorde, Riff, Bass, Rhythmus und Gesangsmelodie. Strophen bekommen neue Silben
        // (zweite Strophe), der Refrain behält seine Haken-Zeile; die Melodie-Spur spielt eine neue Figur.
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

        // Tonhöhe einer Skalenstufe (beliebig groß, auch negativ) ab Basisnote
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
        // nächster Akkordton (Stufe) zu 'degree' für den Akkord auf Stufe 'chord'
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

        std::vector<int> lastVoicing;
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

            for (int b = 0; b < sec.bars; ++b)
            {
                const double beat0 = (sec.startBar + b) * 4.0;
                const int degree = chordAt (b);

                // Flächen: Akkord mit weicher Stimmführung, tiefer Grundton dazu
                if (sec.lanes[Pad] && b % chordBars == 0)
                {
                    std::vector<int> chord { pitchOf (degree, 48, tr), pitchOf (degree + 2, 48, tr), pitchOf (degree + 4, 48, tr) };
                    if (I > 0.6f)
                        chord.push_back (pitchOf (degree + (r.chance (0.5f) ? 7 : 6), 48, tr));
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
                    lastVoicing = best;
                    const double len = juce::jmin ((double) chordBars * 4.0, secEnd - beat0) + 0.1;
                    const float vel = 0.5f + 0.35f * I;
                    for (int p : best)
                        song.notes[Pad].push_back ({ beat0, len, p, vel });
                    if (I > 0.4f && era != Synthpop81)
                        song.notes[Pad].push_back ({ beat0, len, fold (pitchOf (degree, 36, tr), 36, 50), vel * 0.75f });
                }

                // Riff
                if (sec.lanes[Seq])
                {
                    const int steps = m.riffSixteenths ? 16 : 8;
                    const double stepLen = 4.0 / steps;
                    for (int st = 0; st < steps; ++st)
                    {
                        const auto& a = m.riff[(size_t) (m.riffSixteenths ? st : st * 2)];
                        if (a.tone < 0)
                            continue;
                        // geringe Intensität lässt unbetonte Schritte aus (immer dieselben, damit das Riff erkennbar bleibt)
                        const float h = (float) (hash (s.noteSeed, (juce::uint64) st, (juce::uint64) sec.theme) & 0xffff) / 65536.0f;
                        if (! a.accent && h < (1.0f - I) * 0.5f)
                            continue;
                        static const int toneDegrees[] { 0, 2, 4, 7 };
                        const int p = fold (pitchOf (degree + toneDegrees[a.tone], 60, tr) + 12 * a.octave, 52, 84);
                        const float vel = (a.accent ? 1.0f : 0.75f) * (0.7f + 0.3f * I);
                        song.notes[Seq].push_back ({ beat0 + st * stepLen, stepLen * m.riffGate, p, vel });
                    }
                }

                // Bass: Sechzehntel-Raster mit gehaltenen Tönen
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

                // Rhythmus
                if (sec.lanes[Drums])
                {
                    const bool fill = b == sec.bars - 1 && si + 1 < song.sections.size() && r.chance (0.35f + 0.5f * I);
                    const bool halfTime = (sec.type == Intro || sec.type == Bridge) && I < 0.5f;
                    for (int d = 0; d < numDrums; ++d)
                        for (int st = 0; st < 16; ++st)
                        {
                            float v = m.drums[(size_t) d][(size_t) st];
                            if (fill && st >= 8)
                            {
                                if (d == Tom) v = (st % 2 == 0) ? 0.9f : 0.0f;
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
                    if (I > 0.85f && m.drums[Tambourine][4] <= 0.0f)   // volle Intensität: Tamburin kommt dazu
                        for (int st = 4; st < 16; st += 8)
                            song.notes[Drums].push_back ({ beat0 + st * 0.25, 0.25, Tambourine, 0.6f });
                }

                // Melodie: Erkennungsfigur in Intro/Zwischenspiel/Outro; mit Gesang nur Antworten in den Pausen
                if (sec.lanes[Lead] && b % 2 == 0)
                {
                    const bool answersOnly = sec.lanes[Vocal];
                    for (size_t k = 0; k < m.motif.size(); ++k)
                    {
                        auto n = m.motif[k];
                        if (answersOnly && n.beat < 6.0)
                            continue;
                        if ((b / 2) % 4 == 3 && k == m.motif.size() - 1)
                            n.degree += (n.degree % 2 == 0) ? 2 : -1;    // Antwort: letzter Ton anders
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

                // Gesang: Zwei-Takt-Phrasen. Strophe A A B A', Refrain H H H B, sonst A B im Wechsel
                if (sec.lanes[Vocal] && b % 2 == 0)
                {
                    const int unit = (b / 2) % 4;
                    const bool useB = sec.type == Verse ? unit == 2 : sec.type == Chorus ? unit == 3 : (unit % 2 == 1);
                    const auto& line = useB ? m.lineB : m.lineA;
                    // ruhige Strophen bei wenig Intensität: jede zweite Phrase pausiert
                    const bool skip = sec.type == Verse && I < 0.25f && unit % 2 == 1;
                    const bool harmony = sec.type == Chorus && I > 0.55f;
                    for (size_t k = 0; k < line.size() && ! skip; ++k)
                    {
                        auto n = line[k];
                        if (unit == 3 && k == line.size() - 1 && ! useB)
                            n.degree += (n.degree % 2 == 0) ? -2 : 1;     // Zeilenende variiert (Antwort)
                        const double start = beat0 + n.beat;
                        if (start >= secEnd)
                            break;
                        const int chord = chordAt (b + (int) (n.beat / 4.0));
                        int deg = n.degree;
                        // auf schweren Zeiten und bei langen Tönen auf Akkordtöne ziehen
                        if (std::fmod (n.beat, 2.0) < 0.01 || n.length >= 1.5)
                            deg = snapToChord (deg, chord);
                        int p = pitchOf (deg, 48, tr);
                        while (p > 65) p -= 12;
                        while (p < 43) p += 12;
                        const double len = juce::jmin (n.length * 0.96, secEnd - start);
                        const float vel = 0.75f + 0.25f * I - (std::fmod (n.beat, 1.0) > 0.01 ? 0.08f : 0.0f);
                        song.notes[Vocal].push_back ({ start, len, p, vel, n.syllable, false, n.wordEnd });
                        if (harmony)
                        {
                            int hp = pitchOf (deg + 2, 48, tr);
                            while (hp <= p) hp += 12;
                            while (hp - p > 12) hp -= 12;
                            song.notes[Vocal].push_back ({ start, len, hp, vel * 0.8f, n.syllable, true, n.wordEnd });
                        }
                    }
                }
            }

            // Effekte: Atmosphäre in ruhigen Teilen, Metallschläge, Anlauf in den Refrain
            const double len = sec.bars * 4.0;
            if (sec.lanes[Fx])
            {
                if (sec.type == Intro || sec.type == Outro || sec.type == Break || sec.type == Bridge)
                    song.notes[Fx].push_back ({ sec.startBeat(), len, Atmo, 0.7f + 0.3f * closeness });
                const float metalRate = era == Industrial84 ? 2.5f : era == Dark93 ? 1.2f : era == Noir87 ? 0.8f : 0.4f;
                int clangs = juce::roundToInt (I * metalRate * ((float) sec.bars / 8.0f));
                if (sec.type == Intro || sec.type == Outro) clangs = juce::jmin (clangs, 1);
                for (int k = 0; k < clangs; ++k)
                    song.notes[Fx].push_back ({ sec.startBeat() + r.nextInt (sec.bars * 4) * 1.0 + (r.chance (0.3f) ? 0.5 : 0.0),
                                                2.0 + r.next() * 2.0, Clang, 0.55f + 0.45f * r.next() });
            }
            // Anlauf (rückwärts anschwellend) in den letzten Takten vor dem Refrain
            if (si + 1 < song.sections.size())
            {
                const int nextType = song.sections[si + 1].type;
                if ((nextType == Chorus || (nextType == Verse && sec.type == Intro)) && sec.bars >= 4 && r.chance (0.35f + 0.55f * closeness))
                    song.notes[Fx].push_back ({ secEnd - 4.0, 4.0, Swell, 0.8f });
            }
        }

        for (auto& lane : song.notes)
            std::stable_sort (lane.begin(), lane.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });

        song.typicality = decisions > 0 ? (float) typical / (float) decisions : 0.0f;
        return song;
    }
}
