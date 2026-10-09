#pragma once

#include "Parameters.h"
#include "Engine.h"

class DepecheMachineProcessor : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::Timer
{
public:
    DepecheMachineProcessor();
    ~DepecheMachineProcessor() override;

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
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Bedienung aus der Oberfläche (Message-Thread)
    void rollNewTrack();                              // neuer Seed für Form und Noten
    void rollVariation();                             // gleiche Form, neue Noten und Silben
    void rerollSection (int index);                   // nur dieser Abschnitt neu
    void setSectionIntensity (int index, float value); // Intensität eines Abschnitts von Hand (< 0 = automatisch)
    void regenerate();                                // Track aus Seed und Reglern neu berechnen
    void seek (double beat);
    void setRunning (bool shouldRun);
    bool exportMidi (const juce::File& file) const;
    void applyEraSound (int era);                     // Klangvoreinstellung der Epoche setzen
    void setSeeds (juce::uint32 formSeed, juce::uint32 notesSeed);

    std::shared_ptr<const Depeche::Song> getSong() const;
    Depeche::Settings currentSettings() const;

    juce::AudioProcessorValueTreeState apvts;
    Engine engine;

    // Für die Anzeige
    std::atomic<double> shownBeat { 0.0 };
    std::atomic<float> shownBpm { 126.0f };
    std::atomic<bool> shownPlaying { false }, shownHost { false };
    std::atomic<int> songVersion { 0 };
    std::atomic<int> transpose { 0 };

    // eigener Transport (Standalone oder wenn der Host steht)
    std::atomic<bool> running { false };

    // Design der Oberfläche: 0 = 1980 (Schieberegler), 1 = 2100 (Gummipotis); wird mit dem Projekt gespeichert
    std::atomic<int> design { 1 };

private:
    void parameterChanged (const juce::String& id, float) override;
    void timerCallback() override;
    void publish (std::shared_ptr<const Depeche::Song>);
    Engine::Controls makeControls (double bpm) const;
    float param (const char* id) const { return params[id]; }

    Params::Reader params { apvts };

    juce::CriticalSection settingsLock;
    juce::uint32 seed = 1, noteSeed = 1;
    std::vector<float> overrides;
    std::vector<int> salts;

    mutable juce::CriticalSection songLock;
    std::shared_ptr<const Depeche::Song> latest;
    std::vector<std::shared_ptr<const Depeche::Song>> keepAlive;
    std::atomic<const Depeche::Song*> pending { nullptr }, inUse { nullptr };

    std::atomic<bool> dirty { false };
    std::atomic<double> seekRequest { -1.0 };
    double ownBeat = 0.0;
    double sampleRate = 48000.0;
    juce::uint32 lastChange = 0;
    int heldNotes = 0;
    int lastEra = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DepecheMachineProcessor)
};
