#include "PluginProcessor.h"
#include "PluginEditor.h"

KraftTransProcessor::KraftTransProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", Params::createLayout())
{
    for (auto* id : Params::generatorIds)
        apvts.addParameterListener (id, this);
    seed = 19780519u;
    noteSeed = 1974u;
    lastEra = (int) param ("era");
    regenerate();
    startTimerHz (20);
}

KraftTransProcessor::~KraftTransProcessor()
{
    stopTimer();
    for (auto* id : Params::generatorIds)
        apvts.removeParameterListener (id, this);
}

void KraftTransProcessor::prepareToPlay (double sr, int)
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

bool KraftTransProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
Kt::Settings KraftTransProcessor::currentSettings() const
{
    Kt::Settings s;
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
    s.textStyle = (int) param ("vocal_text");
    s.ownText = getOwnText();
    return s;
}

void KraftTransProcessor::regenerate()
{
    dirty.store (false);
    publish (std::make_shared<const Kt::Song> (Kt::generate (currentSettings())));
}

void KraftTransProcessor::publish (std::shared_ptr<const Kt::Song> song)
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

std::shared_ptr<const Kt::Song> KraftTransProcessor::getSong() const
{
    const juce::ScopedLock sl (songLock);
    return latest;
}

void KraftTransProcessor::rollNewTrack()
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
        setParam ("tempo", (float) Kt::suggestTempo (seed, (int) param ("era"), param ("closeness")));
        setParam ("key", (float) Kt::suggestKey (seed, param ("closeness")));
    }
    regenerate();
}

void KraftTransProcessor::setSeeds (juce::uint32 formSeed, juce::uint32 notesSeed)
{
    {
        const juce::ScopedLock sl (settingsLock);
        seed = juce::jmax (1u, formSeed);
        noteSeed = juce::jmax (1u, notesSeed);
        overrides.clear();
        salts.clear();
    }
    regenerate();
}

void KraftTransProcessor::rollVariation()
{
    {
        const juce::ScopedLock sl (settingsLock);
        noteSeed = (juce::uint32) juce::Random::getSystemRandom().nextInt (0x7fffffff) + 1u;
        salts.clear();
    }
    regenerate();
}

void KraftTransProcessor::rerollSection (int index)
{
    if (index < 0) return;
    {
        const juce::ScopedLock sl (settingsLock);
        if ((int) salts.size() <= index) salts.resize ((size_t) index + 1, 0);
        ++salts[(size_t) index];
    }
    regenerate();
}

void KraftTransProcessor::setSectionIntensity (int index, float value)
{
    if (index < 0) return;
    {
        const juce::ScopedLock sl (settingsLock);
        if ((int) overrides.size() <= index) overrides.resize ((size_t) index + 1, -1.0f);
        overrides[(size_t) index] = value < 0.0f ? -1.0f : juce::jlimit (0.0f, 1.0f, value);
    }
    regenerate();
}

void KraftTransProcessor::setOwnText (const juce::String& text)
{
    {
        const juce::ScopedLock sl (textLock);
        ownText = text.substring (0, 2000);
    }
    regenerate();
}

juce::String KraftTransProcessor::getOwnText() const
{
    const juce::ScopedLock sl (textLock);
    return ownText;
}

void KraftTransProcessor::seek (double beat)
{
    seekRequest.store (juce::jmax (0.0, beat));
}

void KraftTransProcessor::setRunning (bool shouldRun)
{
    running.store (shouldRun);
}

void KraftTransProcessor::parameterChanged (const juce::String&, float)
{
    // kann aus dem Audio-Thread kommen (Automation): nur vormerken, gerechnet wird im Timer
    dirty.store (true);
    lastChange = juce::Time::getMillisecondCounter();
}

void KraftTransProcessor::timerCallback()
{
    // Epoche gewechselt: passende Klänge wählen (abschaltbar)
    const int era = (int) param ("era");
    if (era != lastEra)
    {
        lastEra = era;
        if (param ("era_sound") > 0.5f)
            applyEraSound (era);
    }
    // kurz warten, solange am Regler gedreht wird
    if (dirty.load() && juce::Time::getMillisecondCounter() - lastChange > 120)
        regenerate();
}

void KraftTransProcessor::applyEraSound (int era)
{
    const auto& e = Params::eraSounds[juce::jlimit (0, (int) std::size (Params::eraSounds) - 1, era)];
    auto setParam = [this] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    };
    setParam ("pad_type", (float) e.padType);
    setParam ("seq_wave", (float) e.seqWave);
    setParam ("lead_wave", (float) e.leadWave);
    setParam ("drum_kit", (float) e.drumKit);
    setParam ("voice_type", (float) e.voiceType);
    setParam ("pad_vowel", e.padVowel);
    setParam ("seq_cutoff", e.seqCutoff);
    setParam ("bass_drive", e.bassDrive);
    setParam ("echo_mix", e.echoMix);
    setParam ("rev_mix", e.revMix);
    setParam ("rev_size", e.revSize);
}

