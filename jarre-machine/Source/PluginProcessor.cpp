#include "PluginProcessor.h"
#include "PluginEditor.h"

JarreMachineProcessor::JarreMachineProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", Params::createLayout())
{
    for (auto* id : Params::generatorIds)
        apvts.addParameterListener (id, this);
    seed = 20251976u;
    noteSeed = 1978u;
    regenerate();
    startTimerHz (20);
}

JarreMachineProcessor::~JarreMachineProcessor()
{
    stopTimer();
    for (auto* id : Params::generatorIds)
        apvts.removeParameterListener (id, this);
}

void JarreMachineProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    engine.prepare (sr);
    if (auto s = getSong())
    {
        pending.store (nullptr);
        engine.setSong (s.get());
        inUse.store (s.get());
    }
}

bool JarreMachineProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
Jarre::Settings JarreMachineProcessor::currentSettings() const
{
    Jarre::Settings s;
    {
        const juce::ScopedLock sl (settingsLock);
        s.seed = seed;
        s.noteSeed = noteSeed;
        s.intensityOverride = overrides;
        s.sectionSalt = salts;
    }
    s.lengthMinutes = param ("length");
    s.intensity = param ("intensity");
    s.closeness = param ("closeness");
    s.variety = param ("variety");
    s.era = (int) param ("era");
    s.key = (int) param ("key");
    s.scale = (int) param ("scale");
    s.tempo = param ("tempo");
    return s;
}

void JarreMachineProcessor::regenerate()
{
    dirty.store (false);
    publish (std::make_shared<const Jarre::Song> (Jarre::generate (currentSettings())));
}

void JarreMachineProcessor::publish (std::shared_ptr<const Jarre::Song> song)
{
    const juce::ScopedLock sl (songLock);
    latest = song;
    keepAlive.push_back (song);
    pending.store (song.get());
    // alte Tracks erst freigeben, wenn der Audio-Thread sie sicher nicht mehr benutzt
    const auto* used = inUse.load();
    while (keepAlive.size() > 4)
    {
        auto it = std::find_if (keepAlive.begin(), keepAlive.end() - 3, [used] (auto& p) { return p.get() != used; });
        if (it == keepAlive.end() - 3)
            break;
        keepAlive.erase (it);
    }
    ++songVersion;
}

std::shared_ptr<const Jarre::Song> JarreMachineProcessor::getSong() const
{
    const juce::ScopedLock sl (songLock);
    return latest;
}

void JarreMachineProcessor::rollNewTrack()
{
    auto& rng = juce::Random::getSystemRandom();
    {
        const juce::ScopedLock sl (settingsLock);
        seed = (juce::uint32) rng.nextInt (0x7fffffff) + 1u;
        noteSeed = (juce::uint32) rng.nextInt (0x7fffffff) + 1u;
        overrides.clear();
        salts.clear();
    }
    // stiltypisches Tempo und Tonart mitwürfeln
    if (param ("roll_all") > 0.5f)
    {
        auto setParam = [this] (const char* id, float value)
        {
            if (auto* p = apvts.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (value));
                p->endChangeGesture();
            }
        };
        setParam ("tempo", (float) Jarre::suggestTempo (seed, (int) param ("era"), param ("closeness")));
        setParam ("key", (float) Jarre::suggestKey (seed, param ("closeness")));
    }
    regenerate();
}

void JarreMachineProcessor::rollVariation()
{
    {
        const juce::ScopedLock sl (settingsLock);
        noteSeed = (juce::uint32) juce::Random::getSystemRandom().nextInt (0x7fffffff) + 1u;
        salts.clear();
    }
    regenerate();
}

void JarreMachineProcessor::rerollSection (int index)
{
    if (index < 0) return;
    {
        const juce::ScopedLock sl (settingsLock);
        if ((int) salts.size() <= index) salts.resize ((size_t) index + 1, 0);
        ++salts[(size_t) index];
    }
    regenerate();
}

void JarreMachineProcessor::setSectionIntensity (int index, float value)
{
    if (index < 0) return;
    {
        const juce::ScopedLock sl (settingsLock);
        if ((int) overrides.size() <= index) overrides.resize ((size_t) index + 1, -1.0f);
        overrides[(size_t) index] = value < 0.0f ? -1.0f : juce::jlimit (0.0f, 1.0f, value);
    }
    regenerate();
}

void JarreMachineProcessor::seek (double beat)
{
    seekRequest.store (juce::jmax (0.0, beat));
}

