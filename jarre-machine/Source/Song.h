#pragma once

#include <JuceHeader.h>
#include <map>

// Der Track-Generator: baut aus Zufallszahl (Seed) und den Reglern Länge, Intensität, Nähe zum Original
// und Abwechslung einen kompletten Track mit Abschnitten und Noten für sechs Spuren.
// Alles wird nach Stilregeln der Synthesizer-Musik der späten Siebziger neu erfunden; es werden keine
// Melodien, Akkordfolgen oder Klänge aus Originalaufnahmen übernommen. "Nähe zum Original" steuert nur,
// wie oft der Generator zu den typischen Stilmitteln greift (Moll, Akkordpendel, sprudelnde Sequenzen,
// Rhythmusbox, lange gleitende Melodietöne, Wind und Brandung) statt frei zu würfeln.
namespace Jarre
{
    enum Lane { Pad, Seq, Bass, Lead, Drums, Fx, numLanes };
    enum DrumVoice { Kick, Snare, HatClosed, HatOpen, Clave, CongaHigh, CongaLow, Shaker, numDrums };
    enum FxType { Wind, Laser, Riser, Surf, numFx };
    enum SectionType { Intro, Build, ThemeA, Interlude, ThemeB, Break, Climax, Outro, numSectionTypes };
    enum Era { Oxygene, Equinoxe, Magnetic };

    inline juce::String sectionName (int type)
    {
        static const char* names[] { "Intro", "Aufbau", "Thema A", "Zwischenspiel", "Thema B", "Break", "Höhepunkt", "Outro" };
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
        static const char* names[] { "Flächen", "Sequenz", "Bass", "Melodie", "Rhythmus", "Effekte" };
        return juce::String::fromUTF8 (names[juce::jlimit (0, (int) numLanes - 1, lane)]);
    }

    inline constexpr int scaleTable[5][7] {
        { 0, 2, 3, 5, 7, 8, 10 },   // Moll (äolisch)
        { 0, 2, 3, 5, 7, 9, 10 },   // Dorisch
        { 0, 1, 3, 5, 7, 8, 10 },   // Phrygisch
        { 0, 2, 4, 5, 7, 9, 11 },   // Dur
        { 0, 2, 4, 5, 7, 9, 10 },   // Mixolydisch
    };

    // Noten in Vierteln (Beats). Bei Rhythmus = Instrument, bei Effekten = FxType.
    struct Note
    {
        double start = 0.0, length = 0.0;
        int pitch = 0;
        float velocity = 1.0f;
    };

    struct Settings
    {
        juce::uint32 seed = 1, noteSeed = 1;
        float lengthMinutes = 5.0f, intensity = 0.6f, closeness = 0.8f, variety = 0.5f;
        int era = Oxygene, key = 0, scale = 0;    // scale: 0 = Auto, sonst 1..5
        float tempo = 108.0f;
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
        double tempo = 108.0;
        int root = 0, scaleIndex = 0;
        std::array<int, 7> scale {};
        int totalBars = 0;
        juce::String title;
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

        // Prüfsumme über alle Noten (für Selbsttests: gleicher Seed = gleicher Track)
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
            template <typename T, size_t N>
            T pick (const std::array<T, N>& a) { return a[(size_t) nextInt ((int) N)]; }

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

        // Akkordpendel und -folgen, die für den Stil typisch sind (Stufen 0..6 in der Skala)
        inline constexpr int progressionTemplates[][4] {
            { 0, 5, 6, 0 }, { 0, 3, 0, 6 }, { 0, 2, 6, 3 }, { 0, 5, 2, 6 }, { 0, 0, 5, 6 },
            { 0, 6, 5, 6 }, { 0, 4, 5, 3 }, { 0, 5, 3, 4 }, { 0, 3, 5, 6 }, { 0, 6, 0, 5 },
        };

