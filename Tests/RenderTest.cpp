// Selbsttest: spielt in jedem Oszillator-Modus einen Akkord und prüft Pegel, NaN und Ausklingen.
// Danach Mono/Legato, Mod-Matrix, Arpeggiator und Step-Sequenzer.
#include "../Source/PluginProcessor.h"

namespace
{
    void set (MangamanProcessor& p, const juce::String& id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    struct Result { float peak = 0, tail = 0; bool finite = true; int onsets = 0; };

    Result render (MangamanProcessor& proc, std::initializer_list<int> chord, int holdBlocks = 100, int totalBlocks = 500)
    {
        const int block = 512;
        juce::AudioBuffer<float> buffer (2, block);
        Result r;
        bool wasSilent = true;

        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)          for (int n : chord) midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.9f), 0);
            if (b == holdBlocks) for (int n : chord) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            proc.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    if (! std::isfinite (buffer.getSample (ch, i))) r.finite = false;

            const auto m = buffer.getMagnitude (0, block);
            if (b < holdBlocks) r.peak = std::max (r.peak, m);
            if (b > totalBlocks - 50) r.tail = std::max (r.tail, m);
            if (wasSilent && m > 0.01f) ++r.onsets;
            wasSilent = m < 0.002f;
        }
        return r;
    }

    bool check (const char* name, const Result& r, float minPeak = 0.02f)
    {
        const bool ok = r.finite && r.peak > minPeak && r.peak < 1.0f && r.tail < 1.0e-3f;
        std::printf ("%-24s Peak %.3f  Ausklang %.6f  %s\n", name, r.peak, r.tail, ok ? "OK" : "FEHLER");
        return ok;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    MangamanProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    bool ok = true;

    for (int t = 0; t < Params::oscTypes.size(); ++t)
    {
        set (proc, "osc_type", (float) t);
        for (float w : { 0.0f, 0.5f, 1.0f })
        {
            set (proc, "osc_wave", w);
            set (proc, "osc_timbre", w);
            set (proc, "osc_shape", 1.0f - w);
            const auto name = Params::oscTypes[t] + " " + juce::String (w, 1);
            ok &= check (name.toRawUTF8(), render (proc, { 48, 60, 64, 67 }));
        }
    }

    set (proc, "osc_type", 0.0f);
    for (int f = 0; f < 3; ++f)
    {
        set (proc, "flt_type", (float) f);
        set (proc, "flt_res", 0.95f);
        ok &= check (("Filter " + Params::filterTypes[f]).toRawUTF8(), render (proc, { 45, 57 }), 0.005f);
    }
    set (proc, "flt_type", 0.0f);
    set (proc, "flt_res", 0.2f);

    // Volle Mod-Matrix mit allen Quellen auf alle Ziele
    for (int s = 0; s < Params::numSources; ++s)
        for (int d = 0; d < Params::numDests; ++d)
            set (proc, Params::modId (s, d), (s + d) % 2 == 0 ? 0.5f : -0.5f);
    set (proc, "lfo_shape", 5.0f);
    set (proc, "lfo_rate", 7.0f);
    ok &= check ("Mod-Matrix voll", render (proc, { 50, 62, 69 }));
    for (int s = 0; s < Params::numSources; ++s)
        for (int d = 0; d < Params::numDests; ++d)
            set (proc, Params::modId (s, d), 0.0f);

    set (proc, "voice_mode", 1.0f);
    set (proc, "glide", 0.4f);
    ok &= check ("Mono + Glide", render (proc, { 60, 67 }));
    set (proc, "voice_mode", 2.0f);
    ok &= check ("Legato", render (proc, { 60, 64 }));
    set (proc, "voice_mode", 0.0f);
    set (proc, "glide", 0.0f);

    // Arpeggiator: kurze Hüllkurve, damit einzelne Noten als Einsätze erkennbar sind
    set (proc, "env_decay", 0.05f);
    set (proc, "env_sustain", 0.0f);
    set (proc, "arp_on", 1.0f);
    set (proc, "arp_oct", 2.0f);
    const auto arp = render (proc, { 60, 64, 67 }, 300, 400);
    const bool arpOk = check ("Arpeggiator", arp) && arp.onsets >= 20;
    std::printf ("Arp-Noteneinsaetze: %d (erwartet ca. 24 bei 120 BPM, 1/16)\n", arp.onsets);
    ok &= arpOk;

    set (proc, "arp_on", 0.0f);
    set (proc, "arp_oct", 1.0f);

    // Sequenzer: 16 Schritte bei 120 BPM in 1/16 = 2 Sekunden, Muster mit 12 aktiven Schritten
    set (proc, "sq_run", 1.0f);
    auto running = [] (const char* name, const auto& r)
    {
        const bool good = r.finite && r.peak > 0.005f && r.peak < 1.0f;
        std::printf ("%-24s Peak %.3f  (laeuft)  %s\n", name, r.peak, good ? "OK" : "FEHLER");
        return good;
    };
    const auto seq = render (proc, {}, 188, 188);     // 16 Schritte bei 120 BPM = 2 Sekunden
    std::printf ("Sequenzer-Einsaetze: %d (erwartet 11 bis 13)\n", seq.onsets);
    ok &= running ("Sequenzer", seq) && seq.onsets >= 10 && seq.onsets <= 14;

    // Transponieren: eine Taste verschiebt die Folge, die Taste selbst klingt nicht
    {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
        proc.processBlock (buffer, midi);
        bool finite = true;
        for (int b = 0; b < 100; ++b)
        {
            juce::MidiBuffer none;
            proc.processBlock (buffer, none);
            for (int i = 0; i < 512; ++i) finite &= std::isfinite (buffer.getSample (0, i));
        }
        std::printf ("%-24s %s\n", "Seq transponiert", finite ? "OK" : "FEHLER");
        ok &= finite;
    }

    // Reihe A moduliert Cutoff ueber die Matrix
    set (proc, Params::modId (Params::firstSeqSource, Params::DCutoff), 0.8f);
    set (proc, Params::modId (Params::firstSeqSource + 2, Params::DPitch), 0.3f);
    ok &= running ("Seq-Reihen in Matrix", render (proc, {}, 150, 150));
    set (proc, "sq_run", 0.0f);
    const auto stopped = render (proc, {}, 0, 100);
    std::printf ("%-24s Ausklang %.6f  %s\n", "Seq gestoppt", stopped.tail, stopped.tail < 1.0e-3f ? "OK" : "FEHLER");
    ok &= stopped.tail < 1.0e-3f;
    set (proc, Params::modId (Params::firstSeqSource, Params::DCutoff), 0.0f);
    set (proc, Params::modId (Params::firstSeqSource + 2, Params::DPitch), 0.0f);

    // Optional: Oberflaeche je Seite und Design als PNG speichern (MANGAMAN_SNAPSHOT = Ordner)
    if (auto* dir = std::getenv ("MANGAMAN_SNAPSHOT"))
    {
        const char* tabNames[] { "Klang", "Formen", "Modulation", "Sequenzer" };
        for (int m = 0; m < 2; ++m)
        {
            proc.design.store (m);
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            for (auto* tabName : tabNames)
            {
                std::function<juce::Button* (juce::Component&)> find = [&] (juce::Component& c) -> juce::Button*
                {
                    for (auto* child : c.getChildren())
                    {
                        if (auto* b = dynamic_cast<juce::Button*> (child); b != nullptr && b->getName() == tabName)
                            return b;
                        if (auto* f = find (*child))
                            return f;
                    }
                    return nullptr;
                };
                if (auto* b = find (*editor); b != nullptr && b->onClick)
                    b->onClick();
                auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
                juce::File file (juce::File (dir).getChildFile (juce::String ("mangaman-") + (m == 0 ? "1980" : "2100") + "-" + tabName + ".png"));
                file.deleteFile();
                juce::FileOutputStream stream (file);
                juce::PNGImageFormat().writeImageToStream (image, stream);
            }
        }
        proc.design.store (1);
    }

    std::printf (ok ? "\nALLE TESTS OK\n" : "\nTESTS FEHLGESCHLAGEN\n");
    return ok ? 0 : 1;
}
