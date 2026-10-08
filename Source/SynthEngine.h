#pragma once

#include "Voice.h"
#include "Sequencer.h"

// Stimmenverwaltung (Poly / Mono / Legato), globaler LFO und MIDI-Verarbeitung
class SynthEngine
{
public:
    static constexpr int maxVoices = 8;
    static constexpr int controlBlock = 16;   // Modulation wird alle 16 Samples neu berechnet

    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& v : voices)
            v.prepare (sr);
        allNotesOff (true);
    }

    void process (float* out, int numSamples, const juce::MidiBuffer& midi, const Params::Snapshot& p, double bpm,
                  StepSequencer& seq)
    {
        if (p.voiceMode != lastVoiceMode)
        {
            allNotesOff (false);
            lastVoiceMode = p.voiceMode;
        }

        auto it = midi.cbegin();
        int pos = 0;

        while (pos < numSamples)
        {
            while (it != midi.cend() && (*it).samplePosition <= pos)
            {
                handleMidi ((*it).getMessage(), p);
                ++it;
            }

            const int nextEvent = it != midi.cend() ? (*it).samplePosition : numSamples;
            const int n = juce::jmin (controlBlock, nextEvent - pos, numSamples - pos);

            VoiceContext ctx;
            ctx.lfo = advanceLfo (p, n, bpm);
            ctx.bendSemis = bend * 2.0f;
            ctx.pressure = juce::jmax (aftertouch, modWheel);
            seq.rowsFor (pos, n, ctx.seq);

            for (auto& v : voices)
                if (v.isActive())
                    v.render (out + pos, n, p, ctx);

            pos += n;
        }

        for (; it != midi.cend(); ++it)
            handleMidi ((*it).getMessage(), p);
    }

private:
    void handleMidi (const juce::MidiMessage& m, const Params::Snapshot& p)
    {
        if (m.isNoteOn())
            noteOn (m.getNoteNumber(), m.getFloatVelocity(), p);
        else if (m.isNoteOff())
            noteOff (m.getNoteNumber(), p);
        else if (m.isPitchWheel())
            bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        else if (m.isChannelPressure())
            aftertouch = (float) m.getChannelPressureValue() / 127.0f;
        else if (m.isAftertouch())
            aftertouch = (float) m.getAfterTouchValue() / 127.0f;
        else if (m.isControllerOfType (1))
            modWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.isSustainPedalOn())
            sustainPedal = true;
        else if (m.isSustainPedalOff())
        {
            sustainPedal = false;
            for (int n = 0; n < 128; ++n)
                if (sustained[(size_t) n])
                {
                    sustained[(size_t) n] = false;
                    noteOff (n, p);
                }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            allNotesOff (m.isAllSoundOff());
    }

    void noteOn (int note, float vel, const Params::Snapshot& p)
    {
        sustained[(size_t) note] = false;

        if (p.voiceMode == Params::Poly)
        {
            findVoice (note).noteOn (note, vel, lastNote, p, ++stampCounter);
        }
        else
        {
            monoStack.removeFirstMatchingValue (note);
            monoStack.add (note);
            auto& v = voices[0];
            if (p.voiceMode == Params::Legato && v.isGateOn())
                v.glideTo (note, ++stampCounter);
            else
                v.noteOn (note, vel, lastNote, p, ++stampCounter);
            monoVelocity = vel;
        }
        lastNote = (float) note;
    }

    void noteOff (int note, const Params::Snapshot& p)
    {
        if (sustainPedal)
        {
            sustained[(size_t) note] = true;
            return;
        }

        if (p.voiceMode == Params::Poly)
        {
            for (auto& v : voices)
                if (v.isGateOn() && v.getNote() == note)
                    v.noteOff();
            return;
        }

        monoStack.removeFirstMatchingValue (note);
        auto& v = voices[0];
        if (! v.isGateOn() || v.getNote() != note)
            return;

        if (monoStack.isEmpty())
        {
            v.noteOff();
        }
        else
        {
            const int previous = monoStack.getLast();
            if (p.voiceMode == Params::Legato)
                v.glideTo (previous, ++stampCounter);
            else
                v.noteOn (previous, monoVelocity, lastNote, p, ++stampCounter);
            lastNote = (float) previous;
        }
    }

    Voice& findVoice (int note)
    {
        for (auto& v : voices)                       // gleiche Note neu anschlagen
            if (v.isActive() && v.getNote() == note)
                return v;
        for (auto& v : voices)                       // freie Stimme
            if (! v.isActive())
                return v;

        Voice* oldest = nullptr;                     // älteste ausklingende, sonst älteste
        for (auto& v : voices)
            if (! v.isGateOn() && (oldest == nullptr || v.getStamp() < oldest->getStamp()))
                oldest = &v;
        if (oldest == nullptr)
            for (auto& v : voices)
                if (oldest == nullptr || v.getStamp() < oldest->getStamp())
                    oldest = &v;
        return *oldest;
    }

    void allNotesOff (bool hard)
    {
        for (auto& v : voices)
            hard ? v.kill() : v.noteOff();
        monoStack.clear();
        sustained.fill (false);
    }

    float advanceLfo (const Params::Snapshot& p, int numSamples, double bpm)
    {
        const double rate = p.lfoSync ? (bpm / 60.0) / Params::syncBeats[juce::jlimit (0, 5, p.lfoDiv)]
                                      : (double) p.lfoRate;
        const double t = lfoPhase;

        float value = 0.0f;
        switch (p.lfoShape)
        {
            case 0:  value = (float) std::sin (juce::MathConstants<double>::twoPi * t); break;
            case 1:  value = (float) (1.0 - 4.0 * std::abs (t - 0.5)); break;
            case 2:  value = (float) (2.0 * t - 1.0); break;
            case 3:  value = t < 0.5 ? 1.0f : -1.0f; break;
            case 4:  value = shTarget; break;
            default: value = shPrevious + (shTarget - shPrevious) * (float) t; break;
        }

        lfoPhase += rate * numSamples / sampleRate;
        if (lfoPhase >= 1.0)
        {
            lfoPhase -= std::floor (lfoPhase);
            shPrevious = shTarget;
            shTarget = rng.nextFloat() * 2.0f - 1.0f;
        }
        return value;
    }

    double sampleRate = 44100.0;
    std::array<Voice, maxVoices> voices;
    juce::Array<int> monoStack;
    std::array<bool, 128> sustained {};
    bool sustainPedal = false;
    float monoVelocity = 1.0f, lastNote = -1.0f;
    float bend = 0.0f, aftertouch = 0.0f, modWheel = 0.0f;
    double lfoPhase = 0.0;
    float shPrevious = 0.0f, shTarget = 0.0f;
    int lastVoiceMode = Params::Poly;
    juce::uint64 stampCounter = 0;
    juce::Random rng;
};
