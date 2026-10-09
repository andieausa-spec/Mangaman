// Selbsttest: prüft Echozeit, Ping-Pong, Abklingen, Selbstoszillation, ENDLOS, THROW, Federhall,
// zufällige Reglerstellungen (kein NaN, keine Explosion) und das Speichern im Projekt.
#include "../Source/PluginProcessor.h"

namespace
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;

    void set (TrendyDelayProcessor& p, const juce::String& id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // Klare Grundeinstellung zum Messen: ohne Band-Schmutz, ohne Hall, nur Echo
    void clean (TrendyDelayProcessor& p)
    {
        for (auto* prm : p.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                r->setValueNotifyingHost (r->getDefaultValue());
        set (p, "sync", 0.0f);
        set (p, "time", 250.0f);
        set (p, "wow", 0.0f);
        set (p, "flutter", 0.0f);
        set (p, "age", 0.0f);
        set (p, "drive", 0.0f);
        set (p, "spring_mix", 0.0f);
        set (p, "low_cut", 20.0f);
        set (p, "high_cut", 16000.0f);
        set (p, "resonance", 0.0f);
        set (p, "mix", 1.0f);
        set (p, "feedback", 0.5f);
        p.prepareToPlay (sr, block);
    }

    // Spielt 'seconds' Sekunden; input(n) liefert das Eingangssignal, gibt beide Kanäle zurück
    struct Take { std::vector<float> l, r; bool finite = true; float peak = 0; };

    Take run (TrendyDelayProcessor& p, double seconds, std::function<float (int)> input,
              std::function<void (int)> beforeBlock = {})
    {
        Take t;
        const int total = (int) (seconds * sr);
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        for (int start = 0; start < total; start += block)
        {
            if (beforeBlock) beforeBlock (start);
            for (int i = 0; i < block; ++i)
            {
                const float x = input (start + i);
                buffer.setSample (0, i, x);
                buffer.setSample (1, i, x);
            }
            p.processBlock (buffer, midi);
            for (int i = 0; i < block; ++i)
            {
                const float a = buffer.getSample (0, i), b = buffer.getSample (1, i);
                if (! std::isfinite (a) || ! std::isfinite (b)) t.finite = false;
                t.peak = std::max ({ t.peak, std::abs (a), std::abs (b) });
                t.l.push_back (a);
                t.r.push_back (b);
            }
        }
        return t;
    }

    int argMax (const std::vector<float>& v, int from, int to)
    {
        int best = from;
        for (int i = from; i < to; ++i)
            if (std::abs (v[(size_t) i]) > std::abs (v[(size_t) best])) best = i;
        return best;
    }

    float rms (const std::vector<float>& v, int from, int to)
    {
        double s = 0;
        for (int i = from; i < to; ++i) s += (double) v[(size_t) i] * v[(size_t) i];
        return (float) std::sqrt (s / juce::jmax (1, to - from));
    }

    bool report (const char* name, bool ok, const juce::String& detail)
    {
        std::printf ("%-26s %-48s %s\n", name, detail.toRawUTF8(), ok ? "OK" : "FEHLER");
        return ok;
    }

    juce::Random rng (7);
    float noise (int) { return rng.nextFloat() * 0.6f - 0.3f; }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    TrendyDelayProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    bool ok = true;
    const auto impulse = [] (int n) { return n == 0 ? 0.5f : 0.0f; };

    // 1. Echozeit stimmt und jedes Echo ist leiser als das vorige
    {
        // kurzer 1-kHz-Ton (5 ms), damit die Filter der Schleife das Ergebnis nicht verfälschen
        const auto burst = [] (int n)
        {
            if (n >= 240) return 0.0f;
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / 240.0f);
            return 0.5f * w * std::sin (juce::MathConstants<float>::twoPi * 1000.0f * (float) n / (float) sr);
        };
        int p0 = 0;
        for (int n = 0; n < 240; ++n) if (std::abs (burst (n)) > std::abs (burst (p0))) p0 = n;
        clean (proc);
        auto t = run (proc, 1.0, burst);
        // zweites Echo ist durch die Filter leicht verschliffen: Spitze darf um eine halbe Schwingung wandern
        const int e1 = argMax (t.l, 6000, 18000) - p0, e2 = argMax (t.l, 18000, 30000) - p0;
        const float ratio = rms (t.l, e2 + p0 - 400, e2 + p0 + 400) / rms (t.l, e1 + p0 - 400, e1 + p0 + 400);
        ok &= report ("Echozeit 250 ms", std::abs (e1 - 12000) <= 3 && std::abs (e2 - 24000) <= 30 && ratio > 0.45f && ratio < 0.55f && t.finite,
                      juce::String::formatted ("Echo bei %d / %d, Verhaeltnis %.2f", e1, e2, ratio));
    }

    // 2. Tempo-Sync: 1/8 punktiert bei 120 BPM = 375 ms
    {
        clean (proc);
        set (proc, "sync", 1.0f);
        set (proc, "division", 6.0f);
        auto t = run (proc, 0.6, impulse);
        const int e1 = argMax (t.l, 6000, 28000);
        ok &= report ("Sync 1/8. bei 120 BPM", std::abs (e1 - 18000) <= 3, juce::String::formatted ("Echo bei %d (erwartet 18000)", e1));
    }

    // 3. Ping-Pong: erstes Echo links, zweites rechts
    {
        clean (proc);
        set (proc, "mode", (float) Params::PingPong);
        set (proc, "width", 1.0f);
        auto t = run (proc, 0.7, impulse);
        const float l1 = std::abs (t.l[(size_t) argMax (t.l, 11000, 13000)]), r1 = std::abs (t.r[(size_t) argMax (t.r, 11000, 13000)]);
        const float l2 = std::abs (t.l[(size_t) argMax (t.l, 23000, 25000)]), r2 = std::abs (t.r[(size_t) argMax (t.r, 23000, 25000)]);
        ok &= report ("Ping-Pong", l1 > 4.0f * r1 && r2 > 4.0f * l2,
                      juce::String::formatted ("1: L %.3f R %.3f  2: L %.3f R %.3f", l1, r1, l2, r2));
    }

    // 4. Drei Köpfe: Echos bei 1/3, 2/3 und 3/3 der Zeit
    {
        clean (proc);
        set (proc, "mode", (float) Params::ThreeHeads);
        set (proc, "time", 300.0f);
        auto t = run (proc, 0.4, impulse);
        const int a = argMax (t.l, 2000, 6000), b = argMax (t.l, 7000, 11000), c = argMax (t.l, 12000, 16000);
        ok &= report ("Drei Koepfe", std::abs (a - 4800) <= 3 && std::abs (b - 9600) <= 3 && std::abs (c - 14400) <= 3,
                      juce::String::formatted ("Koepfe bei %d / %d / %d", a, b, c));
    }

    // 5. Abklingen mit normalem Feedback und vollem Band-Charakter
    {
        clean (proc);
        set (proc, "wow", 0.5f); set (proc, "flutter", 0.5f); set (proc, "drive", 0.7f);
        set (proc, "spring_mix", 0.5f); set (proc, "spring_place", (float) Params::InLoop);
        set (proc, "feedback", 0.6f); set (proc, "low_cut", 150.0f); set (proc, "high_cut", 3000.0f);
        auto t = run (proc, 14.0, [] (int n) { return n < 4800 ? noise (n) : 0.0f; });
        const int end = (int) t.l.size();
        const float tail = rms (t.l, end - 4800, end);
        ok &= report ("Abklingen", t.finite && tail < 1.0e-3f && t.peak < 2.0f, juce::String::formatted ("Peak %.3f, Rest %.6f", t.peak, tail));
    }

    // 6. Feedback über 100 %: Band schwingt selbst, bleibt aber begrenzt
    {
        clean (proc);
        set (proc, "feedback", 1.2f); set (proc, "drive", 0.6f); set (proc, "resonance", 0.6f);
        set (proc, "low_cut", 100.0f); set (proc, "high_cut", 2500.0f);
        auto t = run (proc, 12.0, [] (int n) { return n < 2400 ? noise (n) : 0.0f; });
        const int end = (int) t.l.size();
        const float tail = rms (t.l, end - 48000, end);
        ok &= report ("Selbstoszillation", t.finite && tail > 0.05f && t.peak < 3.0f, juce::String::formatted ("Peak %.3f, nach 11 s noch %.3f", t.peak, tail));
    }

    // 7. ENDLOS: Schleife kreist weiter, neuer Eingang kommt nicht hinein
    {
        clean (proc);
        set (proc, "feedback", 0.5f);
        auto t = run (proc, 6.0, noise, [&] (int start) { set (proc, "freeze", start >= 24000 ? 1.0f : 0.0f); });
        const int end = (int) t.l.size();
        const float early = rms (t.l, 36000, 48000), late = rms (t.l, end - 24000, end);
        ok &= report ("Endlos", t.finite && late > 0.3f * early && late < 2.5f * early && t.peak < 3.0f,
                      juce::String::formatted ("vorher %.3f, nach 5 s %.3f", early, late));
        set (proc, "freeze", 0.0f);
    }

    // 8. THROW: Send zu, nur der gehaltene Moment landet im Echo
    {
        clean (proc);
        set (proc, "send", 0.0f);
        set (proc, "feedback", 0.0f);
        auto t = run (proc, 1.5, noise, [&] (int start) { set (proc, "throw", start >= 24000 && start < 36000 ? 1.0f : 0.0f); });
        const float quiet = rms (t.l, 12000, 24000);           // Echo von Material ohne THROW
        const float thrown = rms (t.l, 39000, 46000);          // Echo vom geworfenen Material
        ok &= report ("Throw", quiet < 0.01f && thrown > 0.05f, juce::String::formatted ("ohne %.4f, mit %.4f", quiet, thrown));
        set (proc, "throw", 0.0f);
    }

    // 9. Federhall: klingt nach und hat das Boing
    {
        clean (proc);
        set (proc, "feedback", 0.0f);
        set (proc, "spring_mix", 1.0f);
        set (proc, "spring_place", (float) Params::OnAll);
        set (proc, "mix", 0.0f);
        auto t = run (proc, 1.5, impulse);
        const float tail = rms (t.l, 9600, 19200);
        ok &= report ("Federhall", t.finite && tail > 1.0e-4f && t.peak < 2.0f, juce::String::formatted ("Nachhall 200-400 ms: %.5f", tail));
    }

    // 10. Bandtempo-Sprung: halbes Tempo verdoppelt die Echozeit
    {
        clean (proc);
        set (proc, "speed", 0.0f);
        run (proc, 0.2, [] (int) { return 0.0f; });
        auto t = run (proc, 1.0, impulse);
        const int e1 = argMax (t.l, 12000, 36000);
        ok &= report ("Bandtempo halb", std::abs (e1 - 24000) <= 6, juce::String::formatted ("Echo bei %d (erwartet 24000)", e1));
    }

    // 11. Zufällige Reglerstellungen: nie NaN, nie Explosion
    {
        bool good = true;
        float worst = 0;
        juce::Random r (42);
        for (int round = 0; round < 40; ++round)
        {
            for (auto* prm : proc.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm); rp != nullptr && rp->getParameterID() != "output")
                    rp->setValueNotifyingHost (r.nextFloat());
            set (proc, "output", 0.0f);
            if (round % 10 == 0)
                proc.prepareToPlay (round % 20 == 0 ? 44100.0 : sr, block);
            auto t = run (proc, 0.8, noise, [&] (int start) { if (start % 9216 == 0) set (proc, "speed", (float) r.nextInt (3)); });
            good &= t.finite && t.peak < 8.0f;
            worst = std::max (worst, t.peak);
        }
        proc.prepareToPlay (sr, block);
        ok &= report ("Zufallsregler (40x)", good, juce::String::formatted ("hoechster Peak %.2f", worst));
    }

    // 12. Einstellungen und Design werden mit dem Projekt gespeichert
    {
        set (proc, "feedback", 0.77f);
        proc.design.store (0);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        set (proc, "feedback", 0.1f);
        proc.design.store (1);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        const float fb = proc.apvts.getRawParameterValue ("feedback")->load();
        ok &= report ("Projekt speichern", std::abs (fb - 0.77f) < 0.01f && proc.design.load() == 0,
                      juce::String::formatted ("Feedback %.2f, Design %d", fb, proc.design.load()));
        proc.design.store (1);
    }

    // Optional: Oberfläche je Seite und Design als PNG speichern (TRENDY_DELAY_SNAPSHOT = Ordner)
    if (auto* dir = std::getenv ("TRENDY_DELAY_SNAPSHOT"))
    {
        // etwas Signal, damit Anzeige und Pegel leben
        for (auto* prm : proc.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                rp->setValueNotifyingHost (rp->getDefaultValue());
        proc.prepareToPlay (sr, block);
        run (proc, 1.0, noise);

        const char* tabNames[] { "Echo", "Band", "Hall", "Dub" };
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
                juce::File file (juce::File (dir).getChildFile (juce::String ("trendy-delay-") + (m == 0 ? "1980" : "2100") + "-" + tabName + ".png"));
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
