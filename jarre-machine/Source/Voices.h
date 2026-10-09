#pragma once

#include <JuceHeader.h>
#include "Song.h"

// Klangerzeugung: alles synthetisch, keine Samples.
// String-Maschine mit Ensemble und Phaser, Sequenzer-Synth mit Resonanzfilter, Bass, gleitende Melodiestimme
// mit verzögertem Vibrato, Rhythmusbox (drei Kits), Wind, Brandung, Laser und Anlauf.
namespace Dsp
{
    constexpr double pi = juce::MathConstants<double>::pi;

    inline float midiHz (float note) { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }

    struct Noise
    {
        juce::uint32 state = 0x2545F491u;
        float next()
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return (float) state * (2.0f / 4294967295.0f) - 1.0f;
        }
    };

    struct Osc
    {
        double phase = 0.0, inc = 0.0;

        void setFreq (double hz, double sr) { inc = juce::jlimit (0.0, 0.45, hz / sr); }

        static float blep (double t, double dt)
        {
            if (dt <= 0.0) return 0.0f;
            if (t < dt)         { t /= dt; return (float) (t + t - t * t - 1.0); }
            if (t > 1.0 - dt)   { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
            return 0.0f;
        }
        void advance() { phase += inc; if (phase >= 1.0) phase -= 1.0; }

        float saw()
        {
            const float v = (float) (2.0 * phase - 1.0) - blep (phase, inc);
            advance();
            return v;
        }
        float pulse (float width)
        {
            double p2 = phase + width;
            if (p2 >= 1.0) p2 -= 1.0;
            const float v = ((float) (2.0 * phase - 1.0) - blep (phase, inc)) - ((float) (2.0 * p2 - 1.0) - blep (p2, inc));
            advance();
            return 0.5f * v;
        }
        float sine()
        {
            const float v = (float) std::sin (2.0 * pi * phase);
            advance();
            return v;
        }
    };

    // Zustandsvariablen-Filter (TPT)
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;

        void set (double hz, float q, double sr)
        {
            const float g = (float) std::tan (pi * juce::jlimit (12.0, sr * 0.45, hz) / sr);
            k = 1.0f / juce::jmax (0.3f, q);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        void reset() { ic1 = ic2 = 0.0f; }

        void tick (float v0, float& lp, float& bp, float& hp)
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = v0 - k * v1 - v2;
        }
        float lp (float v) { float l, b, h; tick (v, l, b, h); return l; }
        float bp (float v) { float l, b, h; tick (v, l, b, h); return b; }
        float hp (float v) { float l, b, h; tick (v, l, b, h); return h; }
    };

    // Hüllkurve: Attack linear, Decay und Release exponentiell
    struct Env
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release };
        Stage stage = Idle;
        float level = 0.0f, attackInc = 1.0f, decayCoef = 0.0f, sustain = 1.0f, releaseCoef = 0.0f;

        void set (float attack, float decay, float sus, float release, double sr)
        {
            attackInc = 1.0f / juce::jmax (1.0f, attack * (float) sr);
            decayCoef = std::exp (-1.0f / juce::jmax (1.0f, decay * 0.25f * (float) sr));
            sustain = sus;
            releaseCoef = std::exp (-1.0f / juce::jmax (1.0f, release * 0.25f * (float) sr));
        }
        void noteOn()  { stage = Attack; }
        void noteOff() { if (stage != Idle) stage = Release; }
        void kill()    { stage = Idle; level = 0.0f; }
        bool idle() const { return stage == Idle; }

        float next()
        {
            switch (stage)
            {
                case Attack:
                    level += attackInc;
                    if (level >= 1.0f) { level = 1.0f; stage = Decay; }
                    break;
                case Decay:
                    level = sustain + (level - sustain) * decayCoef;
                    if (std::abs (level - sustain) < 1.0e-4f) { level = sustain; stage = sustain > 0.0f ? Sustain : Idle; }
                    break;
                case Release:
                    level *= releaseCoef;
                    if (level < 1.0e-5f) { level = 0.0f; stage = Idle; }
                    break;
                case Sustain: case Idle: default: break;
            }
            return level;
        }
    };

    //==========================================================================
    // String-Maschine: drei verstimmte Sägezähne plus Oktave darunter, weiches Tiefpassfilter
    struct PadVoice
    {
        bool active = false;
        int pitch = 0;
        float velocity = 0.0f;
        juce::int64 gate = 0, order = 0;
        Osc osc[4];
        Svf filter;
        Env env;

        void start (int note, float vel, juce::int64 gateSamples, double sr, float attack, float release, Noise& n, juce::int64 counter)
        {
            pitch = note; velocity = vel; gate = gateSamples; order = counter; active = true;
            static const float detune[] { 0.0f, -0.08f, 0.07f, -12.0f };
            for (int i = 0; i < 4; ++i)
            {
                osc[i].setFreq (midiHz ((float) note + detune[i]), sr);
                osc[i].phase = 0.5 + 0.5 * n.next();
            }
            env.set (attack, 0.5f, 1.0f, release, sr);
            env.noteOn();
        }

        void setCutoff (double hz, double sr) { filter.set (juce::jmin (hz, (double) midiHz ((float) pitch) * 24.0), 0.6f, sr); }

        float render()
        {
            if (gate > 0 && --gate == 0) env.noteOff();
            const float e = env.next();
            if (env.idle()) { active = false; return 0.0f; }
            const float s = 0.3f * (osc[0].saw() + osc[1].saw() + osc[2].saw()) + 0.28f * osc[3].saw();
            return filter.lp (s) * e * velocity;
        }
    };

    //==========================================================================
    // Sequenzer-Stimme: Säge oder Puls, 24-dB-Tiefpass mit Resonanz und schneller Filterkurve
    struct SeqVoice
    {
        bool active = false, pulse = false;
        float velocity = 0.0f, cutoff = 1000.0f, envAmount = 0.5f, resonance = 0.5f;
        juce::int64 gate = 0;
        int counter = 0;
        Osc osc;
        Svf f1, f2;
        Env amp, fenv;
        double sr = 48000.0;

        void start (int note, float vel, juce::int64 gateSamples, double sampleRate, float decay)
        {
            sr = sampleRate;
            osc.setFreq (midiHz ((float) note), sr);
            velocity = vel; gate = gateSamples; active = true;
            amp.set (0.002f, decay * 1.6f + 0.05f, 0.3f, 0.06f, sr);
            fenv.set (0.001f, decay + 0.02f, 0.0f, decay + 0.02f, sr);
            amp.noteOn();
            fenv.level = 0.0f;
            fenv.noteOn();
            counter = 0;
        }

        float render()
        {
            if (! active) return 0.0f;
            if (gate > 0 && --gate == 0) { amp.noteOff(); fenv.noteOff(); }
            const float e = amp.next();
            const float fe = fenv.next();
            if (amp.idle()) { active = false; return 0.0f; }
            if ((counter++ & 3) == 0)
            {
                const double hz = cutoff * std::pow (2.0, envAmount * 6.0 * fe * velocity);
                f1.set (hz, 0.7f + resonance * 6.0f, sr);
                f2.set (hz, 0.6f, sr);
            }
            const float s = pulse ? osc.pulse (0.38f) * 1.4f : osc.saw();
            return std::tanh (1.3f * f2.lp (f1.lp (s))) * e * velocity;
        }
    };

    //==========================================================================
    // Bass: Säge und Rechteck eine Oktave tiefer
    struct BassVoice
    {
        bool active = false;
        float velocity = 0.0f, cutoff = 300.0f, sub = 0.5f;
        juce::int64 gate = 0;
        int counter = 0;
        Osc osc, subOsc;
        Svf filter;
        Env amp, fenv;
        double sr = 48000.0;

        void start (int note, float vel, juce::int64 gateSamples, double sampleRate, float decay)
        {
            sr = sampleRate;
            osc.setFreq (midiHz ((float) note), sr);
            subOsc.setFreq (midiHz ((float) note - 12.0f), sr);
            velocity = vel; gate = gateSamples; active = true;
            amp.set (0.003f, decay + 0.08f, 0.55f, 0.08f, sr);
            fenv.set (0.002f, decay * 0.6f + 0.03f, 0.0f, 0.1f, sr);
            amp.noteOn();
            fenv.level = 0.0f;
            fenv.noteOn();
        }

        float render()
        {
            if (! active) return 0.0f;
            if (gate > 0 && --gate == 0) { amp.noteOff(); fenv.noteOff(); }
            const float e = amp.next();
            const float fe = fenv.next();
            if (amp.idle()) { active = false; return 0.0f; }
            if ((counter++ & 3) == 0)
                filter.set (cutoff * std::pow (2.0, 3.0 * fe), 1.1f, sr);
            const float s = osc.saw() * (1.0f - 0.5f * sub) + subOsc.pulse (0.5f) * 1.6f * sub;
            return filter.lp (s) * e * velocity;
        }
    };

    //==========================================================================
    // Melodiestimme: legato gleitend, Vibrato setzt verzögert ein (wie von Hand am Modulationsrad)
    struct LeadVoice
    {
        bool active = false;
        int wave = 0;
        float velocity = 0.0f, glide = 0.4f, vibrato = 0.5f, bright = 0.5f;
        float current = 60.0f, target = 60.0f;
        juce::int64 gate = 0, noteTime = 0;
        int counter = 0;
        double vibPhase = 0.0, pwmPhase = 0.0;
        Osc osc, osc2;
        Svf filter;
        Env amp;
        Noise breath;
        double sr = 48000.0;

        void start (int note, float vel, juce::int64 gateSamples, double sampleRate)
        {
            sr = sampleRate;
            const bool legato = active && gate > 0;
            target = (float) note;
            if (! legato)
            {
                if (! active) current = target;
                amp.set (0.02f, 0.5f, 0.8f, 0.45f, sr);
                amp.noteOn();
                noteTime = 0;
            }
            velocity = vel; gate = gateSamples; active = true;
        }

        void release() { gate = 0; amp.noteOff(); }

        float render()
        {
            if (! active) return 0.0f;
            if (gate > 0 && --gate == 0) amp.noteOff();
            const float e = amp.next();
            if (amp.idle()) { active = false; return 0.0f; }
            ++noteTime;
            const float glideTime = 0.005f + glide * glide * 0.6f;
            current += (target - current) * (1.0f - std::exp (-1.0f / (glideTime * (float) sr)));
            vibPhase += 5.3 / sr;
            if (vibPhase >= 1.0) vibPhase -= 1.0;
            pwmPhase += 0.35 / sr;
            if (pwmPhase >= 1.0) pwmPhase -= 1.0;
            const float onset = juce::jlimit (0.0f, 1.0f, ((float) noteTime / (float) sr - 0.35f) / 0.8f);
            const float vib = (float) std::sin (2.0 * pi * vibPhase) * vibrato * 0.45f * onset;
            if ((counter++ & 7) == 0)
            {
                const float hz = midiHz (current + vib);
                osc.setFreq (hz, sr);
                osc2.setFreq (hz * 2.0f, sr);
                filter.set (hz * (1.5f + bright * bright * 18.0f), 0.9f, sr);
            }
            float s;
            switch (wave)
            {
                case 1:  s = osc.pulse (0.5f + 0.32f * (float) std::sin (2.0 * pi * pwmPhase)) * 1.5f; break;
                case 2:  s = osc.sine() + 0.18f * osc2.sine() + 0.04f * breath.next(); break;
                default: s = osc.saw(); break;
            }
            return filter.lp (s) * e * velocity;
        }
    };

    //==========================================================================
    // Rhythmusbox: jedes Instrument aus Sinus mit Tonhöhenfall plus gefiltertem Rauschen
    struct DrumKit
    {
        struct Model
        {
            float toneHz, drop, dropDecay, toneDecay, toneLevel;
            int noiseMode;      // 0 Tiefpass, 1 Bandpass, 2 Hochpass
            float noiseHz, noiseQ, noiseDecay, noiseLevel, pan;
        };

        // drei Kits: Minipops-Stil (rund, gedämpft), Rhythm-Ace-Stil (klickig, trocken), Elektronisch (kräftig)
        static const Model& model (int kit, int drum)
        {
            static const Model models[3][Jarre::numDrums] {
                { { 62, 1.8f, 0.03f, 0.22f, 1.0f, 0, 600, 0.7f, 0.01f, 0.15f, 0.0f },
                  { 190, 0.3f, 0.02f, 0.07f, 0.35f, 1, 1600, 0.9f, 0.12f, 0.7f, 0.1f },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 8000, 0.8f, 0.035f, 0.45f, 0.35f },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 7000, 0.8f, 0.22f, 0.4f, 0.35f },
                  { 2300, 0.0f, 0.01f, 0.03f, 0.5f, 1, 2300, 6.0f, 0.02f, 0.05f, -0.4f },
                  { 330, 0.25f, 0.015f, 0.13f, 0.7f, 1, 900, 1.5f, 0.01f, 0.05f, -0.3f },
                  { 210, 0.25f, 0.015f, 0.17f, 0.75f, 1, 600, 1.5f, 0.01f, 0.05f, 0.3f },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 5200, 1.2f, 0.07f, 0.35f, -0.2f } },
                { { 75, 1.2f, 0.015f, 0.12f, 1.0f, 1, 1200, 1.0f, 0.006f, 0.35f, 0.0f },
                  { 240, 0.2f, 0.01f, 0.05f, 0.3f, 1, 2400, 1.2f, 0.08f, 0.8f, 0.1f },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 9000, 1.2f, 0.02f, 0.5f, 0.3f },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 8000, 1.2f, 0.15f, 0.45f, 0.3f },
                  { 2600, 0.0f, 0.01f, 0.02f, 0.6f, 1, 2600, 8.0f, 0.015f, 0.08f, -0.4f },
                  { 380, 0.2f, 0.01f, 0.08f, 0.6f, 1, 1200, 2.0f, 0.008f, 0.1f, -0.3f },
                  { 250, 0.2f, 0.01f, 0.1f, 0.65f, 1, 800, 2.0f, 0.008f, 0.1f, 0.3f },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 6000, 1.5f, 0.05f, 0.4f, -0.2f } },
                { { 50, 3.0f, 0.04f, 0.45f, 1.1f, 0, 400, 0.7f, 0.008f, 0.2f, 0.0f },
                  { 180, 0.6f, 0.03f, 0.12f, 0.45f, 2, 1200, 0.8f, 0.18f, 0.8f, 0.05f },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 7500, 0.9f, 0.05f, 0.5f, 0.3f },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 6500, 0.9f, 0.35f, 0.45f, 0.3f },
                  { 1800, 0.0f, 0.01f, 0.05f, 0.5f, 1, 1800, 5.0f, 0.02f, 0.05f, -0.4f },
                  { 300, 0.5f, 0.02f, 0.2f, 0.7f, 1, 800, 1.5f, 0.01f, 0.05f, -0.3f },
                  { 180, 0.5f, 0.02f, 0.28f, 0.75f, 1, 500, 1.5f, 0.01f, 0.05f, 0.3f },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 4500, 1.0f, 0.1f, 0.35f, -0.2f } },
            };
            return models[juce::jlimit (0, 2, kit)][juce::jlimit (0, (int) Jarre::numDrums - 1, drum)];
        }

        struct Voice
        {
            double phase = 0.0;
            float toneEnv = 0.0f, dropEnv = 0.0f, noiseEnv = 0.0f, velocity = 0.0f;
            float toneCoef = 0.0f, dropCoef = 0.0f, noiseCoef = 0.0f;
            Svf filter;
            Model m {};
        };

        std::array<Voice, Jarre::numDrums> voices;
        Noise noise;
        double sr = 48000.0;
        float tune = 1.0f;

        void prepare (double sampleRate) { sr = sampleRate; }

        void trigger (int drum, float vel, int kit, float tone)
        {
            if (drum < 0 || drum >= Jarre::numDrums) return;
            auto& v = voices[(size_t) drum];
            v.m = model (kit, drum);
            tune = std::pow (2.0f, (tone - 0.5f) * 0.9f);
            v.velocity = vel;
            v.toneEnv = v.m.toneLevel;
            v.dropEnv = 1.0f;
            v.noiseEnv = v.m.noiseLevel;
            v.toneCoef = std::exp (-1.0f / (v.m.toneDecay * 0.25f * (float) sr));
            v.dropCoef = std::exp (-1.0f / (v.m.dropDecay * 0.25f * (float) sr));
            v.noiseCoef = std::exp (-1.0f / (v.m.noiseDecay * 0.25f * (float) sr));
            v.filter.set (v.m.noiseHz * tune, v.m.noiseQ, sr);
            v.phase = 0.0;
            if (drum == Jarre::HatClosed)   // geschlossene Hihat würgt die offene ab
                voices[Jarre::HatOpen].noiseEnv *= 0.05f;
        }

        void render (float& l, float& r)
        {
            l = r = 0.0f;
            for (auto& v : voices)
            {
                if (v.toneEnv < 1.0e-5f && v.noiseEnv < 1.0e-5f)
                    continue;
                float s = 0.0f;
                if (v.toneEnv > 1.0e-5f)
                {
                    const double hz = v.m.toneHz * tune * (1.0f + v.m.drop * v.dropEnv);
                    s += (float) std::sin (2.0 * pi * v.phase) * v.toneEnv;
                    v.phase += hz / sr;
                    if (v.phase >= 1.0) v.phase -= 1.0;
                    v.toneEnv *= v.toneCoef;
                    v.dropEnv *= v.dropCoef;
                }
                if (v.noiseEnv > 1.0e-5f)
                {
                    float lp, bp, hp;
                    v.filter.tick (noise.next(), lp, bp, hp);
                    s += (v.m.noiseMode == 0 ? lp : v.m.noiseMode == 1 ? bp * 1.6f : hp) * v.noiseEnv;
                    v.noiseEnv *= v.noiseCoef;
                }
                s *= v.velocity;
                l += s * (1.0f - v.m.pan) * 0.7f;
                r += s * (1.0f + v.m.pan) * 0.7f;
            }
        }
    };

    //==========================================================================
    // Effekte: Wind, Laser, Anlauf, Brandung
    struct FxVoice
    {
        bool active = false;
        int type = 0;
        float velocity = 0.0f, startHz = 3000.0f, endHz = 100.0f, pan = 0.0f;
        juce::int64 total = 1, pos = 0;
        int counter = 0;
        double phase = 0.0, slow = 0.0, wander = 0.0;
        Osc osc;
        Svf fl, fr, fl2, fr2, tone;
        Noise noise;
        double sr = 48000.0;

        void start (int fxType, float vel, juce::int64 lengthSamples, juce::int64 offsetSamples, double sampleRate, juce::uint32 seed)
        {
            sr = sampleRate;
            type = fxType; velocity = vel; active = true;
            total = juce::jmax ((juce::int64) 1, lengthSamples);
            pos = juce::jlimit ((juce::int64) 0, total - 1, offsetSamples);
            noise.state = seed | 1u;
            startHz = 1800.0f + 3500.0f * (0.5f + 0.5f * noise.next());
            endHz = 60.0f + 160.0f * (0.5f + 0.5f * noise.next());
            pan = 0.8f * noise.next();
            slow = 0.5 + 0.5 * noise.next();
            counter = 0;
        }

        float amount (const float* amounts) const
        {
            return type == Jarre::Wind ? amounts[0] : type == Jarre::Laser ? amounts[1] : amounts[2];
        }

        void render (float& l, float& r, const float* amounts)
        {
            l = r = 0.0f;
            if (! active) return;
            if (pos >= total) { active = false; return; }
            const float t = (float) pos / (float) total;
            const float seconds = (float) pos / (float) sr;
            const float tailSeconds = (float) (total - pos) / (float) sr;
            const float a = amount (amounts) * velocity;
            ++pos;
            const bool update = (counter++ & 7) == 0;

            switch (type)
            {
                case Jarre::Wind:
                {
                    // Bandpass-Rauschen, dessen Mitte langsam umherwandert; weich ein- und ausgeblendet
                    const float env = juce::jmin (1.0f, seconds / 3.0f, tailSeconds / 3.0f);
                    if (update)
                    {
                        slow += 8.0 / sr * 0.09;
                        wander = 0.995 * wander + 0.005 * noise.next();
                        const double c = 350.0 * std::pow (2.0, 2.6 * (0.5 + 0.5 * std::sin (2.0 * pi * slow)) + 1.5 * wander);
                        fl.set (c, 2.5f, sr);
                        fr.set (c * 1.25, 2.5f, sr);
                        fl2.set (c, 1.2f, sr);
                        fr2.set (c * 1.25, 1.2f, sr);
                    }
                    // zwei Bandpässe hintereinander: dunkler, ohne Zischen in den Höhen
                    l = fl2.bp (fl.bp (noise.next())) * 1.1f * env * a;
                    r = fr2.bp (fr.bp (noise.next())) * 1.1f * env * a;
                    break;
                }
                case Jarre::Surf:
                {
                    // Brandung: Wellen aus Rauschen, Tiefpass öffnet mit jeder Welle
                    const float env = juce::jmin (1.0f, seconds / 2.0f, tailSeconds / 2.0f);
                    const float wave = (float) std::pow (0.5 - 0.5 * std::cos (2.0 * pi * 0.11 * seconds + slow * 6.0), 2.0);
                    if (update)
                    {
                        fl.set (500.0 + 2200.0 * wave, 0.7f, sr);
                        fr.set (450.0 + 2000.0 * wave, 0.7f, sr);
                    }
                    l = fl.lp (noise.next()) * (0.15f + 0.85f * wave) * env * a;
                    r = fr.lp (noise.next()) * (0.15f + 0.85f * wave) * env * a;
                    break;
                }
                case Jarre::Laser:
                {
                    // Laser: fallender Ton, wandert durch das Stereobild
                    const float hz = startHz * std::pow (endHz / startHz, std::sqrt (t));
                    osc.setFreq (hz, sr);
                    const float env = juce::jmin (1.0f, seconds / 0.004f) * (1.0f - t) * (1.0f - t);
                    if (update) tone.set (hz * 4.0f, 2.0f, sr);
                    const float s = tone.lp (osc.saw()) * env * a * 0.6f;
                    const float p = pan * (1.0f - 2.0f * t);
                    l = s * (1.0f - p);
                    r = s * (1.0f + p);
                    break;
                }
                default:
                {
                    // Anlauf: Rauschen und Ton steigen, enden scharf
                    const float env = std::pow (t, 1.6f) * juce::jmin (1.0f, tailSeconds / 0.02f);
                    if (update)
                    {
                        fl.set (180.0 * std::pow (40.0, (double) t), 1.4f, sr);
                        fr.set (200.0 * std::pow (40.0, (double) t), 1.4f, sr);
                        osc.setFreq (110.0 * std::pow (12.0, (double) t), sr);
                    }
                    const float o = osc.saw() * 0.15f;
                    l = (fl.bp (noise.next()) * 1.5f + o) * env * a;
                    r = (fr.bp (noise.next()) * 1.5f + o) * env * a;
                    break;
                }
            }
        }
    };

    //==========================================================================
    // Ensemble der String-Maschine: drei modulierte Verzögerungen ergeben Breite und Schwebung
    struct Ensemble
    {
        std::vector<float> buffer;
        int write = 0;
        double sr = 48000.0, slow = 0.0, fast = 0.0;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            buffer.assign ((size_t) (sr * 0.05) + 4, 0.0f);
            write = 0;
        }

        float tap (double delaySamples) const
        {
            const int size = (int) buffer.size();
            double rp = (double) write - delaySamples;
            while (rp < 0.0) rp += size;
            const int i0 = (int) rp;
            const float frac = (float) (rp - i0);
            return buffer[(size_t) (i0 % size)] * (1.0f - frac) + buffer[(size_t) ((i0 + 1) % size)] * frac;
        }

        void process (float in, float depth, float& l, float& r)
        {
            buffer[(size_t) write] = in;
            slow += 0.55 / sr; if (slow >= 1.0) slow -= 1.0;
            fast += 6.1 / sr;  if (fast >= 1.0) fast -= 1.0;
            float taps[3];
            for (int i = 0; i < 3; ++i)
            {
                const double off = (double) i / 3.0;
                const double ms = 7.0 + depth * (2.0 * std::sin (2.0 * pi * (slow + off)) + 0.3 * std::sin (2.0 * pi * (fast + off)));
                taps[i] = tap (ms * 0.001 * sr);
            }
            write = (write + 1) % (int) buffer.size();
            const float dry = 1.0f - 0.45f * depth;
            l = in * dry + (taps[0] + 0.5f * taps[1]) * depth * 0.7f;
            r = in * dry + (taps[2] + 0.5f * taps[1]) * depth * 0.7f;
        }
    };

    // Phaser mit sechs Allpass-Stufen je Seite, langsame Fahrt (der schwebende Flächen-Klang)
    struct Phaser
    {
        float zl[6] {}, zr[6] {}, fbl = 0.0f, fbr = 0.0f, al = 0.0f, ar = 0.0f;
        double phase = 0.0, sr = 48000.0;
        int counter = 0;

        void prepare (double sampleRate) { sr = sampleRate; }

        static float coefFor (double hz, double sr)
        {
            const double t = std::tan (pi * juce::jlimit (20.0, sr * 0.45, hz) / sr);
            return (float) ((t - 1.0) / (t + 1.0));
        }

        void process (float& l, float& r, float rate, float depth)
        {
            phase += (0.03 + rate * rate * 1.6) / sr;
            if (phase >= 1.0) phase -= 1.0;
            if ((counter++ & 15) == 0)
            {
                const double lfoL = 0.5 + 0.5 * std::sin (2.0 * pi * phase);
                const double lfoR = 0.5 + 0.5 * std::sin (2.0 * pi * (phase + 0.25));
                al = coefFor (220.0 * std::pow (18.0, lfoL), sr);
                ar = coefFor (220.0 * std::pow (18.0, lfoR), sr);
            }
            auto chain = [] (float x, float* z, float a)
            {
                for (int i = 0; i < 6; ++i)
                {
                    const float y = a * x + z[i];
                    z[i] = x - a * y;
                    x = y;
                }
                return x;
            };
            const float wl = chain (l + fbl * 0.55f, zl, al);
            const float wr = chain (r + fbr * 0.55f, zr, ar);
            fbl = wl; fbr = wr;
            l = (l + wl * depth) / (1.0f + 0.5f * depth);
            r = (r + wr * depth) / (1.0f + 0.5f * depth);
        }
    };

    // Echo im Takt: Ping-Pong mit Filtern in der Schleife
    struct Echo
    {
        std::vector<float> left, right;
        int write = 0;
        double sr = 48000.0;
        float delay = 12000.0f;
        Svf lpL, lpR, hpL, hpR;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            left.assign ((size_t) (sr * 4.0) + 4, 0.0f);
            right.assign (left.size(), 0.0f);
            write = 0;
            lpL.set (3800.0, 0.6f, sr); lpR.set (3800.0, 0.6f, sr);
            hpL.set (180.0, 0.6f, sr);  hpR.set (180.0, 0.6f, sr);
        }

        float read (const std::vector<float>& b) const
        {
            const int size = (int) b.size();
            double rp = (double) write - delay;
            while (rp < 0.0) rp += size;
            const int i0 = (int) rp;
            const float frac = (float) (rp - i0);
            return b[(size_t) (i0 % size)] * (1.0f - frac) + b[(size_t) ((i0 + 1) % size)] * frac;
        }

        void process (float in, float targetDelay, float feedback, float& l, float& r)
        {
            delay += (juce::jlimit (1.0f, (float) left.size() - 4.0f, targetDelay) - delay) * 0.0005f;
            l = read (left);
            r = read (right);
            const float fl = hpL.hp (lpL.lp (l)), fr = hpR.hp (lpR.lp (r));
            left[(size_t) write] = in + fr * feedback;
            right[(size_t) write] = fl * feedback;
            write = (write + 1) % (int) left.size();
        }
    };
}
