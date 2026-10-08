#pragma once

#include <JuceHeader.h>

// Alle Parameter-IDs, Auswahllisten und das Parameter-Layout an einer Stelle.
namespace Params
{
    inline const juce::StringArray oscTypes   { "Basic Waves", "Superwave", "Two-Op FM", "Harmonic",
                                                "Karplus Strong", "Waveshaper", "Noise" };
    enum OscType { BasicWaves, Superwave, TwoOpFM, Harmonic, KarplusStrong, Waveshaper, Noise };

    inline const juce::StringArray filterTypes { "Lowpass", "Bandpass", "Highpass" };
    inline const juce::StringArray cycModes    { "Env", "Run", "Loop" };
    enum CycMode { CycEnv, CycRun, CycLoop };

    inline const juce::StringArray lfoShapes   { "Sinus", "Dreieck", "Saegezahn", "Rechteck", "S&H", "S&H weich" };
    inline const juce::StringArray syncDivs    { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
    inline constexpr double syncBeats[]        { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };

    inline const juce::StringArray arpModes    { "Up", "Down", "Up/Down", "Order", "Random" };
    inline const juce::StringArray arpRates    { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    inline constexpr double arpBeats[]         { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

    inline const juce::StringArray voiceModes  { "Poly", "Mono", "Legato" };
    enum VoiceMode { Poly, Mono, Legato };

    // Mod-Matrix wie beim MicroFreak, erweitert um die drei Sequenzer-Reihen: 8 Quellen x 7 Ziele
    inline const juce::StringArray modSources  { "Cyc Env", "Envelope", "LFO", "Druck", "Key", "Seq A", "Seq B", "Seq C" };
    inline const juce::StringArray modDests    { "Pitch", "Wave", "Timbre", "Shape", "Cutoff", "Resonanz", "Amp" };
    constexpr int numSources = 8, numDests = 7;
    constexpr int firstSeqSource = 5;

    // Step-Sequenzer nach Art von Doepfer SEQ / Dark Time
    inline const juce::StringArray seqDirs     { "Vor", "Rueck", "Pendel", "Zufall" };
    constexpr int seqSteps = 16, seqRows = 3;
    inline constexpr int seqDefaultPitch[seqSteps] { 0, 0, 12, 0, 7, 0, 10, 12, 0, 0, 15, 12, 7, 5, 3, 0 };
    inline constexpr bool seqDefaultGate[seqSteps] { 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1 };

    inline juce::String seqPitchId (int i)        { return "sq_p_" + juce::String (i); }
    inline juce::String seqGateId (int i)         { return "sq_g_" + juce::String (i); }
    inline juce::String seqRowId (int r, int i)   { return juce::String ("sq_") + "abc"[r] + "_" + juce::String (i); }

    inline juce::String noteName (int n)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" };
        return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    }
    // Rhythmus
    inline const juce::StringArray rhythmKits { "808-Stil", "909-Stil", "606-Stil", "707-Stil", "CR-78-Stil", "LinnDrum-Stil",
                                                "DMX-Stil", "Drumulator-Stil", "SDS-V-Stil", "Mini-Pops-Stil" };
    inline const juce::StringArray rhythmStyles { "Passend zum Modell", "House", "Techno", "Trance", "Garage", "Acid", "Electro",
                                                  "Minimal", "Hip-Hop", "Trap", "Breakbeat", "Drum & Bass", "Italo", "Disco", "Funk",
                                                  "Pop 80er", "Rock", "Bossa Nova", "Samba", "Rumba", "Reggae" };
    constexpr int rhythmTracks = 12;
    inline const char* rhythmTrackShort[] { "BD", "SD", "CP", "RS", "CH", "OH", "LT", "MT", "HT", "CB", "CY", "PC" };
    inline juce::String rhId (const char* what, int t) { return juce::String ("rh_") + what + "_" + juce::String (t); }

    enum Dest { DPitch, DWave, DTimbre, DShape, DCutoff, DReso, DAmp };

    inline juce::String modId (int s, int d) { return "mod_" + juce::String (s) + "_" + juce::String (d); }

    // Werte, die die Klangerzeugung pro Block liest
    struct Snapshot
    {
        int oscType = 0;
        float wave = 0, timbre = 0, shape = 0, glide = 0;
        int filterType = 0;
        float cutoff = 1000, reso = 0, filterEnv = 0;
        float attack = 0.01f, decay = 0.3f, sustain = 0.7f;
        int cycMode = 0;
        float cycRise = 0.1f, cycHold = 0, cycFall = 0.5f, cycAmount = 1;
        int lfoShape = 0;
        float lfoRate = 1;
        bool lfoSync = false;
        int lfoDiv = 2;
        int voiceMode = Poly;
        float mod[numSources][numDests] {};
    };

    namespace detail
    {
        inline juce::String timeText (float v, int)
        {
            return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms"
                            : juce::String (v, 2) + " s";
        }
        inline juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
        inline juce::String hzText (float v, int)
        {
            return v < 1000.0f ? juce::String (v, v < 10.0f ? 2 : 0) + " Hz"
                               : juce::String (v / 1000.0f, 1) + " kHz";
        }
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto choice = [&] (const String& id, const String& name, const StringArray& items, int def)
        {
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
        };
        auto toggle = [&] (const String& id, const String& name, bool def)
        {
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
        };
        auto flt = [&] (const String& id, const String& name, NormalisableRange<float> range, float def,
                        String (*text) (float, int))
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                AudioParameterFloatAttributes().withStringFromValueFunction (text)));
        };
        auto percent = [&] (const String& id, const String& name, float def)
        {
            flt (id, name, { 0.0f, 1.0f, 0.001f }, def, detail::percentText);
        };
        auto bipolar = [&] (const String& id, const String& name, float def)
        {
            flt (id, name, { -1.0f, 1.0f, 0.001f }, def, detail::percentText);
        };
        auto time = [&] (const String& id, const String& name, float min, float max, float def)
        {
            flt (id, name, { min, max, 0.001f, 0.3f }, def, detail::timeText);
        };

