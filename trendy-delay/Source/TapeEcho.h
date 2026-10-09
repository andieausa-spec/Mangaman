#pragma once

#include <JuceHeader.h>
#include "Parameters.h"

// Die Klangmaschine von TRENDY DELAY: Bandecho mit Sättigung, Wow/Flutter, Filtern in der Schleife,
// Bandtempo-Sprüngen und einem Federhall, der auf den Echos, in der Schleife oder auf allem sitzen kann.
namespace Dsp
{
    // Zustandsvariablen-Filter (TPT), stabil auch bei schnellen Änderungen
    struct Svf
    {
        void set (float sampleRate, float hz, float q)
        {
            const float g = std::tan (juce::MathConstants<float>::pi * juce::jmin (hz, sampleRate * 0.45f) / sampleRate);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        void reset() { ic1 = ic2 = 0.0f; }

        float low (float v0)  { tick (v0); return v2; }
        float high (float v0) { tick (v0); return v0 - k * v1 - v2; }

    private:
        void tick (float v0)
        {
            const float v3 = v0 - ic2;
            v1 = a1 * ic1 + a2 * v3;
            v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
        }
        float k = 1.4f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0, v1 = 0, v2 = 0;
    };

    // Bandsättigung: weiches Abrunden mit leichter Unsymmetrie (gerade Obertöne), kleine Signale bleiben unverändert
    inline float tape (float x, float g, float bias)
    {
        const float tb = std::tanh (g * bias);
        return (std::tanh (g * (x + bias)) - tb) / (g * (1.0f - tb * tb));
    }

    // Verzögerungsleitung mit Hermite-Interpolation (Bandkopf liest zwischen den Samples)
    struct Line
    {
        void prepare (int minSize)
        {
            int size = 1;
            while (size < minSize) size <<= 1;
            data.assign ((size_t) size, 0.0f);
            mask = size - 1;
            write = 0;
        }
        void clear() { std::fill (data.begin(), data.end(), 0.0f); }
        int size() const { return mask + 1; }

        void push (float v) { data[(size_t) write] = v; write = (write + 1) & mask; }

        // Wert von vor 'delay' Samples (gemessen vor dem nächsten push)
        float read (float delay) const
        {
            const float pos = (float) write - delay;
            const int i = (int) std::floor (pos);
            const float f = pos - (float) i;
            const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }

    private:
        float at (int i) const { return data[(size_t) (i & mask)]; }
        std::vector<float> data;
        int mask = 0, write = 0;
    };

    //==========================================================================
    // Eine Hallfeder: Schleife aus vielen Allpässen (Dispersion = das typische "Boing") und einer Laufzeit
    struct Spring
    {
        static constexpr int stages = 40;

        void prepare (double sr, float seconds)
        {
            length = (float) (seconds * sr);
            line.prepare ((int) length + 8);
            reset();
        }
        void reset()
        {
            line.clear();
            x1.fill (0.0f);
            y1.fill (0.0f);
            lp = last = 0.0f;
        }

        float process (float in, float a, float feedback, float lpCoef)
        {
            float x = in + feedback * last;
            for (int s = 0; s < stages; ++s)
            {
                const float y = a * x + x1[(size_t) s] - a * y1[(size_t) s];
                x1[(size_t) s] = x;
                y1[(size_t) s] = y;
                x = y;
            }
            line.push (x);
            const float d = line.read (length);
            lp += lpCoef * (d - lp);
            last = lp;
            return lp;
        }

    private:
        Line line;
        float length = 1000.0f, lp = 0.0f, last = 0.0f;
        std::array<float, stages> x1 {}, y1 {};
    };

    // Federhall wie im Röhrenverstärker: zwei Federn je Kanal, Höhen und Bässe begrenzt
    struct SpringTank
    {
        void prepare (double sr)
        {
            sampleRate = (float) sr;
            const float lengths[] { 0.0371f, 0.0437f, 0.0409f, 0.0473f };
            for (int i = 0; i < 4; ++i)
                springs[(size_t) i].prepare (sr, lengths[i]);
            reset();
        }
        void reset()
        {
            for (auto& s : springs) s.reset();
            for (auto& f : inputHp) f.reset();
        }

