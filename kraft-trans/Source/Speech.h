#pragma once

#include <JuceHeader.h>

// Aus Text wird Sprache: zerlegt deutschen Text (auch eigenen, frei eingetippten) in Silben aus Lauten,
// die die Roboterstimme spricht. Regeln nach der deutschen Rechtschreibung (sch, ch, ei, ie, z, sp/st am
// Wortanfang, Auslautverhärtung, Dehnungs-h, unbetontes e am Wortende), Ziffern werden zu Zahlwörtern.
// Keine Wörterbücher, keine Aufnahmen: jede Silbe ist eine Folge synthetischer Laute.
namespace Kt::Phon
{
    // Konsonanten
    enum Phone { PM, PN, PNg, PL, PR, PW, PJ, PS, PZ, PSch, PCh, PX, PF, PH, PT, PK, PP, PD, PG, PB, numPhones };
    // Vokale (Ai = ei, Au = au, Oi = eu/äu, Er = vokalisches r am Wortende, Hum = gesummt)
    enum Nucleus { A, E, I, O, U, Ae, Oe, Ue, Ai, Au, Oi, Schwa, Er, Hum, numNuclei };

    struct Syl
    {
        std::array<juce::uint8, 3> on {}, co {};
        juce::uint8 nOn = 0, nCo = 0, nucleus = A;
        bool spring = false;    // "boing": Tonhöhe federt
        bool operator== (const Syl& o) const
        {
            return on == o.on && co == o.co && nOn == o.nOn && nCo == o.nCo && nucleus == o.nucleus && spring == o.spring;
        }
    };

    struct SylText { Syl syl; juce::String text; };
    using Word = std::vector<SylText>;

    namespace detail
    {
        struct Unit
        {
            bool vowel = false;
            int nucleus = A;
            std::array<int, 2> phones { -1, -1 };
            int nPhones = 0;
            int from = 0, to = 0;   // Zeichen im Wort
        };

        inline bool isVowelChar (juce::juce_wchar c)
        {
            return juce::String ("aeiouy").containsChar (c) || c == 0xe4 || c == 0xf6 || c == 0xfc;
        }

        inline bool legalOnset (const std::vector<int>& p)
        {
            if (p.empty()) return true;
            if (p.size() == 1) return p[0] != PNg;
            auto in = [] (int x, std::initializer_list<int> set) { return std::find (set.begin(), set.end(), x) != set.end(); };
            if (p.size() == 2)
            {
                if (in (p[0], { PP, PB, PK, PG, PF }) && in (p[1], { PR, PL })) return true;
                if (in (p[0], { PT, PD }) && p[1] == PR) return true;
                if (p[0] == PSch && in (p[1], { PT, PP, PW, PL, PM, PN, PR })) return true;
                if (p[0] == PT && in (p[1], { PS, PSch })) return true;
                if (p[0] == PK && in (p[1], { PW, PN })) return true;
                if (p[0] == PG && in (p[1], { PN })) return true;
                if (p[0] == PP && p[1] == PF) return true;
                return false;
            }
            if (p.size() == 3)
                return (p[0] == PSch && (p[1] == PT || p[1] == PP) && (p[2] == PR || p[2] == PL))
                    || (p[0] == PT && p[1] == PS && p[2] == PW);
            return false;
        }

        // Zahl als deutsches Zahlwort (bis 99, darüber Ziffer für Ziffer)
        inline juce::String numberWords (const juce::String& digits)
        {
            static const char* ones[] { "null", "eins", "zwei", "drei", "vier", "fünf", "sechs", "sieben", "acht", "neun",
                                        "zehn", "elf", "zwölf", "dreizehn", "vierzehn", "fünfzehn", "sechzehn", "siebzehn", "achtzehn", "neunzehn" };
            static const char* tens[] { "", "", "zwanzig", "dreißig", "vierzig", "fünfzig", "sechzig", "siebzig", "achtzig", "neunzig" };
            const int n = digits.getIntValue();
            if (digits.length() <= 2)
            {
                if (n < 20) return juce::String::fromUTF8 (ones[n]);
                juce::String t = juce::String::fromUTF8 (tens[n / 10]);
                if (n % 10 == 0) return t;
                return juce::String::fromUTF8 (n % 10 == 1 ? "ein" : ones[n % 10]) + "und" + t;
            }
            juce::String out;
            for (auto c : digits)
                out << juce::String::fromUTF8 (ones[c - '0']) << " ";
            return out.trim();
        }

