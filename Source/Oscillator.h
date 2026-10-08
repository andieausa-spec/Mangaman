#pragma once

#include "Parameters.h"

// Digitaler Oszillator mit mehreren Modi. Jeder Modus wird wie beim MicroFreak
// über die drei Regler Wave, Timbre und Shape gesteuert.
class Oscillator
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        ks.assign ((size_t) (sr / 15.0) + 8, 0.0f);
    }

    void start (int newType, float startWave)
    {
        phase = subPhase = modPhase = 0.0;
        fmPrev = 0.0f;
        noiseLp = holdValue = 0.0f;
        holdCount = 0;
        for (auto& p : superPhases)
            p = rng.nextDouble();

        if (newType == Params::KarplusStrong)
            excite (startWave);
    }

    // Einmal pro Steuerblock aufrufen (teure Berechnungen hier, nicht pro Sample)
    void setParams (int newType, double freq, float w, float t, float s)
    {
        type = newType;
        dt = freq / sampleRate;
        wave = w; timbre = t; shape = s;

        switch (type)
        {
            case Params::Superwave:
            {
                static constexpr double offsets[] { -1.0, -0.62, -0.3, 0.0, 0.28, 0.6, 1.0 };
                for (int i = 0; i < 7; ++i)
                    superDt[i] = dt * std::pow (2.0, offsets[i] * timbre * 70.0 / 1200.0);
                break;
            }
            case Params::TwoOpFM:
            {
                static constexpr double ratios[] { 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0 };
                fmRatio = ratios[juce::jlimit (0, 9, juce::roundToInt (wave * 9.0f))];
                break;
            }
            case Params::Harmonic:
            {
                float sum = 0.0f;
                const float centre = 1.0f + shape * 15.0f;
                for (int k = 1; k <= numPartials; ++k)
                {
                    float a = 0.0f;
                    if (k * dt < 0.48)
                    {
                        a = 1.0f / std::pow ((float) k, 2.5f - 2.2f * wave);
                        if (k % 2 == 0) a *= 1.0f - timbre;
                        if (shape > 0.01f) a *= 1.0f + 4.0f * std::exp (-(k - centre) * (k - centre) / 3.0f);
                    }
                    partials[k - 1] = a;
                    sum += a;
                }
                const float norm = sum > 0.0f ? 1.0f / sum : 0.0f;
                for (auto& a : partials) a *= norm;
                break;
            }
            case Params::KarplusStrong:
                ksDelay = juce::jlimit (2.0, (double) ks.size() - 3.0, sampleRate / freq);
                ksFeedback = 0.97f + 0.0299f * timbre;
                ksBright = 0.25f + 0.75f * shape;
                break;
            default:
                break;
        }
    }

    float next()
    {
        float out = 0.0f;

        switch (type)
        {
            case Params::BasicWaves:
            {
                const float pw = 0.5f - 0.45f * timbre;
                const float main = basic (phase, dt, wave, pw);
                const float sub = square (subPhase, dt * 0.5, 0.5f);
                out = (main + shape * sub) / (1.0f + 0.5f * shape);
                advance (subPhase, dt * 0.5);
                break;
            }
            case Params::Superwave:
            {
                for (int i = 0; i < 7; ++i)
                {
                    out += basic (superPhases[i], superDt[i], wave, 0.5f) * (i == 3 ? 1.0f : shape);
                    advance (superPhases[i], superDt[i]);
                }
                out /= 1.0f + 6.0f * shape * 0.45f;
                break;
            }
            case Params::TwoOpFM:
            {
                const float m = (float) std::sin (twoPi * modPhase + shape * 0.8f * juce::MathConstants<float>::pi * fmPrev);
                fmPrev = m;
                out = (float) std::sin (twoPi * phase + timbre * 8.0f * m);
                advance (modPhase, dt * fmRatio);
                break;
            }
            case Params::Harmonic:
            {
                for (int k = 0; k < numPartials; ++k)
                    if (partials[k] > 0.0f)
                        out += partials[k] * (float) std::sin (twoPi * std::fmod ((k + 1) * phase, 1.0));
                out *= 1.5f;
                break;
            }
            case Params::KarplusStrong:
            {
                const auto size = (int) ks.size();
                double readPos = ksWrite - ksDelay;
                if (readPos < 0) readPos += size;
                const int i0 = (int) readPos;
                const int i1 = (i0 + 1) % size;
                const float frac = (float) (readPos - i0);
                const float y = ks[(size_t) i0] + frac * (ks[(size_t) i1] - ks[(size_t) i0]);
                ksLp += (y - ksLp) * ksBright;
                out = ksLp * ksFeedback;
                ks[(size_t) ksWrite] = out;
                ksWrite = (ksWrite + 1) % size;
                break;
            }
            case Params::Waveshaper:
            {
                const float base = (float) std::sin (twoPi * phase) * (1.0f - wave) + triangle (phase) * wave;
                const float x = base * (1.0f + timbre * 9.0f) + shape * 1.5f;
                out = std::sin (juce::MathConstants<float>::halfPi * x);
                break;
            }
            case Params::Noise:
            default:
            {
                const float white = rng.nextFloat() * 2.0f - 1.0f;
                noiseLp += (white - noiseLp) * (0.02f + 0.98f * wave * wave);
                const int holdLen = 1 + (int) (timbre * timbre * 63.0f);
                if (++holdCount >= holdLen) { holdCount = 0; holdValue = noiseLp; }
                const float ring = holdValue * (float) std::sin (twoPi * phase) * 1.4f;
                out = holdValue * (1.0f - shape) + ring * shape;
                break;
            }
        }

        advance (phase, dt);
        return out;
    }

