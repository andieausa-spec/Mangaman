// Selbsttest: prüft den Track-Generator (gleicher Seed = gleicher Track, Länge, Intensität, Nähe zum Original,
// Töne in der Skala, Neuwürfeln einzelner Abschnitte), die Klangerzeugung (jede Spur klingt, kein NaN, nicht übersteuert),
// Host-Sync, MIDI-Transponieren, MIDI-Export und das Speichern im Projekt.
#include "../Source/PluginProcessor.h"

namespace
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;

    void set (JarreMachineProcessor& p, const juce::String& id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    void defaults (JarreMachineProcessor& p)
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

    Take render (JarreMachineProcessor& p, double seconds, FakeHead* head = nullptr)
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

    Jarre::Settings baseSettings()
    {
        Jarre::Settings s;
        s.seed = 4242; s.noteSeed = 777;
        s.lengthMinutes = 5.0f; s.intensity = 0.6f; s.closeness = 0.8f; s.variety = 0.5f;
        s.era = Jarre::Oxygene; s.key = 2; s.tempo = 110.0f;
        return s;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    bool ok = true;

    // 1. Gleicher Seed = gleicher Track, anderer Seed = anderer Track
    {
        const auto a = Jarre::generate (baseSettings()), b = Jarre::generate (baseSettings());
        auto s2 = baseSettings();
        s2.seed = 4243; s2.noteSeed = 778;
        const auto c = Jarre::generate (s2);
        ok &= report ("Seed wiederholbar", a.fingerprint() == b.fingerprint() && a.fingerprint() != c.fingerprint() && a.noteCount() > 500,
                      juce::String::formatted ("%d Noten, anderer Seed anders", (int) a.noteCount()));
    }

    // 2. Länge: 1, 5 und 15 Minuten treffen die Zielzeit, Abschnitte lückenlos
    {
        bool good = true;
        juce::String detail;
        for (float minutes : { 1.0f, 5.0f, 15.0f })
            for (int era = 0; era < 3; ++era)
            {
                auto s = baseSettings();
                s.lengthMinutes = minutes; s.era = era;
                const auto song = Jarre::generate (s);
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

    // 3. Intensität: mehr Rhythmus, Sequenz und Spuren bei hoher Intensität
    {
        auto lo = baseSettings(), hi = baseSettings();
        lo.intensity = 0.1f; hi.intensity = 1.0f;
        const auto a = Jarre::generate (lo), b = Jarre::generate (hi);
        auto active = [] (const Jarre::Song& s) { int n = 0; for (auto& sec : s.sections) for (bool l : sec.lanes) n += l ? 1 : 0; return n; };
        ok &= report ("Intensitaet", b.notes[Jarre::Drums].size() > 2 * a.notes[Jarre::Drums].size() + 50
                                       && b.notes[Jarre::Seq].size() > a.notes[Jarre::Seq].size() && active (b) > active (a),
                      juce::String::formatted ("Rhythmus %d -> %d, Sequenz %d -> %d", (int) a.notes[Jarre::Drums].size(), (int) b.notes[Jarre::Drums].size(),
                                               (int) a.notes[Jarre::Seq].size(), (int) b.notes[Jarre::Seq].size()));
    }

    // 4. Nähe zum Original: Stiltreue steigt mit dem Regler (Mittel über 20 Seeds)
    {
        float lo = 0.0f, hi = 0.0f;
        int minorHi = 0;
        for (juce::uint32 i = 0; i < 20; ++i)
        {
            auto s = baseSettings();
            s.seed = 1000 + i; s.noteSeed = 2000 + i;
            s.closeness = 0.0f; lo += Jarre::generate (s).typicality / 20.0f;
            s.closeness = 1.0f;
            const auto song = Jarre::generate (s);
            hi += song.typicality / 20.0f;
            minorHi += song.scaleIndex <= 2 ? 1 : 0;
        }
        ok &= report ("Naehe zum Original", hi > 0.9f && lo < 0.25f && minorHi == 20,
                      juce::String::formatted ("Stiltreue %.0f %% -> %.0f %%, Moll-Skalen %d/20", lo * 100.0f, hi * 100.0f, minorHi));
    }

    // 5. Alle Töne liegen in Tonart und Skala (Rückungen eingerechnet)
    {
        int wrong = 0, total = 0;
        for (juce::uint32 i = 0; i < 10; ++i)
        {
            auto s = baseSettings();
            s.seed = 300 + i; s.noteSeed = 400 + i; s.key = (int) i; s.closeness = 0.1f * (float) i; s.variety = 1.0f;
            const auto song = Jarre::generate (s);
            for (int lane : { (int) Jarre::Pad, (int) Jarre::Seq, (int) Jarre::Bass, (int) Jarre::Lead })
                for (auto& n : song.notes[(size_t) lane])
                {
                    const int pc = Jarre::detail::mod (n.pitch - song.pitchClassOffset (n.start + 0.001), 12);
                    ++total;
                    if (std::find (song.scale.begin(), song.scale.end(), pc) == song.scale.end()) ++wrong;
                }
        }
        ok &= report ("Toene in der Skala", wrong == 0 && total > 1000, juce::String::formatted ("%d Toene, %d falsch", total, wrong));
    }

    // 6. Abschnitt neu würfeln ändert nur diesen Abschnitt; Intensität von Hand wirkt
    {
        auto s = baseSettings();
        const auto a = Jarre::generate (s);
        s.sectionSalt = { 0, 0, 1 };
        const auto b = Jarre::generate (s);
        const auto& sec = b.sections[2];
        bool outsideSame = true, insideDiffers = false;
        for (int lane = 0; lane < Jarre::numLanes; ++lane)
        {
            std::vector<Jarre::Note> ao, bo, ai, bi;
            for (auto& n : a.notes[(size_t) lane]) (n.start >= sec.startBeat() && n.start < sec.endBeat() ? ai : ao).push_back (n);
            for (auto& n : b.notes[(size_t) lane]) (n.start >= sec.startBeat() && n.start < sec.endBeat() ? bi : bo).push_back (n);
            auto same = [] (const std::vector<Jarre::Note>& x, const std::vector<Jarre::Note>& y)
            {
                if (x.size() != y.size()) return false;
                for (size_t k = 0; k < x.size(); ++k)
                    if (x[k].start != y[k].start || x[k].pitch != y[k].pitch) return false;
                return true;
            };
            // Flächen hängen über die Stimmführung am Vorgänger: nach dem Abschnitt dürfen sie sich ändern
            if (lane != Jarre::Pad && lane != Jarre::Fx) outsideSame &= same (ao, bo);
            insideDiffers |= ! same (ai, bi);
        }
        auto o = baseSettings();
        o.intensityOverride = { -1.0f, -1.0f, 0.05f };
        const auto c = Jarre::generate (o);
        ok &= report ("Abschnitt neu wuerfeln", outsideSame && insideDiffers && b.sections[2].rerolled
                                                  && c.sections[2].overridden && std::abs (c.sections[2].intensity - 0.05f) < 1.0e-4f,
                      "nur Abschnitt 3 neu, Intensitaet von Hand");
    }

    JarreMachineProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    defaults (proc);
    proc.regenerate();
    proc.prepareToPlay (sr, block);

    // 7. Ganzer Mix klingt, ist endlich und übersteuert nicht (Thema-Abschnitt anspielen)
    {
        auto song = proc.getSong();
        double themeBeat = 0.0;
        for (auto& sec : song->sections) if (sec.type == Jarre::ThemeA) { themeBeat = sec.startBeat(); break; }
        proc.seek (themeBeat);
        proc.setRunning (true);
        auto t = render (proc, 12.0);
        ok &= report ("Mix klingt", t.finite && t.rms > 0.03 && t.peak < 1.0f,
                      juce::String::formatted ("RMS %.3f, Spitze %.3f", t.rms, t.peak));
    }

    // 8. Jede Spur klingt solo
    {
        juce::String detail;
        bool good = true;
        auto song = proc.getSong();
        for (int lane = 0; lane < Jarre::numLanes; ++lane)
        {
            for (int k = 0; k < Jarre::numLanes; ++k)
                set (proc, juce::String ("solo_") + Params::laneIds[k], k == lane ? 1.0f : 0.0f);
            // einen Abschnitt suchen, in dem die Spur spielt
            double at = 0.0;
            for (auto& sec : song->sections) if (sec.lanes[(size_t) lane] && ! song->notes[(size_t) lane].empty()) { at = sec.startBeat(); if (sec.type != Jarre::Intro || lane == Jarre::Fx) break; }
            proc.prepareToPlay (sr, block);
            proc.seek (at);
            proc.setRunning (true);
            auto t = render (proc, 6.0);
            good &= t.finite && t.rms > 0.004;
            detail << Jarre::laneName (lane).substring (0, 3) << " " << juce::String (t.rms, 3) << "  ";
        }
        for (int k = 0; k < Jarre::numLanes; ++k)
            set (proc, juce::String ("solo_") + Params::laneIds[k], 0.0f);
        ok &= report ("Spuren solo", good, detail);
    }

    // 9. Host-Sync: Position folgt dem Host, Tempo vom Host
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

    // 10. MIDI-Taste transponiert den Track (D3 = +2), Stummschalten wirkt
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

    // 11. MIDI-Export
    {
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("jarre-test.mid");
        const bool written = proc.exportMidi (file);
        juce::MidiFile mf;
        juce::FileInputStream in (file);
        const bool read = in.openedOk() && mf.readFrom (in);
        int notes = 0;
        for (int t = 0; t < mf.getNumTracks(); ++t)
            for (auto* e : *mf.getTrack (t))
                if (e->message.isNoteOn()) ++notes;
        const int expected = (int) proc.getSong()->noteCount();
        ok &= report ("MIDI-Export", written && read && mf.getNumTracks() == 7 && notes == expected,
                      juce::String::formatted ("%d Spuren, %d Noten (erwartet %d)", mf.getNumTracks(), notes, expected));
        file.deleteFile();
    }

    // 12. Speichern und Laden: Seed, Regler, Eingriffe in der Zeitleiste kommen wieder
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

        JarreMachineProcessor other;
        other.setStateInformation (state.getData(), (int) state.getSize());
        const auto after = other.getSong()->fingerprint();
        ok &= report ("Speichern", before == after && other.design.load() == 0 && other.getSong()->sections[3].rerolled
                                     && other.getSong()->sections[1].overridden,
                      "Track, Design und Zeitleiste wiederhergestellt");
        proc.design.store (1);
    }

    // 13. Zufällige Reglerstellungen: nie NaN, nie über 0 dBFS
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

    // Optional: Oberfläche je Seite und Design als PNG speichern (JARRE_MACHINE_SNAPSHOT = Ordner)
    if (auto* dir = std::getenv ("JARRE_MACHINE_SNAPSHOT"))
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
            for (auto& sec : song->sections) if (sec.type == Jarre::ThemeA) { at = sec.startBeat() + 8.0; break; }
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
            juce::File file (juce::File (dir).getChildFile ("jarre-machine-" + juce::String (design == 0 ? "1980" : "2100") + "-" + name + ".png"));
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (image, stream);
        };

        prepare ({});
        for (int m = 0; m < 2; ++m)
            for (auto* tab : { "Track", "Zeitleiste", "Klang", "Mischpult" })
                shoot (tab, m, tab);

        // Zusatzbilder: kurzer, wilder Track (wenig Nähe, Magnetfelder) und langer ruhiger Track
        prepare ([&] { set (proc, "closeness", 0.1f); set (proc, "era", 2.0f); set (proc, "intensity", 0.9f); set (proc, "length", 3.0f); });
        for (int m = 0; m < 2; ++m) shoot ("Zeitleiste-frei", m, "Zeitleiste");
        prepare ([&] { set (proc, "closeness", 1.0f); set (proc, "era", 0.0f); set (proc, "intensity", 0.35f); set (proc, "length", 12.0f); });
        for (int m = 0; m < 2; ++m) shoot ("Zeitleiste-ruhig", m, "Zeitleiste");
        proc.design.store (1);
        std::printf ("Bilder gespeichert in %s\n", dir);
    }

    // Optional: ganzen Track als WAV rendern (JARRE_MACHINE_WAV = Datei, JARRE_MACHINE_WAV_ERA = 0..2)
    if (auto* wav = std::getenv ("JARRE_MACHINE_WAV"))
    {
        defaults (proc);
        set (proc, "length", 3.0f);
        if (auto* era = std::getenv ("JARRE_MACHINE_WAV_ERA"))
        {
            set (proc, "era", (float) juce::String (era).getIntValue());
            set (proc, "tempo", (float) Jarre::suggestTempo (20251976u, juce::String (era).getIntValue(), 0.8f));
        }
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