        inline Word parseWord (const juce::String& original)
        {
            const juce::String w = original.toLowerCase()
                                       .replace (juce::String::fromUTF8 ("Ä"), juce::String::fromUTF8 ("ä"))
                                       .replace (juce::String::fromUTF8 ("Ö"), juce::String::fromUTF8 ("ö"))
                                       .replace (juce::String::fromUTF8 ("Ü"), juce::String::fromUTF8 ("ü"));
            const int len = w.length();
            auto at = [&] (int i) -> juce::juce_wchar { return i >= 0 && i < len ? w[i] : 0; };
            auto starts = [&] (int i, const char* s) { return w.substring (i).startsWith (juce::String::fromUTF8 (s)); };

            std::vector<Unit> units;
            int lastNucleus = -1;
            int i = 0;
            while (i < len)
            {
                Unit u;
                u.from = i;
                const auto c = at (i);
                if (isVowelChar (c))
                {
                    u.vowel = true;
                    struct VD { const char* s; int n; };
                    static const VD digraphs[] { { "äu", Oi }, { "eu", Oi }, { "ei", Ai }, { "ai", Ai }, { "ey", Ai }, { "ay", Ai },
                                                 { "au", Au }, { "ie", I }, { "aa", A }, { "ee", E }, { "oo", O } };
                    int used = 1;
                    u.nucleus = c == 'a' ? A : c == 'e' ? E : c == 'i' ? I : c == 'o' ? O : c == 'u' ? U
                              : c == 0xe4 ? Ae : c == 0xf6 ? Oe : Ue;
                    for (auto& d : digraphs)
                        if (starts (i, d.s)) { u.nucleus = d.n; used = 2; break; }
                    i += used;
                    if (at (i) == 'h' && ! isVowelChar (at (i + 1)))   // Dehnungs-h
                        ++i;
                    else if (at (i) == 'h' && isVowelChar (at (i + 1)))
                        ++i;                                            // stummes h zwischen Vokalen (sehen)
                    u.to = i;
                    lastNucleus = u.nucleus;
                    units.push_back (u);
                    continue;
                }

                auto add = [&] (int p1, int p2, int used)
                {
                    u.phones = { p1, p2 };
                    u.nPhones = p2 >= 0 ? 2 : 1;
                    i += used;
                    u.to = i;
                    units.push_back (u);
                };
                const auto next = at (i + 1);
                if (starts (i, "tsch"))      add (PT, PSch, 4);
                else if (starts (i, "sch"))  add (PSch, -1, 3);
                else if (starts (i, "ch"))
                {
                    if (i == 0) add (juce::String ("ei").containsChar (at (i + 2)) ? PCh : PK, -1, 2);
                    else add (lastNucleus == A || lastNucleus == O || lastNucleus == U || lastNucleus == Au ? PX : PCh, -1, 2);
                }
                else if (starts (i, "ck"))   add (PK, -1, 2);
                else if (starts (i, "ng"))   add (PNg, -1, 2);
                else if (starts (i, "nk"))   { add (PNg, -1, 1); }
                else if (starts (i, "ph"))   add (PF, -1, 2);
                else if (starts (i, "pf"))   add (PP, PF, 2);
                else if (starts (i, "qu"))   add (PK, PW, 2);
                else if (starts (i, "th"))   add (PT, -1, 2);
                else if (starts (i, "tz"))   add (PT, PS, 2);
                else if (starts (i, "dt"))   add (PT, -1, 2);
                else if (starts (i, "ss"))   add (PS, -1, 2);
                else if (i == 0 && (starts (i, "sp") || starts (i, "st")))
                {
                    add (PSch, -1, 1);
                }
                else if (c == next && juce::String ("bdfgklmnprt").containsChar (c))
                {
                    // Doppelkonsonant: ein Laut
                    static const juce::String letters ("bdfgklmnprt");
                    static const int phones[] { PB, PD, PF, PG, PK, PL, PM, PN, PP, PR, PT };
                    add (phones[letters.indexOfChar (c)], -1, 2);
                }
                else
                {
                    switch (c)
                    {
                        case 'b': add (PB, -1, 1); break;
                        case 'd': add (PD, -1, 1); break;
                        case 'f': add (PF, -1, 1); break;
                        case 'g': add (PG, -1, 1); break;
                        case 'h': add (PH, -1, 1); break;
                        case 'j': add (PJ, -1, 1); break;
                        case 'k': add (PK, -1, 1); break;
                        case 'l': add (PL, -1, 1); break;
                        case 'm': add (PM, -1, 1); break;
                        case 'n': add (PN, -1, 1); break;
                        case 'p': add (PP, -1, 1); break;
                        case 'r': add (PR, -1, 1); break;
                        case 't': add (PT, -1, 1); break;
                        case 'v': add (PF, -1, 1); break;
                        case 'w': add (PW, -1, 1); break;
                        case 'x': add (PK, PS, 1); break;
                        case 'z': add (PT, PS, 1); break;
                        case 'c': add (juce::String ("ei").containsChar (next) ? PT : PK, juce::String ("ei").containsChar (next) ? PS : -1, 1); break;
                        case 's': add (isVowelChar (next) && (i == 0 || isVowelChar (at (i - 1)) || juce::String ("lnr").containsChar (at (i - 1))) ? PZ : PS, -1, 1); break;
                        case 0xdf: add (PS, -1, 1); break;   // ß
                        default: ++i; break;                  // unbekanntes Zeichen überspringen
                    }
                }
                // sp/st am Wortanfang: nach dem sch folgt p bzw. t als eigener Laut
                if (u.from == 0 && units.size() == 1 && units[0].phones[0] == PSch && (starts (0, "sp") || starts (0, "st")))
                {
                    Unit t;
                    t.from = 1; t.to = 2;
                    t.phones = { w[1] == 'p' ? PP : PT, -1 };
                    t.nPhones = 1;
                    units.push_back (t);
                    i = 2;
                }
            }

            // Silben bilden: jeder Vokal ist ein Kern, Konsonanten dazwischen werden geteilt
            std::vector<int> nuclei;
            for (int k = 0; k < (int) units.size(); ++k)
                if (units[(size_t) k].vowel) nuclei.push_back (k);

            Word word;
            auto phonesOf = [&] (int a, int b)   // Laute der Einheiten [a, b)
            {
                std::vector<int> p;
                for (int k = a; k < b; ++k)
                    for (int q = 0; q < units[(size_t) k].nPhones; ++q)
                        p.push_back (units[(size_t) k].phones[(size_t) q]);
                return p;
            };
            if (nuclei.empty())
            {
                // ohne Vokal (pst, hm): ein Laut-Bündel mit kurzem Murmelvokal oder Summen
                SylText st;
                auto p = phonesOf (0, (int) units.size());
                const bool hum = std::find (p.begin(), p.end(), PM) != p.end();
                st.syl.nucleus = hum ? Hum : Schwa;
                for (int k = 0; k < (int) p.size() && st.syl.nOn < 3; ++k)
                    if (! (hum && p[(size_t) k] == PM)) st.syl.on[st.syl.nOn++] = (juce::uint8) p[(size_t) k];
                st.text = original;
                if (! p.empty()) word.push_back (st);
                return word;
            }

            std::vector<int> sylStart (nuclei.size()), codaEnd (nuclei.size());
            sylStart[0] = 0;
            for (size_t s = 0; s + 1 < nuclei.size(); ++s)
            {
                const int a = nuclei[s] + 1, b = nuclei[s + 1];
                int onsetUnits = 0;
                for (int m = juce::jmin (3, b - a); m >= 1; --m)
                    if (legalOnset (phonesOf (b - m, b)) && (int) phonesOf (b - m, b).size() <= 3) { onsetUnits = m; break; }
                codaEnd[s] = b - onsetUnits;
                sylStart[s + 1] = b - onsetUnits;
            }
            codaEnd.back() = (int) units.size();

            const int n = (int) nuclei.size();
            for (int s = 0; s < n; ++s)
            {
                SylText st;
                auto on = phonesOf (sylStart[(size_t) s], nuclei[(size_t) s]);
                auto co = phonesOf (nuclei[(size_t) s] + 1, codaEnd[(size_t) s]);
                while (on.size() > 3) on.erase (on.begin());
                while (co.size() > 3) co.pop_back();
                // Auslautverhärtung
                for (auto& p : co)
                    p = p == PB ? PP : p == PD ? PT : p == PG ? PK : p == PZ ? PS : p == PW ? PF : p;
                int nucleus = units[(size_t) nuclei[(size_t) s]].nucleus;
                const bool last = s == n - 1;
                // "-ig" am Wortende klingt wie "ich"
                if (last && nucleus == I && co.size() == 1 && co[0] == PK && w.endsWith ("ig")) co[0] = PCh;
                // unbetontes e: Endsilben und Vorsilben be-/ge-
                if (nucleus == E && n > 1)
                {
                    if (last && (co.empty() || (co.size() == 1 && (co[0] == PN || co[0] == PL || co[0] == PS || co[0] == PT))))
                        nucleus = Schwa;
                    else if (last && co.size() == 1 && co[0] == PR)
                    {
                        nucleus = Er;
                        co.clear();
                    }
                    else if (s == 0 && (w.startsWith ("be") || w.startsWith ("ge")) && on.size() == 1 && co.empty())
                        nucleus = Schwa;
                }
                for (auto p : on) st.syl.on[st.syl.nOn++] = (juce::uint8) p;
                for (auto p : co) st.syl.co[st.syl.nCo++] = (juce::uint8) p;
                st.syl.nucleus = (juce::uint8) nucleus;

                const int from = s == 0 ? 0 : units[(size_t) sylStart[(size_t) s]].from;
                const int to = last ? len : units[(size_t) sylStart[(size_t) s + 1]].from;
                st.text = original.substring (from, to);
                word.push_back (st);
            }
            if (w == "boing" && ! word.empty()) word[0].syl.spring = true;
            return word;
        }
    }

