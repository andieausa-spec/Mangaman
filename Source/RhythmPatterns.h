#pragma once

#include "DrumKits.h"
#include <regex>

// Rhythmus-Muster: stiltypische Vorlagen, Zufallsauswahl und Beat aus Text
namespace Rhythm
{
    using Pattern = std::array<juce::uint8, Drums::numTracks * Drums::numSteps>;   // 0 aus, 1 an, 2 Akzent, 3 Wirbel

    inline const char* trackShort[] { "BD", "SD", "CP", "RS", "CH", "OH", "LT", "MT", "HT", "CB", "CY", "PC" };
    inline const char* trackName[]  { "Bassdrum", "Snare", "Klatschen", "Rimshot", "Hi-Hat zu", "Hi-Hat offen",
                                      "Tom tief", "Tom mitte", "Tom hoch", "Kuhglocke", "Becken", "Percussion" };

    struct KitInfo { const char* name; const char* sub; const char* cy; const char* pc; std::vector<int> genres; };

    // Stiltypische Muster: je Spur mehrere Varianten zu 16 Schritten.
    // x = Schlag, X = Akzent, r = Wirbel, 1–9 = Wahrscheinlichkeit in Zehnteln, . = leer. sprinkle: Streu-Schläge bei hoher Dichte.
    struct Genre
    {
        const char* name;
        int bpm;
        float swing;
        std::array<std::vector<const char*>, Drums::numTracks> tracks;
        std::array<float, Drums::numTracks> sprinkle;
    };