        // decay, tone, boing: 0..1
        void setup (float decay, float tone, float boing)
        {
            feedback = 0.5f + decay * 0.42f;
            const float hz = 1400.0f + tone * tone * 6000.0f;
            lpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / sampleRate);
            coef = -(0.3f + boing * 0.45f);
            for (auto& f : inputHp) f.set (sampleRate, 180.0f, 0.7f);
        }

        // Faktor, mit dem der Hall höchstens Verstärkung 1 hat (für den Einsatz in der Echo-Schleife)
        float loopNorm() const { return 1.0f - feedback; }

        void process (float inL, float inR, float& outL, float& outR)
        {
            inL = inputHp[0].high (inL);
            inR = inputHp[1].high (inR);
            outL = 0.5f * (springs[0].process (inL, coef, feedback, lpCoef) + springs[1].process (inL, coef * 0.93f, feedback, lpCoef));
            outR = 0.5f * (springs[2].process (inR, coef * 0.97f, feedback, lpCoef) + springs[3].process (inR, coef * 0.9f, feedback, lpCoef));
        }

    private:
        std::array<Spring, 4> springs;
        std::array<Svf, 2> inputHp;
        float sampleRate = 44100.0f, feedback = 0.7f, lpCoef = 0.3f, coef = -0.5f;
    };

    //==========================================================================
    class TapeEcho
    {
    public:
        struct Settings
        {
            float delaySeconds = 0.375f;   // schon aus Tempo und Teilung berechnet
            int mode = Params::Single;
            float feedback = 0.6f, lowCut = 140.0f, highCut = 3200.0f, resonance = 0.25f;
            float drive = 0.45f, wow = 0.3f, flutter = 0.25f, age = 0.3f;
            float speed = 1.0f, glide = 0.5f;
            float springMix = 0.25f, springDecay = 0.5f, springTone = 0.5f, springBoing = 0.5f;
            int springPlace = Params::OnEchoes;
            float send = 1.0f, mix = 0.35f, outputGain = 1.0f, ducking = 0.0f, width = 0.7f;
            bool throwOn = false, freeze = false;
        };

        static constexpr double maxSeconds = 8.0;

        void prepare (double sr)
        {
            sampleRate = (float) sr;
            for (auto& l : lines)
                l.prepare ((int) (sr * maxSeconds) + 16);
            tank.prepare (sr);
            reset();
        }

        void reset()
        {
            for (auto& l : lines) l.clear();
            tank.reset();
            for (auto& f : hp) f.reset();
            for (auto& f : lp) f.reset();
            ageLp.fill (0.0f);
            started = false;
            env = 0.0f;
            wowPhase = flutterPhase = flutterPhase2 = 0.0;
            drift = driftTarget = 0.0f;
            driftCounter = 0;
        }

        void process (juce::AudioBuffer<float>& buffer, const Settings& s)
        {
            const int numSamples = buffer.getNumSamples();
            const int numCh = juce::jmin (2, buffer.getNumChannels());
            if (numCh == 0 || numSamples == 0)
                return;
            auto* left = buffer.getWritePointer (0);
            auto* right = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

            const float sr = sampleRate;
            const int maxDelay = lines[0].size() - 8;
            const float targetLength = juce::jlimit (8.0f, (float) maxDelay / (s.mode == Params::Wide ? 1.5f : 1.0f) - 400.0f,
                                                     s.delaySeconds * sr / s.speed);
            if (! started)
            {
                length = targetLength;
                sendS = s.send; fbS = s.feedback; mixS = s.mix; outS = s.outputGain; widthS = s.width; springS = s.springMix;
                speedS = s.speed;
                started = true;
            }

            // Filter der Schleife: Resonanz bis kurz vor dem Pfeifen
            const float q = 0.707f + s.resonance * 2.6f;
            for (int c = 0; c < 2; ++c)
            {
                hp[(size_t) c].set (sr, s.lowCut, 0.707f);
                lp[(size_t) c].set (sr, juce::jmax (s.highCut, s.lowCut * 1.2f), q);
            }
            const float ageCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * (18000.0f - 13000.0f * s.age) / sr);
            tank.setup (s.springDecay, s.springTone, s.springBoing);

            const float g = 0.7f + s.drive * 3.3f;
            const float bias = 0.12f * s.drive;
            const float hiss = s.age * 0.0009f;

