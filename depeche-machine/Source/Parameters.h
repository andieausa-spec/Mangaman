#pragma once

#include <JuceHeader.h>

// Alle Parameter-IDs, Auswahllisten und das Parameter-Layout an einer Stelle.
namespace Params
{
    inline juce::String de (const char* text) { return juce::String::fromUTF8 (text); }

    // Stil-Epochen: bestimmen Tempo, Riffs, Bass, Drumcomputer, Melodieführung und Gesangsphrasen
    inline const juce::StringArray eras { "Synthpop 1981", "Industrial 1984", "Synth-Noir 1987", "Elektro-Blues 1990", "Dunkel 1993" };

    inline const juce::StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "B", "H" };

    // 0 = der Generator wählt passend zur Epoche und zur Nähe zum Original
    inline const juce::StringArray scales { "Auto", "Moll", "Dorisch", "Phrygisch", "Dur", "Mixolydisch" };

    inline const juce::StringArray padTypes { "Streicher", "Chor", "Glas" };
    inline const juce::StringArray seqWaves { de ("Säge"), "Puls", "Metall" };
    inline const juce::StringArray leadWaves { "Puls", de ("Säge"), "Metall", "Slide-Gitarre" };
    inline const juce::StringArray drumKits { "Analog 1981", "Metall 1984", de ("Groß 1987"), "Elektro 1990" };
    inline const juce::StringArray textStyles { de ("Kunstwörter"), "Lautmalerei", "Summen" };

    // Echo-Teilungen in Vierteln
    inline const juce::StringArray echoDivisions { "1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2" };
    inline constexpr double echoBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };

    inline constexpr int numLanes = 7;
    inline const char* const laneIds[] { "pad", "seq", "bass", "lead", "vocal", "drums", "fx" };

    // Parameter, deren Änderung einen neuen Song berechnet
    inline const char* const generatorIds[] { "length", "intensity", "closeness", "variety", "era", "key", "scale", "tempo", "vocal_text" };

    // Klangvoreinstellung je Epoche (Flächen, Riff-Welle, Melodie-Welle, Kit), wird beim Epochenwechsel gesetzt
    struct EraSound { int padType, seqWave, leadWave, drumKit; float padVowel, seqCutoff, bassDrive, echoMix, revMix, revSize; };
    inline constexpr EraSound eraSounds[] {
        { 0, 1, 0, 0, 0.5f, 0.55f, 0.2f, 0.3f, 0.25f, 0.55f },   // Synthpop 1981
        { 1, 2, 2, 1, 0.3f, 0.45f, 0.5f, 0.3f, 0.35f, 0.7f },    // Industrial 1984
        { 2, 0, 2, 2, 0.6f, 0.5f, 0.3f, 0.35f, 0.5f, 0.85f },    // Synth-Noir 1987
        { 0, 0, 3, 3, 0.4f, 0.5f, 0.45f, 0.3f, 0.3f, 0.65f },    // Elektro-Blues 1990
        { 1, 0, 3, 2, 0.2f, 0.35f, 0.4f, 0.35f, 0.5f, 0.9f },    // Dunkel 1993
    };

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
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "length", 1 }, de ("Länge"), NormalisableRange<float> (1.0f, 10.0f, 0.5f), 4.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            {
                const int secs = roundToInt (v * 60.0f);
                return String (secs / 60) + ":" + String (secs % 60).paddedLeft ('0', 2) + " min";
            })));
        addPercent ("intensity", de ("Intensität"), 0.6f);
        addPercent ("closeness", de ("Nähe zum Original"), 0.8f);
        addPercent ("variety", "Abwechslung", 0.5f);
        addChoice ("era", "Epoche", eras, 0);
        addChoice ("key", "Tonart", keys, 9);
        addChoice ("scale", "Skala", scales, 0);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo", NormalisableRange<float> (60.0f, 170.0f, 1.0f), 126.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " BPM"; })));
        addBool ("roll_all", de ("Tempo und Tonart mitwürfeln"), true);
        addBool ("era_sound", "Klang folgt der Epoche", true);
        addBool ("follow_host", "Host-Tempo", true);
        addBool ("loop", "Schleife", true);
        addBool ("midi_transpose", "MIDI transponiert", true);

        // Gesang
        addBool ("vocal_on", "Gesang", true);
        addChoice ("vocal_text", "Silben", textStyles, 0);
        addPercent ("vocal_depth", "Tiefe", 0.65f);
        addPercent ("vocal_breath", "Hauch", 0.3f);
        addPercent ("vocal_vibrato", "Vibrato", 0.45f);
        addPercent ("vocal_glide", "Gleiten", 0.35f);
        addPercent ("vocal_double", "Doppelung", 0.5f);
        addPercent ("vocal_harmony", "Zweitstimme", 0.5f);
        addPercent ("vocal_reverb", "Gesangshall", 0.45f);
        addPercent ("vocal_echo", "Gesangsecho", 0.3f);

        // Flächen
        addChoice ("pad_type", "Flächen-Klang", padTypes, 0);
        addPercent ("pad_bright", "Helligkeit", 0.5f);
        addPercent ("pad_attack", "Anschwellen", 0.4f);
        addPercent ("pad_ensemble", "Ensemble", 0.6f);
        addPercent ("pad_vowel", "Chor-Vokal", 0.5f);

        // Riff
        addPercent ("seq_cutoff", "Cutoff", 0.55f);
        addPercent ("seq_reso", "Resonanz", 0.45f);
        addPercent ("seq_env", "Filter-Kurve", 0.5f);
        addPercent ("seq_decay", "Abklingen", 0.3f);
        addChoice ("seq_wave", "Welle", seqWaves, 1);

        // Bass
        addPercent ("bass_cutoff", "Cutoff", 0.4f);
        addPercent ("bass_decay", "Abklingen", 0.35f);
        addPercent ("bass_sub", "Sub", 0.4f);
        addPercent ("bass_drive", "Biss", 0.2f);

        // Melodie
        addPercent ("lead_glide", "Gleiten", 0.3f);
        addPercent ("lead_vibrato", "Vibrato", 0.4f);
        addPercent ("lead_bright", "Helligkeit", 0.55f);
        addChoice ("lead_wave", "Welle", leadWaves, 0);

        // Drumcomputer
        addChoice ("drum_kit", "Kit", drumKits, 0);
        addPercent ("drum_tone", "Ton", 0.5f);
        addPercent ("drum_swing", "Swing", 0.0f);

        // Effekte
        addPercent ("fx_atmo", de ("Atmosphäre"), 0.55f);
        addPercent ("fx_swell", "Anlauf", 0.5f);
        addPercent ("fx_metal", "Metall", 0.5f);

        // Mischpult
        const char* volNames[] { "Flächen", "Riff", "Bass", "Melodie", "Gesang", "Rhythmus", "Effekte" };
        const float volDefaults[] { 0.62f, 0.62f, 0.68f, 0.58f, 0.72f, 0.66f, 0.55f };
        for (int i = 0; i < numLanes; ++i)
        {
            addPercent (String ("vol_") + laneIds[i], de (volNames[i]), volDefaults[i]);
            addBool (String ("mute_") + laneIds[i], de (volNames[i]) + " stumm", false);
            addBool (String ("solo_") + laneIds[i], de (volNames[i]) + " solo", false);
        }
        addChoice ("echo_div", "Echo-Teilung", echoDivisions, 2);
        addPercent ("echo_fb", "Echo-Feedback", 0.35f);
        addPercent ("echo_mix", "Echo", 0.3f);
        addPercent ("rev_size", de ("Raumgröße"), 0.55f);
        addPercent ("rev_mix", "Hall", 0.25f);
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