        // Oszillator
        choice ("osc_type", "Osc Typ", oscTypes, BasicWaves);
        percent ("osc_wave",   "Wave",   0.0f);
        percent ("osc_timbre", "Timbre", 0.3f);
        percent ("osc_shape",  "Shape",  0.0f);
        percent ("glide",      "Glide",  0.0f);

        // Filter (SEM-artiges State-Variable-Filter)
        choice ("flt_type", "Filter Typ", filterTypes, 0);
        flt ("flt_cutoff", "Cutoff", { 20.0f, 20000.0f, 1.0f, 0.25f }, 4000.0f, detail::hzText);
        percent ("flt_res", "Resonanz", 0.2f);
        bipolar ("flt_env", "Filter Env", 0.3f);

        // Hüllkurve (wie MicroFreak: Attack, Decay/Release, Sustain)
        time ("env_attack", "Attack", 0.001f, 10.0f, 0.005f);
        time ("env_decay",  "Decay/Release", 0.005f, 10.0f, 0.4f);
        percent ("env_sustain", "Sustain", 0.7f);

        // Zyklische Hüllkurve
        choice ("cyc_mode", "Cyc Modus", cycModes, CycLoop);
        time ("cyc_rise", "Rise", 0.001f, 10.0f, 0.2f);
        time ("cyc_hold", "Hold", 0.0f,   5.0f,  0.0f);
        time ("cyc_fall", "Fall", 0.001f, 10.0f, 0.5f);
        percent ("cyc_amount", "Cyc Menge", 1.0f);

        // LFO
        choice ("lfo_shape", "LFO Form", lfoShapes, 0);
        flt ("lfo_rate", "LFO Rate", { 0.05f, 50.0f, 0.001f, 0.3f }, 2.0f, detail::hzText);
        toggle ("lfo_sync", "LFO Sync", false);
        choice ("lfo_div", "LFO Teilung", syncDivs, 2);