void JarreMachineProcessor::setRunning (bool shouldRun)
{
    running.store (shouldRun);
}

void JarreMachineProcessor::parameterChanged (const juce::String&, float)
{
    // kann aus dem Audio-Thread kommen (Automation): nur vormerken, gerechnet wird im Timer
    dirty.store (true);
    lastChange = juce::Time::getMillisecondCounter();
}

void JarreMachineProcessor::timerCallback()
{
    // kurz warten, solange am Regler gedreht wird
    if (dirty.load() && juce::Time::getMillisecondCounter() - lastChange > 120)
        regenerate();
}

//==============================================================================
Engine::Controls JarreMachineProcessor::makeControls (double bpm) const
{
    Engine::Controls c;
    c.bpm = bpm;
    c.loop = param ("loop") > 0.5f;
    c.transpose = param ("midi_transpose") > 0.5f ? transpose.load() : 0;

    bool anySolo = false;
    for (auto* id : Params::laneIds)
        anySolo |= param ((juce::String ("solo_") + id).toRawUTF8()) > 0.5f;
    for (int i = 0; i < Jarre::numLanes; ++i)
    {
        const juce::String id (Params::laneIds[i]);
        const bool mute = params["mute_" + id] > 0.5f;
        const bool solo = params["solo_" + id] > 0.5f;
        const float v = params["vol_" + id];
        c.laneGain[(size_t) i] = (mute || (anySolo && ! solo)) ? 0.0f : v * v * 1.6f;
    }

    c.padEnsemble = param ("pad_ensemble");   c.padPhaser = param ("pad_phaser");
    c.padPhaserRate = param ("pad_phaser_rate"); c.padBright = param ("pad_bright"); c.padAttack = param ("pad_attack");
    c.seqCutoff = param ("seq_cutoff"); c.seqReso = param ("seq_reso"); c.seqEnv = param ("seq_env");
    c.seqDecay = param ("seq_decay");   c.seqWave = (int) param ("seq_wave");
    c.bassCutoff = param ("bass_cutoff"); c.bassDecay = param ("bass_decay"); c.bassSub = param ("bass_sub");
    c.leadGlide = param ("lead_glide"); c.leadVibrato = param ("lead_vibrato"); c.leadBright = param ("lead_bright");
    c.leadWave = (int) param ("lead_wave");
    c.drumKit = (int) param ("drum_kit"); c.drumTone = param ("drum_tone"); c.drumSwing = param ("drum_swing");
    c.fxWind = param ("fx_wind"); c.fxLaser = param ("fx_laser"); c.fxSurf = param ("fx_surf");
    c.echoBeats = Params::echoBeats[juce::jlimit (0, Params::echoDivisions.size() - 1, (int) param ("echo_div"))];
    c.echoFeedback = param ("echo_fb"); c.echoMix = param ("echo_mix");
    c.reverbSize = param ("rev_size"); c.reverbMix = param ("rev_mix"); c.width = param ("width");
    c.master = juce::Decibels::decibelsToGain (param ("master")) * 1.25f;
    return c;
}

void JarreMachineProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // neuen Track übernehmen
    if (auto* p = pending.exchange (nullptr))
    {
        engine.setSong (p);
        inUse.store (p);
    }
    const auto* song = engine.getSong();

    // MIDI-Tasten transponieren den Track (C3 = Originallage); die letzte Taste bleibt stehen
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            transpose.store (juce::jlimit (-24, 24, m.getNoteNumber() - 60));
            ++heldNotes;
        }
        else if (m.isNoteOff())
            heldNotes = juce::jmax (0, heldNotes - 1);
    }
    midi.clear();

    const bool loop = param ("loop") > 0.5f;
    const double total = song != nullptr ? song->totalBeats() : 0.0;

    // Position: dem Host folgen, wenn er läuft, sonst eigene Uhr
    bool hostPlaying = false;
    double hostBpm = 0.0, hostPpq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto b = pos->getBpm()) hostBpm = *b;
            if (auto q = pos->getPpqPosition()) hostPpq = *q;
        }

    const double seekTo = seekRequest.exchange (-1.0);
    if (seekTo >= 0.0)
        ownBeat = juce::jmin (seekTo, juce::jmax (0.0, total - 0.001));

    double beat = ownBeat, bpm = song != nullptr ? song->tempo : (double) param ("tempo");
    bool playing = false, host = false;
    if (param ("follow_host") > 0.5f && hostPlaying)
    {
        host = playing = true;
        if (hostBpm > 0.0) bpm = juce::jlimit (20.0, 400.0, hostBpm);
        beat = juce::jmax (0.0, hostPpq);
        if (loop && total > 0.0) beat = std::fmod (beat, total);
    }
    else if (running.load() && total > 0.0)
    {
        playing = true;
        if (ownBeat >= total)
        {
            if (loop) ownBeat = std::fmod (ownBeat, total);
            else { running.store (false); playing = false; ownBeat = 0.0; }
        }
        beat = ownBeat;
        if (playing)
            ownBeat += numSamples * bpm / 60.0 / sampleRate;
    }
    if (playing && ! loop && beat >= total)
        playing = false;

    engine.process (buffer.getWritePointer (0), buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr,
                    numSamples, beat, playing, makeControls (bpm));

    shownBeat.store (beat);
    shownBpm.store ((float) bpm);
    shownPlaying.store (playing);
    shownHost.store (host);
}