    inline const std::vector<Genre>& genres()
    {
        static const std::vector<Genre> list {
        { "House", 124, 0.0f,
          {{ { "X...x...x...x...", "X...x...x...x..2" }, { "................", "................", "....3.......3..." }, { "....x.......x...", "....x.......x...", "....x.......x.3." }, { "................", "...4..4....4..4.", ".......3.......3" }, { "x.x.x.x.x.x.x.x.", "xx.xxx.xxx.xxx.x", "................", "x3x3x3x3x3x3x3x3" }, { "..X...X...X...X.", "..x...x...x...x." }, {}, {}, {}, { "................", "................", "..........3....." }, {}, { "................", "..4...4...4...4.", "x4x4x4x4x4x4x4x4" } }},
          {{ 0.04f, 0.0f, 0.0f, 0.15f, 0.3f, 0.0f, 0.03f, 0.0f, 0.0f, 0.05f, 0.0f, 0.25f }} },
        { "Techno", 132, 0.0f,
          {{ { "X...x...x...x...", "X...x...x...x..3", "X...x...x...x.x." }, {}, { "....x.......x...", "................", "....x.......x..." }, { "..4..4....4..4..", "................", ".....x.....x...." }, { "xxxxxxxxxxxxxxxx", "x.x.x.x.x.x.x.x.", "................" }, { "..x...x...x...x.", "..X...X...X...X.", "................" }, { "................", "...........4..4." }, {}, {}, {}, {}, { "x.x.x.x.x.x.x.x.", "................", "..x...x...x...x." } }},
          {{ 0.05f, 0.0f, 0.0f, 0.2f, 0.35f, 0.0f, 0.04f, 0.04f, 0.0f, 0.0f, 0.0f, 0.15f }} },
        { "Trance", 138, 0.0f,
          {{ { "X...x...x...x..." }, { "....x.......x...", "................" }, { "....x.......x..." }, {}, { "xxxxxxxxxxxxxxxx", "x.x.x.x.x.x.x.x." }, { "..X...X...X...X." }, {}, {}, {}, {}, {}, { "................", "x.x.x.x.x.x.x.x." } }},
          {{ 0.0f, 0.08f, 0.0f, 0.1f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }} },
        { "Garage", 132, 0.22f,
          {{ { "X.........x.....", "X......3..x.....", "X.....x...x..3.." }, { "....x.......x...", "....x.......x..3" }, { "....x.......x...", "................" }, { "...3....3.....3.", ".......4.......4" }, { "..x...x...x...x.", "x.xx..x.x.xx..x.", ".xx..xx..xx..xx." }, { "......x.......x.", "................" }, {}, {}, {}, {}, {}, { "x3x3x3x3x3x3x3x3", "................" } }},
          {{ 0.08f, 0.0f, 0.0f, 0.2f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f }} },
        { "Acid", 126, 0.0f,
          {{ { "X...x...x...x...", "X...x...x..3x..." }, { "................", "....x.......x..." }, { "....x.......x...", "................" }, {}, { "..x...x...x...x.", "x.x.x.x.x.x.x.x.", "xxxxxxxxxxxxxxxx" }, { "......x.......x.", "................", "..x...x...x...x." }, {}, {}, {}, { "................", "..3.....3...3..." }, {}, { "xxxxxxxxxxxxxxxx", "................" } }},
          {{ 0.05f, 0.0f, 0.0f, 0.08f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.06f, 0.0f, 0.0f }} },
        { "Electro", 120, 0.0f,
          {{ { "X.....x...x.....", "X.....x..x..x...", "X..x..x...x.....", "X.........x..x.." }, { "....X.......X...", "....X.......X..3" }, { "....x.......x...", "................" }, {}, { "x.x.x.x.x.x.x.x.", "xxxxxxxxxxxxxxxx", "x.xxx.xxx.xxx.xx" }, { "................", "..............x." }, { "................", "............x..." }, { "................", ".............x.." }, { "................", "..............x." }, { "................", "......x.......x.", "...3......3....." }, {}, { "................", "...x..x....x..x." } }},
          {{ 0.08f, 0.0f, 0.0f, 0.08f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f, 0.0f, 0.06f }} },
        { "Minimal", 124, 0.0f,
          {{ { "X...x...x...x...", "X...x...x...x..3" }, {}, { "................", "....x.......x..." }, { "...x..x....x..x.", "..x..x....x..x..", ".......x.......x" }, { "..x...x...x...x.", "................", ".x.x.x.x.x.x.x.x" }, { "................", "......x.......x." }, {}, {}, {}, { "................", ".........3......" }, {}, { "..4...4...4...4.", "x.4.x.4.x.4.x.4." } }},
          {{ 0.04f, 0.0f, 0.0f, 0.2f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.25f }} },
        { "Hip-Hop", 92, 0.15f,
          {{ { "X......x..x.....", "X.........x..x..", "X......xx.x.....", "X.x.......x..x.." }, { "....X.......X...", "....X.......X..3", "....X..3....X..." }, { "................", "....x.......x..." }, { "................", ".......3.......3" }, { "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.xxx.", "x3x3x3x3x3x3x3x3" }, { "................", "..............x.", "......x........." }, {}, {}, {}, {}, {}, { "................", "..x...x...x...x." } }},
          {{ 0.1f, 0.08f, 0.0f, 0.06f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }} },
        { "Trap", 140, 0.0f,
          {{ { "X.......x.....x.", "X......x..x.....", "X..x......x...x.", "X.........x....." }, { "........X.......", "........X......3" }, { "........X......." }, { "................", "...x.......x...." }, { "xxxxxxxxxxxxxxxx", "x.x.x.x.x.xrx.x.", "xxxxxxrxxxxxxxrr", "x.x.xrx.x.x.xrrr" }, { "................", "..............x." }, {}, {}, {}, {}, {}, { "................", "......x.......x." } }},
          {{ 0.08f, 0.0f, 0.0f, 0.06f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }} },
        { "Breakbeat", 130, 0.0f,
          {{ { "X.x.......xx....", "X.x.......x..x..", "X.........x.....", "X..x......x.x..." }, { "....X..x.x..X..x", "....X.......X...", "....X..3.3..X..3" }, {}, {}, { "x.x.x.x.x.x.x.x.", "xxxxxxxxxxxxxxxx" }, { "................", "..........x.....", "..............x." }, {}, {}, {}, {}, { "x...............", "................" }, { "................", "x.x.x.x.x.x.x.x." } }},
          {{ 0.06f, 0.1f, 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }} },
        { "Drum & Bass", 172, 0.0f,
          {{ { "X.........x.....", "X.........xx....", "X......x..x....." }, { "....X.......X...", "....X..3....X..3", "....X.......X.3." }, {}, { "................", ".....3.....3...." }, { "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "xxxxxxxxxxxxxxxx" }, { "................", "......x........." }, {}, {}, {}, {}, { "x...............", "................" }, { "x.x.x.x.x.x.x.x.", "................" } }},
          {{ 0.05f, 0.1f, 0.0f, 0.1f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }} },
        { "Italo", 118, 0.0f,
          {{ { "X...x...x...x..." }, { "....x.......x..." }, { "................", "....x.......x..." }, {}, { "xxxxxxxxxxxxxxxx", "x.x.x.x.x.x.x.x." }, { "..x...x...x...x.", "................" }, { "................", "..............x." }, { "................", ".............x.." }, { "................", "............x..." }, { "................", "......x.......x." }, {}, { "..x...x...x...x.", "................" } }},
          {{ 0.0f, 0.0f, 0.0f, 0.0f, 0.15f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f, 0.0f, 0.1f }} },
        { "Disco", 118, 0.0f,
          {{ { "X...x...x...x..." }, { "....x.......x...", "....x.......x..3" }, { "................", "....x.......x..." }, {}, { "x.x.x.x.x.x.x.x.", "xxxxxxxxxxxxxxxx" }, { "..X...X...X...X." }, {}, {}, {}, { "................", "x...x...x...x...", "..........x....." }, {}, { "xxxxxxxxxxxxxxxx", "..x...x...x...x." } }},
          {{ 0.0f, 0.0f, 0.0f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f, 0.0f, 0.2f }} },
        { "Funk", 104, 0.1f,
          {{ { "X.....x...x..x..", "X..x......x.....", "X.x...x....x....", "X......x.x..x..." }, { "....X..3.3..X..3", "....X.......X...", "....X..3....X.3.", ".3..X.3..3..X..3" }, {}, { "................", "..3.......3....." }, { "xxxxxxxxxxxxxxxx", "x.x.x.x.x.x.x.x.", "x3x3x3x3x3x3x3x3" }, { "................", "......x.........", "..............x." }, {}, {}, {}, { "................", "x..x..x...x.x..." }, {}, { "................", "x4x4x4x4x4x4x4x4" } }},
          {{ 0.08f, 0.12f, 0.0f, 0.05f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }} },
        { "Pop 80er", 112, 0.0f,
          {{ { "X.......x.x.....", "X......xx.......", "X.......X.......", "X..x....x.x....." }, { "....X.......X..." }, { "....x.......x...", "................" }, {}, { "x.x.x.x.x.x.x.x.", "xxxxxxxxxxxxxxxx" }, { "................", "..............x." }, { "................", "..............x." }, { "................", ".............x.." }, { "................", "............x..." }, {}, { "x...............", "................" }, { "xxxxxxxxxxxxxxxx", "..x...x...x...x.", "................" } }},
          {{ 0.06f, 0.0f, 0.0f, 0.0f, 0.15f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.15f }} },
        { "Rock", 120, 0.0f,
          {{ { "X.......X.X.....", "X.x.....X.......", "X.......X..x....", "X.x...x.X......." }, { "....X.......X...", "....X.......X..3" }, {}, {}, { "x.x.x.x.x.x.x.x.", "X.x.X.x.X.x.X.x." }, { "................", "..............x." }, { "................", "..............xx" }, { "................", ".............x.." }, { "................", "............x..." }, {}, { "X...............", "................" }, { "................", "x.x.x.x.x.x.x.x." } }},
          {{ 0.06f, 0.05f, 0.0f, 0.0f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }} },
        { "Bossa Nova", 128, 0.0f,
          {{ { "X..x....X..x....", "X..xx..xX..xx..x" }, {}, {}, { "x..x..x...x..x..", "..x..x...x..x..." }, { "xxxxxxxxxxxxxxxx", "x.x.x.x.x.x.x.x.", "x3x3x3x3x3x3x3x3" }, {}, {}, {}, {}, { "................", "x.......x......." }, {}, { "x3x3x3x3x3x3x3x3", "................" } }},
          {{ 0.0f, 0.0f, 0.0f, 0.05f, 0.15f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f }} },
        { "Samba", 100, 0.0f,
          {{ { "x..xX..xx..xX..x", "X..x..x.X..x..x." }, { "..x...x...x...x.", "x.xx.xx.x.xx.xx." }, {}, { "x.x.xx.x.x.xx.x.", "x..x..x...x..x.." }, { "................", "x.x.x.x.x.x.x.x." }, {}, { "................", "........x......." }, {}, {}, { "x.x...x.x.x...x.", "................" }, {}, { "xXxxxXxxxXxxxXxx", "xxxxxxxxxxxxxxxx" } }},
          {{ 0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.2f }} },
        { "Rumba", 110, 0.0f,
          {{ { "x.......x.......", "X.....x.X.......", "...x.......x...." }, {}, {}, { "x..x..x...x.x...", "x..x...x..x.x..." }, { "................", "x.x.x.x.x.x.x.x." }, {}, { "......xx......xx", "................" }, {}, {}, { "x...x...x.x.x...", "x...x...x...x..." }, {}, { "..x...xx..x...xx", "x3x3x3x3x3x3x3x3" } }},
          {{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.05f, 0.0f, 0.0f, 0.06f, 0.0f, 0.15f }} },
        { "Reggae", 76, 0.2f,
          {{ { "........X.......", "X.......X.......", "........X.....x." }, { "................", "........x......." }, {}, { "........X.......", "....x.......x..." }, { "x.x.x.x.x.x.x.x.", "x.xxx.xxx.xxx.xx", "..x...x...x...x." }, { "................", "......x.......x." }, { "................", "..............xx" }, {}, {}, {}, {}, { "..x...x...x...x.", "................" } }},
          {{ 0.0f, 0.0f, 0.0f, 0.06f, 0.15f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.08f }} }
        };
        return list;
    }

    inline int genreIndex (const char* name)
    {
        const auto& g = genres();
        for (int i = 0; i < (int) g.size(); ++i)
            if (juce::String (g[(size_t) i].name) == name)
                return i;
        jassertfalse;
        return 0;
    }

    inline const std::array<KitInfo, 10>& kitInfo()
    {
        static const auto list = []
        {
            auto ids = [] (std::initializer_list<const char*> names) { std::vector<int> v; for (auto* n : names) v.push_back (genreIndex (n)); return v; };
            return std::array<KitInfo, 10> {{
                { "808", "Boom-Bass, Electro, Hip-Hop", "Becken", "Clave", ids ({ "Electro", "Hip-Hop", "Trap", "Breakbeat", "Rumba" }) },
                { "909", "House, Techno, Trance", "Crash", "Ride", ids ({ "House", "Techno", "Trance", "Garage" }) },
                { "606", "Acid, Electro, Minimal", "Becken", "Shaker", ids ({ "Acid", "Electro", "Minimal", "Techno" }) },
                { "707", "Italo, Hi-NRG, House", "Crash", "Tamburin", ids ({ "Italo", "Disco", "House", "Pop 80er" }) },
                { "CR-78", "Disco, Funk, Bossa", "Becken", "Guiro", ids ({ "Disco", "Funk", "Bossa Nova", "Pop 80er", "Reggae" }) },
                { "LinnDrum", "80er-Pop, Funk, Rock", "Crash", "Cabasa", ids ({ "Pop 80er", "Funk", "Rock", "Hip-Hop" }) },
                { "DMX", "Hip-Hop, Electro, Funk", "Crash", "Tamburin", ids ({ "Hip-Hop", "Electro", "Funk", "Breakbeat" }) },
                { "Drumulator", "80er-Rock, Pop", "Crash", "Clave", ids ({ "Rock", "Pop 80er", "Funk" }) },
                { "SDS-V", "Elektro-Toms, Synth-Rock", "Becken", "Tom-Effekt", ids ({ "Rock", "Pop 80er", "Breakbeat", "Drum & Bass" }) },
                { "Mini Pops", "Bossa, Samba, Rumba", "Becken", "Maracas", ids ({ "Bossa Nova", "Samba", "Rumba", "Disco", "Reggae" }) },
            }};
        }();
        return list;
    }

    // Name einer Spur im gewählten Modell (Becken und Percussion heißen je Modell anders)
    inline juce::String trackLabel (int kit, int t)
    {
        const auto& k = kitInfo()[(size_t) juce::jlimit (0, 9, kit)];
        return t == 11 ? juce::String (k.pc) : t == 10 ? juce::String (k.cy) : juce::String (trackName[t]);
    }

    // Gleiche Zufallsfolge wie die Web-Version (mulberry32), damit Muster-Nummern dieselben Beats ergeben
    struct Rng
    {
        explicit Rng (juce::uint32 seed) : a (seed) {}
        double operator()()
        {
            a += 0x6D2B79F5u;
            juce::uint32 t = a;
            t = (t ^ (t >> 15)) * (t | 1u);
            t ^= t + (t ^ (t >> 7)) * (t | 61u);
            return (double) (t ^ (t >> 14)) / 4294967296.0;
        }
        juce::uint32 a;
    };

    inline juce::uint32 hashText (const juce::String& s)
    {
        juce::uint32 h = 2166136261u;
        for (auto p = s.getCharPointer(); ! p.isEmpty();)
        {
            const auto c = p.getAndAdvance();
            if (c > 0xffff)   // wie JavaScript: Zeichen außerhalb der Grundebene als zwei Hälften
            {
                const auto v = c - 0x10000;
                h ^= (juce::uint32) (0xd800 + (v >> 10)); h *= 16777619u;
                h ^= (juce::uint32) (0xdc00 + (v & 0x3ff)); h *= 16777619u;
                continue;
            }
            h ^= (juce::uint32) c;
            h *= 16777619u;
        }
        return h;
    }

    // Stil-Liste für Modell und Stil-Wahl (0 = passend zum Modell)
    inline int genreFor (int kit, int style, int nr)
    {
        if (style > 0)
            return juce::jlimit (0, (int) genres().size() - 1, style - 1);
        const auto& list = kitInfo()[(size_t) juce::jlimit (0, 9, kit)].genres;
        return list[(size_t) (nr % (int) list.size())];
    }

    // Muster erzeugen: Modell (oder Stil), Muster-Nummer und Dichte ergeben immer dasselbe Ergebnis
    inline Pattern generate (int kit, int style, int nr, float density)
    {
        const int gi = genreFor (kit, style, nr);
        const auto& g = genres()[(size_t) gi];
        const juce::uint32 seed = (juce::uint32) (nr * 7919 + gi * 104729 + 1);
        Rng pick (seed);
        const double uTom = pick();
        const float D = juce::jlimit (0.0f, 1.0f, density);
        Pattern out {};
        for (int t = 0; t < Drums::numTracks; ++t)
        {
            const auto& alts = g.tracks[(size_t) t];
            const double u = t >= 6 && t <= 8 ? uTom : pick();
            const char* s = alts.empty() ? nullptr : alts[(size_t) juce::jmin ((int) alts.size() - 1, (int) (u * (double) alts.size()))];
            const float sp = g.sprinkle[(size_t) t] * juce::jmax (0.0f, D - 0.5f) * 2.0f;
            for (int i = 0; i < Drums::numSteps; ++i)
            {
                // je Schritt eine feste Zufallszahl: mehr Dichte fügt nur hinzu, weniger nimmt nur weg
                const double h = Rng (seed ^ ((juce::uint32) (t * 977 + i * 131 + 7) * 2654435761u))();
                const char c = s != nullptr ? s[i] : '.';
                int v = 0;
                if (c == 'X') v = 2;
                else if (c == 'x') v = 1;
                else if (c == 'r') v = D < 0.3f ? 1 : 3;
                else if (c >= '1' && c <= '9') v = h < (double) (c - '0') / 10.0 * D * 2.0 ? 1 : 0;
                else if (sp > 0.0f && h < sp && (i % 4 != 0 || t == 4 || t == 11)) v = 1;
                if (v != 0 && D < 0.35f && i % 4 != 0 && c != 'X' && h > D / 0.35f) v = 0;
                out[(size_t) (t * Drums::numSteps + i)] = (juce::uint8) v;
            }
        }
        return out;
    }

    inline juce::String toString (const Pattern& p)
    {
        juce::String s;
        for (auto v : p)
            s << juce::String::charToString ((juce::juce_wchar) ('0' + juce::jlimit (0, 3, (int) v)));
        return s;
    }

    inline bool fromString (const juce::String& s, Pattern& p)
    {
        if (s.length() != (int) p.size())
            return false;
        for (int i = 0; i < (int) p.size(); ++i)
            p[(size_t) i] = (juce::uint8) juce::jlimit (0, 3, (int) (s[i] - '0'));
        return true;
    }

    //==========================================================================
    // Beat aus Text: Modell, Stil, Tempo, Eigenschaften, Spuren ohne/mit, getippte Raster und Lautmalerei
    struct TextBeat
    {
        int kit = -1, style = -1, bpm = 0, fill = -1, rate = -1;
        float density = -1, variation = -1, swing = -1, humanize = -1, accent = -1;
        juce::uint32 mutes = 0, adds = 0;
        bool rolls = false, typed = false;
        std::map<int, std::array<juce::uint8, Drums::numSteps>> grid;   // getippte Spuren
        bool ono = false;                                               // Lautmalerei ersetzt das ganze Muster
        Pattern onoPattern {};
        juce::StringArray read;                                         // was erkannt wurde
    };

    namespace detail
    {
        inline std::string ascii (const juce::String& text)
        {
            return text.toLowerCase().replace (juce::String::fromUTF8 ("\xc3\xa4"), "ae")
                                     .replace (juce::String::fromUTF8 ("\xc3\xb6"), "oe")
                                     .replace (juce::String::fromUTF8 ("\xc3\xbc"), "ue")
                                     .replace (juce::String::fromUTF8 ("\xc3\x9f"), "ss").toStdString();
        }

        inline bool has (const std::string& s, const char* rx) { return std::regex_search (s, std::regex (rx)); }

        // Wort -> Spuren
        inline std::vector<int> trackWords (const std::string& w)
        {
            static const std::vector<std::pair<std::regex, std::vector<int>>> table = []
            {
                std::vector<std::pair<std::regex, std::vector<int>>> t;
                auto add = [&t] (const char* rx, std::vector<int> v) { t.emplace_back (std::regex (rx), std::move (v)); };
                add ("^(kick|bass.?drum|bassdrum|bd|baesse|bass)$", { 0 });
                add ("^(snare|sd|snares)$", { 1 });
                add ("^(clap|claps|klatschen|klatscher|handclaps?|cp)$", { 2 });
                add ("^(rim|rims|rimshots?|sidestick|rs)$", { 3 });
                add ("^(offenen?|open)$", { 5 });
                add ("^(hi.?hats?|hats?|hihats?|ch)$", { 4 });
                add ("^(toms?|tomtoms?|trommeln)$", { 6, 7, 8 });
                add ("^(cowbell|kuhglocke|glocke|cb)$", { 9 });
                add ("^(becken|crash|cymbal|cy)$", { 10 });
                add ("^(ride|clave|shaker|tamburin|percussion|perc|conga|congas|guiro|cabasa|maracas|pc)$", { 11 });
                return t;
            }();
            for (auto& [rx, v] : table)
                if (std::regex_match (w, rx))
                    return v;
            return {};
        }

        // Lautmalerei: Silbe -> Spur (-1 = Pause, -2 = unbekannt)
        inline int onoWord (const std::string& w)
        {
            static const std::vector<std::pair<std::regex, int>> table = []
            {
                std::vector<std::pair<std::regex, int>> t;
                t.emplace_back (std::regex ("^(b+u+m+s*|b+o+o+m+|d+u+m+|d+o+m+|t+u+m+|w+u+m+|bd|kick)$"), 0);
                t.emplace_back (std::regex ("^(t+s+c+h+a+c*k+|tschack|t+a+c*k+|p+a+h*|k+a+|z+a+c*k+|snare|sd|kla+t*sch|kla+p+)$"), 1);
                t.emplace_back (std::regex ("^(p+s+h*|t+s+s+s+c*h+|k+s+c+h+|zisch)$"), 5);
                t.emplace_back (std::regex ("^(t+s+|t+z+|z+|t+s+c+h|t+i+c+k+|t+k|ch|hat)$"), 4);
                t.emplace_back (std::regex ("^(-+|\\.+|_+|pause)$"), -1);
                return t;
            }();
            for (auto& [rx, v] : table)
                if (std::regex_match (w, rx))
                    return v;
            return -2;
        }

        inline std::vector<std::string> splitLines (const std::string& s)
        {
            std::vector<std::string> out;
            std::string cur;
            for (char c : s)
            {
                if (c == '\n' || c == ';') { out.push_back (cur); cur.clear(); }
                else if (c != '\r') cur += c;
            }
            out.push_back (cur);
            return out;
        }

        inline const std::array<const char*, Drums::numTracks>& addPatterns()
        {
            static const std::array<const char*, Drums::numTracks> a { "x...x...x...x...", "....x.......x...", "....x.......x...", "...x..x....x..x.",
                "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "..............x.", ".............x..", "............x...", "......x.......x.", "x...............", "..x...x...x...x." };
            return a;
        }
    }

    inline TextBeat parse (const juce::String& text)
    {
        using namespace detail;
        TextBeat r;
        const std::string raw = text.toStdString();
        const std::string T = ascii (text);

        // getippte Raster: "bd: x...x..." je Zeile
        static const std::regex gridLine ("^\\s*([a-z\\-]{2,10})\\s*[:=]\\s*([xo.\\-_r0-9| ]{4,})\\s*$");
        static const std::regex gridLineRaw ("^\\s*([A-Za-z\\-]{2,10})\\s*[:=]\\s*([xXoO.\\-_r0-9| ]{4,})\\s*$");
        const auto rawLines = splitLines (raw);
        std::string body;
        for (auto& line : rawLines)
        {
            std::smatch m;
            if (std::regex_match (line, m, gridLineRaw))
            {
                auto ts = trackWords (juce::String (m[1].str()).toLowerCase().toStdString());
                if (! ts.empty())
                {
                    std::vector<int> a;
                    for (char c : m[2].str())
                    {
                        if (c == '|' || c == ' ') continue;
                        a.push_back (c == 'X' || c == 'O' ? 2 : c == 'r' ? 3 : (c == 'x' || c == 'o' || (c >= '1' && c <= '9')) ? 1 : 0);
                    }
                    const int L = (int) a.size();
                    std::array<juce::uint8, Drums::numSteps> row {};
                    for (int i = 0; i < Drums::numSteps; ++i)
                        row[(size_t) i] = (juce::uint8) (Drums::numSteps % L == 0 || L >= Drums::numSteps ? a[(size_t) (i % L)] : (i < L ? a[(size_t) i] : 0));
                    r.grid[ts[0]] = row;
                    continue;
                }
            }
            body += ascii (juce::String (line)) + " ";
        }
        juce::ignoreUnused (T, gridLine);
        if (! r.grid.empty())
        {
            r.typed = true;
            r.read.add (juce::String ((int) r.grid.size()) + (r.grid.size() == 1 ? " Spur getippt" : " Spuren getippt"));
        }

        // Modell
        static const char* kitRx[] { "\\b808\\b|acht.?null.?acht", "\\b909\\b|neun.?null.?neun", "\\b606\\b", "\\b707\\b",
                                     "\\bcr.?78\\b|compu.?rhythm", "linn", "\\bdmx\\b|oberheim", "drumulator|\\be-?mu\\b",
                                     "simmons|\\bsds", "mini.?pops?" };
        std::string rest = body;
        for (int i = 0; i < 10; ++i)
            if (r.kit < 0 && has (rest, kitRx[i]))
                r.kit = i;
        for (auto* rx : kitRx)
            rest = std::regex_replace (rest, std::regex (rx), " ");
        if (r.kit >= 0)
            r.read.add (juce::String (kitInfo()[(size_t) r.kit].name) + "-Stil");

        // Lautmalerei: "bum tschak bum bum tschak"
        std::vector<std::string> toks;
        {
            std::string cur;
            auto flush = [&]
            {
                std::string w;
                for (char c : cur)
                    if ((c >= 'a' && c <= 'z') || c == '-' || c == '.') w += c;
                if (! w.empty()) toks.push_back (w);
                cur.clear();
            };
            for (size_t i = 0; i < rest.size(); ++i)
            {
                const char c = rest[i];
                const bool sep = std::isspace ((unsigned char) c) || c == ',' || c == ';' || c == '/' || c == '!' || c == '?';
                // Bindestrich zwischen zwei Buchstaben trennt Silben ("bum-tschak")
                const bool hyphen = c == '-' && i > 0 && i + 1 < rest.size() && std::isalnum ((unsigned char) rest[i - 1]) && std::isalnum ((unsigned char) rest[i + 1]);
                if (sep || hyphen) flush(); else cur += c;
            }
            flush();
        }
        std::vector<int> seq;
        for (auto& w : toks)
            if (const int t = onoWord (w); t != -2)
                seq.push_back (t);
        if (seq.size() >= 3 && (double) seq.size() >= (double) toks.size() * 0.6)
        {
            if (seq.size() > 16) seq.resize (16);
            const int n = (int) seq.size(), per = n <= 4 ? 4 : n <= 8 ? 2 : 1, span = n * per;
            for (int k = 0; k < n; ++k)
                if (seq[(size_t) k] >= 0)
                    r.onoPattern[(size_t) (seq[(size_t) k] * Drums::numSteps + k * per)] = (juce::uint8) (k == 0 ? 2 : 1);
            if (Drums::numSteps % span == 0)
                for (int t = 0; t < Drums::numTracks; ++t)
                    for (int i = span; i < Drums::numSteps; ++i)
                        r.onoPattern[(size_t) (t * Drums::numSteps + i)] = r.onoPattern[(size_t) (t * Drums::numSteps + i % span)];
            r.ono = true;
            r.read.add ("Lautmalerei: " + juce::String (n) + " Schlaege");
            return r;
        }

        // Stil
        static const std::vector<std::pair<const char*, const char*>> genreRx {
            { "house", "House" }, { "techno|\\brave\\b|industrial", "Techno" }, { "trance", "Trance" }, { "garage|2.?step", "Garage" },
            { "acid", "Acid" }, { "drum.?(and|&|n|'n'|und).?bass|\\bdnb\\b|\\bd&b\\b|jungle", "Drum & Bass" },
            { "electro|miami", "Electro" }, { "minimal", "Minimal" }, { "hip.?hop|boom.?bap|\\brap\\b|lo.?fi", "Hip-Hop" },
            { "\\btrap\\b|\\bdrill\\b", "Trap" }, { "breakbeat|big.?beat|\\bbreaks\\b", "Breakbeat" },
            { "italo|hi.?nrg|eurodisco", "Italo" }, { "disco", "Disco" }, { "funk", "Funk" },
            { "\\bpop|80er|achtziger|new.?wave|synth.?pop", "Pop 80er" }, { "rock|metal|punk|grunge", "Rock" },
            { "bossa", "Bossa Nova" }, { "samba|brasil|karneval", "Samba" }, { "rumba|cha.?cha|salsa|latin|mambo", "Rumba" },
            { "reggae|\\bdub\\b|\\bska\\b|one.?drop", "Reggae" } };
        for (auto& [rx, name] : genreRx)
            if (has (rest, rx)) { r.style = genreIndex (name); r.read.add (name); break; }

        // Tempo
        std::smatch bm;
        if (std::regex_search (rest, bm, std::regex ("\\b(\\d{2,3})\\s*(bpm|schlaege|beats)?\\b")) && std::stoi (bm[1].str()) >= 40 && std::stoi (bm[1].str()) <= 240)
        {
            r.bpm = std::stoi (bm[1].str());
            r.read.add (juce::String (r.bpm) + " BPM");
        }
        else if (r.style >= 0)
            r.bpm = genres()[(size_t) r.style].bpm;
        if (has (rest, "langsam|gemuetlich|\\bchill|schleppend")) { r.bpm = juce::roundToInt ((r.bpm > 0 ? r.bpm : 120) * 0.85); r.read.add ("langsam"); }
        if (has (rest, "schnell|rasend|hektisch|gehetzt"))        { r.bpm = juce::roundToInt ((r.bpm > 0 ? r.bpm : 120) * 1.15); r.read.add ("schnell"); }

        // Eigenschaften
        auto feature = [&] (const char* rx, const char* label) { if (has (rest, rx)) { r.read.add (label); return true; } return false; };
        if (feature ("treibend|dicht|\\bvoll|busy|viele noten|komplex|wuchtig|fett", "dicht")) r.density = 0.8f;
        if (feature ("sparsam|\\bleer|reduziert|\\bwenig|ruhig|einfach|simpel|schlicht|luftig|minimalistisch", "sparsam")) r.density = 0.25f;
        if (feature ("wild|chaos|chaotisch|verspielt|lebendig|abwechslung|variation|ueberrasch", "verspielt")) { r.variation = 0.55f; r.fill = 1; }
        if (feature ("stur|monoton|hypnotisch|immer gleich", "stur")) { r.variation = 0.0f; r.fill = 0; }
        if (feature ("\\bfills?\\b|snare.?wirbel|trommelwirbel|uebergaenge", "Fills"))
            r.fill = has (rest, "alle 8|8 takte") ? 2 : has (rest, "alle 16|16 takte") ? 3 : 1;
        if (has (rest, "ohne fill")) r.fill = 0;
        if (feature ("shuffle|swing|groov|laessig|laid.?back|triolisch|wipp", "Shuffle")) r.swing = 0.28f;
        if (feature ("gerade|straight|steif|exakt|maschinell|roboter", "gerade")) { r.swing = 0.0f; r.humanize = 0.0f; }
        if (feature ("menschlich|locker|\\blive\\b|human|handgespielt|organisch|lo.?fi", "locker")) r.humanize = 0.45f;
        if (feature ("hart|aggressiv|knallig|druckvoll|\\blaut|brutal|punchy", "hart")) r.accent = 0.85f;
        if (feature ("weich|sanft|leise|zart|\\bsoft", "sanft")) r.accent = 0.2f;
        if (feature ("half.?time|halbes tempo|halftime", "Half-Time")) r.rate = 0;
        if (feature ("double.?time|doppeltes? tempo", "Double-Time")) r.rate = 2;

        // ohne … / mit …
        static const std::regex without ("\\bohne\\s+(?:die\\s+|den\\s+|das\\s+|jede\\s+)?(offenen?\\s+)?([a-z\\-]+)");
        for (std::sregex_iterator it (rest.begin(), rest.end(), without), end; it != end; ++it)
        {
            const auto ts = (*it)[1].matched ? std::vector<int> { 5 } : trackWords ((*it)[2].str());
            if (ts.empty()) continue;
            for (int t : ts) r.mutes |= 1u << t;
            r.read.add (juce::String ("ohne ") + trackName[ts[0]]);
        }
        static const std::regex with ("\\b(?:mit|plus|und|dazu)\\s+(?:viel\\s+|vielen\\s+|der\\s+|den\\s+|dem\\s+|einer?\\s+|einem\\s+)?(offenen?\\s+)?([a-z\\-]+)");
        for (std::sregex_iterator it (rest.begin(), rest.end(), with), end; it != end; ++it)
        {
            const auto ts = (*it)[1].matched ? std::vector<int> { 5 } : trackWords ((*it)[2].str());
            if (ts.empty()) continue;
            bool muted = false;
            for (int t : ts) muted |= (r.mutes >> t) & 1u;
            if (muted) continue;
            for (int t : ts) r.adds |= 1u << t;
            r.read.add (juce::String ("+ ") + ((*it)[1].matched ? "offene Hi-Hat" : trackName[ts[0]]));
        }
        if (has (rest, "hi.?hat.?wirbel|hat.?rolls?|wirbeln")) r.rolls = true;
        return r;
    }

    // Muster zum gelesenen Text: Vorlage erzeugen, dann Spuren ergänzen, Wirbel und getippte Raster einsetzen
    inline Pattern patternFor (const TextBeat& r, int kit, int style, int nr, float density)
    {
        if (r.ono)
            return r.onoPattern;
        auto pat = generate (kit, style, nr, density);
        const auto& add = detail::addPatterns();
        for (int t = 0; t < Drums::numTracks; ++t)
        {
            if (((r.adds >> t) & 1u) == 0) continue;
            bool any = false;
            for (int i = 0; i < Drums::numSteps; ++i) any |= pat[(size_t) (t * Drums::numSteps + i)] != 0;
            if (! any)
                for (int i = 0; i < Drums::numSteps; ++i)
                    pat[(size_t) (t * Drums::numSteps + i)] = add[(size_t) t][i] == 'x' ? 1 : 0;
        }
        if (r.rolls)
            for (int i : { 6, 7, 14, 15 })
                if (pat[(size_t) (4 * Drums::numSteps + i)] != 0)
                    pat[(size_t) (4 * Drums::numSteps + i)] = 3;
        if (r.grid.size() >= 2)
            pat.fill (0);
        for (auto& [t, row] : r.grid)
            for (int i = 0; i < Drums::numSteps; ++i)
                pat[(size_t) (t * Drums::numSteps + i)] = row[(size_t) i];
        return pat;
    }

    inline const juce::StringArray& ideas()
    {
        static const juce::StringArray list {
            juce::String::fromUTF8 ("Treibender 909-Techno, 132 BPM, mit offenen Hats"),
            juce::String::fromUTF8 ("L\xc3\xa4ssiger 808 Hip-Hop mit Shuffle und Kuhglocke"),
            "LinnDrum Funk, locker, viele Fills",
            "Bossa Nova aus der Mini Pops",
            "CR-78 Disco, sanft",
            "Harter DMX Electro ohne Klatschen",
            "Trap auf der 808 mit Hi-Hat-Wirbeln",
            "Bum tschak bum bum tschak",
            "bd: x...x...x..x.x..\nsd: ....x.......x...\nch: x.xxx.xxx.xxx.xx" };
        return list;
    }
}
