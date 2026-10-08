#include "PluginProcessor.h"
#include "PluginEditor.h"

MangamanProcessor::MangamanProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", Params::createLayout())
{
    rememberGenerator();
    setPattern (Rhythm::generate ((int) param ("rh_kit"), (int) param ("rh_style"), juce::roundToInt (param ("rh_rand") * 100.0f), param ("rh_dens")));
    startTimerHz (20);
}

void MangamanProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    engine.prepare (sampleRate);
    sequencer.prepare (sampleRate);
    arp.prepare (sampleRate);
    drums.prepare (sampleRate);
    drumBuffer.setSize (2, juce::jmax (1, samplesPerBlock));
    clockWasRunning = false;
    gainSmoothed.reset (sampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));
}

bool MangamanProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

// Tempo vom Host; ohne Host (Standalone) gilt der Regler "Tempo"
double MangamanProcessor::getHostBpm() const
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                return juce::jlimit (20.0, 400.0, *bpm);
    return juce::jlimit (40.0, 240.0, (double) param ("tempo"));
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

//==============================================================================
void MangamanProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

Rhythm::Pattern MangamanProcessor::getPattern() const
{
    Rhythm::Pattern p {};
    for (size_t i = 0; i < p.size(); ++i)
        p[i] = pattern[i].load (std::memory_order_relaxed);
    return p;
}

void MangamanProcessor::setPattern (const Rhythm::Pattern& p)
{
    for (size_t i = 0; i < p.size(); ++i)
        pattern[i].store (p[i], std::memory_order_relaxed);
    ++patternVersion;
}

void MangamanProcessor::setStep (int track, int step, int value)
{
    if (track < 0 || track >= Drums::numTracks || step < 0 || step >= Drums::numSteps)
        return;
    pattern[(size_t) (track * Drums::numSteps + step)].store ((juce::uint8) juce::jlimit (0, 3, value));
    ++patternVersion;
}

void MangamanProcessor::rememberGenerator()
{
    genStyle.store (param ("rh_style"));
    genRand.store (param ("rh_rand"));
    genDens.store (param ("rh_dens"));
}

void MangamanProcessor::regeneratePattern()
{
    rememberGenerator();
    setPattern (Rhythm::generate ((int) param ("rh_kit"), (int) param ("rh_style"), juce::roundToInt (param ("rh_rand") * 100.0f), param ("rh_dens")));
}

juce::String MangamanProcessor::getRhythmTag() const
{
    const int nr = juce::roundToInt (param ("rh_rand") * 100.0f);
    const int gi = Rhythm::genreFor ((int) param ("rh_kit"), (int) param ("rh_style"), nr);
    return "Muster " + juce::String (nr + 1) + "  -  " + Rhythm::genres()[(size_t) gi].name;
}

juce::StringArray MangamanProcessor::applyRhythmText (const juce::String& text)
{
    if (text.trim().isEmpty())
        return {};
    rhythmText = text;
    const auto r = Rhythm::parse (text);
    const auto h = Rhythm::hashText (text.trim().toLowerCase());

    if (r.kit >= 0) setParam ("rh_kit", (float) r.kit);
    const int kit = (int) param ("rh_kit");
    setParam ("rh_style", r.style >= 0 ? (float) (r.style + 1) : 0.0f);
    setParam ("rh_rand", (float) (h % 100) / 100.0f);
    if (r.bpm > 0) setParam ("tempo", (float) juce::jlimit (40, 240, r.bpm));
    const auto* g = r.style >= 0 ? &Rhythm::genres()[(size_t) r.style] : nullptr;
    setParam ("rh_dens",  r.density >= 0 ? r.density : 0.5f);
    setParam ("rh_var",   r.variation >= 0 ? r.variation : 0.15f);
    setParam ("rh_fill",  r.fill >= 0 ? (float) r.fill : 1.0f);
    setParam ("rh_swing", r.swing >= 0 ? r.swing : g != nullptr ? g->swing : 0.0f);
    setParam ("rh_hum",   r.humanize >= 0 ? r.humanize : 0.0f);
    setParam ("rh_acc",   r.accent >= 0 ? r.accent : 0.5f);
    setParam ("rh_rate",  r.rate >= 0 ? (float) r.rate : 1.0f);
    setParam ("rh_len",   16.0f);
    for (int t = 0; t < Drums::numTracks; ++t)
        setParam (Params::rhId ("mu", t), ((r.mutes >> t) & 1u) != 0 ? 1.0f : 0.0f);

    rememberGenerator();
    setPattern (Rhythm::patternFor (r, kit, (int) param ("rh_style"), juce::roundToInt (param ("rh_rand") * 100.0f), param ("rh_dens")));
    if (param ("rh_run") < 0.5f)
        setParam ("rh_run", 1.0f);

    auto read = r.read;
    if (read.isEmpty())
        read.add ("frei erfunden: " + juce::String (Rhythm::kitInfo()[(size_t) kit].name) + "-Stil, " + getRhythmTag().fromFirstOccurrenceOf ("-  ", false, false));
    return read;
}

