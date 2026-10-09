#pragma once

#include <JuceHeader.h>

// Alle Parameter-IDs, Auswahllisten und das Parameter-Layout an einer Stelle.
namespace Params
{
    inline juce::String de (const char* text) { return juce::String::fromUTF8 (text); }

    // Stil-Epochen: bestimmen Tempo, Sequenzer-Dichte, Rhythmusbox-Muster und Melodieführung
    inline const juce::StringArray eras { de ("Oxygène 1976"), de ("Équinoxe 1978"), de ("Magnetfelder 1981") };

    inline const juce::StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "B", "H" };

    // 0 = der Generator wählt passend zur Nähe zum Original
    inline const juce::StringArray scales { "Auto", "Moll", "Dorisch", "Phrygisch", "Dur", "Mixolydisch" };

    inline const juce::StringArray seqWaves { de ("Säge"), "Puls" };
    inline const juce::StringArray leadWaves { de ("Säge"), "Puls", "Pfeife" };
    inline const juce::StringArray drumKits { "Minipops-Stil", "Rhythm-Ace-Stil", "Elektronisch" };

    // Echo-Teilungen in Vierteln
    inline const juce::StringArray echoDivisions { "1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2" };
    inline constexpr double echoBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };

    inline constexpr int numLanes = 6;
    inline const char* const laneIds[] { "pad", "seq", "bass", "lead", "drums", "fx" };

    // Parameter, deren Änderung einen neuen Track berechnet
    inline const char* const generatorIds[] { "length", "intensity", "closeness", "variety", "era", "key", "scale", "tempo" };

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto addPercent = [&] (const String& id, const String& name, float def)
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
        };
        auto addChoice = [&] (const String& id, const String& name, const StringArray& items, int def)
        {
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
        };
        auto addBool = [&] (const String& id, const String& name, bool def)
        {
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
        };

        // Generator
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "length", 1 }, de ("Länge"), NormalisableRange<float> (1.0f, 15.0f, 0.5f), 5.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            {
                const int secs = roundToInt (v * 60.0f);
                return String (secs / 60) + ":" + String (secs % 60).paddedLeft ('0', 2) + " min";
            })));
        addPercent ("intensity", de ("Intensität"), 0.6f);
        addPercent ("closeness", de ("Nähe zum Original"), 0.8f);
        addPercent ("variety", "Abwechslung", 0.5f);
        addChoice ("era", "Epoche", eras, 0);
        addChoice ("key", "Tonart", keys, 0);
        addChoice ("scale", "Skala", scales, 0);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo", NormalisableRange<float> (60.0f, 160.0f, 1.0f), 108.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " BPM"; })));
        addBool ("roll_all", de ("Tempo und Tonart mitwürfeln"), true);
        addBool ("follow_host", "Host-Tempo", true);
        addBool ("loop", "Schleife", true);
        addBool ("midi_transpose", "MIDI transponiert", true);

        // Flächen (String-Maschine)
        addPercent ("pad_ensemble", "Ensemble", 0.65f);
        addPercent ("pad_phaser", "Phaser", 0.6f);
        addPercent ("pad_phaser_rate", "Phaser-Tempo", 0.25f);
        addPercent ("pad_bright", "Helligkeit", 0.5f);
        addPercent ("pad_attack", "Anschwellen", 0.55f);

        // Sequenz
        addPercent ("seq_cutoff", "Cutoff", 0.45f);
        addPercent ("seq_reso", "Resonanz", 0.55f);
        addPercent ("seq_env", "Filter-Kurve", 0.55f);
        addPercent ("seq_decay", "Abklingen", 0.35f);
        addChoice ("seq_wave", "Welle", seqWaves, 0);

        // Bass
        addPercent ("bass_cutoff", "Cutoff", 0.35f);
        addPercent ("bass_decay", "Abklingen", 0.5f);
        addPercent ("bass_sub", "Sub", 0.5f);

        // Melodie
        addPercent ("lead_glide", "Gleiten", 0.45f);
        addPercent ("lead_vibrato", "Vibrato", 0.5f);
        addPercent ("lead_bright", "Helligkeit", 0.55f);
        addChoice ("lead_wave", "Welle", leadWaves, 0);

        // Rhythmusbox
        addChoice ("drum_kit", "Kit", drumKits, 0);
        addPercent ("drum_tone", "Ton", 0.5f);
        addPercent ("drum_swing", "Swing", 0.0f);

        // Effekte
        addPercent ("fx_wind", "Wind", 0.6f);
        addPercent ("fx_laser", "Laser", 0.5f);
        addPercent ("fx_surf", "Brandung", 0.45f);

        // Mischpult
        const char* volNames[] { "Flächen", "Sequenz", "Bass", "Melodie", "Rhythmus", "Effekte" };
        const float volDefaults[] { 0.75f, 0.65f, 0.65f, 0.6f, 0.6f, 0.55f };
        for (int i = 0; i < numLanes; ++i)
        {
            addPercent (String ("vol_") + laneIds[i], de (volNames[i]), volDefaults[i]);
            addBool (String ("mute_") + laneIds[i], de (volNames[i]) + " stumm", false);
            addBool (String ("solo_") + laneIds[i], de (volNames[i]) + " solo", false);
        }
        addChoice ("echo_div", "Echo-Teilung", echoDivisions, 2);
        addPercent ("echo_fb", "Echo-Feedback", 0.45f);
        addPercent ("echo_mix", "Echo", 0.45f);
        addPercent ("rev_size", de ("Raumgröße"), 0.75f);
        addPercent ("rev_mix", "Hall", 0.45f);
        addPercent ("width", "Breite", 0.8f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master", NormalisableRange<float> (-24.0f, 6.0f), -3.0f,
            AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));

        return layout;
    }

    // Schneller Zugriff auf die Rohwerte im Audio-Thread
    struct Reader
    {
        explicit Reader (juce::AudioProcessorValueTreeState& s)
        {
            for (auto* p : s.processor.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    values.set (r->getParameterID(), s.getRawParameterValue (r->getParameterID()));
        }

        float operator[] (const juce::String& id) const { return values[id]->load(); }

    private:
        juce::HashMap<juce::String, std::atomic<float>*> values;
    };
}
