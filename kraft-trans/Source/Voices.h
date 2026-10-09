#pragma once

#include <JuceHeader.h>
#include "Song.h"

// Klangerzeugung: alles synthetisch, keine Samples.
// Flächen (Streichermaschine, Chor, Orgel), Sequenzer-Synth (Säge, Puls, FM-Glocke), Bass, Melodiestimme mit Flöte und Piepser,
// elektronische Percussion (sechs Kits mit Metall, Syndrum und Piepsern), Maschinengeräusche (Zug, Autobahn, Zählrohr,
// Morsezeichen, Datenpiepser, Funkrauschen) und eine Roboterstimme (Vocoder, Sprachchip, Roboter), die Text spricht.
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
        float bpNorm (float v) { return bp (v) * k; }   // Bandpass mit Verstärkung 1 in der Mitte
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
    // Flächen: 0 Streicher (verstimmte Sägezähne), 1 Chor (Sägezähne, die Formanten kommen im Bus dazu),
    // 2 Orgel (Sinus-Zugriegel 16', 8', 4', 2 2/3' mit kurzem Tastenklick)
    struct PadVoice
    {
        bool active = false;
        int pitch = 0, type = 0;
        float velocity = 0.0f;
        juce::int64 gate = 0, order = 0;
        Osc osc[4];
        Svf filter;
        Env env, bell;

        void start (int note, float vel, juce::int64 gateSamples, double sr, float attack, float release, int padType, Noise& n, juce::int64 counter)
        {
            pitch = note; velocity = vel; gate = gateSamples; order = counter; active = true; type = padType;
            static const float detune[] { 0.0f, -0.09f, 0.08f, -12.0f };
            static const float ratios[] { 0.5f, 1.0f, 2.0f, 3.0f };
            for (int i = 0; i < 4; ++i)
            {
                const float hz = type == 2 ? midiHz ((float) note) * ratios[i] * (i == 0 ? 1.0015f : 1.0f) : midiHz ((float) note + detune[i]);
                osc[i].setFreq (hz, sr);
                osc[i].phase = type == 2 ? 0.0 : 0.5 + 0.5 * n.next();
            }
            if (type == 2)
                env.set (0.004f + attack * 0.1f, 0.2f, 1.0f, 0.06f + release * 0.1f, sr);
            else
                env.set (attack, 0.5f, 1.0f, release, sr);
            env.noteOn();
            bell.set (0.0005f, 0.02f, 0.0f, 0.02f, sr);
            bell.level = 0.0f;
            bell.noteOn();
        }

        void setCutoff (double hz, double sr) { filter.set (juce::jmin (hz, (double) midiHz ((float) pitch) * 24.0), 0.6f, sr); }

        float render()
        {
            if (gate > 0 && --gate == 0) env.noteOff();
            const float e = env.next();
            if (env.idle()) { active = false; return 0.0f; }
            float s;
            if (type == 2)
            {
                const float b = bell.next();
                s = 0.45f * osc[0].sine() + 0.8f * osc[1].sine() + 0.45f * osc[2].sine() + 0.25f * osc[3].sine();
                s = std::tanh (1.2f * s) * 0.85f + b * 0.25f * osc[3].pulse (0.5f);   // Tastenklick
            }
            else
                s = 0.3f * (osc[0].saw() + osc[1].saw() + osc[2].saw()) + 0.25f * osc[3].saw();
            return filter.lp (s) * e * velocity;
        }
    };

    //==========================================================================
    // Riff-Stimme: Säge oder Puls durch 24-dB-Tiefpass mit Resonanz, oder Glocke (FM mit glockigem Verhältnis)
    struct SeqVoice
    {
        bool active = false;
        int wave = 0;
        float velocity = 0.0f, cutoff = 1000.0f, envAmount = 0.5f, resonance = 0.5f;
        juce::int64 gate = 0;
        int counter = 0;
        double hz = 220.0, carrier = 0.0, modulator = 0.0;
        Osc osc;
        Svf f1, f2;
        Env amp, fenv;
        double sr = 48000.0;

        void start (int note, float vel, juce::int64 gateSamples, double sampleRate, float decay)
        {
            sr = sampleRate;
            hz = midiHz ((float) note);
            osc.setFreq (hz, sr);
            velocity = vel; gate = gateSamples; active = true;
            amp.set (0.002f, decay * 1.6f + 0.05f, wave == 2 ? 0.15f : 0.3f, wave == 2 ? 0.25f : 0.06f, sr);
            fenv.set (0.001f, decay + 0.02f, 0.0f, decay + 0.02f, sr);
            amp.noteOn();
            fenv.level = 0.0f;
            fenv.noteOn();
            counter = 0;
            carrier = modulator = 0.0;
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
                const double c = cutoff * std::pow (2.0, envAmount * 6.0 * fe * velocity);
                f1.set (c, 0.7f + resonance * 6.0f, sr);
                f2.set (c, 0.6f, sr);
            }
            if (wave == 2)
            {
                // Glocke: Phasenmodulation, Index folgt der Filterkurve
                modulator += hz * 3.5 / sr; if (modulator >= 1.0) modulator -= 1.0;
                carrier += hz / sr;         if (carrier >= 1.0) carrier -= 1.0;
                const double index = (0.4 + 3.0 * fe * (0.3 + envAmount)) * (0.5 + resonance);
                const float s = (float) std::sin (2.0 * pi * carrier + index * std::sin (2.0 * pi * modulator));
                return f2.lp (s) * e * velocity * 0.9f;
            }
            const float s = wave == 1 ? osc.pulse (0.32f) * 1.4f : osc.saw();
            return std::tanh (1.3f * f2.lp (f1.lp (s))) * e * velocity;
        }
    };

    //==========================================================================
    // Sequenzer-Bass: Säge und Rechteck eine Oktave tiefer, Biss = Übersteuerung
    struct BassVoice
    {
        bool active = false;
        float velocity = 0.0f, cutoff = 300.0f, sub = 0.5f, drive = 0.3f;
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
            amp.set (0.002f, decay + 0.08f, 0.6f, 0.05f, sr);
            fenv.set (0.001f, decay * 0.6f + 0.03f, 0.0f, 0.1f, sr);
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
                filter.set (cutoff * std::pow (2.0, 3.2 * fe), 1.3f, sr);
            const float s = osc.saw() * (1.0f - 0.5f * sub) + subOsc.pulse (0.5f) * 1.6f * sub;
            const float d = 1.0f + drive * 5.0f;
            return std::tanh (filter.lp (s) * d) / std::sqrt (d) * e * velocity;
        }
    };

    //==========================================================================
    // Melodiestimme: 0 Puls, 1 Säge, 2 FM-Glocke, 3 Flöte (Sinus mit Anblasgeräusch), 4 Piepser (kleines Rechteck wie ein Taschenrechner)
    struct LeadVoice
    {
        bool active = false;
        int wave = 0;
        float velocity = 0.0f, glide = 0.4f, vibrato = 0.5f, bright = 0.5f;
        float current = 60.0f, target = 60.0f;
        juce::int64 gate = 0, noteTime = 0;
        int counter = 0;
        double vibPhase = 0.0, pwmPhase = 0.0, carrier = 0.0, modulator = 0.0, hz = 440.0;
        Osc osc, osc2;
        Svf filter, breathFilter;
        Env amp, bell;
        Noise noise;
        double sr = 48000.0;

        void start (int note, float vel, juce::int64 gateSamples, double sampleRate)
        {
            sr = sampleRate;
            const bool legato = active && gate > 0;
            target = (float) note;
            if (! legato)
            {
                if (! active) current = target;
                if (wave == 4)
                    amp.set (0.001f, 0.05f, 0.9f, 0.01f, sr);
                else
                    amp.set (wave == 3 ? 0.04f : 0.01f, 0.5f, wave == 2 ? 0.35f : 0.85f, wave == 3 ? 0.12f : 0.35f, sr);
                amp.noteOn();
                bell.set (0.001f, wave == 3 ? 0.08f : 0.6f, 0.0f, 0.3f, sr);
                bell.level = 0.0f;
                bell.noteOn();
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
            const float be = bell.next();
            if (amp.idle()) { active = false; return 0.0f; }
            ++noteTime;
            const float glideTime = 0.005f + glide * glide * 0.5f;
            if (wave == 4) current = target;   // Piepser springt
            else current += (target - current) * (1.0f - std::exp (-1.0f / (glideTime * (float) sr)));
            vibPhase += (wave == 3 ? 4.8 : 5.4) / sr;
            if (vibPhase >= 1.0) vibPhase -= 1.0;
            pwmPhase += 0.4 / sr;
            if (pwmPhase >= 1.0) pwmPhase -= 1.0;
            const float onset = juce::jlimit (0.0f, 1.0f, ((float) noteTime / (float) sr - 0.3f) / 0.6f);
            const float vib = wave == 4 ? 0.0f : (float) std::sin (2.0 * pi * vibPhase) * vibrato * 0.4f * onset;
            if ((counter++ & 7) == 0)
            {
                hz = midiHz (current + vib);
                osc.setFreq (hz, sr);
                osc2.setFreq (hz * 1.006, sr);
                filter.set (hz * (1.5f + bright * bright * 16.0f), 0.9f, sr);
                breathFilter.set (hz * 2.0, 2.5f, sr);
            }
            float s;
            switch (wave)
            {
                case 1:  s = 0.6f * (osc.saw() + osc2.saw()); break;
                case 2:
                {
                    modulator += hz * 3.5 / sr; if (modulator >= 1.0) modulator -= 1.0;
                    carrier += hz / sr;         if (carrier >= 1.0) carrier -= 1.0;
                    const double index = 0.3 + (1.0 + 3.0 * bright) * be;
                    return (float) std::sin (2.0 * pi * carrier + index * std::sin (2.0 * pi * modulator)) * e * velocity * 0.8f;
                }
                case 3:
                {
                    // Flöte: Sinus mit etwas Oktave, Anblasgeräusch am Tonanfang, leiser Atem
                    const float ph = (float) osc.phase;
                    const float tone = (float) std::sin (2.0 * pi * ph) + 0.12f * bright * (float) std::sin (4.0 * pi * ph);
                    osc.advance();
                    const float air = breathFilter.bpNorm (noise.next()) * (0.05f + 0.6f * be) * (0.4f + bright);
                    return (tone + air) * e * velocity * 0.9f;
                }
                case 4:
                {
                    // Piepser: hartes Rechteck ohne Filter, leise
                    const float sq = osc.phase < 0.5 ? 0.5f : -0.5f;
                    osc.advance();
                    return sq * e * velocity * (0.5f + 0.3f * bright);
                }
                default: s = osc.pulse (0.5f + 0.3f * (float) std::sin (2.0 * pi * pwmPhase)) * 1.5f; break;
            }
            return filter.lp (s) * e * velocity;
        }
    };

    //==========================================================================
    // Roboterstimme: eine Formant-Sprachsynthese spricht die Silben (Anlaute, Vokal, Auslaute aus Speech.h).
    // Drei Arten:
    //  0 Vocoder:    die Sprache wird in Bänder zerlegt und steuert einen Synthesizer-Träger (Säge + Rechteck) in der Notenhöhe
    //  1 Sprachchip: kleiner Sprach-Baustein: Impulskette, wenig Bandbreite, grob abgetastet (Körnung)
    //  2 Roboter:    monotone Stimme mit Ringmodulation und kurzer Metallresonanz
    // Ohne Vibrato, ohne Zittern: Maschine, kein Mensch.
    struct RobotVoice
    {
        struct Formants { float f[5]; float g[5]; };
        enum Shape { VA, VE, VI, VO, VU, VAe, VOe, VUe, VSchwa, VM, VN, VL, VW, VJ, VR, numShapes };

        static const Formants& shape (int s)
        {
            // männlich-neutrale Lage (Roboter sind eher tief)
            static const Formants table[numShapes] {
                { { 730, 1150, 2500, 3400, 4300 }, { 1.0f, 0.65f, 0.35f, 0.25f, 0.12f } },  // a
                { { 450, 1900, 2550, 3400, 4300 }, { 1.0f, 0.45f, 0.4f, 0.25f, 0.1f } },    // e
                { { 280, 2250, 2900, 3500, 4300 }, { 1.0f, 0.3f, 0.36f, 0.25f, 0.1f } },    // i
                { { 470, 820, 2450, 3300, 4300 },  { 1.0f, 0.55f, 0.2f, 0.18f, 0.08f } },   // o
                { { 320, 750, 2350, 3300, 4300 },  { 1.0f, 0.35f, 0.12f, 0.1f, 0.05f } },   // u
                { { 600, 1700, 2500, 3400, 4300 }, { 1.0f, 0.5f, 0.35f, 0.25f, 0.1f } },    // ä
                { { 430, 1450, 2350, 3300, 4300 }, { 1.0f, 0.45f, 0.3f, 0.2f, 0.08f } },    // ö
                { { 290, 1650, 2200, 3300, 4300 }, { 1.0f, 0.35f, 0.3f, 0.2f, 0.08f } },    // ü
                { { 520, 1400, 2450, 3400, 4300 }, { 1.0f, 0.45f, 0.3f, 0.22f, 0.1f } },    // Schwa
                { { 260, 1100, 2300, 3300, 4300 }, { 0.65f, 0.06f, 0.04f, 0.03f, 0.02f } }, // m
                { { 260, 1600, 2500, 3300, 4300 }, { 0.65f, 0.09f, 0.07f, 0.04f, 0.02f } }, // n
                { { 360, 1250, 2600, 3400, 4300 }, { 0.85f, 0.3f, 0.16f, 0.1f, 0.05f } },   // l
                { { 300, 1300, 2300, 3300, 4300 }, { 0.6f, 0.2f, 0.1f, 0.06f, 0.03f } },    // w
                { { 270, 2150, 2850, 3500, 4300 }, { 0.75f, 0.22f, 0.22f, 0.15f, 0.06f } }, // j
                { { 600, 1300, 2200, 3300, 4300 }, { 0.9f, 0.4f, 0.25f, 0.15f, 0.06f } },   // vokalisches r
            };
            return table[juce::jlimit (0, (int) numShapes - 1, s)];
        }

        // Ein Lautabschnitt: Formant-Ziel, Stimmanteil, Reibelaut (Art und Pegel), Hauch
        struct Seg { int phone = -1; float dur = 0.0f; };

        bool active = false;
        int type = 0;
        float velocity = 0.0f, bands = 0.6f, carrierBright = 0.5f, crush = 0.2f, metal = 0.25f;
        float current = 60.0f, target = 60.0f, bend = 0.0f;
        juce::int64 gate = 0, gateTotal = 1, noteTime = 0;
        int counter = 0;
        Kt::Phon::Syl syl;
        std::array<Seg, 3> onset {}, coda {};
        float onsetTime = 0.0f, codaTime = 0.0f;
        int vowelFrom = VA, vowelTo = VA;
        float form[5] {}, gain[5] {};
        float voiceLevel = 0.0f, noiseLevel = 0.0f, aspirationLevel = 0.0f;
        int fricKind = 0;
        Osc glottis, carrierA, carrierB, carrierSub;
        double ringPhase = 0.0;
        Svf formant[5], fric, tiltFilter, carrierFilter, chipFilter;
        static constexpr int maxBands = 20;
        Svf anaBand[maxBands], synBand[maxBands];
        float bandEnv[maxBands] {};
        int numBands = 14;
        std::array<float, 512> comb {};
        int combPos = 0;
        float held = 0.0f;
        double holdPhase = 0.0;
        Env amp;
        Noise noise;
        double sr = 48000.0;
        bool fresh = true;

        static int vowelShape (int nucleus)
        {
            using namespace Kt::Phon;
            switch (nucleus)
            {
                case A: case Ai: case Au: return VA;
                case E:  return VE;
                case I:  return VI;
                case O: case Oi: return VO;
                case U:  return VU;
                case Ae: return VAe;
                case Oe: return VOe;
                case Ue: return VUe;
                case Er: return VR;
                case Hum: return VM;
                default: return VSchwa;
            }
        }

        static float phoneDuration (int p)
        {
            using namespace Kt::Phon;
            switch (p)
            {
                case PM: case PN: case PNg: return 0.06f;
                case PL: case PJ: return 0.05f;
                case PR: return 0.045f;
                case PW: return 0.06f;
                case PS: case PSch: case PCh: case PX: return 0.09f;
                case PZ: return 0.075f;
                case PF: return 0.08f;
                case PH: return 0.055f;
                case PT: case PK: return 0.065f;
                case PP: return 0.06f;
                default: return 0.035f;   // d, g, b
            }
        }

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            setupBands();
        }

        void setupBands()
        {
            numBands = juce::jlimit (6, maxBands, juce::roundToInt (6.0f + bands * 14.0f));
            for (int i = 0; i < numBands; ++i)
            {
                const double hz = 140.0 * std::pow (7200.0 / 140.0, (double) i / (double) (numBands - 1));
                const float q = 2.0f + 6.0f * (float) numBands / (float) maxBands;
                anaBand[i].set (hz, q, sr);
                synBand[i].set (hz, q, sr);
            }
        }

        void start (float note, float vel, juce::int64 gateSamples, double sampleRate, const Kt::Phon::Syl& s, juce::uint32 seed)
        {
            sr = sampleRate;
            const bool legato = active && amp.stage != Env::Release && amp.stage != Env::Idle;
            target = note;
            syl = s;
            vowelFrom = vowelShape (syl.nucleus);
            vowelTo = syl.nucleus == Kt::Phon::Ai || syl.nucleus == Kt::Phon::Oi ? VI : syl.nucleus == Kt::Phon::Au ? VU : vowelFrom;
            if (! legato || type != 0)
            {
                current = target;
                amp.set (0.006f, 0.2f, 1.0f, 0.04f, sr);
                amp.noteOn();
                noise.state = seed | 1u;
            }
            current = target;   // Roboter springen auf den Ton
            bend = syl.spring ? 7.0f : 0.0f;
            if (fresh)
            {
                const auto& f = shape (vowelFrom);
                for (int i = 0; i < 5; ++i) { form[i] = f.f[i]; gain[i] = f.g[i]; }
                fresh = false;
            }
            velocity = vel; gate = gateTotal = juce::jmax ((juce::int64) 1, gateSamples); active = true;
            noteTime = 0;

            // Lautabschnitte: Anlaute vorn, Auslaute hinten; bei kurzen Noten gestaucht
            onsetTime = codaTime = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                onset[(size_t) i] = i < syl.nOn ? Seg { syl.on[(size_t) i], phoneDuration (syl.on[(size_t) i]) } : Seg {};
                coda[(size_t) i]  = i < syl.nCo ? Seg { syl.co[(size_t) i], phoneDuration (syl.co[(size_t) i]) } : Seg {};
                onsetTime += onset[(size_t) i].dur;
                codaTime += coda[(size_t) i].dur;
            }
            const float total = (float) gateTotal / (float) sr;
            const float maxCons = total * 0.6f;
            if (onsetTime + codaTime > maxCons && onsetTime + codaTime > 0.0f)
            {
                const float k = maxCons / (onsetTime + codaTime);
                for (auto& sg : onset) sg.dur *= k;
                for (auto& sg : coda) sg.dur *= k;
                onsetTime *= k; codaTime *= k;
            }
        }

        void release() { gate = 0; amp.noteOff(); }

        // Laut 'phone' am Ort t (0..1 innerhalb des Lauts): Formantziel, Stimme, Reibung, Hauch
        static void consonant (int phone, float t, bool final, int& shapeOut, float& voiced, float& fricative, int& fricType, float& aspiration)
        {
            using namespace Kt::Phon;
            switch (phone)
            {
                case PM:  shapeOut = VM; voiced = 0.7f; break;
                case PN:  shapeOut = VN; voiced = 0.7f; break;
                case PNg: shapeOut = VN; voiced = 0.6f; break;
                case PL:  shapeOut = VL; voiced = 0.85f; break;
                case PJ:  shapeOut = VJ; voiced = 0.8f; break;
                case PW:  shapeOut = VW; voiced = 0.6f; fricative = 0.25f; fricType = 2; break;
                case PR:  if (final) { shapeOut = VR; voiced = 0.9f; }
                          else voiced = (t > 0.3f && t < 0.65f) ? 0.25f : 0.8f; break;
                case PS:  voiced = 0.0f; fricative = 1.0f; fricType = 0; break;
                case PZ:  voiced = 0.45f; fricative = 0.7f; fricType = 0; break;
                case PSch: voiced = 0.0f; fricative = 1.0f; fricType = 1; break;
                case PCh: voiced = 0.0f; fricative = 0.9f; fricType = 6; break;
                case PX:  voiced = 0.0f; fricative = 0.8f; fricType = 7; break;
                case PF:  voiced = 0.0f; fricative = 0.6f; fricType = 2; break;
                case PH:  voiced = 0.0f; aspiration = 1.0f; break;
                case PT: case PK: case PP:
                {
                    // Verschluss (Stille), Sprengung, Hauch
                    const int burst = phone == PT ? 3 : phone == PK ? 4 : 5;
                    if (t < 0.35f) voiced = 0.0f;
                    else if (t < 0.6f) { voiced = 0.0f; fricative = 0.9f; fricType = burst; }
                    else { voiced = final ? 0.0f : 0.2f; aspiration = final ? 0.3f : 0.7f; }
                    break;
                }
                default:
                {
                    // stimmhafte Verschlusslaute d, g, b: kurzes Brummen, kleine Sprengung
                    const int burst = phone == PD ? 3 : phone == PG ? 4 : 5;
                    if (t < 0.5f) { voiced = 0.3f; shapeOut = VM; }
                    else { voiced = 0.3f; fricative = 0.5f; fricType = burst; }
                    break;
                }
            }
        }

        void articulation (float t, float rest, int& shapeOut, float& voiced, float& fricative, int& fricType, float& aspiration) const
        {
            const float total = (float) gateTotal / (float) sr;
            shapeOut = (vowelTo != vowelFrom && t > total * 0.5f) ? vowelTo : vowelFrom;
            voiced = 1.0f; fricative = 0.0f; fricType = 0; aspiration = 0.0f;
            if (syl.nucleus == Kt::Phon::Hum) shapeOut = VM;
            if (t < onsetTime)
            {
                float acc = 0.0f;
                for (auto& sg : onset)
                {
                    if (sg.phone < 0) break;
                    if (t < acc + sg.dur)
                    {
                        consonant (sg.phone, (t - acc) / juce::jmax (1.0e-4f, sg.dur), false, shapeOut, voiced, fricative, fricType, aspiration);
                        return;
                    }
                    acc += sg.dur;
                }
            }
            if (rest < codaTime && gate > 0)
            {
                // Auslaute von hinten gezählt: der letzte Laut endet mit der Note
                float acc = codaTime;
                for (auto& sg : coda)
                {
                    if (sg.phone < 0) break;
                    const float from = acc, to = acc - sg.dur;   // Restzeit, in der dieser Laut klingt
                    if (rest <= from && rest > to)
                    {
                        consonant (sg.phone, (from - rest) / juce::jmax (1.0e-4f, sg.dur), true, shapeOut, voiced, fricative, fricType, aspiration);
                        return;
                    }
                    acc = to;
                }
            }
        }

        float render()
        {
            if (! active) return 0.0f;
            if (gate > 0 && --gate == 0) amp.noteOff();
            const float e = amp.next();
            if (amp.idle()) { active = false; fresh = true; return 0.0f; }
            ++noteTime;
            const float t = (float) noteTime / (float) sr;
            const float rest = (float) gate / (float) sr;

            if ((counter++ & 15) == 0)
            {
                int shapeNow; float voiced, fricative, aspiration; int fricType;
                articulation (t, gate > 0 ? rest : 0.0f, shapeNow, voiced, fricative, fricType, aspiration);
                const auto& f = shape (shapeNow);
                const float speed = 1.0f - std::exp (-16.0f / (0.012f * (float) sr));   // schnelle, mechanische Übergänge
                const float sharp = 0.6f + 0.8f * bands;
                for (int i = 0; i < 5; ++i)
                {
                    form[i] += (f.f[i] - form[i]) * speed;
                    gain[i] += (f.g[i] - gain[i]) * speed;
                    const float bw = std::array<float, 5> { 90.0f, 110.0f, 150.0f, 200.0f, 260.0f }[(size_t) i];
                    formant[i].set (form[i], form[i] / bw * sharp, sr);
                }
                const float lvlSpeed = 1.0f - std::exp (-16.0f / (0.004f * (float) sr));
                voiceLevel += (voiced - voiceLevel) * lvlSpeed;
                noiseLevel += (fricative - noiseLevel) * lvlSpeed;
                fricKind = fricType;
                aspirationLevel = aspiration;
                static const float fricHz[] { 6000.0f, 2700.0f, 4200.0f, 4000.0f, 1800.0f, 1000.0f, 3900.0f, 1400.0f };
                static const float fricQ[]  { 2.2f, 1.6f, 0.6f, 1.2f, 1.5f, 0.9f, 2.0f, 1.3f };
                fric.set (fricHz[juce::jlimit (0, 7, fricType)], fricQ[juce::jlimit (0, 7, fricType)], sr);
                tiltFilter.set (2500.0 + 3000.0 * carrierBright, 0.6f, sr);
                carrierFilter.set (900.0 * std::pow (2.0, 3.2 * carrierBright), 0.7f, sr);
                chipFilter.set (3400.0, 0.8f, sr);

                // Tonhöhe: springt auf den Ton; "boing" federt nach
                float spring = 0.0f;
                if (bend > 0.0f)
                    spring = bend * std::exp (-t * 7.0f) * std::cos (2.0f * juce::MathConstants<float>::pi * 5.5f * t);
                const float pitch = current + spring;
                if (type == 0)
                {
                    glottis.setFreq (98.0, sr);   // Sprache zur Analyse: feste, neutrale Lage
                    carrierA.setFreq (midiHz (pitch), sr);
                    carrierB.setFreq (midiHz (pitch) * 1.0035, sr);
                    carrierSub.setFreq (midiHz (pitch - 12.0f), sr);
                }
                else
                    glottis.setFreq (midiHz (pitch - (type == 2 ? 12.0f : 0.0f)), sr);
            }

            const float n = noise.next();
            const float fricOut = fric.bpNorm (noise.next()) * noiseLevel * (fricKind == 2 ? 0.25f : 0.5f) + n * aspirationLevel * 0.25f;
            float out;
            if (type == 0)
            {
                // Vocoder: Sprache analysieren, Bänder steuern den Träger
                const float speech = speechSample (glottis.saw() * 1.4f, n, fricOut);
                const float carrier = carrierFilter.lp (0.5f * (carrierA.saw() + carrierB.saw()) + 0.35f * carrierSub.pulse (0.5f));
                const float unvoiced = noise.next() * (noiseLevel + aspirationLevel * 0.6f);
                const float exc = carrier * (0.25f + 0.75f * voiceLevel) + unvoiced * 0.8f;
                const float att = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));
                const float rel = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));
                out = 0.0f;
                for (int i = 0; i < numBands; ++i)
                {
                    const float a = std::abs (anaBand[i].bpNorm (speech));
                    bandEnv[i] += (a - bandEnv[i]) * (a > bandEnv[i] ? att : rel);
                    out += synBand[i].bpNorm (exc) * bandEnv[i];
                }
                out *= 22.0f / std::sqrt ((float) numBands);
            }
            else if (type == 1)
            {
                // Sprachchip: Impulskette statt Säge, schmalbandig
                const float pulse = glottis.phase < 0.12 ? 1.0f : -0.12f;
                glottis.advance();
                out = chipFilter.lp (speechSample (pulse * 1.6f, n, fricOut)) * 0.65f;
            }
            else
            {
                // Roboter: Säge-Stimme, ringmoduliert
                out = speechSample (glottis.saw() * 1.4f, n, fricOut);
                ringPhase += (55.0 + 50.0 * metal) / sr;
                if (ringPhase >= 1.0) ringPhase -= 1.0;
                const float ring = (float) std::sin (2.0 * pi * ringPhase);
                out = out * (1.0f - 0.7f * metal) + out * ring * 0.9f * metal;
            }

            // Metall: kurze Kammfilter-Resonanz (blecherne Röhre)
            if (metal > 0.01f)
            {
                const int delay = 60 + (int) (metal * 200.0f);
                const int rp = (combPos - delay + (int) comb.size()) % (int) comb.size();
                const float y = out + comb[(size_t) rp] * (0.35f + 0.5f * metal);
                comb[(size_t) combPos] = y;
                combPos = (combPos + 1) % (int) comb.size();
                out = out * (1.0f - 0.5f * metal) + y * 0.5f * metal;
            }

            // Körnung: grobe Abtastung und wenige Bits (beim Sprachchip immer etwas)
            const float grain = type == 1 ? 0.35f + 0.65f * crush : crush;
            if (grain > 0.01f)
            {
                const double rate = 16000.0 - 12500.0 * grain;   // 16 kHz bis 3,5 kHz
                holdPhase += rate / sr;
                if (holdPhase >= 1.0)
                {
                    holdPhase -= std::floor (holdPhase);
                    const float levels = std::pow (2.0f, 12.0f - 8.0f * grain);
                    held = std::round (out * levels) / levels;
                }
                out = held;
            }
            return out * e * velocity * 0.8f;
        }

        // Formant-Sprache aus einer Quelle (Stimme) plus Rauschen (Reibelaute)
        float speechSample (float source, float, float fricOut)
        {
            const float src = tiltFilter.lp (source) * voiceLevel;
            float out = 0.0f;
            for (int i = 0; i < 5; ++i)
                out += formant[i].bpNorm (src) * gain[i];
            return out + fricOut;
        }
    };

    //==========================================================================
    // Elektronische Percussion: Sinus mit Tonhöhenfall plus gefiltertes Rauschen. Metall = unharmonische Teiltöne
    // (wie Amboss und Blech), Tom = Syndrum mit großem Tonhöhenfall, Piepser = kurzer hoher Sinus, Klatschen = mehrere Rauschstöße
    struct DrumKit
    {
        struct Model
        {
            float toneHz, drop, dropDecay, toneDecay, toneLevel;
            int noiseMode;      // 0 Tiefpass, 1 Bandpass, 2 Hochpass
            float noiseHz, noiseQ, noiseDecay, noiseLevel, pan;
            int metal, bursts;  // metal: 0 Sinus, 1 Amboss
        };

        static constexpr int numKits = 6;

        // sechs Kits: Motorik 74, Funk 75, Schiene 77, Maschine 78, Rechner 81, Digital 86
        static const Model& model (int kit, int drum)
        {
            static const Model models[numKits][Kt::numDrums] {
                { { 55, 1.4f, 0.03f, 0.35f, 1.0f, 0, 500, 0.7f, 0.01f, 0.1f, 0.0f, 0, 0 },          // weiche Bassdrum
                  { 180, 0.3f, 0.02f, 0.08f, 0.35f, 0, 3200, 0.7f, 0.12f, 0.7f, 0.05f, 0, 0 },     // Rausch-Snare (Tiefpass)
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 7000, 0.8f, 0.03f, 0.35f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 6500, 0.8f, 0.2f, 0.3f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 1200, 1.2f, 0.12f, 0.8f, -0.1f, 0, 3 },
                  { 420, 0.0f, 0.01f, 0.25f, 0.5f, 1, 3000, 2.0f, 0.02f, 0.1f, 0.35f, 1, 0 },
                  { 160, 1.2f, 0.06f, 0.3f, 0.85f, 1, 600, 1.2f, 0.02f, 0.05f, -0.3f, 0, 0 },
                  { 1800, 0.3f, 0.01f, 0.05f, 0.4f, 2, 9000, 1.0f, 0.005f, 0.05f, -0.35f, 0, 0 } },
                { { 50, 1.8f, 0.04f, 0.45f, 1.0f, 0, 400, 0.7f, 0.008f, 0.08f, 0.0f, 0, 0 },
                  { 200, 0.3f, 0.02f, 0.06f, 0.3f, 1, 2400, 0.8f, 0.1f, 0.6f, 0.05f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 9000, 1.5f, 0.008f, 0.5f, 0.3f, 0, 0 },          // Knacken wie ein Zählrohr
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 7000, 0.8f, 0.3f, 0.3f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 1300, 1.2f, 0.12f, 0.8f, -0.1f, 0, 3 },
                  { 380, 0.0f, 0.01f, 0.4f, 0.4f, 1, 3000, 2.0f, 0.02f, 0.1f, 0.35f, 1, 0 },
                  { 120, 1.5f, 0.08f, 0.4f, 0.8f, 1, 500, 1.0f, 0.02f, 0.05f, -0.3f, 0, 0 },
                  { 2400, 0.0f, 0.01f, 0.03f, 0.35f, 2, 9000, 1.0f, 0.004f, 0.08f, -0.35f, 0, 0 } },
                { { 58, 2.0f, 0.03f, 0.3f, 1.0f, 0, 600, 0.7f, 0.01f, 0.12f, 0.0f, 0, 0 },
                  { 240, 0.2f, 0.015f, 0.06f, 0.35f, 1, 2800, 1.2f, 0.09f, 0.7f, 0.05f, 1, 0 },     // Snare mit Blech
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 5200, 2.5f, 0.025f, 0.5f, 0.25f, 0, 0 },           // Schienen-Klackern
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 6000, 0.8f, 0.25f, 0.3f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 1300, 1.3f, 0.12f, 0.8f, -0.1f, 0, 3 },
                  { 330, 0.0f, 0.01f, 0.6f, 0.75f, 1, 2500, 3.0f, 0.03f, 0.25f, 0.2f, 1, 0 },       // Amboss
                  { 140, 1.0f, 0.05f, 0.3f, 0.8f, 1, 600, 1.2f, 0.02f, 0.05f, -0.3f, 0, 0 },
                  { 2000, 0.0f, 0.01f, 0.04f, 0.35f, 2, 9000, 1.0f, 0.004f, 0.05f, -0.35f, 0, 0 } },
                { { 52, 2.6f, 0.03f, 0.32f, 1.1f, 0, 500, 0.7f, 0.008f, 0.12f, 0.0f, 0, 0 },
                  { 190, 0.4f, 0.02f, 0.08f, 0.4f, 0, 4200, 0.7f, 0.14f, 0.9f, 0.05f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 8000, 0.8f, 0.03f, 0.38f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 7500, 0.8f, 0.22f, 0.35f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 1250, 1.3f, 0.14f, 0.9f, -0.1f, 0, 3 },
                  { 460, 0.0f, 0.01f, 0.3f, 0.5f, 1, 3000, 2.0f, 0.02f, 0.1f, 0.35f, 1, 0 },
                  { 220, 2.2f, 0.12f, 0.35f, 0.9f, 1, 800, 1.2f, 0.02f, 0.05f, -0.3f, 0, 0 },       // Syndrum: pjuuu
                  { 2200, -0.3f, 0.02f, 0.05f, 0.35f, 2, 9000, 1.0f, 0.004f, 0.05f, -0.35f, 0, 0 } },
                { { 56, 2.2f, 0.025f, 0.28f, 1.05f, 0, 800, 0.7f, 0.006f, 0.2f, 0.0f, 0, 0 },
                  { 210, 0.5f, 0.015f, 0.07f, 0.45f, 0, 6000, 0.7f, 0.2f, 1.0f, 0.05f, 0, 0 },      // knackige Snare mit langem Rauschen
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 8500, 0.9f, 0.04f, 0.45f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 8000, 0.9f, 0.26f, 0.45f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 1500, 1.3f, 0.1f, 0.8f, -0.1f, 0, 3 },
                  { 520, 0.0f, 0.01f, 0.25f, 0.5f, 1, 3000, 2.0f, 0.02f, 0.1f, 0.35f, 1, 0 },
                  { 180, 1.6f, 0.08f, 0.25f, 0.8f, 1, 700, 1.2f, 0.02f, 0.05f, -0.3f, 0, 0 },
                  { 1600, 0.6f, 0.01f, 0.06f, 0.45f, 2, 9000, 1.0f, 0.004f, 0.05f, -0.35f, 0, 0 } },  // Rechner-Piepser
                { { 50, 3.0f, 0.035f, 0.4f, 1.15f, 0, 450, 0.7f, 0.008f, 0.2f, 0.0f, 0, 0 },
                  { 200, 0.4f, 0.02f, 0.1f, 0.45f, 1, 1900, 0.7f, 0.18f, 0.95f, 0.05f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 9500, 0.9f, 0.03f, 0.42f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 8000, 0.9f, 0.26f, 0.5f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 1300, 1.3f, 0.16f, 1.0f, -0.1f, 0, 4 },
                  { 300, 0.0f, 0.01f, 0.7f, 0.8f, 1, 2800, 3.0f, 0.04f, 0.3f, 0.25f, 1, 0 },        // großer Metallschlag
                  { 150, 1.4f, 0.06f, 0.3f, 0.85f, 1, 500, 1.0f, 0.03f, 0.05f, -0.35f, 0, 0 },
                  { 2600, 0.0f, 0.01f, 0.05f, 0.35f, 2, 9500, 1.2f, 0.004f, 0.05f, -0.35f, 0, 0 } },
            };
            return models[juce::jlimit (0, numKits - 1, kit)][juce::jlimit (0, (int) Kt::numDrums - 1, drum)];
        }

        struct Voice
        {
            double phase[5] {};
            float toneEnv = 0.0f, dropEnv = 0.0f, noiseEnv = 0.0f, velocity = 0.0f;
            float toneCoef = 0.0f, dropCoef = 0.0f, noiseCoef = 0.0f;
            int burstsLeft = 0, burstTimer = 0;
            Svf filter;
            Model m {};
        };

        std::array<Voice, Kt::numDrums> voices;
        Noise noise;
        double sr = 48000.0;
        float tune = 1.0f;

        void prepare (double sampleRate) { sr = sampleRate; }

        void trigger (int drum, float vel, int kit, float tone)
        {
            if (drum < 0 || drum >= Kt::numDrums) return;
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
            v.burstsLeft = v.m.bursts;
            v.burstTimer = 0;
            for (auto& p : v.phase) p = 0.0;
            if (drum == Kt::HatClosed)   // geschlossene Hihat würgt die offene ab
                voices[Kt::HatOpen].noiseEnv *= 0.05f;
        }

        void render (float& l, float& r)
        {
            static const float ratios[] { 1.0f, 2.76f, 5.40f, 8.93f, 13.34f };   // Amboss / Blechplatte: unharmonisch
            static const float weights[] { 1.0f, 0.8f, 0.55f, 0.35f, 0.2f };
            l = r = 0.0f;
            for (auto& v : voices)
            {
                if (v.toneEnv < 1.0e-5f && v.noiseEnv < 1.0e-5f && v.burstsLeft == 0)
                    continue;
                float s = 0.0f;
                if (v.toneEnv > 1.0e-5f)
                {
                    const double hz = v.m.toneHz * tune * (1.0f + v.m.drop * v.dropEnv);
                    if (v.m.metal)
                    {
                        float m = 0.0f;
                        for (int k = 0; k < 5; ++k)
                        {
                            m += (float) std::sin (2.0 * pi * v.phase[k]) * weights[k];
                            v.phase[k] += hz * ratios[k] / sr;
                            if (v.phase[k] >= 1.0) v.phase[k] -= 1.0;
                        }
                        s += std::tanh (m * 1.4f) * 0.55f * v.toneEnv;
                    }
                    else
                    {
                        s += (float) std::sin (2.0 * pi * v.phase[0]) * v.toneEnv;
                        v.phase[0] += hz / sr;
                        if (v.phase[0] >= 1.0) v.phase[0] -= 1.0;
                    }
                    v.toneEnv *= v.toneCoef;
                    v.dropEnv *= v.dropCoef;
                }
                if (v.burstsLeft > 0 && ++v.burstTimer >= (int) (0.011 * sr))
                {
                    v.burstTimer = 0;
                    --v.burstsLeft;
                    v.noiseEnv = v.m.noiseLevel;
                }
                if (v.noiseEnv > 1.0e-5f)
                {
                    float lp, bp, hp;
                    v.filter.tick (noise.next(), lp, bp, hp);
                    s += (v.m.noiseMode == 0 ? lp : v.m.noiseMode == 1 ? bp * 1.6f : hp) * v.noiseEnv;
                    v.noiseEnv *= v.burstsLeft > 0 ? 0.9992f * v.noiseCoef : v.noiseCoef;
                }
                s *= v.velocity;
                l += s * (1.0f - v.m.pan) * 0.7f;
                r += s * (1.0f + v.m.pan) * 0.7f;
            }
        }
    };

    //==========================================================================
    // Geräusche, alles synthetisch:
    //  Funkrauschen (gefiltertes Rauschen mit Knistern und wanderndem Pfeifton), Anlauf (anschwellendes Rauschen),
    //  Zug (Rollen, Schienenstöße im Takt, Pfeife), Autobahn (Motor fährt mit Doppler-Effekt vorbei, Hupe),
    //  Zählrohr (zufälliges Knacken), Morsezeichen (Piepton im Funkrauschen), Datenpiepser (Rechteck-Töne im Sechzehntel-Raster)
    struct FxVoice
    {
        bool active = false;
        int type = 0;
        float velocity = 0.0f, pan = 0.0f, baseHz = 300.0f;
        juce::int64 total = 1, pos = 0;
        int counter = 0;
        double slow = 0.0, wander = 0.0, samplesPerBeat = 24000.0;
        double partial[6] {};
        Svf fl, fr, fl2, fr2, rumble;
        Noise noise;
        double sr = 48000.0;
        // Morse: Folge aus Tönen und Pausen (in Einheiten)
        std::array<juce::uint8, 64> morse {};
        int morseLen = 0;
        float dataPitch = 0.0f, toneEnv = 0.0f;
        juce::int64 nextEvent = 0;

        void start (int fxType, float vel, juce::int64 lengthSamples, juce::int64 offsetSamples, double sampleRate, juce::uint32 seed,
                    int rootNote, double spb)
        {
            sr = sampleRate;
            type = fxType; velocity = vel; active = true;
            samplesPerBeat = juce::jmax (100.0, spb);
            total = juce::jmax ((juce::int64) 1, lengthSamples);
            pos = juce::jlimit ((juce::int64) 0, total - 1, offsetSamples);
            noise.state = seed | 1u;
            pan = 0.7f * noise.next();
            slow = 0.5 + 0.5 * noise.next();
            baseHz = midiHz ((float) juce::jlimit (36, 84, rootNote));
            for (auto& p : partial) p = 0.0;
            counter = 0;
            rumble.set (90.0, 0.8f, sr);
            nextEvent = 0;
            toneEnv = 0.0f;
            // zufällige Morsezeichen: Punkt = 1, Strich = 3 Einheiten, Lücken 1 bzw. 3
            morseLen = 0;
            while (morseLen < 60)
            {
                const int letters = 1 + (int) ((noise.next() * 0.5f + 0.5f) * 4.0f);
                for (int k = 0; k < letters && morseLen < 60; ++k)
                {
                    morse[(size_t) morseLen++] = noise.next() > 0.0f ? 3 : 1;
                    morse[(size_t) morseLen++] = 0;   // Lücke 1
                }
                morse[(size_t) juce::jmax (0, morseLen - 1)] = 100;   // Buchstabenlücke
            }
        }

        static int group (int t)   // Regler: 0 Rauschen, 1 Maschinen, 2 Signale
        {
            return t == Kt::Static || t == Kt::Swell ? 0 : t == Kt::Train || t == Kt::Car || t == Kt::Geiger ? 1 : 2;
        }

        void render (float& l, float& r, const float* amounts)
        {
            l = r = 0.0f;
            if (! active) return;
            if (pos >= total) { active = false; return; }
            const float t = (float) pos / (float) total;
            const float seconds = (float) pos / (float) sr;
            const float tailSeconds = (float) (total - pos) / (float) sr;
            const float a = amounts[group (type)] * velocity;
            const juce::int64 here = pos++;
            const bool update = (counter++ & 7) == 0;

            switch (type)
            {
                case Kt::Static:
                {
                    const float env = juce::jmin (1.0f, seconds / 3.0f, tailSeconds / 3.0f);
                    if (update)
                    {
                        slow += 8.0 / sr * 0.05;
                        wander = 0.995 * wander + 0.005 * noise.next();
                        const double c = 900.0 * std::pow (2.0, 2.0 * (0.5 + 0.5 * std::sin (2.0 * pi * slow)) + 1.2 * wander);
                        fl.set (c, 1.2f, sr);
                        fr.set (c * 1.2, 1.2f, sr);
                    }
                    // Pfeifton beim "Sendersuchen"
                    partial[0] += (1200.0 + 900.0 * std::sin (2.0 * pi * slow * 0.7)) / sr;
                    if (partial[0] >= 1.0) partial[0] -= 1.0;
                    const float whistle = (float) std::sin (2.0 * pi * partial[0]) * 0.05f;
                    const float crackle = noise.next() > 0.9985f ? noise.next() * 2.0f : 0.0f;
                    l = (fl.bp (noise.next()) * 0.5f + whistle + crackle) * env * a;
                    r = (fr.bp (noise.next()) * 0.5f + whistle * 0.8f + crackle) * env * a;
                    break;
                }
                case Kt::Swell:
                {
                    const float env = std::pow (t, 2.4f) * juce::jmin (1.0f, tailSeconds / 0.01f);
                    if (update)
                    {
                        fl.set (300.0 * std::pow (25.0, (double) t), 0.9f, sr);
                        fr.set (340.0 * std::pow (25.0, (double) t), 0.9f, sr);
                    }
                    l = fl.bp (noise.next()) * 2.0f * env * a;
                    r = fr.bp (noise.next()) * 2.0f * env * a;
                    break;
                }
                case Kt::Train:
                {
                    // Rollen + Schienenstöße "ta-tam" auf jedem Schlag, am Anfang die Pfeife
                    const float env = juce::jmin (1.0f, seconds / 4.0f, tailSeconds / 3.0f);
                    const double inBeat = std::fmod ((double) here, samplesPerBeat) / samplesPerBeat;
                    float clack = 0.0f;
                    for (double hit : { 0.0, 0.25, 0.5 })
                    {
                        const double d = (inBeat - hit) * samplesPerBeat / sr;
                        if (d >= 0.0 && d < 0.06)
                            clack += (float) std::exp (-d * 70.0) * (hit == 0.5 ? 1.0f : 0.6f);
                    }
                    if (update) { fl.set (2600.0, 3.0f, sr); fr.set (3400.0, 3.0f, sr); }
                    const float rail = fl.bp (noise.next()) * clack * 2.5f;
                    const float roll = rumble.lp (noise.next()) * 1.2f;
                    float whistle = 0.0f;
                    if (seconds < 2.2f)
                    {
                        const float we = juce::jmin (1.0f, seconds / 0.08f, (2.2f - seconds) / 0.3f);
                        partial[0] += 523.25 / sr; partial[1] += 659.26 / sr; partial[2] += 784.0 / sr;
                        for (int k = 0; k < 3; ++k) if (partial[k] >= 1.0) partial[k] -= 1.0;
                        whistle = (float) (std::sin (2.0 * pi * partial[0]) + std::sin (2.0 * pi * partial[1]) + 0.7 * std::sin (2.0 * pi * partial[2]))
                                  * 0.12f * we + fr.bp (noise.next()) * 0.08f * we;
                    }
                    l = (roll + rail * 0.9f + whistle) * env * a;
                    r = (roll + rail * 1.1f + whistle) * env * a;
                    break;
                }
                case Kt::Car:
                {
                    // Vorbeifahrt: kommt von links, wird lauter, Doppler senkt den Ton, fährt nach rechts weg
                    const float x = t * 2.0f - 1.0f;                       // -1 .. 1 Strecke
                    const float dist = std::sqrt (x * x * 9.0f + 0.15f);
                    const float doppler = 1.0f / (1.0f + 0.12f * (-x / dist));
                    const float env = juce::jmin (1.0f, 0.5f / dist) * juce::jmin (1.0f, tailSeconds / 0.2f);
                    if (update)
                    {
                        fl.set (300.0 + 1800.0 / dist, 0.7f, sr);
                        partial[1] = baseHz;   // unbenutzt
                    }
                    partial[0] += 55.0 * doppler / sr;
                    if (partial[0] >= 1.0) partial[0] -= 1.0;
                    const float engine = fl.lp ((float) (2.0 * partial[0] - 1.0) + 0.3f * noise.next());
                    const float wind = rumble.lp (noise.next()) * 0.8f;
                    float horn = 0.0f;
                    if (std::abs (x + 0.15f) < 0.08f || std::abs (x - 0.05f) < 0.05f)
                    {
                        partial[2] += 370.0 * doppler / sr; partial[3] += 466.0 * doppler / sr;
                        for (int k = 2; k < 4; ++k) if (partial[k] >= 1.0) partial[k] -= 1.0;
                        horn = ((partial[2] < 0.5 ? 0.15f : -0.15f) + (partial[3] < 0.5 ? 0.15f : -0.15f));
                    }
                    const float s = (engine * 0.9f + wind + horn) * env * a;
                    const float p = juce::jlimit (-1.0f, 1.0f, x);
                    l = s * (1.0f - 0.8f * p);
                    r = s * (1.0f + 0.8f * p);
                    break;
                }
                case Kt::Geiger:
                {
                    // Knacken in zufälligen Abständen, Dichte schwankt langsam
                    const float env = juce::jmin (1.0f, seconds / 1.0f, tailSeconds / 1.0f);
                    if (here >= nextEvent)
                    {
                        slow += 0.01;
                        const float rate = 6.0f + 30.0f * (0.5f + 0.5f * (float) std::sin (2.0 * pi * slow));
                        const float u = juce::jmax (1.0e-4f, 0.5f + 0.5f * noise.next());
                        nextEvent = here + (juce::int64) (-std::log (u) / rate * sr);
                        toneEnv = 1.0f;
                    }
                    const float click = toneEnv * (noise.next() > 0.0f ? 1.0f : -1.0f);
                    toneEnv *= 0.82f;
                    const float s = fl2.hp (click) * 0.9f * env * a;
                    if (update) fl2.set (2500.0, 0.7f, sr);
                    l = s * (1.0f - pan * 0.5f);
                    r = s * (1.0f + pan * 0.5f);
                    break;
                }
                case Kt::Morse:
                {
                    const double unit = 0.07 * sr;
                    const juce::int64 u = (juce::int64) (here / unit);
                    juce::int64 acc = 0;
                    bool on = false;
                    for (int k = 0; k < morseLen; ++k)
                    {
                        const int len = morse[(size_t) k] == 0 ? 1 : morse[(size_t) k] == 100 ? 3 : morse[(size_t) k];
                        if (u < acc + len) { on = morse[(size_t) k] == 1 || morse[(size_t) k] == 3; break; }
                        acc += len;
                    }
                    toneEnv += ((on ? 1.0f : 0.0f) - toneEnv) * 0.01f;
                    partial[0] += 880.0 / sr;
                    if (partial[0] >= 1.0) partial[0] -= 1.0;
                    const float env = juce::jmin (1.0f, tailSeconds / 0.05f);
                    if (update) fl.set (1500.0, 0.8f, sr);
                    const float s = ((float) std::sin (2.0 * pi * partial[0]) * toneEnv * 0.35f + fl.bp (noise.next()) * 0.05f) * env * a;
                    l = s; r = s;
                    break;
                }
                default:
                {
                    // Datenpiepser: Rechteck-Töne im Sechzehntel-Raster, Tonhöhen aus dem Dur-Dreiklang über dem Grundton
                    const double step = samplesPerBeat * 0.25;
                    if (here >= nextEvent)
                    {
                        nextEvent = (juce::int64) ((std::floor ((double) here / step) + 1.0) * step);
                        static const float intervals[] { 0, 7, 12, 4, 16, 19, 24, 12 };
                        const bool sound = noise.next() > -0.4f;
                        dataPitch = intervals[(int) ((noise.next() * 0.5f + 0.5f) * 7.99f)];
                        toneEnv = sound ? 1.0f : 0.0f;
                    }
                    const double hz = baseHz * std::pow (2.0, dataPitch / 12.0);
                    partial[0] += hz / sr;
                    if (partial[0] >= 1.0) partial[0] -= 1.0;
                    const float env = juce::jmin (1.0f, seconds / 0.5f, tailSeconds / 0.5f);
                    const float s = (partial[0] < 0.5 ? 0.25f : -0.25f) * toneEnv * env * a;
                    toneEnv *= 0.9996f;
                    l = s * (1.0f - pan);
                    r = s * (1.0f + pan);
                    break;
                }
            }
        }
    };

    //==========================================================================
    // Ensemble der Flächen: drei modulierte Verzögerungen ergeben Breite und Schwebung
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

    // Chor-Formanten für die Flächen: drei Bandpässe, Vokal von u über o nach a
    struct ChoirFilter
    {
        Svf f[3];
        double sr = 48000.0;
        float shown = -1.0f;

        void prepare (double sampleRate) { sr = sampleRate; shown = -1.0f; for (auto& x : f) x.reset(); }

        float process (float in, float vowel)
        {
            if (std::abs (vowel - shown) > 0.002f)
            {
                shown = vowel;
                const float v = juce::jlimit (0.0f, 1.0f, vowel);
                const float f1 = 330.0f + 400.0f * v, f2 = 720.0f + 480.0f * v, f3 = 2400.0f + 200.0f * v;
                f[0].set (f1, f1 / 90.0f, sr);
                f[1].set (f2, f2 / 110.0f, sr);
                f[2].set (f3, f3 / 180.0f, sr);
            }
            return f[0].bpNorm (in) * 1.0f + f[1].bpNorm (in) * 0.6f + f[2].bpNorm (in) * 0.25f + in * 0.08f;
        }
    };

    // Doppelung der Stimme: zwei kurz verzögerte, leicht schwankende Kopien links und rechts
    struct Doubler
    {
        std::vector<float> buffer;
        int write = 0;
        double sr = 48000.0, phase = 0.0;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            buffer.assign ((size_t) (sr * 0.08) + 4, 0.0f);
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

        void process (float in, float amount, float& l, float& r)
        {
            buffer[(size_t) write] = in;
            phase += 0.37 / sr; if (phase >= 1.0) phase -= 1.0;
            const double a = 0.0016 * std::sin (2.0 * pi * phase), b = 0.0013 * std::sin (2.0 * pi * (phase + 0.37));
            const float d1 = tap ((0.019 + a) * sr), d2 = tap ((0.031 + b) * sr);
            write = (write + 1) % (int) buffer.size();
            l = in + d1 * amount * 0.8f;
            r = in + d2 * amount * 0.8f;
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