        // Sequenzer-Figuren: 0..3 = Grundton, Terz, Quinte, Oktave; 4..7 = dieselben eine Oktave höher; '.' = Pause
        inline const char* const arpTemplates[] {
            "0123012301230123",   // aufsteigend
            "0303030303030303",   // sprudelnder Oktavsprung
            "0402040104020406",   // Orgelpunkt mit Sprüngen
            "0.002.223.332.22",   // galoppierend
            "0123210101232101",   // auf und ab
            "0202320202023202",   // Quint-Orgelpunkt
            "0.30.20.30.20321",   // synkopiert
            "0012001200120012",   // Bassfigur
        };
        inline constexpr int arpByEra[3][4] { { 1, 2, 3, 5 }, { 0, 4, 7, 1 }, { 6, 0, 5, 7 } };

        // Bass in Achteln: 0 Grundton, 1 Oktave, 5 Quinte, '.' Pause
        inline const char* const bassTemplates[] { "01010101", "0.0.0.0.", "0..0..0.", "0.050.05", "00000000" };
        inline constexpr int bassByEra[3][3] { { 1, 3, 2 }, { 4, 0, 2 }, { 2, 0, 3 } };

        // Rhythmusbox-Muster (16 Schritte): Bassdrum, Snare, Hihat zu, Hihat offen, Clave, Conga hoch, Conga tief, Shaker
        // x = voll, o = leise, . = nichts
        struct DrumTemplate { const char* rows[numDrums]; };
        inline const DrumTemplate drumTemplates[] {
            { { "x.......x.x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "................", "................", "................", "................", "................" } }, // Slow Rock
            { { "x..x..x.x..x..x.", "................", "................", "................", "x..x...x..x.x...", "..x...x...x...xo", "x......x.x......", "oooooooooooooooo" } }, // Rumba
            { { "x..xx..xx..xx..x", "................", "xoxoxoxoxoxoxoxo", "................", "x..x..x...x..x..", "................", "................", "..o...o...o...o." } }, // Bossa
            { { "x.......x.......", "................", "x.x.x.x.x.x.x.x.", "................", "................", "...x..x....x..x.", "x.....x.x.......", "................" } }, // Beguine
            { { "x...x...x...x...", "....x.......x...", "x...x...x...x...", "..x...x...x...x.", "................", "................", "................", "o.o.o.o.o.o.o.o." } }, // Disco
            { { "x...x...x...x...", "....x..o....x.oo", "xoxoxoxoxoxoxoxo", "................", "................", "................", "................", "................" } }, // Marsch
            { { "x..x..x...x..x..", "....x.......x...", "x.xxx.xxx.xxx.xx", "................", "..x...x...x...x.", "...........o.x..", "................", "................" } }, // Magnetisch
        };
        inline constexpr int drumByEra[3][4] { { 0, 1, 2, 3 }, { 5, 4, 1, 0 }, { 6, 4, 2, 5 } };

        struct ArpStep  { int tone = -1, octave = 0; bool accent = false; };
        struct BassStep { bool on = false; int octave = 0; bool fifth = false; };
        struct MelNote  { double beat = 0.0, length = 1.0; int degree = 0; };

        // Material eines Themas: kehrt bei jeder Wiederholung des Themas wieder (Wiedererkennung)
        struct Material
        {
            std::array<int, 4> progression {};
            int chordBars = 1;
            std::array<ArpStep, 16> arp {};
            bool arpSixteenths = true;
            float arpGate = 0.6f;
            std::array<BassStep, 8> bass {};
            std::array<std::array<float, 16>, numDrums> drums {};
            std::vector<MelNote> motifA, motifB;
            int typical = 0, decisions = 0;
        };

        inline std::vector<MelNote> makeMotif (Rng& r, int era, bool typical)
        {
            static const double oxy[] { 4.0, 3.0, 2.0, 1.5, 1.0 };
            static const double equ[] { 2.0, 1.5, 1.0, 0.5, 1.0 };
            static const double mag[] { 1.0, 0.5, 0.75, 0.25, 1.5 };
            static const double free[] { 0.25, 0.5, 1.0, 1.5, 2.0, 3.0 };
            std::vector<MelNote> notes;
            double t = 0.0;
            int degree = typical ? (r.chance (0.5f) ? 4 : 2) : r.nextInt (7);
            while (t < 7.99)
            {
                double d;
                if (! typical)               d = free[(size_t) r.nextInt (6)];
                else if (era == Oxygene)     d = oxy[(size_t) r.weighted ({ 3.0f, 2.0f, 3.0f, 1.0f, 1.0f })];
                else if (era == Equinoxe)    d = equ[(size_t) r.weighted ({ 2.0f, 2.0f, 3.0f, 2.0f, 1.0f })];
                else                         d = mag[(size_t) r.weighted ({ 3.0f, 3.0f, 2.0f, 1.0f, 1.0f })];
                d = juce::jmin (d, 8.0 - t);
                if (t > 0.0 && r.chance (0.14f))
                {
                    t += juce::jmin (d, 1.0);
                    continue;
                }
                // auf schweren Zeiten bevorzugt Akkordtöne
                if (typical && std::fmod (t, 2.0) < 0.01)
                {
                    static const int chordTones[] { -3, 0, 2, 4, 7, 9 };
                    int best = 0;
                    for (int c : chordTones)
                        if (std::abs (c - degree) < std::abs (best - degree)) best = c;
                    degree = best;
                }
                notes.push_back ({ t, d, degree });
                t += d;
                static const int steps[] { -2, -1, -1, 1, 1, 2, 3, -3, 0 };
                degree = juce::jlimit (-3, 9, degree + steps[r.nextInt (9)]);
            }
            if (notes.empty())
                notes.push_back ({ 0.0, 4.0, 4 });
            return notes;
        }

        inline Material makeMaterial (juce::uint64 key, const Settings& s)
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
            const int era = juce::jlimit (0, 2, s.era);

            // Akkordfolge
            if (typical())
            {
                const auto& t = progressionTemplates[r.nextInt ((int) std::size (progressionTemplates))];
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
            const float twoBarChance = era == Oxygene ? 0.75f : era == Equinoxe ? 0.4f : 0.3f;
            m.chordBars = r.chance (twoBarChance) ? 2 : 1;

            // Sequenzer-Figur
            if (typical())
            {
                const char* t = arpTemplates[arpByEra[era][r.nextInt (4)]];
                for (int i = 0; i < 16; ++i)
                {
                    const char c = t[i];
                    if (c == '.') { m.arp[(size_t) i] = {}; continue; }
                    const int v = c - '0';
                    m.arp[(size_t) i] = { v % 4, v / 4, i % 4 == 0 };
                }
            }
            else
            {
                for (int i = 0; i < 16; ++i)
                    m.arp[(size_t) i] = r.chance (0.25f) ? ArpStep {} : ArpStep { r.nextInt (4), r.nextInt (3) - 1, r.chance (0.3f) };
            }
            m.arpSixteenths = era != Oxygene || r.chance (0.5f);
            m.arpGate = 0.35f + 0.45f * r.next();

            // Bass
            if (typical())
            {
                const char* t = bassTemplates[bassByEra[era][r.nextInt (3)]];
                for (int i = 0; i < 8; ++i)
                    m.bass[(size_t) i] = { t[i] != '.', t[i] == '1' ? 1 : 0, t[i] == '5' };
            }
            else
            {
                for (int i = 0; i < 8; ++i)
                    m.bass[(size_t) i] = { i == 0 || r.chance (0.5f), r.chance (0.3f) ? 1 : 0, r.chance (0.2f) };
            }

            // Rhythmusbox
            if (typical())
            {
                const auto& t = drumTemplates[drumByEra[era][r.nextInt (4)]];
                for (int d = 0; d < numDrums; ++d)
                    for (int i = 0; i < 16; ++i)
                        m.drums[(size_t) d][(size_t) i] = t.rows[d][i] == 'x' ? 1.0f : t.rows[d][i] == 'o' ? 0.55f : 0.0f;
            }
            else
            {
                const float density[] { 0.25f, 0.12f, 0.5f, 0.1f, 0.2f, 0.18f, 0.15f, 0.25f };
                for (int d = 0; d < numDrums; ++d)
                    for (int i = 0; i < 16; ++i)
                        m.drums[(size_t) d][(size_t) i] = r.chance (density[d]) ? (r.chance (0.7f) ? 1.0f : 0.55f) : 0.0f;
                m.drums[Kick][0] = 1.0f;
                if (r.chance (0.6f)) m.drums[Snare][4] = m.drums[Snare][12] = 1.0f;
            }

            m.motifA = makeMotif (r, era, typical());
            m.motifB = makeMotif (r, era, typical());
            return m;
        }

        // Titel aus Wortlisten (frei erfunden)
        inline juce::String makeTitle (juce::uint64 seed, float closeness)
        {
            Rng r (hash (seed, 9001));
            static const char* nouns[] { "Sauerstoff", "Polarlicht", "Kristallnebel", "Sternenwind", "Gezeiten", "Ozon", "Äther",
                                         "Morgenröte", "Laserharfe", "Zeitmaschine", "Orbit", "Chronik", "Tagundnachtgleiche",
                                         "Magnetfeld", "Leuchtfeuer", "Nordlicht", "Meridian", "Kosmos", "Brandung", "Horizont",
                                         "Planetarium", "Mondphase", "Lichtjahre", "Signal" };
            static const char* adjectives[] { "Ferne", "Gläserne", "Elektrische", "Schwebende", "Leuchtende", "Stille", "Kalte",
                                              "Ewige", "Blaue", "Unendliche", "Rotierende", "Flüchtige" };
            static const char* romans[] { "I", "II", "III", "IV", "V", "VI", "VII", "VIII" };
            const juce::String noun = juce::String::fromUTF8 (nouns[r.nextInt ((int) std::size (nouns))]);
            if (r.chance (0.3f + 0.6f * closeness))
                return noun + " Teil " + romans[r.nextInt (8)];
            juce::String adj = juce::String::fromUTF8 (adjectives[r.nextInt ((int) std::size (adjectives))]);
            // einfache Anpassung an Neutrum/Maskulinum ("Ferne Orbit" klingt falsch): "Ferner Orbit", sonst Femininum lassen
            static const char* masculine[] { "Sauerstoff", "Kristallnebel", "Sternenwind", "Äther", "Orbit", "Kosmos", "Horizont",
                                             "Meridian" };
            static const char* neuter[] { "Ozon", "Polarlicht", "Magnetfeld", "Leuchtfeuer", "Nordlicht", "Planetarium", "Signal" };
            for (auto* m : masculine) if (noun == juce::String::fromUTF8 (m)) adj << "r";
            for (auto* n : neuter)    if (noun == juce::String::fromUTF8 (n)) adj << "s";
            if (noun == "Gezeiten" || noun == "Lichtjahre") adj << "n";
            return adj + " " + noun;
        }
    }

    // Vorschläge beim Würfeln: stiltypisches Tempo und Tonart
    inline int suggestTempo (juce::uint32 seed, int era, float closeness)
    {
        detail::Rng r (detail::hash (seed, 77));
        const float ranges[3][2] { { 98.0f, 118.0f }, { 110.0f, 128.0f }, { 116.0f, 134.0f } };
        const float widen = (1.0f - closeness) * 30.0f;
        const float lo = juce::jmax (60.0f, ranges[juce::jlimit (0, 2, era)][0] - widen);
        const float hi = juce::jmin (160.0f, ranges[juce::jlimit (0, 2, era)][1] + widen);
        return juce::roundToInt (lo + r.next() * (hi - lo));
    }

    inline int suggestKey (juce::uint32 seed, float closeness)
    {
        detail::Rng r (detail::hash (seed, 78));
        if (r.chance (closeness))
        {
            static const int typicalKeys[] { 0, 2, 4, 9, 7, 5 };
            return typicalKeys[r.weighted ({ 3.0f, 3.0f, 2.0f, 2.0f, 1.0f, 1.0f })];
        }
        return r.nextInt (12);
    }

    //==========================================================================
    inline Song generate (const Settings& s)
    {
        using namespace detail;
        Song song;
        song.settings = s;
        song.tempo = juce::jlimit (40.0f, 220.0f, s.tempo);
        song.root = detail::mod (s.key, 12);
        const int era = juce::jlimit (0, 2, s.era);
        const float closeness = juce::jlimit (0.0f, 1.0f, s.closeness);
        const float variety = juce::jlimit (0.0f, 1.0f, s.variety);
        int typical = 0, decisions = 0;
        auto count = [&] (bool t) { ++decisions; if (t) ++typical; return t; };

        Rng form (hash (s.seed, 1));

        // Skala
        if (s.scale >= 1 && s.scale <= 5)
            song.scaleIndex = s.scale - 1;
        else if (count (form.next() < 0.03f + 0.97f * closeness))
            song.scaleIndex = form.weighted ({ 6.0f, 3.0f, 1.0f });                 // Moll, Dorisch, Phrygisch
        else
            song.scaleIndex = form.nextInt (5);
        for (int i = 0; i < 7; ++i)
            song.scale[(size_t) i] = scaleTable[song.scaleIndex][i];

        song.totalBars = juce::jmax (8, juce::roundToInt (s.lengthMinutes * song.tempo / 4.0));
        song.title = makeTitle (s.seed, closeness);

        //------------------------------------------------------------------
        // Form: Intro, Aufbau, dann Thema A / Zwischenspiel / Thema B / Break / Höhepunkt im Kreis, Outro
        {
            const int total = song.totalBars;
            const int introBars = total >= 40 ? (closeness > 0.5f ? 16 : 8) : (total >= 16 ? 4 : 2);
            const int outroBars = total >= 48 ? (form.chance (0.5f) ? 16 : 8) : (total >= 16 ? 4 : 2);
            std::vector<std::pair<int, int>> plan;   // Typ, Takte
            plan.push_back ({ Intro, introBars });
            int remaining = total - introBars - outroBars;
            if (remaining >= 24)
            {
                plan.push_back ({ Build, 8 });
                remaining -= 8;
            }
            static const int cycle[] { ThemeA, Interlude, ThemeB, Break, Climax };
            static const int middleTypes[] { ThemeA, Interlude, ThemeB, Break };
            const float formChance = 0.03f + 0.97f * std::sqrt (closeness);
            int step = 0, previous = Build;
            while (remaining > 0)
            {
                int type = cycle[step % 5];
                if (! count (form.next() < formChance))
                {
                    type = middleTypes[form.nextInt (4)];
                    if (type == previous) type = middleTypes[(form.nextInt (3) + 1 + type) % 4];
                }
                int len = (type == ThemeA || type == ThemeB || type == Climax) ? 16 : 8;
                if (form.chance (variety * 0.5f))
                    len = len == 16 ? 8 : 16;
                if (total < 40)
                    len /= 2;
                else if (total >= 240)
                    len *= 2;   // lange Tracks entwickeln sich langsamer
                len = juce::jmin (len, remaining);
                if (remaining - len < 4)
                    len = remaining;
                plan.push_back ({ type, len });
                previous = type;
                remaining -= len;
                ++step;
            }
            // vor dem Outro soll der Höhepunkt stehen (typisch), wenn der Track lang genug ist
            if (plan.size() > 3 && closeness > 0.5f && plan.back().first != Climax)
                plan.back().first = Climax;
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
        static const float baseIntensity[] { 0.25f, 0.5f, 0.75f, 0.5f, 0.8f, 0.35f, 1.0f, 0.3f };
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

            auto& L = sec.lanes;
            switch (sec.type)
            {
                case Intro:     L = { true, I > 0.7f, false, false, false, true }; break;
                case Build:     L = { true, true, I > 0.45f, false, I > 0.55f || era != Oxygene, I > 0.6f }; break;
                case ThemeA:
                case ThemeB:    L = { true, true, I > 0.2f, true, I > 0.25f, I > 0.65f }; break;
                case Interlude: L = { true, true, I > 0.6f, I > 0.7f, I > 0.75f, true }; break;
                case Break:     L = { true, I > 0.3f, false, false, false, true }; break;
                case Climax:    L = { true, true, true, true, I > 0.15f, I > 0.5f }; break;
                case Outro:     L = { true, I > 0.6f, false, false, false, true }; break;
                default: break;
            }
            // Wenig Nähe zum Original: Spuren freier ein- und ausschalten
            for (int l = 0; l < numLanes; ++l)
                if (r.chance ((1.0f - closeness) * 0.3f))
                    L[(size_t) l] = ! L[(size_t) l];
            if (std::none_of (L.begin(), L.end(), [] (bool b) { return b; }))
                L[Pad] = true;

            // Thema: Intro/Aufbau/Höhepunkt/Outro greifen Thema A auf, Break Thema B
            switch (sec.type)
            {
                case Intro: case Build: case ThemeA: case Climax: case Outro: sec.theme = 0; break;
                case ThemeB: case Break: sec.theme = 1; break;
                default: sec.theme = 2; break;
            }
            const int occ = occurrences[sec.type]++;
            sec.variant = (occ > 0 && r.chance (variety)) ? occ : 0;
            if (sec.type == Climax && occ == 0 && r.chance (variety * 0.7f))
                sec.transpose = r.chance (0.6f) ? 2 : 1;   // Rückung nach oben für den Schluss-Höhepunkt
            if (i < s.sectionSalt.size() && s.sectionSalt[i] > 0)
            {
                sec.theme = 1000 + (int) i * 37 + s.sectionSalt[i];
                sec.rerolled = true;
            }

            const float level = 0.45f + 0.4f * I;
            sec.filterFrom = sec.filterTo = level;
            switch (sec.type)
            {
                case Intro:  sec.fadeFrom = closeness > 0.3f ? 0.25f : 1.0f; sec.filterFrom = 0.2f; sec.filterTo = 0.4f; break;
                case Build:  sec.filterFrom = 0.15f; sec.filterTo = 0.8f; break;
                case Break:  sec.filterFrom = 0.3f;  sec.filterTo = 0.85f; break;
                case Climax: sec.filterFrom = 0.75f; sec.filterTo = 1.0f; break;
                case Outro:  sec.fadeTo = 0.0f; sec.filterFrom = level; sec.filterTo = 0.15f; break;
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
        auto baseMaterial = [&] (int theme) -> const Material&
        {
            const auto key = keyFor (theme, 0);
            auto it = materials.find (key);
            if (it != materials.end())
                return it->second;
            Material m = makeMaterial (key, s);
            typical += m.typical;
            decisions += m.decisions;
            return materials.emplace (key, std::move (m)).first->second;
        };
        // Varianten behalten Akkordfolge, Figur und Rhythmus des Themas (Wiedererkennung), nur die Melodie ändert sich
        auto materialFor = [&] (int theme, int variant) -> const Material&
        {
            const auto& base = baseMaterial (theme);
            if (variant == 0)
                return base;
            const auto key = keyFor (theme, variant);
            auto it = materials.find (key);
            if (it != materials.end())
                return it->second;
            Material m = makeMaterial (key, s);
            m.progression = base.progression;
            m.chordBars = base.chordBars;
            m.arp = base.arp;
            m.arpSixteenths = base.arpSixteenths;
            m.drums = base.drums;
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

        std::vector<int> lastVoicing;
        for (size_t si = 0; si < song.sections.size(); ++si)
        {
            auto& sec = song.sections[si];
            const auto& m = materialFor (sec.theme, sec.variant);
            sec.progression = m.progression;
            int chordBars = m.chordBars;
            if (sec.type == Intro || sec.type == Outro || sec.type == Break)
                chordBars = juce::jmin (4, chordBars * 2);
            sec.chordBars = chordBars;
            const float I = sec.intensity;
            const int tr = sec.transpose;
            Rng r (hash (s.noteSeed, 500 + si, (juce::uint64) juce::jmax (0, sec.theme)));
            const double secEnd = sec.endBeat();

            for (int b = 0; b < sec.bars; ++b)
            {
                const double beat0 = (sec.startBar + b) * 4.0;
                const int chordIndex = (b / chordBars) % 4;
                const int degree = m.progression[(size_t) chordIndex];

                // Flächen: Akkord mit weicher Stimmführung, tiefer Grundton dazu
                if (sec.lanes[Pad] && b % chordBars == 0)
                {
                    std::vector<int> chord { pitchOf (degree, 48, tr), pitchOf (degree + 2, 48, tr), pitchOf (degree + 4, 48, tr) };
                    if (I > 0.6f)
                        chord.push_back (pitchOf (degree + (r.chance (0.5f) ? 7 : 8), 48, tr));
                    // beste Umkehrung im Bereich 53..77 nahe am vorigen Akkord
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
                    const float vel = 0.55f + 0.35f * I;
                    for (int p : best)
                        song.notes[Pad].push_back ({ beat0, len, p, vel });
                    if (I > 0.35f)
                        song.notes[Pad].push_back ({ beat0, len, fold (pitchOf (degree, 36, tr), 36, 50), vel * 0.8f });
                }

                // Sequenz
                if (sec.lanes[Seq])
                {
                    const int steps = m.arpSixteenths ? 16 : 8;
                    const double stepLen = 4.0 / steps;
                    for (int st = 0; st < steps; ++st)
                    {
                        const auto& a = m.arp[(size_t) st];
                        if (a.tone < 0)
                            continue;
                        // geringe Intensität lässt unbetonte Schritte aus (immer dieselben, damit die Figur erkennbar bleibt)
                        const float h = (float) (hash (s.noteSeed, (juce::uint64) st, (juce::uint64) sec.theme) & 0xffff) / 65536.0f;
                        if (! a.accent && h < (1.0f - I) * 0.5f)
                            continue;
                        static const int toneDegrees[] { 0, 2, 4, 7 };
                        const int p = fold (pitchOf (degree + toneDegrees[a.tone], 48, tr) + 12 * a.octave, 40, 79);
                        const float vel = (a.accent ? 1.0f : 0.72f) * (0.7f + 0.3f * I);
                        song.notes[Seq].push_back ({ beat0 + st * stepLen, stepLen * m.arpGate, p, vel });
                    }
                }

                // Bass
                if (sec.lanes[Bass])
                    for (int st = 0; st < 8; ++st)
                    {
                        const auto& bs = m.bass[(size_t) st];
                        if (! bs.on)
                            continue;
                        const int p = fold (pitchOf (degree + (bs.fifth ? 4 : 0), 36, tr), 33, 52) + 12 * bs.octave;
                        song.notes[Bass].push_back ({ beat0 + st * 0.5, 0.42, p, st % 2 == 0 ? 1.0f : 0.8f });
                    }

                // Rhythmusbox
                if (sec.lanes[Drums])
                {
                    const bool fill = b == sec.bars - 1 && si + 1 < song.sections.size() && r.chance (0.35f + 0.5f * I);
                    for (int d = 0; d < numDrums; ++d)
                        for (int st = 0; st < 16; ++st)
                        {
                            float v = m.drums[(size_t) d][(size_t) st];
                            if (fill && st >= 8 && (d == CongaHigh || d == CongaLow))
                                v = ((st + d) % 2 == 0) ? 0.9f : 0.0f;
                            if (v <= 0.0f)
                                continue;
                            const bool core = d == Kick || d == Snare;
                            const float h = (float) (hash (s.noteSeed, (juce::uint64) (d * 16 + st), (juce::uint64) sec.theme, 3) & 0xffff) / 65536.0f;
                            if (core ? (I < 0.3f && st % 8 != 0) : h > 0.35f + 0.75f * I)
                                continue;
                            song.notes[Drums].push_back ({ beat0 + st * 0.25, 0.25, d, v * (0.75f + 0.25f * I) });
                        }
                    if (I > 0.85f && m.drums[Shaker][2] <= 0.0f)   // volle Intensität: Shaker kommt dazu
                        for (int st = 2; st < 16; st += 4)
                            song.notes[Drums].push_back ({ beat0 + st * 0.25, 0.25, Shaker, 0.5f });
                }

                // Melodie: Zwei-Takt-Motive in der Phrase A A B A'
                if (sec.lanes[Lead] && b % 2 == 0)
                {
                    const int unit = (b / 2) % 4;
                    const auto& motif = unit == 2 ? m.motifB : m.motifA;
                    for (size_t k = 0; k < motif.size(); ++k)
                    {
                        auto n = motif[k];
                        if (unit == 3 && k == motif.size() - 1)
                            n.degree += (n.degree % 2 == 0) ? 2 : -1;    // Antwort: letzter Ton anders
                        const double start = beat0 + n.beat;
                        if (start >= secEnd)
                            break;
                        const int bar = b + (int) (n.beat / 4.0);
                        const int chordDeg = m.progression[(size_t) ((bar / chordBars) % 4)];
                        const int p = fold (pitchOf (chordDeg + n.degree, 72, tr), 62, 88);
                        // legato: Ton reicht bis zum nächsten, damit die Melodie gleitet
                        double len = n.length;
                        if (k + 1 < motif.size() && std::abs (motif[k + 1].beat - (n.beat + n.length)) < 0.01)
                            len += 0.05;
                        song.notes[Lead].push_back ({ start, juce::jmin (len, secEnd - start), p, 0.8f + 0.2f * I });
                    }
                }
            }

            // Effekte: Wind und Brandung in ruhigen Teilen, Laser, Anlauf vor Themen
            const double len = sec.bars * 4.0;
            if (sec.lanes[Fx])
            {
                if (sec.type == Intro || sec.type == Outro || sec.type == Break || (sec.type == Interlude && r.chance (0.5f)))
                    song.notes[Fx].push_back ({ sec.startBeat(), len, Wind, 0.7f + 0.3f * closeness });
                if ((sec.type == Intro || sec.type == Outro) && r.chance (era == Oxygene ? 0.7f : 0.35f))
                    song.notes[Fx].push_back ({ sec.startBeat() + 2.0, len - 2.0, Surf, 0.8f });
                int lasers = juce::roundToInt (I * (sec.type == Break || sec.type == Interlude ? 4.0f : 1.5f) * (era == Magnetic ? 2.0f : 1.0f) * (sec.bars / 8.0f));
                if (sec.type == Intro || sec.type == Outro)
                    lasers = 0;
                for (int k = 0; k < lasers; ++k)
                    song.notes[Fx].push_back ({ sec.startBeat() + r.nextInt (sec.bars * 8) * 0.5, 0.5 + r.next() * 1.5, Laser, 0.6f + 0.4f * r.next() });
            }
            // Anlauf (Riser) in den letzten zwei Takten vor Thema oder Höhepunkt
            if (si + 1 < song.sections.size())
            {
                const int nextType = song.sections[si + 1].type;
                if ((nextType == ThemeA || nextType == Climax) && sec.bars >= 4 && r.chance (0.4f + 0.5f * closeness))
                    song.notes[Fx].push_back ({ secEnd - 8.0, 8.0, Riser, 0.8f });
            }
        }

        for (auto& lane : song.notes)
            std::stable_sort (lane.begin(), lane.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });

        song.typicality = decisions > 0 ? (float) typical / (float) decisions : 0.0f;
        return song;
    }
}