            // Gleiten: wie schnell das Band einer neuen Zeit oder einem Tempo-Sprung folgt
            const float glideTau = 0.03f * std::pow (60.0f, s.glide);
            const float glideCoef = 1.0f - std::exp (-1.0f / (glideTau * sr));
            const float smooth = 1.0f - std::exp (-1.0f / (0.03f * sr));

            const float sendTarget = s.freeze ? 0.0f : s.throwOn ? 1.4f : s.send;
            const float fbTarget = s.freeze ? 1.0f : s.feedback;
            const float wowDepth = s.wow * 0.0026f * sr;
            const float flutterDepth = s.flutter * 0.00009f * sr;
            const double wowInc = 0.55 / sr, flutterInc = 6.4 / sr, flutterInc2 = 11.3 / sr;
            const float envAttack = 1.0f - std::exp (-1.0f / (0.004f * sr));
            const float envRelease = 1.0f - std::exp (-1.0f / (0.25f * sr));

            float peakIn = 0.0f, peakOut = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float inL = left[i];
                const float inR = right != nullptr ? right[i] : inL;
                peakIn = juce::jmax (peakIn, std::abs (inL), std::abs (inR));

                // geglättete Regler
                sendS += (sendTarget - sendS) * smooth;
                fbS += (fbTarget - fbS) * smooth;
                mixS += (s.mix - mixS) * smooth;
                outS += (s.outputGain - outS) * smooth;
                widthS += (s.width - widthS) * smooth;
                springS += (s.springMix - springS) * smooth;
                speedS += (s.speed - speedS) * glideCoef;

                // Bandlänge folgt träge; der Lesekopf wird dabei schneller oder langsamer (Tonhöhe rutscht)
                const float step = juce::jlimit (-0.5f, 0.5f, (targetLength - length) * glideCoef);
                length += step;

                // Wow (langsam, mit Zufallsdrift) und Flutter (schnell)
                wowPhase += wowInc; if (wowPhase >= 1.0) wowPhase -= 1.0;
                flutterPhase += flutterInc; if (flutterPhase >= 1.0) flutterPhase -= 1.0;
                flutterPhase2 += flutterInc2; if (flutterPhase2 >= 1.0) flutterPhase2 -= 1.0;
                if (--driftCounter <= 0)
                {
                    driftTarget = random.nextFloat() * 2.0f - 1.0f;
                    driftCounter = (int) (sr * 0.4f);
                }
                drift += (driftTarget - drift) * (2.0f / sr);
                const float twoPi = juce::MathConstants<float>::twoPi;
                const float wowL = (float) std::sin (twoPi * wowPhase) * 0.7f + drift * 0.6f;
                const float wowR = (float) std::sin (twoPi * wowPhase + 0.9f) * 0.7f + drift * 0.6f;
                const float fl = (float) std::sin (twoPi * flutterPhase) + 0.45f * (float) std::sin (twoPi * flutterPhase2);
                const float modL = wowDepth * (wowL + 1.3f) + flutterDepth * (fl + 1.5f);
                const float modR = wowDepth * (wowR + 1.3f) + flutterDepth * (fl * 0.9f + 1.5f);

                auto rd = [&] (int c, float d) { return lines[(size_t) c].read (juce::jlimit (4.0f, (float) maxDelay, d)); };

                float wetL, wetR, fbL, fbR;
                switch (s.mode)
                {
                    case Params::ThreeHeads:
                    {
                        const float l1 = rd (0, length / 3.0f + modL), l2 = rd (0, length * 2.0f / 3.0f + modL), l3 = rd (0, length + modL);
                        const float r1 = rd (1, length / 3.0f + modR), r2 = rd (1, length * 2.0f / 3.0f + modR), r3 = rd (1, length + modR);
                        wetL = (0.5f * l1 + 0.7f * l2 + l3) * 0.6f;
                        wetR = (0.5f * r1 + 0.7f * r2 + r3) * 0.6f;
                        fbL = (0.35f * l1 + 0.45f * l2 + l3) / 1.8f;
                        fbR = (0.35f * r1 + 0.45f * r2 + r3) / 1.8f;
                        break;
                    }
                    case Params::Wide:
                        wetL = fbL = rd (0, length + modL);
                        wetR = fbR = rd (1, length * 1.5f + modR);
                        break;
                    default:
                        wetL = fbL = rd (0, length + modL);
                        wetR = fbR = rd (1, length + modR);
                        break;
                }

