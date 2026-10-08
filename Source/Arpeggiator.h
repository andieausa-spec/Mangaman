#pragma once

#include "Parameters.h"

// Arpeggiator: ersetzt gehaltene Noten durch eine taktsynchrone Notenfolge
class Arpeggiator
{
public:
    struct Settings
    {
        bool enabled = false;
        int mode = 0, rate = 3, octaves = 1;
        bool hold = false;
        double bpm = 120.0;
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        notes.clear();
        pressed.clear();
        currentNote = -1;
        wasEnabled = false;
    }

    void process (juce::MidiBuffer& midi, int numSamples, const Settings& s)
    {
        if (! s.enabled)
        {
            if (wasEnabled)
            {
                if (currentNote >= 0)
                    midi.addEvent (juce::MidiMessage::noteOff (1, currentNote), 0);
                currentNote = -1;
                notes.clear();
                pressed.clear();
                wasEnabled = false;
            }
            return;
        }

        juce::MidiBuffer out;
        if (! wasEnabled)
        {
            out.addEvent (juce::MidiMessage::allNotesOff (1), 0);
            wasEnabled = true;
        }

        if (! s.hold && wasHold)
            for (int i = notes.size(); --i >= 0;)
                if (! pressed.contains (notes[i]))
                    notes.remove (i);
        wasHold = s.hold;

        stepLength = sampleRate * 60.0 / s.bpm * Params::arpBeats[juce::jlimit (0, 5, s.rate)];
        settings = s;

        int pos = 0;
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            advance (out, pos, meta.samplePosition);
            pos = meta.samplePosition;

            if (m.isNoteOn())
            {
                if (s.hold && pressed.isEmpty())
                    notes.clear();
                if (notes.isEmpty())
                    samplesToStep = 0.0;          // erste Note sofort spielen
                pressed.addIfNotAlreadyThere (m.getNoteNumber());
                notes.addIfNotAlreadyThere (m.getNoteNumber());
                velocity = m.getFloatVelocity();
            }
            else if (m.isNoteOff())
            {
                pressed.removeFirstMatchingValue (m.getNoteNumber());
                if (! s.hold)
                    notes.removeFirstMatchingValue (m.getNoteNumber());
            }
            else
            {
                out.addEvent (m, meta.samplePosition);
            }
        }
        advance (out, pos, numSamples);
        midi.swapWith (out);
    }

private:
    void advance (juce::MidiBuffer& out, int from, int to)
    {
        double pos = from;
        while (true)
        {
            const double offAt  = currentNote >= 0 ? pos + noteOffIn : 1.0e12;
            const double stepAt = notes.isEmpty() ? 1.0e12 : pos + samplesToStep;
            const double t = juce::jmin (offAt, stepAt);

            if (t >= to)
            {
                noteOffIn -= to - pos;
                samplesToStep = juce::jmax (0.0, samplesToStep - (to - pos));
                return;
            }

            noteOffIn -= t - pos;
            samplesToStep -= t - pos;
            pos = t;
            const int sample = (int) pos;

            if (currentNote >= 0 && (noteOffIn <= 0.0 || stepAt <= offAt))
            {
                out.addEvent (juce::MidiMessage::noteOff (1, currentNote), sample);
                currentNote = -1;
            }

            if (stepAt <= t && ! notes.isEmpty())
            {
                currentNote = nextNote();
                out.addEvent (juce::MidiMessage::noteOn (1, currentNote, velocity), sample);
                noteOffIn = stepLength * 0.5;
                samplesToStep = stepLength;
            }
        }
    }

    int nextNote()
    {
        juce::Array<int> base (notes);
        if (settings.mode != 3)
            base.sort();

        juce::Array<int> seq;
        for (int o = 0; o < settings.octaves; ++o)
            for (int n : base)
                if (n + 12 * o < 128)
                    seq.add (n + 12 * o);

        if (settings.mode == 1)                      // Down
        {
            std::reverse (seq.begin(), seq.end());
        }
        else if (settings.mode == 2 && seq.size() > 2)  // Up/Down
        {
            for (int i = seq.size() - 2; i > 0; --i)
                seq.add (seq[i]);
        }

        if (seq.isEmpty())
            return notes.getFirst();

        if (settings.mode == 4)                      // Random
            return seq[rng.nextInt (seq.size())];

        return seq[step++ % seq.size()];
    }

    double sampleRate = 44100.0, stepLength = 10000.0;
    double samplesToStep = 0.0, noteOffIn = 0.0;
    juce::Array<int> notes, pressed;
    int currentNote = -1, step = 0;
    float velocity = 0.8f;
    bool wasEnabled = false, wasHold = false;
    Settings settings;
    juce::Random rng;
};