//==============================================================================
bool JarreMachineProcessor::exportMidi (const juce::File& file) const
{
    auto song = getSong();
    if (song == nullptr)
        return false;
    constexpr int ppq = 960;
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);

    juce::MidiMessageSequence meta;
    meta.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / song->tempo)), 0);
    meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    meta.addEvent (juce::MidiMessage::textMetaEvent (3, song->title), 0);
    mf.addTrack (meta);

    // Rhythmus nach General MIDI auf Kanal 10, Effekte als Marker-Noten auf Kanal 16
    static const int gmDrums[] { 36, 38, 42, 46, 75, 62, 64, 70 };
    static const int fxNotes[] { 24, 25, 26, 27 };
    for (int lane = 0; lane < Jarre::numLanes; ++lane)
    {
        juce::MidiMessageSequence seq;
        const int ch = lane == Jarre::Drums ? 10 : lane == Jarre::Fx ? 16 : lane + 1;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, Jarre::laneName (lane)), 0);
        for (const auto& n : song->notes[(size_t) lane])
        {
            int pitch = n.pitch;
            if (lane == Jarre::Drums) pitch = gmDrums[juce::jlimit (0, 7, n.pitch)];
            if (lane == Jarre::Fx)    pitch = fxNotes[juce::jlimit (0, 3, n.pitch)];
            const auto vel = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (n.velocity * 127.0f));
            const double on = n.start * ppq, off = (n.start + juce::jmax (0.05, n.length)) * ppq;
            seq.addEvent (juce::MidiMessage::noteOn (ch, juce::jlimit (0, 127, pitch), vel), on);
            seq.addEvent (juce::MidiMessage::noteOff (ch, juce::jlimit (0, 127, pitch)), off);
        }
        seq.updateMatchedPairs();
        mf.addTrack (seq);
    }

    file.deleteFile();
    juce::FileOutputStream out (file);
    return out.openedOk() && mf.writeTo (out);
}

//==============================================================================
juce::AudioProcessorEditor* JarreMachineProcessor::createEditor()
{
    return new JarreMachineEditor (*this);
}

void JarreMachineProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("design", design.load(), nullptr);
    {
        const juce::ScopedLock sl (settingsLock);
        state.setProperty ("seed", (juce::int64) seed, nullptr);
        state.setProperty ("note_seed", (juce::int64) noteSeed, nullptr);
        juce::StringArray o, s;
        for (auto v : overrides) o.add (juce::String (v, 3));
        for (auto v : salts) s.add (juce::String (v));
        state.setProperty ("overrides", o.joinIntoString (","), nullptr);
        state.setProperty ("salts", s.joinIntoString (","), nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void JarreMachineProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            design.store ((int) state.getProperty ("design", 1));
            {
                const juce::ScopedLock sl (settingsLock);
                seed = (juce::uint32) (juce::int64) state.getProperty ("seed", (juce::int64) seed);
                noteSeed = (juce::uint32) (juce::int64) state.getProperty ("note_seed", (juce::int64) noteSeed);
                overrides.clear();
                salts.clear();
                for (auto& t : juce::StringArray::fromTokens (state.getProperty ("overrides").toString(), ",", ""))
                    if (t.isNotEmpty()) overrides.push_back (t.getFloatValue());
                for (auto& t : juce::StringArray::fromTokens (state.getProperty ("salts").toString(), ",", ""))
                    if (t.isNotEmpty()) salts.push_back (t.getIntValue());
            }
            apvts.replaceState (state);
            regenerate();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JarreMachineProcessor();
}