                // Federhall
                float revInL = wetL, revInR = wetR;
                if (s.springPlace == Params::OnAll) { revInL += inL; revInR += inR; }
                float revL, revR;
                tank.process (revInL, revInR, revL, revR);
                revL *= 1.6f; revR *= 1.6f;
                if (s.springPlace == Params::InLoop)
                {
                    // Überblenden statt Addieren: die Schleife bleibt so stabil wie ohne Hall
                    const float k = 0.6f * springS, norm = tank.loopNorm() / 1.6f;
                    fbL = fbL * (1.0f - k) + revL * norm * k;
                    fbR = fbR * (1.0f - k) + revR * norm * k;
                }

                // Schleife: Low Cut, High Cut mit Resonanz, Alterung
                auto loop = [&] (int c, float x)
                {
                    x = hp[(size_t) c].high (x);
                    x = lp[(size_t) c].low (x);
                    auto& a = ageLp[(size_t) c];
                    a += ageCoef * (x - a);
                    return a;
                };
                const float loopL = loop (0, fbL) * fbS;
                const float loopR = loop (1, fbR) * fbS;

                // Aufnahmekopf: Eingang + Rückkopplung + Bandrauschen, durch die Sättigung aufs Band
                const float noiseL = (random.nextFloat() * 2.0f - 1.0f) * hiss;
                const float noiseR = (random.nextFloat() * 2.0f - 1.0f) * hiss;
                if (s.mode == Params::PingPong)
                {
                    const float mono = 0.5f * (inL + inR);
                    lines[0].push (tape (mono * sendS + loopR + noiseL, g, bias));
                    lines[1].push (tape (loopL + noiseR, g, bias));
                }
                else
                {
                    lines[0].push (tape (inL * sendS + loopL + noiseL, g, bias));
                    lines[1].push (tape (inR * sendS + loopR + noiseR, g, bias));
                }

                if (s.springPlace != Params::OnAll)
                {
                    wetL += springS * revL;
                    wetR += springS * revR;
                }

                // Stereobreite der Echos
                const float mid = 0.5f * (wetL + wetR), side = 0.5f * (wetL - wetR) * widthS;
                wetL = mid + side;
                wetR = mid - side;

                // Ducking: Echos weichen dem trockenen Signal aus
                const float level = juce::jmax (std::abs (inL), std::abs (inR));
                env += (level > env ? envAttack : envRelease) * (level - env);
                const float duck = 1.0f / (1.0f + s.ducking * 14.0f * env);

                const float dry = juce::jmin (1.0f, 2.0f * (1.0f - mixS));
                const float wet = juce::jmin (1.0f, 2.0f * mixS) * duck;
                float outL = inL * dry + wetL * wet;
                float outR = inR * dry + wetR * wet;
                if (s.springPlace == Params::OnAll)
                {
                    outL += springS * revL;
                    outR += springS * revR;
                }
                outL *= outS;
                outR *= outS;

                left[i] = outL;
                if (right != nullptr)
                    right[i] = outR;
                peakOut = juce::jmax (peakOut, std::abs (outL), std::abs (outR));

                tapeTravel += speedS / sr;
            }

            // Für die Anzeige
            inLevel.store (juce::jmax (peakIn, inLevel.load() * 0.85f));
            outLevel.store (juce::jmax (peakOut, outLevel.load() * 0.85f));
            shownMs.store (length / sr * 1000.0f);
            tapePosition.store ((float) std::fmod (tapeTravel, 1000.0));
        }

        std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, shownMs { 0.0f }, tapePosition { 0.0f };

    private:
        float sampleRate = 44100.0f;
        std::array<Line, 2> lines;
        SpringTank tank;
        std::array<Svf, 2> hp, lp;
        std::array<float, 2> ageLp {};
        bool started = false;
        float length = 1000.0f, env = 0.0f;
        float sendS = 1, fbS = 0, mixS = 0, outS = 1, widthS = 1, springS = 0, speedS = 1;
        double wowPhase = 0, flutterPhase = 0, flutterPhase2 = 0, tapeTravel = 0;
        float drift = 0, driftTarget = 0;
        int driftCounter = 0;
        juce::Random random { 1234 };
    };
}
