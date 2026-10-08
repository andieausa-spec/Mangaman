#pragma once

#include "SynthEngine.h"
#include "Arpeggiator.h"

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

    // Design der Oberfläche: 0 = 1980 (Schieberegler), 1 = 2100 (Gummipotis); wird mit dem Projekt gespeichert
    std::atomic<int> design { 1 };

private:
    double getHostBpm() const;
    bool hostJustStarted();
    void timerCallback() override { applyRecordedSteps(); }

    Params::Reader params { apvts };
    Params::SeqReader seqParams { apvts };
    StepSequencer sequencer;
    SynthEngine engine;
    bool hostWasPlaying = false;
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