void MangamanProcessor::timerCallback()
{
    applyRecordedSteps();

    // Stil, Zufall oder Dichte geändert (Regler oder Automation): neues Muster
    if (! juce::exactlyEqual (param ("rh_style"), genStyle.load()) || ! juce::exactlyEqual (param ("rh_rand"), genRand.load())
        || ! juce::exactlyEqual (param ("rh_dens"), genDens.load()))
        regeneratePattern();

    // gekoppelt: Sequenzer und Drums starten und stoppen zusammen
    const bool seqRun = param ("sq_run") > 0.5f, drumRun = param ("rh_run") > 0.5f;
    if (param ("rh_link") > 0.5f)
    {
        if (seqRun != lastSeqRun && drumRun != seqRun)       setParam ("rh_run", seqRun ? 1.0f : 0.0f);
        else if (drumRun != lastDrumRun && seqRun != drumRun) setParam ("sq_run", drumRun ? 1.0f : 0.0f);
    }
    lastSeqRun = param ("sq_run") > 0.5f;
    lastDrumRun = param ("rh_run") > 0.5f;
}

//==============================================================================
namespace
{
    // General-MIDI-Schlagzeug auf Kanal 10 -> Spuren
    int gmDrumTrack (int note)
    {
        switch (note)
        {
            case 35: case 36: return 0;
            case 38: case 40: return 1;
            case 39: return 2;
            case 37: return 3;
            case 42: case 44: return 4;
            case 46: return 5;
            case 41: case 43: return 6;
            case 45: case 47: return 7;
            case 48: case 50: return 8;
            case 56: return 9;
            case 49: case 57: return 10;
            case 51: case 59: case 75: case 70: case 54: return 11;
            default: return -1;
        }
    }
}

void MangamanProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int numSamples = buffer.getNumSamples();
    const double bpm = getHostBpm();

    // Gemeinsamer Takt: Song-Position des Hosts, wenn er spielt, sonst eigene Uhr
    const auto seqSettings = seqParams.read();
    const auto rh = rhythmParams.read();
    Clock::Block clock;
    clock.bpm = bpm;
    clock.perSample = bpm / 60.0 / sampleRateHz;
    clock.sync = seqSettings.run || rh.run;
    juce::Optional<double> hostPpq;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getIsPlaying())
                hostPpq = pos->getPpqPosition();
    if (hostPpq)
        clock.ppq = *hostPpq;
    else
    {
        if (clock.sync && ! clockWasRunning)
            internalPpq = 0.0;   // der Erste von Sequenzer und Drums beginnt den Takt bei null
        clock.ppq = internalPpq;
    }
    internalPpq = clock.ppq + numSamples * clock.perSample;
    clockWasRunning = clock.sync;

    // Noten von der Bildschirmtastatur mit einmischen
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Drum-Einstellungen für diesen Block
    DrumMachine::Settings ds;
    ds.run = rh.run; ds.kit = rh.kit; ds.rate = rh.rate; ds.length = rh.length; ds.fill = rh.fill;
    ds.swing = rh.swing; ds.variation = rh.variation; ds.humanize = rh.humanize; ds.accent = rh.accent; ds.volume = rh.volume;
    for (int t = 0; t < Drums::numTracks; ++t)
    {
        ds.level[t] = rh.level[t]; ds.tune[t] = rh.tune[t]; ds.decay[t] = rh.decay[t];
        ds.pan[t] = rh.pan[t]; ds.mute[t] = rh.mute[t];
    }
    drums.update (ds);

    // MIDI-Kanal 10 spielt die Drums
    if (rh.midi)
    {
        juce::MidiBuffer rest;
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            if (m.getChannel() == 10)
            {
                if (m.isNoteOn())
                    drums.hit (gmDrumTrack (m.getNoteNumber()), m.getFloatVelocity(), meta.samplePosition);
                continue;
            }
            rest.addEvent (m, meta.samplePosition);
        }
        midi.swapWith (rest);
    }
    if (const int t = auditionTrack.exchange (-1); t >= 0)
        drums.hit (t, auditionVel.load());

    // Sequenzer vor dem Arpeggiator: er erzeugt Noten, gespielte Tasten transponieren
    sequencer.process (midi, numSamples, seqSettings, clock);

    Arpeggiator::Settings arpSettings;
    arpSettings.enabled = arpOn->load() > 0.5f;
    arpSettings.mode    = (int) arpMode->load();
    arpSettings.rate    = (int) arpRate->load();
    arpSettings.octaves = (int) arpOctaves->load();
    arpSettings.hold    = arpHold->load() > 0.5f;
    arpSettings.bpm     = bpm;
    arpSettings.clock   = clock;
    arp.process (midi, numSamples, arpSettings);

    engine.process (buffer.getWritePointer (0), numSamples, midi, params.read(), bpm, sequencer);

    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));
    const float startGain = gainSmoothed.getCurrentValue();
    gainSmoothed.applyGain (buffer.getWritePointer (0), numSamples);
    const float endGain = gainSmoothed.getCurrentValue();

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    // Drums auf eigenem Weg dazu (ohne Effekte), mit derselben Gesamtlautstärke
    if (drumBuffer.getNumSamples() < numSamples)
        drumBuffer.setSize (2, numSamples, false, false, true);
    drumBuffer.clear();
    drums.render (drumBuffer.getWritePointer (0), drumBuffer.getWritePointer (1), numSamples, ds, clock, getPattern());
    const float k = drumMix;
    if (buffer.getNumChannels() >= 2)
    {
        buffer.addFromWithRamp (0, 0, drumBuffer.getReadPointer (0), numSamples, startGain * k, endGain * k);
        buffer.addFromWithRamp (1, 0, drumBuffer.getReadPointer (1), numSamples, startGain * k, endGain * k);
    }
    else
    {
        buffer.addFromWithRamp (0, 0, drumBuffer.getReadPointer (0), numSamples, startGain * k * 0.5f, endGain * k * 0.5f);
        buffer.addFromWithRamp (0, 0, drumBuffer.getReadPointer (1), numSamples, startGain * k * 0.5f, endGain * k * 0.5f);
    }
}

juce::AudioProcessorEditor* MangamanProcessor::createEditor()
{
    return new MangamanEditor (*this);
}

void MangamanProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("design", design.load(), nullptr);
    state.setProperty ("rhythm", Rhythm::toString (getPattern()), nullptr);
    state.setProperty ("rhythmText", rhythmText, nullptr);
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
            rhythmText = state.getProperty ("rhythmText", "").toString();
            Rhythm::Pattern p {};
            const bool hasPattern = Rhythm::fromString (state.getProperty ("rhythm", "").toString(), p);
            state.removeProperty ("rhythm", nullptr);
            state.removeProperty ("rhythmText", nullptr);
            apvts.replaceState (state);
            if (hasPattern)
            {
                rememberGenerator();   // geladenes Muster nicht gleich neu würfeln
                setPattern (p);
            }
            else
                regeneratePattern();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MangamanProcessor();
}
