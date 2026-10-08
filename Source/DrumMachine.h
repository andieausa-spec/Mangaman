#pragma once

#include "RhythmPatterns.h"
#include "Clock.h"

// Drumcomputer: spielt das Muster auf dem gemeinsamen Takt, mit Variation, Fills, Humanize, Shuffle und Wirbeln
class DrumMachine
{
public:
    static constexpr int numTracks = Drums::numTracks, numSteps = Drums::numSteps;
    static constexpr double rateBeats[] { 0.5, 0.25, 0.125 };   // 1/8 (halbes Tempo), 1/16, 1/32 (doppelt)

    struct Settings
    {
        bool run = false;
        int kit = 1, rate = 1, length = 16, fill = 1;
        float swing = 0, variation = 0.15f, humanize = 0, accent = 0.5f, volume = 0.8f;
        float level[numTracks] {}, tune[numTracks] {}, decay[numTracks] {}, pan[numTracks] {};
        bool mute[numTracks] {};
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& v : voices) v = {};
        follower.joined = false;
        wasRunning = false;
        playingStep.store (-1);
        holdCount = 0; heldL = heldR = 0.0f;
    }

    // Einstellungen vor hit() übernehmen (Modell, Stimmung, Stummschaltung)
    void update (const Settings& s) { settings = s; }

    // Einzelner Schlag (MIDI-Kanal 10, Vorhören)
    void hit (int t, float vel, int delay = 0)
    {
        if (t < 0 || t >= numTracks || settings.mute[t])
            return;
        auto& vc = voices[(size_t) t];
        if (delay > 0) { vc.wait = delay; vc.waitVel = vel; return; }
        trigger (t, vel);
    }

    // rechnet die Drums auf L/R (addierend); pattern: aktuelles Muster
    void render (float* L, float* R, int numSamples, const Settings& s, const Clock::Block& blk, const Rhythm::Pattern& pattern)
    {
        settings = s;
        pat = pattern;
        const auto& kit = Drums::kits()[(size_t) juce::jlimit (0, 9, s.kit)];
        const double stepL = rateBeats[juce::jlimit (0, 2, s.rate)];
        const int len = juce::jlimit (1, numSteps, s.length);
        const double sw = s.swing;

        if (s.run && ! wasRunning)
        {
            follower.joined = false;
            bar = -1; fillActive = false; wasFill = false; firstStep = true;
        }
        if (! s.run && wasRunning)
            playingStep.store (-1);
        wasRunning = s.run;

        if (s.run)
            follower.begin (blk, stepL, sw);

        for (int t = 0; t < numTracks; ++t)
        {
            const float a = (s.pan[t] + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            gainL[t] = std::cos (a) * juce::MathConstants<float>::sqrt2;
            gainR[t] = std::sin (a) * juce::MathConstants<float>::sqrt2;
        }
        const float busGain = s.volume * s.volume * 0.55f;
        const float bits = kit.bits > 0 ? std::pow (2.0f, (float) kit.bits - 1.0f) : 0.0f;
        const int hold = juce::jmax (1, kit.hold);
        const float drive = 1.0f + kit.drive;

        int pos = 0;
        while (pos < numSamples)
        {
            int end = juce::jmin (numSamples, pos + chunk);
            if (s.run)
            {
                const double next = follower.nextSample (blk, stepL, sw);
                if (next < (double) pos + 1.0)
                {
                    ++follower.k;
                    const int step = (int) (((follower.k % len) + len) % len);
                    const double stepSamples = Clock::duration (follower.k, stepL, sw) / juce::jmax (1.0e-9, blk.perSample);
                    playStep (step, len, stepSamples);
                    continue;   // gleich nochmal prüfen (sehr kurze Schritte)
                }
                if (next < (double) end)
                    end = juce::jmax (pos + 1, (int) std::ceil (next));
            }
            const int n = end - pos;

            for (int t = 0; t < numTracks; ++t)
            {
                auto& vc = voices[(size_t) t];
                if (vc.wait >= 0)
                {
                    vc.wait -= n;
                    if (vc.wait < 0) { vc.wait = -1; trigger (t, vc.waitVel); }
                }
                if (vc.rollLeft > 0)
                {
                    vc.rollIn -= n;
                    if (vc.rollIn <= 0) { --vc.rollLeft; vc.rollIn += vc.rollEvery; trigger (t, vc.rollVel); }
                }
                if (! vc.on)
                    continue;
                const float g = s.level[t] * s.level[t] * vc.vel * vc.gain * busGain;
                const float gl = g * gainL[t], gr = g * gainR[t];
                for (int i = pos; i < end; ++i)
                {
                    const float x = vc.next();
                    L[i] += x * gl;
                    R[i] += x * gr;
                }
            }

            // Charakter des Modells: grobe Auflösung und Sättigung wie bei den frühen Sample-Maschinen
            for (int i = pos; i < end; ++i)
            {
                float l = L[i], r = R[i];
                if (bits > 0.0f)
                {
                    if (holdCount++ % hold == 0) { heldL = std::round (l * bits) / bits; heldR = std::round (r * bits) / bits; }
                    l = heldL; r = heldR;
                }
                L[i] = Drums::softclip (l * drive) / drive * 1.1f;
                R[i] = Drums::softclip (r * drive) / drive * 1.1f;
            }
            pos = end;
        }
    }

    // Für die Oberfläche: gerade spielender Schritt (-1 = gestoppt) und welche Spuren dort geklungen haben
    std::atomic<int> playingStep { -1 };
    std::atomic<juce::uint32> playingHits { 0 };

private:
    void trigger (int t, float vel)
    {
        const auto& kit = Drums::kits()[(size_t) juce::jlimit (0, 9, settings.kit)];
        voices[(size_t) t].trigger (kit.voices[(size_t) t], settings.tune[t], settings.decay[t], vel, sampleRate);
        if (t == 4)
            voices[5].choke();
    }

    // Füllmuster für das Ende eines Abschnitts: Snare-Wirbel, Tom-Lauf oder beides
    void makeFill (int len)
    {
        fillStart = juce::jmax (0, len - (settings.variation > 0.6f ? 8 : 4));
        fillKind = rng.nextInt (4);
        fillActive = true;
    }

    int fillHits (int i, int len, int* tracks, float* vels) const
    {
        const float u = (float) (i - fillStart) / (float) juce::jmax (1, len - fillStart);
        int n = 0;
        auto add = [&] (int t, float v) { tracks[n] = t; vels[n] = v; ++n; };
        switch (fillKind)
        {
            case 0:  add (1, 0.45f + u * 0.55f); break;
            case 1:  add (u < 0.34f ? 8 : u < 0.67f ? 7 : 6, 0.75f + u * 0.25f); break;
            case 2:  if (i % 2 == 0) add (1, 0.85f); else add (u < 0.5f ? 8 : 6, 0.8f); break;
            default: if (i % 4 == 0 || i % 4 == 3) add (0, 0.9f); add (i % 2 != 0 ? 2 : 1, 0.6f + u * 0.4f); break;
        }
        return n;
    }

    void playStep (int step, int len, double stepSamples)
    {
        const auto& s = settings;
        const float va = s.variation;
        auto R = [this] { return rng.nextFloat(); };

        if (step == 0 || bar < 0)
            ++bar;
        static constexpr int fillEvery[] { 0, 4, 8, 16 };
        const int every = fillEvery[juce::jlimit (0, 3, s.fill)];
        if (step == 0 || firstStep)
        {
            const bool crash = wasFill;
            wasFill = false;
            fillActive = false;
            if (every > 0 && bar % every == every - 1) { makeFill (len); wasFill = true; }
            if (crash && step == 0) hit (10, 1.0f);
        }
        firstStep = false;

        const float normal = 1.0f - 0.45f * s.accent;
        juce::uint32 hits = 0;
        const bool inFill = fillActive && step >= fillStart;
        for (int t = 0; t < numTracks; ++t)
        {
            int v = pat[(size_t) (t * numSteps + step)];
            float vel = v == 2 ? 1.0f : normal;
            if (inFill && (t == 1 || t == 2 || (t >= 6 && t <= 8)))
                v = 0;
            if (va > 0.0f)
            {
                if (v == 0)
                {
                    // Geisternoten
                    const bool odd = (step & 1) != 0;
                    const float pr = t == 4 ? 0.22f : t == 1 ? (odd ? 0.1f : 0.03f) : t == 0 ? (odd ? 0.07f : 0.03f) : (t == 11 || t == 3) ? 0.07f : 0.0f;
                    if (pr > 0.0f && R() < pr * va) { v = 1; vel = t == 0 ? 0.75f : 0.38f + R() * 0.15f; }
                }
                else if (step % 4 != 0 && R() < va * 0.1f) { v = 0; }
                else if (t == 4 && R() < va * 0.08f) { hit (5, vel); hits |= 1u << 5; v = 0; }
            }
            if (v == 0)
                continue;
            if (s.humanize > 0.0f) vel *= 1.0f - R() * s.humanize * 0.3f;
            const int delay = s.humanize > 0.0f ? (int) (R() * s.humanize * 0.012f * (float) sampleRate) : 0;
            hit (t, vel, delay);
            if (v == 3)
            {
                auto& vc = voices[(size_t) t];
                vc.rollEvery = juce::jmax (1, (int) (stepSamples / 2.0));
                vc.rollIn = vc.rollEvery + delay;
                vc.rollLeft = 1;
                vc.rollVel = vel * 0.8f;
            }
            hits |= 1u << t;
        }
        if (inFill)
        {
            int tracks[4]; float vels[4];
            const int n = fillHits (step, len, tracks, vels);
            for (int k = 0; k < n; ++k) { hit (tracks[k], vels[k]); hits |= 1u << tracks[k]; }
        }
        playingHits.store (hits);
        playingStep.store (step);
    }

    static constexpr int chunk = 32;
    double sampleRate = 44100.0;
    Settings settings;
    Rhythm::Pattern pat {};
    std::array<Drums::Voice, numTracks> voices;
    float gainL[numTracks] {}, gainR[numTracks] {};
    Clock::Follower follower;
    bool wasRunning = false, fillActive = false, wasFill = false, firstStep = true;
    int bar = -1, fillStart = 12, fillKind = 0;
    int holdCount = 0;
    float heldL = 0.0f, heldR = 0.0f;
    juce::Random rng;
};
