#include "PluginProcessor.h"
#include "PluginEditor.h"

MangamanProcessor::MangamanProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", Params::createLayout())
{
    startTimerHz (20);
}

void MangamanProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    sequencer.prepare (sampleRate);
    arp.prepare (sampleRate);
    gainSmoothed.reset (sampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));
}

bool MangamanProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

double MangamanProcessor::getHostBpm() const
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                return juce::jlimit (20.0, 400.0, *bpm);
    return 120.0;
}

bool MangamanProcessor::hostJustStarted()
{
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            playing = pos->getIsPlaying();
    const bool started = playing && ! hostWasPlaying;
    hostWasPlaying = playing;
    return started;
}

void MangamanProcessor::applyRecordedSteps()
{
    StepSequencer::Recorded r;
    while (sequencer.popRecorded (r))
    {
        if (auto* p = apvts.getParameter (Params::seqPitchId (r.step)))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) r.semis));
        if (auto* g = apvts.getParameter (Params::seqGateId (r.step)))
            g->setValueNotifyingHost (1.0f);
    }
}

void MangamanProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    const double bpm = getHostBpm();

    // Noten von der Bildschirmtastatur mit einmischen
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Sequenzer vor dem Arpeggiator: er erzeugt Noten, gespielte Tasten transponieren
    sequencer.process (midi, numSamples, seqParams.read(), bpm, hostJustStarted());

    Arpeggiator::Settings arpSettings;
    arpSettings.enabled = arpOn->load() > 0.5f;
    arpSettings.mode    = (int) arpMode->load();
    arpSettings.rate    = (int) arpRate->load();
    arpSettings.octaves = (int) arpOctaves->load();
    arpSettings.hold    = arpHold->load() > 0.5f;
    arpSettings.bpm     = bpm;
    arp.process (midi, numSamples, arpSettings);

    engine.process (buffer.getWritePointer (0), numSamples, midi, params.read(), bpm, sequencer);

    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));
    gainSmoothed.applyGain (buffer.getWritePointer (0), numSamples);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

juce::AudioProcessorEditor* MangamanProcessor::createEditor()
{
    return new MangamanEditor (*this);
}

void MangamanProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("design", design.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MangamanProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            design.store ((int) state.getProperty ("design", 1));
            apvts.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MangamanProcessor();
}