    // Text in Wörter aus Silben zerlegen. Satzzeichen und Leerzeichen trennen Wörter, Ziffern werden zu Zahlwörtern.
    inline std::vector<Word> parseText (const juce::String& text)
    {
        std::vector<Word> words;
        juce::String current;
        auto flush = [&]
        {
            if (current.isEmpty()) return;
            if (current.containsOnly ("0123456789"))
            {
                for (auto& part : juce::StringArray::fromTokens (detail::numberWords (current), " ", ""))
                    if (auto wd = detail::parseWord (part); ! wd.empty()) words.push_back (wd);
            }
            else if (auto wd = detail::parseWord (current); ! wd.empty())
                words.push_back (wd);
            current.clear();
        };
        for (auto c : text)
        {
            const bool letter = juce::CharacterFunctions::isLetter (c) || c == 0xdf;
            const bool digit = juce::CharacterFunctions::isDigit (c);
            if (letter || digit)
            {
                if (! current.isEmpty() && digit != current.containsOnly ("0123456789"))
                    flush();
                current << juce::String::charToString (c);
            }
            else
                flush();
        }
        flush();
        return words;
    }

    // Lautschrift einer Silbe (für Tests und Fehlersuche)
    inline juce::String describe (const Syl& s)
    {
        static const char* ph[] { "m", "n", "ng", "l", "r", "w", "j", "s", "z", "sch", "ch", "x", "f", "h", "t", "k", "p", "d", "g", "b" };
        static const char* nu[] { "a", "e", "i", "o", "u", "ä", "ö", "ü", "ai", "au", "oi", "@", "er", "mm" };
        juce::String t;
        for (int i = 0; i < s.nOn; ++i) t << ph[s.on[(size_t) i]] << ".";
        t << "[" << juce::String::fromUTF8 (nu[s.nucleus]) << "]";
        for (int i = 0; i < s.nCo; ++i) t << "." << ph[s.co[(size_t) i]];
        return t;
    }
}