        // Arpeggiator
        toggle ("arp_on", "Arp an", false);
        choice ("arp_mode", "Arp Modus", arpModes, 0);
        choice ("arp_rate", "Arp Rate", arpRates, 3);
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { "arp_oct", 1 }, "Arp Oktaven", 1, 4, 1));
        toggle ("arp_hold", "Arp Hold", false);

        // Master
        choice ("voice_mode", "Stimmen", voiceModes, Poly);
        flt ("master_gain", "Lautstaerke", { -48.0f, 6.0f, 0.1f }, -6.0f,
             [] (float v, int) { return String (v, 1) + " dB"; });

        // Step-Sequenzer
        toggle ("sq_run",   "Seq Start", false);
        toggle ("sq_rec",   "Seq Aufnahme", false);
        toggle ("sq_trans", "Seq Tasten transponieren", true);
        choice ("sq_dir",   "Seq Richtung", seqDirs, 0);
        choice ("sq_rate",  "Seq Schrittlaenge", arpRates, 3);
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { "sq_len", 1 }, "Seq Laenge", 1, seqSteps, seqSteps));
        flt ("sq_gate",  "Seq Gate",  { 0.05f, 1.0f, 0.001f }, 0.55f, detail::percentText);
        flt ("sq_swing", "Seq Swing", { 0.0f, 0.5f, 0.001f }, 0.0f,
             [] (float v, int) { return String (roundToInt (50.0f + v * 50.0f)) + " %"; });
        percent ("sq_slew", "Seq Glaetten", 0.15f);
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { "sq_root", 1 }, "Seq Grundton", 24, 72, 48,
            AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v); })));

        for (int i = 0; i < seqSteps; ++i)
        {
            const auto n = String (i + 1);
            layout.add (std::make_unique<AudioParameterInt> (ParameterID { seqPitchId (i), 1 }, "Seq Schritt " + n + " Ton",
                -24, 24, seqDefaultPitch[i],
                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                    { return (v > 0 ? "+" : "") + String (v) + " HT"; })));
            toggle (seqGateId (i), "Seq Schritt " + n + " an", seqDefaultGate[i]);
            const float a = (float) roundToInt (std::sin ((float) i / 16.0f * MathConstants<float>::twoPi) * 70.0f) / 100.0f;
            bipolar (seqRowId (0, i), "Seq Reihe A " + n, a);
            bipolar (seqRowId (1, i), "Seq Reihe B " + n, i % 4 == 0 ? 0.6f : (i % 2 != 0 ? -0.2f : 0.2f));
            bipolar (seqRowId (2, i), "Seq Reihe C " + n, 0.0f);
        }

        // Rhythmus: Drumcomputer
        toggle ("rh_run",  "Drums Start", false);
        toggle ("rh_link", "Drums mit Sequenzer koppeln", false);
        choice ("rh_kit",  "Drumcomputer", rhythmKits, 1);
        choice ("rh_style", "Drums Stil", rhythmStyles, 0);
        flt ("rh_rand", "Drums Zufall", { 0.0f, 0.99f, 0.01f }, 0.0f,
             [] (float v, int) { return "Nr. " + String (roundToInt (v * 100.0f) + 1); });
        percent ("rh_dens", "Drums Dichte", 0.5f);
        percent ("rh_var",  "Drums Variation", 0.15f);
        flt ("rh_swing", "Drums Shuffle", { 0.0f, 0.5f, 0.001f }, 0.0f,
             [] (float v, int) { return String (roundToInt (50.0f + v * 50.0f)) + " %"; });
        percent ("rh_hum", "Drums Humanize", 0.0f);
        percent ("rh_acc", "Drums Akzent", 0.5f);
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { "rh_len", 1 }, "Drums Laenge", 1, 16, 16,
            AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + " Schritte"; })));
        choice ("rh_rate", "Drums Raster", StringArray { "1/8", "1/16", "1/32" }, 1);
        choice ("rh_fill", "Drums Fill", StringArray { "Aus", "4 Takte", "8 Takte", "16 Takte" }, 1);
        percent ("rh_vol", "Drums Lautstaerke", 0.8f);
        toggle ("rh_midi", "Drums auf MIDI-Kanal 10", true);
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { "tempo", 1 }, "Tempo ohne Host", 40, 240, 120,
            AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + " BPM"; })));
        for (int t = 0; t < rhythmTracks; ++t)
        {
            const String n = String ("Drums ") + rhythmTrackShort[t] + " ";
            percent (rhId ("lv", t), n + "Pegel", 0.8f);
            flt (rhId ("tu", t), n + "Stimmung", { -12.0f, 12.0f, 0.5f }, 0.0f,
                 [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " HT"; });
            flt (rhId ("de", t), n + "Ausklang", { -1.0f, 1.0f, 0.001f }, 0.0f,
                 [] (float v, int) { return "x" + String (std::pow (2.0f, v * 2.0f), 2); });
            flt (rhId ("pn", t), n + "Panorama", { -1.0f, 1.0f, 0.001f }, 0.0f,
                 [] (float v, int) { return std::abs (v) < 0.02f ? String ("Mitte") : (v < 0 ? "L " : "R ") + String (roundToInt (std::abs (v) * 100.0f)); });
            toggle (rhId ("mu", t), n + "stumm", false);
        }

        // Mod-Matrix
        for (int s = 0; s < numSources; ++s)
            for (int d = 0; d < numDests; ++d)
                bipolar (modId (s, d), "Mod " + modSources[s] + " > " + modDests[d], 0.0f);

        return layout;
    }

    // Werte des Sequenzers pro Block
    struct SeqSnapshot
    {
        bool run = false, rec = false, trans = true;
        int dir = 0, rate = 3, length = seqSteps, root = 48;
        float gate = 0.55f, swing = 0, slew = 0.15f;
        int pitch[seqSteps] {};
        bool on[seqSteps] {};
        float rows[seqRows][seqSteps] {};
    };

    class SeqReader
    {
    public:
        explicit SeqReader (juce::AudioProcessorValueTreeState& s)
        {
            auto get = [&s] (const juce::String& id) { auto* v = s.getRawParameterValue (id); jassert (v != nullptr); return v; };
            run = get ("sq_run"); rec = get ("sq_rec"); trans = get ("sq_trans");
            dir = get ("sq_dir"); rate = get ("sq_rate"); length = get ("sq_len"); root = get ("sq_root");
            gate = get ("sq_gate"); swing = get ("sq_swing"); slew = get ("sq_slew");
            for (int i = 0; i < seqSteps; ++i)
            {
                pitch[i] = get (seqPitchId (i));
                on[i] = get (seqGateId (i));
                for (int r = 0; r < seqRows; ++r)
                    rows[r][i] = get (seqRowId (r, i));
            }
        }

        SeqSnapshot read() const
        {
            SeqSnapshot p;
            p.run = run->load() > 0.5f;  p.rec = rec->load() > 0.5f;  p.trans = trans->load() > 0.5f;
            p.dir = (int) dir->load();   p.rate = (int) rate->load();
            p.length = juce::jlimit (1, seqSteps, (int) length->load());
            p.root = (int) root->load();
            p.gate = gate->load();       p.swing = swing->load();     p.slew = slew->load();
            for (int i = 0; i < seqSteps; ++i)
            {
                p.pitch[i] = juce::roundToInt (pitch[i]->load());
                p.on[i] = on[i]->load() > 0.5f;
                for (int r = 0; r < seqRows; ++r)
                    p.rows[r][i] = rows[r][i]->load();
            }
            return p;
        }

    private:
        std::atomic<float> *run, *rec, *trans, *dir, *rate, *length, *root, *gate, *swing, *slew;
        std::atomic<float>* pitch[seqSteps];
        std::atomic<float>* on[seqSteps];
        std::atomic<float>* rows[seqRows][seqSteps];
    };

    // Werte des Drumcomputers pro Block
    struct RhythmSnapshot
    {
        bool run = false, midi = true;
        int kit = 1, rate = 1, length = 16, fill = 1;
        float swing = 0, variation = 0.15f, humanize = 0, accent = 0.5f, volume = 0.8f;
        float level[rhythmTracks] {}, tune[rhythmTracks] {}, decay[rhythmTracks] {}, pan[rhythmTracks] {};
        bool mute[rhythmTracks] {};
    };

    class RhythmReader
    {
    public:
        explicit RhythmReader (juce::AudioProcessorValueTreeState& s)
        {
            auto get = [&s] (const juce::String& id) { auto* v = s.getRawParameterValue (id); jassert (v != nullptr); return v; };
            run = get ("rh_run"); midi = get ("rh_midi"); kit = get ("rh_kit"); rate = get ("rh_rate"); length = get ("rh_len");
            fill = get ("rh_fill"); swing = get ("rh_swing"); variation = get ("rh_var"); humanize = get ("rh_hum");
            accent = get ("rh_acc"); volume = get ("rh_vol");
            for (int t = 0; t < rhythmTracks; ++t)
            {
                level[t] = get (rhId ("lv", t)); tune[t] = get (rhId ("tu", t)); decay[t] = get (rhId ("de", t));
                pan[t] = get (rhId ("pn", t)); mute[t] = get (rhId ("mu", t));
            }
        }

        RhythmSnapshot read() const
        {
            RhythmSnapshot p;
            p.run = run->load() > 0.5f; p.midi = midi->load() > 0.5f;
            p.kit = (int) kit->load(); p.rate = (int) rate->load(); p.length = (int) length->load(); p.fill = (int) fill->load();
            p.swing = swing->load(); p.variation = variation->load(); p.humanize = humanize->load();
            p.accent = accent->load(); p.volume = volume->load();
            for (int t = 0; t < rhythmTracks; ++t)
            {
                p.level[t] = level[t]->load(); p.tune[t] = tune[t]->load(); p.decay[t] = decay[t]->load();
                p.pan[t] = pan[t]->load(); p.mute[t] = mute[t]->load() > 0.5f;
            }
            return p;
        }

    private:
        std::atomic<float> *run, *midi, *kit, *rate, *length, *fill, *swing, *variation, *humanize, *accent, *volume;
        std::atomic<float> *level[rhythmTracks], *tune[rhythmTracks], *decay[rhythmTracks], *pan[rhythmTracks], *mute[rhythmTracks];
    };

    // Liest alle Parameter lock-frei aus dem APVTS
    class Reader
    {
    public:
        explicit Reader (juce::AudioProcessorValueTreeState& s) : apvts (s)
        {
            for (int i = 0; i < numSources; ++i)
                for (int d = 0; d < numDests; ++d)
                    mod[i][d] = get (modId (i, d));
        }

        Snapshot read() const
        {
            Snapshot p;
            p.oscType    = (int) oscType->load();
            p.wave       = wave->load();
            p.timbre     = timbre->load();
            p.shape      = shape->load();
            p.glide      = glide->load();
            p.filterType = (int) filterType->load();
            p.cutoff     = cutoff->load();
            p.reso       = reso->load();
            p.filterEnv  = filterEnv->load();
            p.attack     = attack->load();
            p.decay      = decay->load();
            p.sustain    = sustain->load();
            p.cycMode    = (int) cycMode->load();
            p.cycRise    = cycRise->load();
            p.cycHold    = cycHold->load();
            p.cycFall    = cycFall->load();
            p.cycAmount  = cycAmount->load();
            p.lfoShape   = (int) lfoShape->load();
            p.lfoRate    = lfoRate->load();
            p.lfoSync    = lfoSync->load() > 0.5f;
            p.lfoDiv     = (int) lfoDiv->load();
            p.voiceMode  = (int) voiceMode->load();
            for (int s = 0; s < numSources; ++s)
                for (int d = 0; d < numDests; ++d)
                    p.mod[s][d] = mod[s][d]->load();
            return p;
        }

    private:
        std::atomic<float>* get (const juce::String& id) const
        {
            auto* v = apvts.getRawParameterValue (id);
            jassert (v != nullptr);
            return v;
        }

        juce::AudioProcessorValueTreeState& apvts;
        std::atomic<float>* oscType    = get ("osc_type");
        std::atomic<float>* wave       = get ("osc_wave");
        std::atomic<float>* timbre     = get ("osc_timbre");
        std::atomic<float>* shape      = get ("osc_shape");
        std::atomic<float>* glide      = get ("glide");
        std::atomic<float>* filterType = get ("flt_type");
        std::atomic<float>* cutoff     = get ("flt_cutoff");
        std::atomic<float>* reso       = get ("flt_res");
        std::atomic<float>* filterEnv  = get ("flt_env");
        std::atomic<float>* attack     = get ("env_attack");
        std::atomic<float>* decay      = get ("env_decay");
        std::atomic<float>* sustain    = get ("env_sustain");
        std::atomic<float>* cycMode    = get ("cyc_mode");
        std::atomic<float>* cycRise    = get ("cyc_rise");
        std::atomic<float>* cycHold    = get ("cyc_hold");
        std::atomic<float>* cycFall    = get ("cyc_fall");
        std::atomic<float>* cycAmount  = get ("cyc_amount");
        std::atomic<float>* lfoShape   = get ("lfo_shape");
        std::atomic<float>* lfoRate    = get ("lfo_rate");
        std::atomic<float>* lfoSync    = get ("lfo_sync");
        std::atomic<float>* lfoDiv     = get ("lfo_div");
        std::atomic<float>* voiceMode  = get ("voice_mode");
        std::atomic<float>* mod[numSources][numDests] {};
    };
}
