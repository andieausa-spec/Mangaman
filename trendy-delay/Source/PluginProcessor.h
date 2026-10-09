#pragma once

#include "TapeEcho.h"

class TrendyDelayProcessor : public juce::AudioProcessor
{
public:
    TrendyDelayProcessor();
    ~TrendyDelayProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Echozeit in Sekunden aus Sync, Teilung, Zeit-Regler und Tempo (ohne Bandtempo)
    double delaySeconds (double bpm) const;

    juce::AudioProcessorValueTreeState apvts;

    // Für die Anzeige
    Dsp::TapeEcho echo;
    std::atomic<float> shownBpm { 120.0f };

    // Design der Oberfläche: 0 = 1980 (Schieberegler), 1 = 2100 (Gummipotis); wird mit dem Projekt gespeichert
    std::atomic<int> design { 1 };

private:
    double getHostBpm() const;
    float param (const char* id) const { return params[id]; }

    Params::Reader params { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrendyDelayProcessor)
};
