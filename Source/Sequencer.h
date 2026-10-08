#pragma once

#include "Parameters.h"

// Step-Sequenzer nach Art von Doepfer SEQ / Dark Time:
// 16 Schritte, oberste Reihe spielt die Tonhöhe, die Reihen A bis C sind Quellen in der Mod-Matrix.
// Läuft der Sequenzer, transponieren gespielte Tasten die Folge (Grundton = keine Verschiebung).
class StepSequencer
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        currentNote = -1;
        pos = -1;
        wasRunning = false;
        out[0] = out[1] = out[2] = 0.0f;
        transposeKeys.fill (false);
        playingStep.store (-1);
    }

    // Ersetzt bzw. ergänzt die MIDI-Noten des Blocks. hostStarted: Transport des Hosts ist gerade angelaufen.
    void process (juce::MidiBuffer& midi, int numSamples, const Params::SeqSnapshot& s, double bpm, bool hostStarted)
    {
        settings = s;
        numEvents = 0;
        stepAtStart = s.run ? pos : -1;

        if (s.rec && ! wasRecording)
            recordStep = 0;
        wasRecording = s.rec;

        if (! s.run)
        {
            if (wasRunning)
            {
                if (currentNote >= 0)
                    midi.addEvent (juce::MidiMessage::noteOff (1, currentNote), 0);
                currentNote = -1;
                pos = -1;
                stepAtStart = -1;
                transposeKeys.fill (false);
                wasRunning = false;
                playingStep.store (-1);
            }
            if (s.rec)
                for (const auto meta : midi)
                    if (meta.getMessage().isNoteOn())
                        record (meta.getMessage().getNoteNumber());
            return;
        }

        if (! wasRunning || hostStarted)
            restart();
        wasRunning = true;

        beatLength = sampleRate * 60.0 / bpm * Params::arpBeats[juce::jlimit (0, 5, s.rate)];

        juce::MidiBuffer result;
        int at = 0;
        for (const auto meta : midi)
        {
            advance (result, at, meta.samplePosition);
            at = meta.samplePosition;

            const auto m = meta.getMessage();
            const int n = m.getNoteNumber();
            if (m.isNoteOn())
            {
                if (s.rec)
                {
                    record (n);
                    result.addEvent (m, at);
                }
                else if (s.trans)
                {
                    transpose = n - s.root;
                    transposeKeys[(size_t) n] = true;
                }
                else
                {
                    result.addEvent (m, at);
                }
            }
            else if (m.isNoteOff() && transposeKeys[(size_t) n])
            {
                transposeKeys[(size_t) n] = false;   // Transponier-Tasten erzeugen keinen Ton
            }
            else
            {
                result.addEvent (m, at);
            }
        }
        advance (result, at, numSamples);
        midi.swapWith (result);
    }

    // Geglättete Werte der Reihen A bis C für den Steuerblock ab Sample 'at'
    void rowsFor (int at, int numSamples, float* dest)
    {
        int step = stepAtStart;
        for (int e = 0; e < numEvents; ++e)
            if (events[e].sample <= at)
                step = events[e].step;

        const float tau = settings.slew * settings.slew * 0.6f + 0.0015f;
        const float a = 1.0f - std::exp (-(float) numSamples / (tau * (float) sampleRate));
        for (int r = 0; r < Params::seqRows; ++r)
        {
            const float target = settings.run && step >= 0 ? settings.rows[r][step] : 0.0f;
            out[r] += (target - out[r]) * a;
            dest[r] = out[r];
        }
    }

    // Für die Oberfläche: gerade spielender Schritt (-1 = gestoppt)
    std::atomic<int> playingStep { -1 };

    // Aufgenommene Schritte (Schritt, Halbtöne zum Grundton) für den Message-Thread
    struct Recorded { int step, semis; };
    bool popRecorded (Recorded& r)
    {
        int start1, size1, start2, size2;
        recordFifo.prepareToRead (1, start1, size1, start2, size2);
        if (size1 == 0)
            return false;
        r = recordBuffer[(size_t) start1];
        recordFifo.finishedRead (1);
        return true;
    }

private:
    void restart()
    {
        pos = -1;
        stepAtStart = -1;
        clock = 0.0;
        offIn = -1.0;
        count = 0;
        direction = 1;
        transpose = 0;
    }

    void record (int note)
    {
        int start1, size1, start2, size2;
        recordFifo.prepareToWrite (1, start1, size1, start2, size2);
        if (size1 > 0)
        {
            recordBuffer[(size_t) start1] = { recordStep, juce::jlimit (-24, 24, note - settings.root) };
            recordFifo.finishedWrite (1);
        }
        recordStep = (recordStep + 1) % settings.length;
    }

    int nextStep()
    {
        const int len = settings.length;
        int i = pos;
        switch (settings.dir)
        {
            case 1:   // Rück
                i = i <= 0 || i >= len ? len - 1 : i - 1;
                break;
            case 2:   // Pendel
                if (len == 1) { i = 0; break; }
                i += direction;
                if (i >= len)     { i = len - 2; direction = -1; }
                else if (i < 0)   { i = 1; direction = 1; }
                break;
            case 3:   // Zufall
                i = rng.nextInt (len);
                break;
            default:  // Vor
                i = i + 1 >= len ? 0 : i + 1;
                break;
        }
        return i;
    }

    void advance (juce::MidiBuffer& result, int from, int to)
    {
        double t = from;
        while (true)
        {
            const double stepAt = t + juce::jmax (0.0, clock);
            const double offAt  = currentNote >= 0 && offIn >= 0.0 ? t + offIn : 1.0e12;
            const double next   = juce::jmin (stepAt, offAt);
            if (next >= to)
            {
                clock -= to - t;
                if (offIn >= 0.0) offIn -= to - t;
                return;
            }

            clock -= next - t;
            if (offIn >= 0.0) offIn -= next - t;
            t = next;
            const int sample = (int) t;

            if (offAt <= stepAt)
            {
                result.addEvent (juce::MidiMessage::noteOff (1, currentNote), sample);
                currentNote = -1;
                offIn = -1.0;
                continue;
            }

            // nächster Schritt
            if (currentNote >= 0)
                result.addEvent (juce::MidiMessage::noteOff (1, currentNote), sample);
            currentNote = -1;

            pos = nextStep();
            const double dur = beatLength * (count % 2 == 0 ? 1.0 + settings.swing : 1.0 - settings.swing);
            ++count;
            clock += dur;
            offIn = settings.gate < 0.99f ? dur * settings.gate : -1.0;

            if (settings.on[pos])
            {
                currentNote = juce::jlimit (0, 127, settings.root + settings.pitch[pos] + transpose);
                result.addEvent (juce::MidiMessage::noteOn (1, currentNote, 0.85f), sample);
            }

            if (numEvents < maxEvents)
                events[numEvents++] = { sample, pos };
            playingStep.store (pos);
        }
    }

    struct StepEvent { int sample, step; };
    static constexpr int maxEvents = 64;
    StepEvent events[maxEvents] {};
    int numEvents = 0, stepAtStart = -1;

    Params::SeqSnapshot settings;
    double sampleRate = 44100.0, beatLength = 10000.0, clock = 0.0, offIn = -1.0;
    int pos = -1, count = 0, direction = 1, transpose = 0, currentNote = -1, recordStep = 0;
    bool wasRunning = false, wasRecording = false;
    std::array<bool, 128> transposeKeys {};
    float out[Params::seqRows] {};

    juce::AbstractFifo recordFifo { 64 };
    std::array<Recorded, 64> recordBuffer {};
    juce::Random rng;
};
