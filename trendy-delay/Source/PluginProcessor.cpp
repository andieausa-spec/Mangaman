#include "PluginProcessor.h"
#include "PluginEditor.h"

TrendyDelayProcessor::TrendyDelayProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", Params::createLayout())
{
}

void TrendyDelayProcessor::prepareToPlay (double sampleRate, int)
{
    echo.prepare (sampleRate);
}

bool TrendyDelayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

// Tempo vom Host; ohne Host (Standalone) 120 BPM
double TrendyDelayProcessor::getHostBpm() const
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                return juce::jlimit (20.0, 400.0, *bpm);
    return 120.0;
}

double TrendyDelayProcessor::delaySeconds (double bpm) const
{
    if (param ("sync") > 0.5f)
    {
        const int d = juce::jlimit (0, Params::divisions.size() - 1, (int) param ("division"));
        return juce::jmin (2.5, Params::divisionBeats[d] * 60.0 / bpm);
    }
    return param ("time") / 1000.0;
}

void TrendyDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const double bpm = getHostBpm();
    shownBpm.store ((float) bpm);

    Dsp::TapeEcho::Settings s;
    s.delaySeconds = (float) delaySeconds (bpm);
    s.mode = (int) param ("mode");
    s.feedback = param ("feedback");
    s.lowCut = param ("low_cut");
    s.highCut = param ("high_cut");
    s.resonance = param ("resonance");
    s.drive = param ("drive");
    s.wow = param ("wow");
    s.flutter = param ("flutter");
    s.age = param ("age");
    s.speed = Params::speedFactors[juce::jlimit (0, 2, (int) param ("speed"))];
    s.glide = param ("glide");
    s.springMix = param ("spring_mix");
    s.springDecay = param ("spring_decay");
    s.springTone = param ("spring_tone");
    s.springBoing = param ("spring_boing");
    s.springPlace = (int) param ("spring_place");
    s.send = param ("send");
    s.mix = param ("mix");
    s.outputGain = juce::Decibels::decibelsToGain (param ("output"));
    s.ducking = param ("ducking");
    s.width = param ("width");
    s.throwOn = param ("throw") > 0.5f;
    s.freeze = param ("freeze") > 0.5f;

    echo.process (buffer, s);
}

juce::AudioProcessorEditor* TrendyDelayProcessor::createEditor()
{
    return new TrendyDelayEditor (*this);
}

void TrendyDelayProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("design", design.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TrendyDelayProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    return new TrendyDelayProcessor();
}