//==============================================================================
Engine::Controls KraftTransProcessor::makeControls (double bpm) const
{
    Engine::Controls c;
    c.bpm = bpm;
    c.loop = param ("loop") > 0.5f;
    c.transpose = param ("midi_transpose") > 0.5f ? transpose.load() : 0;

    bool anySolo = false;
    for (auto* id : Params::laneIds)
        anySolo |= param ((juce::String ("solo_") + id).toRawUTF8()) > 0.5f;
    for (int i = 0; i < Kt::numLanes; ++i)
    {
        const juce::String id (Params::laneIds[i]);
        const bool mute = params["mute_" + id] > 0.5f;
        const bool solo = params["solo_" + id] > 0.5f;
        const float v = params["vol_" + id];
        c.laneGain[(size_t) i] = (mute || (anySolo && ! solo)) ? 0.0f : v * v * 1.6f;
    }

    c.padType = (int) param ("pad_type"); c.padBright = param ("pad_bright"); c.padAttack = param ("pad_attack");
    c.padEnsemble = param ("pad_ensemble"); c.padVowel = param ("pad_vowel");
    c.seqCutoff = param ("seq_cutoff"); c.seqReso = param ("seq_reso"); c.seqEnv = param ("seq_env");
    c.seqDecay = param ("seq_decay");   c.seqWave = (int) param ("seq_wave");
    c.bassCutoff = param ("bass_cutoff"); c.bassDecay = param ("bass_decay"); c.bassSub = param ("bass_sub"); c.bassDrive = param ("bass_drive");
    c.leadGlide = param ("lead_glide"); c.leadVibrato = param ("lead_vibrato"); c.leadBright = param ("lead_bright");
    c.leadWave = (int) param ("lead_wave");
    c.drumKit = (int) param ("drum_kit"); c.drumTone = param ("drum_tone"); c.drumSwing = param ("drum_swing");
    c.fxNoise = param ("fx_noise"); c.fxMachine = param ("fx_machine"); c.fxSignal = param ("fx_signal");
    c.vocalOn = param ("vocal_on") > 0.5f;
    c.voiceType = (int) param ("voice_type");
    c.vocalMelody = param ("vocal_melody"); c.vocalBands = param ("vocal_bands"); c.vocalCarrier = param ("vocal_carrier");
    c.vocalCrush = param ("vocal_crush"); c.vocalMetal = param ("vocal_metal");
    c.vocalDouble = param ("vocal_double"); c.vocalHarmony = param ("vocal_harmony");
    c.vocalReverb = param ("vocal_reverb"); c.vocalEcho = param ("vocal_echo");
    c.echoBeats = Params::echoBeats[juce::jlimit (0, Params::echoDivisions.size() - 1, (int) param ("echo_div"))];
    c.echoFeedback = param ("echo_fb"); c.echoMix = param ("echo_mix");
    c.reverbSize = param ("rev_size"); c.reverbMix = param ("rev_mix"); c.width = param ("width");
    c.master = juce::Decibels::decibelsToGain (param ("master")) * 1.25f;
    return c;
}

void KraftTransProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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
bool KraftTransProcessor::exportMidi (const juce::File& file) const
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

    // Rhythmus nach General MIDI auf Kanal 10, Geräusche als Marker-Noten auf Kanal 16, Stimme mit Silben als Liedtext
    static const int gmDrums[] { 36, 38, 42, 46, 39, 56, 45, 54 };
    static const int fxNotes[] { 24, 25, 26, 27, 28, 29, 30 };
    for (int lane = 0; lane < Kt::numLanes; ++lane)
    {
        juce::MidiMessageSequence seq;
        const int ch = lane == Kt::Drums ? 10 : lane == Kt::Fx ? 16 : lane + 1;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, Kt::laneName (lane)), 0);
        for (const auto& n : song->notes[(size_t) lane])
        {
            int pitch = n.pitch;
            if (lane == Kt::Drums) pitch = gmDrums[juce::jlimit (0, 7, n.pitch)];
            if (lane == Kt::Fx)    pitch = fxNotes[juce::jlimit (0, (int) Kt::numFx - 1, n.pitch)];
            if (lane == Kt::Vocal && ! n.harmony && n.syllable >= 0)
                seq.addEvent (juce::MidiMessage::textMetaEvent (5, song->syllableAt (n.syllable) + (n.wordEnd ? " " : "-")), n.start * ppq);
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
juce::AudioProcessorEditor* KraftTransProcessor::createEditor()
{
    return new KraftTransEditor (*this);
}

void KraftTransProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("design", design.load(), nullptr);
    state.setProperty ("own_text", getOwnText(), nullptr);
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

void KraftTransProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            design.store ((int) state.getProperty ("design", 1));
            {
                const juce::ScopedLock sl (textLock);
                ownText = state.getProperty ("own_text", ownText).toString();
            }
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
            lastEra = (int) param ("era");
            regenerate();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KraftTransProcessor();
}
