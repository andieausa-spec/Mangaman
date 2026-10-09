#pragma once

#include <JuceHeader.h>
#include "Song.h"

// Klangerzeugung: alles synthetisch, keine Samples.
// Flächen (Streicher, Chor, Glas), Riff-Synth (Säge, Puls, Metall-FM), Sequenzer-Bass, Melodiestimme mit Slide-Gitarre,
// Drumcomputer (vier Kits mit Metallschlägen), Atmosphäre, Anlauf, Metallklang und eine Gesangsstimme aus Formantfiltern.
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
    // 2 Glas (Sinus mit glockigen Obertönen, die schnell verklingen)
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
            static const float ratios[] { 1.0f, 2.0f, 3.01f, 4.2f };
            for (int i = 0; i < 4; ++i)
            {
                const float hz = type == 2 ? midiHz ((float) note) * ratios[i] : midiHz ((float) note + detune[i]);
                osc[i].setFreq (hz, sr);
                osc[i].phase = 0.5 + 0.5 * n.next();
            }
            env.set (type == 2 ? juce::jmin (attack, 0.4f) : attack, 0.5f, 1.0f, release, sr);
            env.noteOn();
            bell.set (0.002f, 1.2f, 0.0f, 0.5f, sr);
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
                s = 0.7f * osc[0].sine() + (0.12f + 0.35f * b) * osc[1].sine() + 0.4f * b * osc[2].sine() + 0.2f * b * osc[3].sine();
            }
            else
                s = 0.3f * (osc[0].saw() + osc[1].saw() + osc[2].saw()) + 0.25f * osc[3].saw();
            return filter.lp (s) * e * velocity;
        }
    };

    //==========================================================================
    // Riff-Stimme: Säge oder Puls durch 24-dB-Tiefpass mit Resonanz, oder Metall (FM mit unharmonischem Verhältnis)
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
                // Metall: Phasenmodulation, Index folgt der Filterkurve
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
    // Melodiestimme: 0 Puls, 1 Säge, 2 Metall (FM-Glocke), 3 Slide-Gitarre (gezupfte Saite, die gleitet)
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
        Svf filter, cab;
        Env amp, bell;
        Noise noise;
        double sr = 48000.0;

        // Saite (Karplus-Strong)
        std::array<float, 4096> string {};
        int write = 0, excite = 0;
        float loop = 0.0f;

        void start (int note, float vel, juce::int64 gateSamples, double sampleRate)
        {
            sr = sampleRate;
            const bool legato = active && gate > 0;
            target = (float) note;
            if (! legato)
            {
                if (! active) current = target;
                amp.set (wave == 3 ? 0.002f : 0.01f, 0.5f, wave == 2 ? 0.35f : 0.85f, wave == 3 ? 0.25f : 0.35f, sr);
                amp.noteOn();
                bell.set (0.001f, 0.6f, 0.0f, 0.3f, sr);
                bell.level = 0.0f;
                bell.noteOn();
                noteTime = 0;
                excite = (int) (sr / midiHz (target)) + 8;   // Saite neu anzupfen
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
            const float glideTime = 0.005f + glide * glide * (wave == 3 ? 0.35f : 0.5f);
            current += (target - current) * (1.0f - std::exp (-1.0f / (glideTime * (float) sr)));
            vibPhase += 5.4 / sr;
            if (vibPhase >= 1.0) vibPhase -= 1.0;
            pwmPhase += 0.4 / sr;
            if (pwmPhase >= 1.0) pwmPhase -= 1.0;
            const float onset = juce::jlimit (0.0f, 1.0f, ((float) noteTime / (float) sr - 0.3f) / 0.6f);
            const float vib = (float) std::sin (2.0 * pi * vibPhase) * vibrato * 0.4f * onset;
            if ((counter++ & 7) == 0)
            {
                hz = midiHz (current + vib);
                osc.setFreq (hz, sr);
                osc2.setFreq (hz * 1.006, sr);
                filter.set (hz * (1.5f + bright * bright * 16.0f), 0.9f, sr);
                cab.set (2600.0 + 2500.0 * bright, 0.8f, sr);
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
                    // Saite: Verzögerung = eine Periode, Tiefpass in der Schleife, beim Gleiten ändert sich nur die Länge
                    const float damp = 0.35f + 0.55f * bright;
                    const double delay = juce::jlimit (2.0, 4000.0, sr / hz - (1.0 - damp) / damp);
                    double rp = (double) write - delay;
                    while (rp < 0.0) rp += (double) string.size();
                    const int i0 = (int) rp;
                    const float frac = (float) (rp - i0);
                    const float rd = string[(size_t) (i0 % (int) string.size())] * (1.0f - frac) + string[(size_t) ((i0 + 1) % (int) string.size())] * frac;
                    loop += damp * (rd - loop);
                    float in = loop * 0.9985f;
                    if (excite > 0) { --excite; in += noise.next() * 0.7f; }
                    string[(size_t) write] = in;
                    write = (write + 1) % (int) string.size();
                    s = cab.lp (std::tanh (loop * 2.6f)) * 1.1f;
                    return s * e * velocity;
                }
                default: s = osc.pulse (0.5f + 0.3f * (float) std::sin (2.0 * pi * pwmPhase)) * 1.5f; break;
            }
            return filter.lp (s) * e * velocity;
        }
    };

    //==========================================================================
    // Gesang: Stimmband-Quelle (Sägezahn mit Zittern und Atemrauschen) durch fünf Formantfilter.
    // Jede Silbe bringt ihren Anlaut (Nasal, Zischlaut, Verschlusslaut, Gleitlaut), den Vokal und den Auslaut mit.
    // Eine tiefe, dunkle Baritonstimme im Stil, aber kein Nachbau der Stimme einer bestimmten Person.
    struct VocalVoice
    {
        struct Formants { float f[5]; float g[5]; };

        enum Shape { VA, VE, VI, VO, VU, VAe, VSchwa, VM, VN, VL, VW, VJ, numShapes };

        static const Formants& shape (int s)
        {
            static const Formants table[numShapes] {
                { { 700, 1150, 2550, 3200, 3700 }, { 1.0f, 0.65f, 0.38f, 0.3f, 0.12f } },   // a
                { { 420, 1850, 2450, 3200, 3700 }, { 1.0f, 0.42f, 0.36f, 0.25f, 0.1f } },   // e
                { { 290, 2150, 2850, 3350, 3800 }, { 1.0f, 0.25f, 0.32f, 0.25f, 0.1f } },   // i
                { { 460, 800, 2550, 3200, 3700 },  { 1.0f, 0.6f, 0.2f, 0.2f, 0.08f } },     // o
                { { 330, 720, 2350, 3150, 3700 },  { 1.0f, 0.35f, 0.1f, 0.12f, 0.05f } },   // u
                { { 570, 1650, 2450, 3200, 3700 }, { 1.0f, 0.5f, 0.35f, 0.28f, 0.1f } },    // ä
                { { 500, 1400, 2450, 3200, 3700 }, { 1.0f, 0.45f, 0.3f, 0.25f, 0.1f } },    // Schwa
                { { 260, 1100, 2300, 3200, 3700 }, { 0.65f, 0.06f, 0.04f, 0.03f, 0.02f } }, // m (Summen)
                { { 260, 1600, 2500, 3200, 3700 }, { 0.65f, 0.09f, 0.07f, 0.04f, 0.02f } }, // n
                { { 360, 1250, 2700, 3200, 3700 }, { 0.85f, 0.3f, 0.15f, 0.1f, 0.05f } },   // l
                { { 300, 650, 2300, 3150, 3700 },  { 0.75f, 0.22f, 0.05f, 0.05f, 0.03f } }, // w
                { { 280, 2100, 2900, 3350, 3800 }, { 0.75f, 0.2f, 0.2f, 0.15f, 0.05f } },   // j
            };
            return table[juce::jlimit (0, (int) numShapes - 1, s)];
        }

        bool active = false;
        float velocity = 0.0f, depth = 0.6f, breath = 0.35f, vibrato = 0.5f, glide = 0.4f;
        float current = 48.0f, target = 48.0f;
        juce::int64 gate = 0, gateTotal = 1, noteTime = 0;
        int counter = 0, onset = 0, nucleus = 0, coda = 0;
        int vowelFrom = VA, vowelTo = VA;
        float form[5] {}, gain[5] {};
        double vibPhase = 0.0, vibRate = 5.1;
        float jitter = 0.0f, drift = 0.0f, shimmer = 1.0f, tilt = 0.0f;
        float voiceLevel = 0.0f, noiseLevel = 0.0f;
        Osc glottis;
        Svf formant[5], fric, tiltFilter;
        Env amp;
        Noise noise;
        double sr = 48000.0;
        bool fresh = true;

        static int vowelShape (int nucleus)
        {
            using namespace DarkK::Phon;
            switch (nucleus)
            {
                case A: case Ai: case Au: return VA;
                case E:  return VE;
                case I:  return VI;
                case O: case Oi: return VO;
                case U:  return VU;
                case Ae: return VAe;
                case Hum: return VM;
                default: return VSchwa;
            }
        }

        void start (int note, float vel, juce::int64 gateSamples, double sampleRate, int syllable, juce::uint32 seed)
        {
            using namespace DarkK::Phon;
            sr = sampleRate;
            const bool legato = active && amp.stage != Env::Release && amp.stage != Env::Idle;
            target = (float) note;
            const int code = juce::jmax (0, syllable);
            onset = DarkK::Phon::onset (code);
            nucleus = DarkK::Phon::nucleus (code);
            coda = DarkK::Phon::coda (code);
            vowelFrom = vowelShape (nucleus);
            vowelTo = nucleus == Ai || nucleus == Oi ? VI : nucleus == Au ? VU : vowelFrom;
            if (! legato)
            {
                current = target - 0.7f * (0.3f + glide);   // Ansingen von unten
                amp.set (0.025f, 0.3f, 1.0f, 0.12f, sr);
                amp.noteOn();
                noise.state = seed | 1u;
                vibRate = 4.8 + 0.6 * (0.5 + 0.5 * noise.next());
            }
            if (fresh)
            {
                const auto& f = shape (vowelFrom);
                for (int i = 0; i < 5; ++i) { form[i] = f.f[i]; gain[i] = f.g[i]; }
                fresh = false;
            }
            velocity = vel; gate = gateTotal = juce::jmax ((juce::int64) 1, gateSamples); active = true;
            noteTime = 0;
        }

        void release() { gate = 0; amp.noteOff(); }

        // Zielformanten und Pegel für den Moment t (Sekunden seit Silbenbeginn), rest = Sekunden bis Notenende
        void articulation (float t, float rest, int& shapeOut, float& voiced, float& fricative, int& fricType, float& aspiration) const
        {
            using namespace DarkK::Phon;
            const float total = (float) gateTotal / (float) sr;
            // Vokal mit Doppelvokal-Gleiten im letzten Teil der Note
            shapeOut = (vowelTo != vowelFrom && t > total * 0.55f) ? vowelTo : vowelFrom;
            voiced = 1.0f; fricative = 0.0f; fricType = 0; aspiration = 0.0f;

            switch (onset)
            {
                case OM: if (t < 0.07f) { shapeOut = VM; voiced = 0.7f; } break;
                case ON: if (t < 0.06f) { shapeOut = VN; voiced = 0.7f; } break;
                case OL: if (t < 0.05f) { shapeOut = VL; voiced = 0.85f; } break;
                case OW: if (t < 0.06f) { shapeOut = VW; voiced = 0.75f; } break;
                case OJ: if (t < 0.05f) { shapeOut = VJ; voiced = 0.8f; } break;
                case OR: if (t < 0.045f) { voiced = (t > 0.015f && t < 0.03f) ? 0.25f : 0.8f; } break;
                case OS: if (t < 0.09f) { voiced = 0.0f; fricative = 1.0f; fricType = 0; } break;
                case OSch: if (t < 0.1f) { voiced = 0.0f; fricative = 1.0f; fricType = 1; } break;
                case OF: if (t < 0.08f) { voiced = 0.0f; fricative = 0.6f; fricType = 2; } break;
                case OH: if (t < 0.06f) { voiced = 0.0f; aspiration = 1.0f; } break;
                case OT: if (t < 0.02f) voiced = 0.0f; else if (t < 0.04f) { voiced = 0.0f; fricative = 0.9f; fricType = 3; }
                         else if (t < 0.07f) { voiced = 0.2f; aspiration = 0.7f; } break;
                case OK: if (t < 0.025f) voiced = 0.0f; else if (t < 0.045f) { voiced = 0.0f; fricative = 0.9f; fricType = 4; }
                         else if (t < 0.075f) { voiced = 0.2f; aspiration = 0.7f; } break;
                case OD: if (t < 0.015f) { voiced = 0.3f; fricative = 0.5f; fricType = 3; } break;
                default: break;
            }
            if (nucleus == Hum)
            {
                shapeOut = onset == OH && t < 0.06f ? VSchwa : VM;
                return;
            }
            // Auslaut in den letzten Millisekunden der Note
            switch (coda)
            {
                case CN: if (rest < 0.08f) { shapeOut = VN; voiced = juce::jmin (voiced, 0.7f); } break;
                case CM: if (rest < 0.08f) { shapeOut = VM; voiced = juce::jmin (voiced, 0.7f); } break;
                case CS: if (rest < 0.1f) { voiced = rest / 0.1f * 0.5f; fricative = 1.0f; fricType = 0; } break;
                case CT: if (rest < 0.05f) { voiced = 0.0f; if (rest < 0.02f) { fricative = 0.8f; fricType = 3; } } break;
                default: break;
            }
        }

        // Stereo-Ausgabe nicht nötig: Doppelung und Raum kommen im Bus dazu
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
                // Formanten gleiten zum Ziel (Artikulation), dunkler mit "Tiefe"
                const float shift = 1.07f - 0.24f * depth;
                const auto& f = shape (shapeNow);
                const float speed = 1.0f - std::exp (-16.0f / (0.018f * (float) sr));
                for (int i = 0; i < 5; ++i)
                {
                    form[i] += (f.f[i] * (i < 3 ? shift : 1.0f) - form[i]) * speed;
                    gain[i] += (f.g[i] - gain[i]) * speed;
                    const float bw = std::array<float, 5> { 70.0f, 90.0f, 120.0f, 160.0f, 220.0f }[(size_t) i];
                    formant[i].set (form[i], form[i] / bw, sr);
                }
                const float lvlSpeed = 1.0f - std::exp (-16.0f / (0.006f * (float) sr));
                voiceLevel += (voiced - voiceLevel) * lvlSpeed;
                noiseLevel += ((float) fricative - noiseLevel) * lvlSpeed;
                fricKind = fricType;
                aspirationLevel = aspiration;
                static const float fricHz[] { 6200.0f, 2900.0f, 4200.0f, 4300.0f, 1900.0f };
                static const float fricQ[]  { 2.2f, 1.6f, 0.6f, 1.2f, 1.5f };
                fric.set (fricHz[juce::jlimit (0, 4, fricType)], fricQ[juce::jlimit (0, 4, fricType)], sr);
                tiltFilter.set (900.0 + 3800.0 * (1.0 - depth * 0.7), 0.6f, sr);

                // Tonhöhe: Gleiten, Ansingen, verzögertes Vibrato, leichtes Zittern
                const float glideTime = 0.02f + glide * glide * 0.18f;
                current += (target - current) * (1.0f - std::exp (-16.0f / (glideTime * (float) sr)));
                drift = 0.999f * drift + 0.001f * noise.next() * 6.0f;
                jitter = 0.7f * jitter + 0.3f * noise.next();
                shimmer = 1.0f + 0.06f * jitter;
                vibPhase += 16.0 * vibRate / sr;
                if (vibPhase >= 1.0) vibPhase -= 1.0;
                const float onsetVib = juce::jlimit (0.0f, 1.0f, (t - 0.28f) / 0.45f);
                const float vib = (float) std::sin (2.0 * pi * vibPhase) * (0.06f + 0.5f * vibrato) * onsetVib;
                glottis.setFreq (midiHz (current + vib + drift * 0.08f + jitter * 0.03f), sr);
            }

            // Quelle: Sägezahn (entspricht grob Stimmband plus Abstrahlung), Atem im Takt der Stimmlippen
            const float phase = (float) glottis.phase;
            const float src = tiltFilter.lp (glottis.saw()) * 1.6f;
            const float n = noise.next();
            const float breathy = n * breath * 0.35f * (phase < 0.45f ? 1.0f : 0.35f);
            const float excite = (src * (1.0f - 0.4f * breath) + breathy) * voiceLevel * shimmer + n * aspirationLevel * 0.5f;

            float out = 0.0f;
            for (int i = 0; i < 5; ++i)
                out += formant[i].bpNorm (excite) * gain[i];
            out += fric.bpNorm (noise.next()) * noiseLevel * (fricKind == 2 ? 0.2f : 0.45f);
            return out * e * velocity * 0.8f;
        }

        int fricKind = 0;
        float aspirationLevel = 0.0f;
    };

    //==========================================================================
    // Drumcomputer: Sinus mit Tonhöhenfall plus gefiltertes Rauschen; Metall = unharmonische Teiltöne,
    // Klatschen = mehrere kurze Rauschstöße hintereinander
    struct DrumKit
    {
        struct Model
        {
            float toneHz, drop, dropDecay, toneDecay, toneLevel;
            int noiseMode;      // 0 Tiefpass, 1 Bandpass, 2 Hochpass
            float noiseHz, noiseQ, noiseDecay, noiseLevel, pan;
            int metal, bursts;
        };

        // vier Kits: Analog 1981, Metall 1984, Groß 1987, Elektro 1990
        static const Model& model (int kit, int drum)
        {
            static const Model models[4][DarkK::numDrums] {
                { { 58, 2.0f, 0.03f, 0.3f, 1.0f, 0, 500, 0.7f, 0.01f, 0.12f, 0.0f, 0, 0 },
                  { 200, 0.4f, 0.02f, 0.08f, 0.4f, 1, 1800, 0.8f, 0.14f, 0.75f, 0.05f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 8000, 0.8f, 0.04f, 0.45f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 7000, 0.8f, 0.25f, 0.4f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 1300, 1.4f, 0.12f, 0.9f, -0.1f, 0, 3 },
                  { 420, 0.0f, 0.01f, 0.25f, 0.5f, 1, 3000, 2.0f, 0.05f, 0.15f, 0.35f, 1, 0 },
                  { 150, 0.6f, 0.03f, 0.25f, 0.8f, 1, 600, 1.2f, 0.02f, 0.1f, -0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 9000, 1.0f, 0.09f, 0.4f, -0.35f, 0, 2 } },
                { { 52, 3.0f, 0.025f, 0.35f, 1.15f, 0, 700, 0.7f, 0.012f, 0.3f, 0.0f, 0, 0 },
                  { 180, 1.0f, 0.01f, 0.12f, 0.5f, 1, 1400, 0.6f, 0.22f, 1.0f, 0.0f, 1, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 9000, 1.0f, 0.03f, 0.5f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 7500, 1.0f, 0.2f, 0.45f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 1100, 1.0f, 0.18f, 1.0f, -0.1f, 0, 4 },
                  { 310, 0.0f, 0.01f, 0.6f, 0.8f, 1, 2400, 3.0f, 0.08f, 0.25f, 0.35f, 1, 0 },
                  { 120, 0.8f, 0.02f, 0.3f, 0.85f, 1, 500, 1.0f, 0.03f, 0.2f, -0.3f, 1, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 8500, 1.2f, 0.08f, 0.45f, -0.35f, 0, 2 } },
                { { 55, 1.5f, 0.03f, 0.4f, 1.0f, 0, 450, 0.7f, 0.01f, 0.15f, 0.0f, 0, 0 },
                  { 190, 0.3f, 0.02f, 0.12f, 0.45f, 1, 1500, 0.7f, 0.35f, 0.85f, 0.05f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 7800, 0.8f, 0.05f, 0.4f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 6800, 0.8f, 0.35f, 0.4f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 1200, 1.2f, 0.25f, 0.9f, -0.1f, 0, 3 },
                  { 260, 0.0f, 0.01f, 0.9f, 0.6f, 1, 2000, 2.5f, 0.1f, 0.2f, 0.4f, 1, 0 },
                  { 110, 0.5f, 0.04f, 0.45f, 0.9f, 1, 400, 1.0f, 0.03f, 0.12f, -0.35f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 8000, 1.0f, 0.12f, 0.4f, -0.35f, 0, 2 } },
                { { 50, 3.5f, 0.04f, 0.45f, 1.1f, 0, 400, 0.7f, 0.008f, 0.2f, 0.0f, 0, 0 },
                  { 210, 0.5f, 0.02f, 0.07f, 0.35f, 2, 2500, 0.8f, 0.1f, 0.8f, 0.05f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 8500, 0.9f, 0.035f, 0.5f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 7000, 0.9f, 0.28f, 0.45f, 0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 1, 1400, 1.3f, 0.1f, 0.9f, -0.1f, 0, 3 },
                  { 520, 0.0f, 0.01f, 0.2f, 0.45f, 1, 3500, 2.0f, 0.04f, 0.15f, 0.35f, 1, 0 },
                  { 140, 0.7f, 0.03f, 0.22f, 0.8f, 1, 700, 1.2f, 0.02f, 0.1f, -0.3f, 0, 0 },
                  { 0, 0, 0.01f, 0.01f, 0.0f, 2, 9500, 1.2f, 0.11f, 0.55f, -0.35f, 0, 2 } },
            };
            return models[juce::jlimit (0, 3, kit)][juce::jlimit (0, (int) DarkK::numDrums - 1, drum)];
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

        std::array<Voice, DarkK::numDrums> voices;
        Noise noise;
        double sr = 48000.0;
        float tune = 1.0f;

        void prepare (double sampleRate) { sr = sampleRate; }

        void trigger (int drum, float vel, int kit, float tone)
        {
            if (drum < 0 || drum >= DarkK::numDrums) return;
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
            if (drum == DarkK::HatClosed)   // geschlossene Hihat würgt die offene ab
                voices[DarkK::HatOpen].noiseEnv *= 0.05f;
        }

        void render (float& l, float& r)
        {
            static const float ratios[] { 1.0f, 1.47f, 2.09f, 2.56f, 3.12f };
            static const float weights[] { 1.0f, 0.8f, 0.6f, 0.5f, 0.35f };
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
                        s += m * 0.3f * v.toneEnv;
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
                    // Klatschen und Tamburin: schneller Abfall zwischen den Stößen
                    v.noiseEnv *= v.burstsLeft > 0 ? 0.9992f * v.noiseCoef : v.noiseCoef;
                }
                s *= v.velocity;
                l += s * (1.0f - v.m.pan) * 0.7f;
                r += s * (1.0f + v.m.pan) * 0.7f;
            }
        }
    };

    //==========================================================================
    // Effekte: Atmosphäre (dunkles, wanderndes Rauschen mit Grollen), Anlauf (rückwärts anschwellend), Metallklang
    struct FxVoice
    {
        bool active = false;
        int type = 0;
        float velocity = 0.0f, pan = 0.0f, baseHz = 300.0f;
        juce::int64 total = 1, pos = 0;
        int counter = 0;
        double slow = 0.0, wander = 0.0;
        double partial[6] {};
        Svf fl, fr, fl2, fr2, rumble;
        Noise noise;
        double sr = 48000.0;

        void start (int fxType, float vel, juce::int64 lengthSamples, juce::int64 offsetSamples, double sampleRate, juce::uint32 seed)
        {
            sr = sampleRate;
            type = fxType; velocity = vel; active = true;
            total = juce::jmax ((juce::int64) 1, lengthSamples);
            pos = juce::jlimit ((juce::int64) 0, total - 1, offsetSamples);
            noise.state = seed | 1u;
            pan = 0.7f * noise.next();
            slow = 0.5 + 0.5 * noise.next();
            baseHz = 160.0f + 260.0f * (0.5f + 0.5f * noise.next());
            for (auto& p : partial) p = 0.0;
            counter = 0;
            rumble.set (90.0, 0.8f, sr);
        }

        float amount (const float* amounts) const
        {
            return type == DarkK::Atmo ? amounts[0] : type == DarkK::Swell ? amounts[1] : amounts[2];
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
                case DarkK::Atmo:
                {
                    const float env = juce::jmin (1.0f, seconds / 3.0f, tailSeconds / 3.0f);
                    if (update)
                    {
                        slow += 8.0 / sr * 0.06;
                        wander = 0.995 * wander + 0.005 * noise.next();
                        const double c = 180.0 * std::pow (2.0, 2.2 * (0.5 + 0.5 * std::sin (2.0 * pi * slow)) + 1.2 * wander);
                        fl.set (c, 3.0f, sr);
                        fr.set (c * 1.31, 3.0f, sr);
                        fl2.set (c, 1.4f, sr);
                        fr2.set (c * 1.31, 1.4f, sr);
                    }
                    const float low = rumble.lp (noise.next()) * 1.2f;
                    l = (fl2.bp (fl.bp (noise.next())) * 1.2f + low) * env * a;
                    r = (fr2.bp (fr.bp (noise.next())) * 1.2f + low) * env * a;
                    break;
                }
                case DarkK::Swell:
                {
                    // rückwärts klingender Schwall: wird immer lauter und heller, bricht auf der Eins ab
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
                default:
                {
                    // Metallklang: unharmonische Teiltöne, kurzer Anschlag, langes Ausklingen
                    static const float ratios[] { 1.0f, 1.59f, 2.14f, 2.83f, 3.41f, 4.76f };
                    const float env = std::exp (-seconds * 2.2f) * juce::jmin (1.0f, tailSeconds / 0.05f);
                    float s = 0.0f;
                    for (int k = 0; k < 6; ++k)
                    {
                        s += (float) std::sin (2.0 * pi * partial[k]) * std::exp (-seconds * (1.0f + (float) k * 0.9f)) / (1.0f + (float) k * 0.5f);
                        partial[k] += baseHz * ratios[k] / sr;
                        if (partial[k] >= 1.0) partial[k] -= 1.0;
                    }
                    const float hit = seconds < 0.02f ? noise.next() * (1.0f - seconds / 0.02f) * 0.6f : 0.0f;
                    s = (s * 0.45f * env + hit) * a;
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

    // Doppelung der Stimme: zwei kurz verzögerte, leicht schwankende Kopien links und rechts (wie zweimal eingesungen)
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