private:
    static constexpr double twoPi = juce::MathConstants<double>::twoPi;
    static constexpr int numPartials = 24;

    static void advance (double& p, double inc) { p += inc; if (p >= 1.0) p -= 1.0; }

    // PolyBLEP gegen Aliasing an Sprungstellen
    static float blep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    static float saw (double t, double dt)      { return (float) (2.0 * t - 1.0) - blep (t, dt); }
    static float triangle (double t)            { return (float) (1.0 - 4.0 * std::abs (t - 0.5)); }
    static float square (double t, double dt, float pw)
    {
        double t2 = t - pw;
        if (t2 < 0.0) t2 += 1.0;
        return (t < pw ? 1.0f : -1.0f) + blep (t, dt) - blep (t2, dt);
    }

    // Wave blendet Saegezahn -> Rechteck -> Dreieck
    static float basic (double t, double dt, float w, float pw)
    {
        if (w < 0.5f)
        {
            const float a = w * 2.0f;
            return saw (t, dt) * (1.0f - a) + square (t, dt, pw) * a;
        }
        const float a = (w - 0.5f) * 2.0f;
        return square (t, dt, pw) * (1.0f - a) + triangle (t) * a;
    }

    void excite (float color)
    {
        const float c = 0.05f + 0.95f * color;
        const float norm = 0.5f / std::sqrt (c / (2.0f - c));
        float lp = 0.0f;
        for (auto& s : ks)
        {
            lp += (rng.nextFloat() * 2.0f - 1.0f - lp) * c;
            s = lp * norm;
        }
        ksWrite = 0;
        ksLp = 0.0f;
    }

    double sampleRate = 44100.0, dt = 0.0;
    int type = 0;
    float wave = 0, timbre = 0, shape = 0;

    double phase = 0, subPhase = 0, modPhase = 0;
    double superPhases[7] {}, superDt[7] {};
    double fmRatio = 1.0;
    float fmPrev = 0;
    float partials[numPartials] {};
    std::vector<float> ks;
    int ksWrite = 0;
    double ksDelay = 100.0;
    float ksFeedback = 0.99f, ksBright = 0.5f, ksLp = 0.0f;
    float noiseLp = 0, holdValue = 0;
    int holdCount = 0;
    juce::Random rng;
};
