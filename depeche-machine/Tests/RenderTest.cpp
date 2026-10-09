// Selbsttest: prüft den Song-Generator (gleicher Seed = gleicher Song, Länge, Intensität, Nähe zum Original,
// Töne in der Skala, Neuwürfeln einzelner Abschnitte, Gesangslinie mit Silben und Refrain-Haken, Epochen),
// die Klangerzeugung (jede Spur klingt, Gesang schaltbar, kein NaN, nicht übersteuert),
// Host-Sync, MIDI-Transponieren, MIDI-Export und das Speichern im Projekt.
#include "../Source/PluginProcessor.h"

namespace
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;

    void set (DepecheMachineProcessor& p, const juce::String& id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    void defaults (DepecheMachineProcessor& p)
    {
        for (auto* prm : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                r->setValueNotifyingHost (r->getDefaultValue());
    }

    bool report (const char* name, bool ok, const juce::String& detail)
    {
        std::printf ("%-30s %-58s %s\n", name, detail.toRawUTF8(), ok ? "OK" : "FEHLER");
        return ok;
    }

    // Spielhead für den Host-Test
    struct FakeHead : juce::AudioPlayHead
    {
        double ppq = 0.0, bpm = 120.0;
        bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            return info;
        }
    };

    struct Take { std::vector<float> l, r; bool finite = true; float peak = 0.0f; double rms = 0.0; };

    Take render (DepecheMachineProcessor& p, double seconds, FakeHead* head = nullptr)
    {
        Take t;
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        const int total = (int) (seconds * sr);
        double sum = 0.0;
        for (int start = 0; start < total; start += block)
        {
            p.processBlock (buffer, midi);
            if (head != nullptr) head->ppq += block * head->bpm / 60.0 / sr;
            for (int i = 0; i < block; ++i)
            {
                const float a = buffer.getSample (0, i), b = buffer.getSample (1, i);
                if (! std::isfinite (a) || ! std::isfinite (b)) t.finite = false;
                t.peak = std::max ({ t.peak, std::abs (a), std::abs (b) });
                sum += (double) a * a + (double) b * b;
                t.l.push_back (a);
                t.r.push_back (b);
            }
        }
        t.rms = std::sqrt (sum / juce::jmax (1.0, 2.0 * total));
        return t;
    }

    Depeche::Settings baseSettings()
    {
        Depeche::Settings s;
        s.seed = 4242; s.noteSeed = 777;
        s.lengthMinutes = 4.0f; s.intensity = 0.6f; s.closeness = 0.8f; s.variety = 0.5f;
        s.era = Depeche::Synthpop81; s.key = 9; s.tempo = 124.0f;
        return s;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    bool ok = true;

    // 1. Gleicher Seed = gleicher Song, anderer Seed = anderer Song
    {
        const auto a = Depeche::generate (baseSettings()), b = Depeche::generate (baseSettings());
        auto s2 = baseSettings();
        s2.seed = 4243; s2.noteSeed = 778;
        const auto c = Depeche::generate (s2);
        ok &= report ("Seed wiederholbar", a.fingerprint() == b.fingerprint() && a.fingerprint() != c.fingerprint() && a.noteCount() > 500,
                      juce::String::formatted ("%d Noten, anderer Seed anders", (int) a.noteCount()));
    }

    // 2. Länge: 1, 4 und 10 Minuten treffen die Zielzeit, Abschnitte lückenlos
    {
        bool good = true;
        juce::String detail;
        for (float minutes : { 1.0f, 4.0f, 10.0f })
            for (int era = 0; era < Depeche::numEras; ++era)
            {
                auto s = baseSettings();
                s.lengthMinutes = minutes; s.era = era;
                const auto song = Depeche::generate (s);
                const double seconds = song.totalBeats() * 60.0 / song.tempo;
                int bar = 0;
                for (auto& sec : song.sections) { good &= sec.startBar == bar && sec.bars > 0; bar += sec.bars; }
                good &= bar == song.totalBars && std::abs (seconds - minutes * 60.0) < 4.0 * 60.0 / song.tempo * 0.5 + 0.01;
                for (auto& lane : song.notes)
                    for (auto& n : lane) good &= n.start >= 0.0 && n.start < song.totalBeats() && n.length > 0.0;
                if (era == 0) detail << juce::String (minutes, 0) << " min = " << juce::String (seconds, 1) << " s  ";
            }
        ok &= report ("Laenge", good, detail);
    }

    // 3. Intensität: mehr Rhythmus, Riff und Spuren bei hoher Intensität
    {
        auto lo = baseSettings(), hi = baseSettings();
        lo.intensity = 0.1f; hi.intensity = 1.0f;
        const auto a = Depeche::generate (lo), b = Depeche::generate (hi);
        auto active = [] (const Depeche::Song& s) { int n = 0; for (auto& sec : s.sections) for (bool l : sec.lanes) n += l ? 1 : 0; return n; };
        ok &= report ("Intensitaet", b.notes[Depeche::Drums].size() * 2 > 3 * a.notes[Depeche::Drums].size() + 100
                                       && b.notes[Depeche::Seq].size() > a.notes[Depeche::Seq].size() && active (b) > active (a),
                      juce::String::formatted ("Rhythmus %d -> %d, Riff %d -> %d", (int) a.notes[Depeche::Drums].size(), (int) b.notes[Depeche::Drums].size(),
                                               (int) a.notes[Depeche::Seq].size(), (int) b.notes[Depeche::Seq].size()));
    }

    // 4. Nähe zum Original: Stiltreue steigt mit dem Regler (Mittel über 20 Seeds)
    {
        float lo = 0.0f, hi = 0.0f;
        int minorHi = 0;
        for (juce::uint32 i = 0; i < 20; ++i)
        {
            auto s = baseSettings();
            s.seed = 1000 + i; s.noteSeed = 2000 + i;
            s.closeness = 0.0f; lo += Depeche::generate (s).typicality / 20.0f;
            s.closeness = 1.0f;
            s.era = (int) (i % Depeche::numEras);
            const auto song = Depeche::generate (s);
            hi += song.typicality / 20.0f;
            minorHi += Depeche::scaleTypical (s.era, song.scaleIndex) ? 1 : 0;
        }
        ok &= report ("Naehe zum Original", hi > 0.9f && lo < 0.25f && minorHi == 20,
                      juce::String::formatted ("Stiltreue %.0f %% -> %.0f %%, Epochen-Skalen %d/20", lo * 100.0f, hi * 100.0f, minorHi));
    }

    // 5. Alle Töne liegen in Tonart und Skala (Rückungen eingerechnet)
    {
        int wrong = 0, total = 0;
        for (juce::uint32 i = 0; i < 10; ++i)
        {
            auto s = baseSettings();
            s.seed = 300 + i; s.noteSeed = 400 + i; s.key = (int) i; s.closeness = 0.1f * (float) i; s.variety = 1.0f;
            s.era = (int) (i % Depeche::numEras);
            const auto song = Depeche::generate (s);
            for (int lane : { (int) Depeche::Pad, (int) Depeche::Seq, (int) Depeche::Bass, (int) Depeche::Lead, (int) Depeche::Vocal })
                for (auto& n : song.notes[(size_t) lane])
                {
                    const int pc = Depeche::detail::mod (n.pitch - song.pitchClassOffset (n.start + 0.001), 12);
                    ++total;
                    if (std::find (song.scale.begin(), song.scale.end(), pc) == song.scale.end()) ++wrong;
                }
        }
        ok &= report ("Toene in der Skala", wrong == 0 && total > 1000, juce::String::formatted ("%d Toene, %d falsch", total, wrong));
    }

    // 6. Abschnitt neu würfeln ändert nur diesen Abschnitt; Intensität von Hand wirkt
    {
        auto s = baseSettings();
        const auto a = Depeche::generate (s);
        s.sectionSalt = { 0, 0, 1 };
        const auto b = Depeche::generate (s);
        const auto& sec = b.sections[2];
        bool outsideSame = true, insideDiffers = false;
        for (int lane = 0; lane < Depeche::numLanes; ++lane)
        {
            std::vector<Depeche::Note> ao, bo, ai, bi;
            for (auto& n : a.notes[(size_t) lane]) (n.start >= sec.startBeat() && n.start < sec.endBeat() ? ai : ao).push_back (n);
            for (auto& n : b.notes[(size_t) lane]) (n.start >= sec.startBeat() && n.start < sec.endBeat() ? bi : bo).push_back (n);
            auto same = [] (const std::vector<Depeche::Note>& x, const std::vector<Depeche::Note>& y)
            {
                if (x.size() != y.size()) return false;
                for (size_t k = 0; k < x.size(); ++k)
                    if (x[k].start != y[k].start || x[k].pitch != y[k].pitch) return false;
                return true;
            };
            // Flächen hängen über die Stimmführung am Vorgänger: nach dem Abschnitt dürfen sie sich ändern
            if (lane != Depeche::Pad && lane != Depeche::Fx) outsideSame &= same (ao, bo);
            insideDiffers |= ! same (ai, bi);
        }
        auto o = baseSettings();
        o.intensityOverride = { -1.0f, -1.0f, 0.05f };
        const auto c = Depeche::generate (o);
        ok &= report ("Abschnitt neu wuerfeln", outsideSame && insideDiffers && b.sections[2].rerolled
                                                  && c.sections[2].overridden && std::abs (c.sections[2].intensity - 0.05f) < 1.0e-4f,
                      "nur Abschnitt 3 neu, Intensitaet von Hand");
    }

    // 7. Gesang: Strophen und Refrains singen, Silben gesetzt, Refrain höher als Strophe, Haken-Wort am Refrain-Anfang,
    //    Zweitstimme im vollen Refrain, Summen nur mit geschlossenem Mund
    {
        auto s = baseSettings();
        s.intensity = 0.9f;
        const auto song = Depeche::generate (s);
        double verseSum = 0.0, chorusSum = 0.0;
        int verseN = 0, chorusN = 0, harmonies = 0, badSyllables = 0, hookStarts = 0, choruses = 0;
        juce::String hook;
        for (auto& sec : song.sections)
        {
            bool first = true;
            for (auto& n : song.notes[Depeche::Vocal])
            {
                if (n.start < sec.startBeat() || n.start >= sec.endBeat()) continue;
                if (n.syllable < 0) ++badSyllables;
                if (n.harmony) { ++harmonies; continue; }
                if (sec.type == Depeche::Verse)  { verseSum += n.pitch; ++verseN; }
                if (sec.type == Depeche::Chorus)
                {
                    chorusSum += n.pitch; ++chorusN;
                    if (first) { ++choruses; hookStarts += song.hookWord.startsWith (Depeche::Phon::text (n.syllable)) ? 1 : 0; }
                }
                first = false;
            }
        }
        auto h = s;
        h.textStyle = Depeche::Humming;
        const auto hum = Depeche::generate (h);
        bool closed = ! hum.notes[Depeche::Vocal].empty();
        for (auto& n : hum.notes[Depeche::Vocal])
        {
            const int nu = Depeche::Phon::nucleus (n.syllable);
            closed &= nu == Depeche::Phon::Hum || nu == Depeche::Phon::U || nu == Depeche::Phon::O;
        }
        const double v = verseN > 0 ? verseSum / verseN : 0.0, c = chorusN > 0 ? chorusSum / chorusN : 0.0;
        ok &= report ("Gesangslinie", verseN > 20 && chorusN > 20 && c > v + 1.0 && badSyllables == 0 && harmonies > 10
                                        && choruses > 0 && hookStarts == choruses && closed,
                      juce::String::formatted ("Strophe %.1f, Refrain %.1f (MIDI), %d Zweitstimme, Haken %d/%d, Summen ok",
                                               v, c, harmonies, hookStarts, choruses));
    }

    // 8. Epochen: eigene Tempi und eigene Muster; Dunkel 1993 langsamer als Synthpop 1981
    {
        float tSum[Depeche::numEras] {};
        bool different = true;
        juce::uint64 prints[Depeche::numEras] {};
        for (int e = 0; e < Depeche::numEras; ++e)
        {
            for (juce::uint32 i = 0; i < 20; ++i) tSum[e] += (float) Depeche::suggestTempo (900 + i, e, 1.0f) / 20.0f;
            auto s = baseSettings();
            s.era = e;
            prints[e] = Depeche::generate (s).fingerprint();
            for (int k = 0; k < e; ++k) different &= prints[k] != prints[e];
        }
        ok &= report ("Epochen", different && tSum[Depeche::Dark93] + 20.0f < tSum[Depeche::Synthpop81],
                      juce::String::formatted ("Tempo 81: %.0f, 84: %.0f, 87: %.0f, 90: %.0f, 93: %.0f BPM", tSum[0], tSum[1], tSum[2], tSum[3], tSum[4]));
    }

    DepecheMachineProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    defaults (proc);
    proc.regenerate();
    proc.prepareToPlay (sr, block);

    // 9. Ganzer Mix klingt, ist endlich und übersteuert nicht (Refrain anspielen)
    {
        auto song = proc.getSong();
        double themeBeat = 0.0;
        for (auto& sec : song->sections) if (sec.type == Depeche::Chorus) { themeBeat = sec.startBeat(); break; }
        proc.seek (themeBeat);
        proc.setRunning (true);
        auto t = render (proc, 12.0);
        ok &= report ("Mix klingt", t.finite && t.rms > 0.03 && t.peak < 1.0f,
                      juce::String::formatted ("RMS %.3f, Spitze %.3f", t.rms, t.peak));
    }

    // 10. Jede Spur klingt solo
    {
        juce::String detail;
        bool good = true;
        auto song = proc.getSong();
        for (int lane = 0; lane < Depeche::numLanes; ++lane)
        {
            for (int k = 0; k < Depeche::numLanes; ++k)
                set (proc, juce::String ("solo_") + Params::laneIds[k], k == lane ? 1.0f : 0.0f);
            // einen Abschnitt suchen, in dem die Spur spielt
            double at = 0.0;
            for (auto& sec : song->sections) if (sec.lanes[(size_t) lane] && ! song->notes[(size_t) lane].empty()) { at = sec.startBeat(); if (sec.type != Depeche::Intro || lane == Depeche::Fx) break; }
            proc.prepareToPlay (sr, block);
            proc.seek (at);
            proc.setRunning (true);
            auto t = render (proc, 6.0);
            good &= t.finite && t.rms > 0.004;
            detail << Depeche::laneName (lane).substring (0, 3) << " " << juce::String (t.rms, 3) << "  ";
        }
        for (int k = 0; k < Depeche::numLanes; ++k)
            set (proc, juce::String ("solo_") + Params::laneIds[k], 0.0f);
        ok &= report ("Spuren solo", good, detail);
    }

    // Gesang schaltbar: Gesang solo im Refrain klingt, mit ausgeschaltetem Gesang ist es still
    {
        auto song = proc.getSong();
        double at = 0.0;
        for (auto& sec : song->sections) if (sec.type == Depeche::Chorus) { at = sec.startBeat(); break; }
        set (proc, "solo_vocal", 1.0f);
        proc.prepareToPlay (sr, block);
        proc.seek (at);
        proc.setRunning (true);
        auto on = render (proc, 6.0);
        set (proc, "vocal_on", 0.0f);
        proc.prepareToPlay (sr, block);
        proc.seek (at);
        render (proc, 1.0);
        auto off = render (proc, 4.0);
        set (proc, "vocal_on", 1.0f);
        set (proc, "solo_vocal", 0.0f);
        ok &= report ("Gesang schaltbar", on.finite && on.rms > 0.01 && off.rms < 0.002,
                      juce::String::formatted ("an RMS %.3f, aus RMS %.4f", on.rms, off.rms));
    }

    // 11. Host-Sync: Position folgt dem Host, Tempo vom Host
    {
        FakeHead head;
        head.ppq = 32.0; head.bpm = 126.0;
        proc.setPlayHead (&head);
        proc.setRunning (false);
        render (proc, 1.0, &head);
        const double expected = 32.0 + (1.0 * sr / block) * block * 126.0 / 60.0 / sr - block * 126.0 / 60.0 / sr;
        const bool good = proc.shownHost.load() && std::abs (proc.shownBeat.load() - expected) < 0.05 && std::abs (proc.shownBpm.load() - 126.0f) < 0.01f;
        ok &= report ("Host-Sync", good, juce::String::formatted ("Beat %.2f (erwartet %.2f), %.0f BPM", proc.shownBeat.load(), expected, proc.shownBpm.load()));
        head.playing = false;
        render (proc, 0.1, &head);
        proc.setPlayHead (nullptr);
    }

    // 12. MIDI-Taste transponiert den Song (D3 = +2), Stummschalten wirkt
    {
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 0);
        proc.processBlock (buffer, midi);
        const int tr = proc.transpose.load();
        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::noteOff (1, 62), 0);
        proc.processBlock (buffer, off);
        const bool latched = proc.transpose.load() == 2;
        for (auto* id : Params::laneIds) set (proc, juce::String ("mute_") + id, 1.0f);
        proc.seek (64.0);
        proc.setRunning (true);
        render (proc, 2.0);   // Ausklingen
        auto t = render (proc, 2.0);
        for (auto* id : Params::laneIds) set (proc, juce::String ("mute_") + id, 0.0f);
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        proc.processBlock (buffer, midi);
        ok &= report ("MIDI transponiert, stumm", tr == 2 && latched && t.rms < 0.002 && proc.transpose.load() == 0,
                      juce::String::formatted ("D3 = %+d, bleibt stehen, stumm RMS %.4f", tr, t.rms));
    }

    // 13. MIDI-Export
    {
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("depeche-test.mid");
        const bool written = proc.exportMidi (file);
        juce::MidiFile mf;
        juce::FileInputStream in (file);
        const bool read = in.openedOk() && mf.readFrom (in);
        int notes = 0, lyrics = 0;
        for (int t = 0; t < mf.getNumTracks(); ++t)
            for (auto* e : *mf.getTrack (t))
            {
                if (e->message.isNoteOn()) ++notes;
                if (e->message.isTextMetaEvent() && e->message.getMetaEventType() == 5) ++lyrics;
            }
        const int expected = (int) proc.getSong()->noteCount();
        ok &= report ("MIDI-Export", written && read && mf.getNumTracks() == 8 && notes == expected && lyrics > 20,
                      juce::String::formatted ("%d Spuren, %d Noten (erwartet %d), %d Silben", mf.getNumTracks(), notes, expected, lyrics));
        file.deleteFile();
    }

    // 14. Speichern und Laden: Seed, Regler, Eingriffe in der Zeitleiste kommen wieder
    {
        set (proc, "length", 7.5f);
        set (proc, "closeness", 0.3f);
        set (proc, "era", 2.0f);
        proc.rollNewTrack();
        proc.rerollSection (3);
        proc.setSectionIntensity (1, 0.9f);
        proc.design.store (0);
        const auto before = proc.getSong()->fingerprint();
        juce::MemoryBlock state;
        proc.getStateInformation (state);

        DepecheMachineProcessor other;
        other.setStateInformation (state.getData(), (int) state.getSize());
        const auto after = other.getSong()->fingerprint();
        ok &= report ("Speichern", before == after && other.design.load() == 0 && other.getSong()->sections[3].rerolled
                                     && other.getSong()->sections[1].overridden,
                      "Song, Design und Zeitleiste wiederhergestellt");
        proc.design.store (1);
    }

    // 15. Zufällige Reglerstellungen: nie NaN, nie über 0 dBFS
    {
        juce::Random rng (11);
        bool good = true;
        float worst = 0.0f;
        for (int round = 0; round < 8; ++round)
        {
            for (auto* prm : proc.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                    if (! r->getParameterID().startsWith ("mute_") && ! r->getParameterID().startsWith ("solo_"))
                        r->setValueNotifyingHost (rng.nextFloat());
            set (proc, "length", 1.0f + rng.nextFloat() * 3.0f);
            proc.rollNewTrack();
            proc.prepareToPlay (sr, block);
            proc.seek (rng.nextFloat() * (float) proc.getSong()->totalBeats() * 0.8f);
            proc.setRunning (true);
            auto t = render (proc, 3.0);
            good &= t.finite && t.peak <= 1.0f;
            worst = juce::jmax (worst, t.peak);
        }
        ok &= report ("Zufallsregler", good, juce::String::formatted ("8 Runden, groesste Spitze %.3f", worst));
    }

    // Optional: Abschnitte und Spuren vieler Songs auflisten (DEPECHE_MACHINE_DUMP = Anzahl Songs)
    if (auto* dump = std::getenv ("DEPECHE_MACHINE_DUMP"))
    {
        const char* names[] { "Intro", "Strophe", "Vorrefrain", "Refrain", "Zwischenspiel", "Bridge", "Break", "Outro" };
        int chorus = 0, chorusNoDrums = 0;
        for (int n = 0; n < juce::String (dump).getIntValue(); ++n)
        {
            auto s = baseSettings();
            s.seed = 1000 + (juce::uint32) n * 7919; s.era = n % Depeche::numEras;
            const auto song = Depeche::generate (s);
            for (const auto& sec : song.sections)
            {
                int drums = 0;
                for (const auto& note : song.notes[Depeche::Drums])
                    if (note.start >= sec.startBeat() && note.start < sec.endBeat()) ++drums;
                if (sec.type == Depeche::Chorus) { ++chorus; if (drums == 0) ++chorusNoDrums; }
                if (n < 2)
                    std::printf ("%-14s I=%.2f lanes=%d%d%d%d%d%d%d drums=%d\n", names[sec.type], sec.intensity,
                                 sec.lanes[0], sec.lanes[1], sec.lanes[2], sec.lanes[3], sec.lanes[4], sec.lanes[5], sec.lanes[6], drums);
            }
        }
        std::printf ("Refrains ohne Rhythmus: %d von %d\n", chorusNoDrums, chorus);
    }

    // Optional: Oberfläche je Seite und Design als PNG speichern (DEPECHE_MACHINE_SNAPSHOT = Ordner)
    if (auto* dir = std::getenv ("DEPECHE_MACHINE_SNAPSHOT"))
    {
        auto prepare = [&] (std::function<void()> tweak)
        {
            defaults (proc);
            proc.setRunning (false);
            if (tweak) tweak();
            proc.regenerate();
            proc.prepareToPlay (sr, block);
            auto song = proc.getSong();
            // mitten im ersten Thema, damit Anzeige, Pegel und Laserharfe leben
            double at = song->totalBeats() * 0.3;
            for (auto& sec : song->sections) if (sec.type == Depeche::Chorus) { at = sec.startBeat() + 2.0; break; }
            proc.seek (at);
            proc.setRunning (true);
            render (proc, 2.0);
        };
        auto shoot = [&] (const juce::String& name, int design, const char* tab)
        {
            proc.design.store (design);
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            std::function<juce::Button* (juce::Component&)> find = [&] (juce::Component& c) -> juce::Button*
            {
                for (auto* child : c.getChildren())
                {
                    if (auto* b = dynamic_cast<juce::Button*> (child); b != nullptr && b->getName() == tab)
                        return b;
                    if (auto* f = find (*child))
                        return f;
                }
                return nullptr;
            };
            if (auto* b = find (*editor); b != nullptr && b->onClick)
                b->onClick();
            // Anzeigen auffrischen (Timer laufen im Test nicht)
            for (int i = 0; i < 3; ++i)
            {
                render (proc, 0.05);
                if (auto* b = find (*editor); b != nullptr && b->onClick) b->onClick();
            }
            auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            juce::File file (juce::File (dir).getChildFile ("depeche-machine-" + juce::String (design == 0 ? "1980" : "2100") + "-" + name + ".png"));
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (image, stream);
        };

        prepare ({});
        for (int m = 0; m < 2; ++m)
            for (auto* tab : { "Song", "Zeitleiste", "Klang", "Gesang", "Mischpult" })
                shoot (tab, m, tab);

        // Zusatzbilder: kurzer, wilder Song (wenig Nähe, Industrial) und langer ruhiger Song (Dunkel 1993)
        prepare ([&] { set (proc, "closeness", 0.1f); set (proc, "era", 1.0f); set (proc, "intensity", 0.9f); set (proc, "length", 2.5f); });
        for (int m = 0; m < 2; ++m) shoot ("Zeitleiste-frei", m, "Zeitleiste");
        prepare ([&] { set (proc, "closeness", 1.0f); set (proc, "era", 4.0f); set (proc, "tempo", 86.0f); set (proc, "intensity", 0.4f); set (proc, "length", 7.0f); });
        for (int m = 0; m < 2; ++m) shoot ("Zeitleiste-ruhig", m, "Zeitleiste");
        for (int m = 0; m < 2; ++m) shoot ("Gesang-ruhig", m, "Gesang");
        proc.design.store (1);
        std::printf ("Bilder gespeichert in %s\n", dir);
    }

    // Optional: ganzen Song als WAV rendern (DEPECHE_MACHINE_WAV = Datei, DEPECHE_MACHINE_WAV_ERA = 0..4,
    // DEPECHE_MACHINE_WAV_SEED = Zahl für einen anderen Song)
    if (auto* wav = std::getenv ("DEPECHE_MACHINE_WAV"))
    {
        defaults (proc);
        set (proc, "length", 3.0f);
        const auto seed = (juce::uint32) (std::getenv ("DEPECHE_MACHINE_WAV_SEED") != nullptr
                                             ? juce::String (std::getenv ("DEPECHE_MACHINE_WAV_SEED")).getLargeIntValue()
                                             : 19810405 + (std::getenv ("DEPECHE_MACHINE_WAV_ERA") != nullptr ? juce::String (std::getenv ("DEPECHE_MACHINE_WAV_ERA")).getIntValue() * 11 : 0));
        proc.setSeeds (seed, seed * 7u + 3u);
        if (auto* era = std::getenv ("DEPECHE_MACHINE_WAV_ERA"))
        {
            const int e = juce::String (era).getIntValue();
            set (proc, "era", (float) e);
            proc.applyEraSound (e);
            set (proc, "tempo", (float) Depeche::suggestTempo (19810405u + (juce::uint32) e, e, 0.8f));
        }
        if (auto* solo = std::getenv ("DEPECHE_MACHINE_WAV_SOLO"))   // z. B. "vocal": nur diese Spur
            set (proc, "solo_" + juce::String (solo), 1.0f);
        set (proc, "loop", 0.0f);
        proc.regenerate();
        proc.prepareToPlay (sr, block);
        proc.seek (0.0);
        proc.setRunning (true);
        const double seconds = proc.getSong()->totalBeats() * 60.0 / proc.getSong()->tempo + 4.0;
        auto t = render (proc, seconds);
        juce::AudioBuffer<float> out (2, (int) t.l.size());
        for (int i = 0; i < out.getNumSamples(); ++i) { out.setSample (0, i, t.l[(size_t) i]); out.setSample (1, i, t.r[(size_t) i]); }
        juce::File file (wav);
        file.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> writer (juce::WavAudioFormat().createWriterFor (new juce::FileOutputStream (file), sr, 2, 16, {}, 0));
        if (writer != nullptr) writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
        std::printf ("WAV: %s (%s, %.0f s, Spitze %.2f)\n", wav, proc.getSong()->title.toRawUTF8(), seconds, t.peak);
    }

    std::printf ("\n%s\n", ok ? "ALLE TESTS BESTANDEN" : "TESTS FEHLGESCHLAGEN");
    return ok ? 0 : 1;
}
