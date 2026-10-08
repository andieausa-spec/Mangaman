#pragma once

#include "SynthEngine.h"
#include "Arpeggiator.h"
#include "DrumMachine.h"

class MangamanProcessor : public juce::AudioProcessor,
                          private juce::Timer
{
public:
    MangamanProcessor();
    ~MangamanProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // Gerade spielender Sequenzer-Schritt für die Oberfläche (-1 = gestoppt)
    int getSeqStep() const { return sequencer.playingStep.load(); }

    // Übernimmt per Tastatur aufgenommene Schritte in die Parameter (Message-Thread)
    void applyRecordedSteps();

    // Rhythmus: Muster (Message-Thread schreibt, Audio-Thread liest), gerade spielender Schritt, Vorhören
    int getDrumStep() const { return drums.playingStep.load(); }
    juce::uint32 getDrumHits() const { return drums.playingHits.load(); }
    Rhythm::Pattern getPattern() const;
    void setPattern (const Rhythm::Pattern&);
    void setStep (int track, int step, int value);
    int getPatternVersion() const { return patternVersion.load(); }
    void audition (int track, float velocity) { auditionVel.store (velocity); auditionTrack.store (track); }

    // Muster neu würfeln aus Modell, Stil, Zufall und Dichte
    void regeneratePattern();
    // Beat aus Text: setzt Modell, Stil, Regler und Muster, startet die Drums; liefert, was erkannt wurde
    juce::StringArray applyRhythmText (const juce::String& text);
    juce::String getRhythmText() const { return rhythmText; }
    juce::String getRhythmTag() const;

    // Design der Oberfläche: 0 = 1980 (Schieberegler), 1 = 2100 (Gummipotis); wird mit dem Projekt gespeichert
    std::atomic<int> design { 1 };

private:
    double getHostBpm() const;
    void timerCallback() override;
    void setParam (const juce::String& id, float value);
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void rememberGenerator();

    Params::Reader params { apvts };
    Params::SeqReader seqParams { apvts };
    Params::RhythmReader rhythmParams { apvts };
    DrumMachine drums;
    std::array<std::atomic<juce::uint8>, Drums::numTracks * Drums::numSteps> pattern {};
    std::atomic<int> patternVersion { 0 }, auditionTrack { -1 };
    std::atomic<float> auditionVel { 0.8f };
    juce::AudioBuffer<float> drumBuffer;
    double sampleRateHz = 44100.0, internalPpq = 0.0;
    bool clockWasRunning = false;
    juce::String rhythmText;
    // zuletzt gewürfelt mit (Stil, Zufall, Dichte); ändert sich einer davon, entsteht ein neues Muster
    std::atomic<float> genStyle { 0 }, genRand { 0 }, genDens { 0.5f };
    bool lastSeqRun = false, lastDrumRun = false;
    static constexpr float drumMix = 0.6f;   // Bassdrum etwa doppelt so laut wie eine Synth-Note (wie im Web)
    StepSequencer sequencer;
    SynthEngine engine;
    Arpeggiator arp;

    std::atomic<float>* gainParam   = apvts.getRawParameterValue ("master_gain");
    std::atomic<float>* arpOn       = apvts.getRawParameterValue ("arp_on");
    std::atomic<float>* arpMode     = apvts.getRawParameterValue ("arp_mode");
    std::atomic<float>* arpRate     = apvts.getRawParameterValue ("arp_rate");
    std::atomic<float>* arpOctaves  = apvts.getRawParameterValue ("arp_oct");
    std::atomic<float>* arpHold     = apvts.getRawParameterValue ("arp_hold");

    juce::LinearSmoothedValue<float> gainSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MangamanProcessor)
};
