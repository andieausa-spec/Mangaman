#pragma once

#include "Oscillator.h"

// Zustandsvariablen-Filter (TPT) mit leichter Eingangssättigung, ähnlich dem SEM-Filter
class SvfFilter
{
public:
    void reset() { ic1 = ic2 = 0.0f; }

    void setParams (int newType, float cutoff, float reso, double sr)
    {
        type = newType;
        const float g = std::tan (juce::MathConstants<float>::pi * cutoff / (float) sr);
        k = 2.0f - 1.96f * reso;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    float process (float x)
    {
        x = std::tanh (x * 1.2f);
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (type == 1) return v1;                 // Bandpass
        if (type == 2) return x - k * v1 - v2;    // Highpass
        return v2;                                // Lowpass
    }

private:
    int type = 0;
    float k = 2.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
};

// Zyklische Hüllkurve: Rise -> Hold -> Fall, je nach Modus einmalig oder als Schleife
class CyclingEnvelope
{
public:
    void trigger (int mode)
    {
        if (mode == Params::CycRun && stage != Idle)
            return;                    // Run läuft frei weiter
        value = 0.0f;
        stage = Rise;
    }

    float next (const Params::Snapshot& p, double sr)
    {
        switch (stage)
        {
            case Rise:
                value += 1.0f / (float) (p.cycRise * sr);
                if (value >= 1.0f) { value = 1.0f; stage = Hold; holdLeft = p.cycHold * (float) sr; }
                break;
            case Hold:
                if (--holdLeft <= 0.0f) stage = Fall;
                break;
            case Fall:
                value -= 1.0f / (float) (p.cycFall * sr);
                if (value <= 0.0f) { value = 0.0f; stage = p.cycMode == Params::CycEnv ? Idle : Rise; }
                break;
            case Idle:
            default:
                if (p.cycMode == Params::CycRun) stage = Rise;
                break;
        }
        return value;
    }

    float current() const { return value; }

private:
    enum Stage { Idle, Rise, Hold, Fall };
    Stage stage = Idle;
    float value = 0.0f, holdLeft = 0.0f;
};

// Was alle Stimmen pro Steuerblock gemeinsam haben
struct VoiceContext
{
    float lfo = 0.0f;
    float bendSemis = 0.0f;
    float pressure = 0.0f;
    float seq[Params::seqRows] {};   // Sequenzer-Reihen A bis C
};

class Voice
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        osc.prepare (sr);
        adsr.setSampleRate (sr);
    }

    void noteOn (int newNote, float velocity, float glideFrom, const Params::Snapshot& p, juce::uint64 newStamp)
    {
        note = newNote;
        vel = velocity;
        stamp = newStamp;
        currentNote = p.glide > 0.0f && glideFrom >= 0.0f ? glideFrom : (float) newNote;
        gate = true;

        osc.start (p.oscType, p.wave);
        filter.reset();
        cyc.trigger (p.cycMode);
        setAdsr (p);
        adsr.reset();
        adsr.noteOn();
    }

    // Legato: Tonhöhe ändern ohne Hüllkurven neu zu starten
    void glideTo (int newNote, juce::uint64 newStamp)
    {
        note = newNote;
        stamp = newStamp;
    }

    void noteOff()
    {
        gate = false;
        adsr.noteOff();
    }

    void kill()
    {
        gate = false;
        adsr.reset();
    }

    bool isActive() const  { return adsr.isActive(); }
    bool isGateOn() const  { return gate; }
    int getNote() const    { return note; }
    juce::uint64 getStamp() const { return stamp; }

    void render (float* out, int numSamples, const Params::Snapshot& p, const VoiceContext& ctx)
    {
        using namespace Params;

        const float src[numSources] { cyc.current() * p.cycAmount, envValue, ctx.lfo, ctx.pressure,
                                      ((float) note - 60.0f) / 48.0f, ctx.seq[0], ctx.seq[1], ctx.seq[2] };
        float m[numDests] {};
        for (int s = 0; s < numSources; ++s)
            for (int d = 0; d < numDests; ++d)
                m[d] += src[s] * p.mod[s][d];

        // Glide
        const float glideTime = p.glide * p.glide * 2.0f;
        if (glideTime > 0.0001f)
            currentNote = (float) note + (currentNote - (float) note)
                          * std::exp (-(float) numSamples / (glideTime * (float) sampleRate));
        else
            currentNote = (float) note;

        const float pitch = currentNote + ctx.bendSemis + m[DPitch] * 24.0f;
        const double freq = juce::jlimit (1.0, sampleRate * 0.45, 440.0 * std::pow (2.0, (pitch - 69.0) / 12.0));

        osc.setParams (p.oscType, freq,
                       juce::jlimit (0.0f, 1.0f, p.wave + m[DWave]),
                       juce::jlimit (0.0f, 1.0f, p.timbre + m[DTimbre]),
                       juce::jlimit (0.0f, 1.0f, p.shape + m[DShape]));

        const float cutoff = juce::jlimit (20.0f, juce::jmin (20000.0f, (float) sampleRate * 0.45f),
                                           p.cutoff * std::exp2 (m[DCutoff] * 5.0f + p.filterEnv * envValue * 7.0f));
        filter.setParams (p.filterType, cutoff, juce::jlimit (0.0f, 1.0f, p.reso + m[DReso]), sampleRate);

        setAdsr (p);
        const float targetAmp = juce::jlimit (0.0f, 2.0f, 1.0f + m[DAmp]) * (0.3f + 0.7f * vel) * 0.25f;
        const float ampStep = (targetAmp - amp) / (float) numSamples;

        for (int i = 0; i < numSamples; ++i)
        {
            float x = filter.process (osc.next());

            // DC-Blocker
            const float y = x - dcX + 0.995f * dcY;
            dcX = x; dcY = y;

            envValue = adsr.getNextSample();
            amp += ampStep;
            out[i] += y * envValue * amp;
            cyc.next (p, sampleRate);
        }
        amp = targetAmp;
    }

private:
    void setAdsr (const Params::Snapshot& p)
    {
        adsr.setParameters ({ p.attack, p.decay, p.sustain, p.decay });
    }

    double sampleRate = 44100.0;
    Oscillator osc;
    SvfFilter filter;
    juce::ADSR adsr;
    CyclingEnvelope cyc;

    int note = 60;
    float vel = 1.0f, currentNote = 60.0f, envValue = 0.0f, amp = 0.0f;
    float dcX = 0.0f, dcY = 0.0f;
    bool gate = false;
    juce::uint64 stamp = 0;
};
