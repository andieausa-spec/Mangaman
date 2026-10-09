#pragma once

#include <JuceHeader.h>

// Alle Parameter-IDs, Auswahllisten und das Parameter-Layout an einer Stelle.
namespace Params
{
    inline juce::String de (const char* text) { return juce::String::fromUTF8 (text); }

    // Stil-Epochen: bestimmen Tempo, Akkorde, Sequenzen, Bass, Rhythmus, Melodieführung, Geräusche und Sprechweise
    inline const juce::StringArray eras { "Motorik 1974", "Radiowellen 1975", "Schienen 1977", "Maschinen 1978", "Rechner 1981", "Digital 1986" };

    inline const juce::StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "B", "H" };

    // 0 = der Generator wählt passend zur Epoche und zur Nähe zum Original
    inline const juce::StringArray scales { "Auto", "Moll", "Dorisch", "Phrygisch", "Dur", "Mixolydisch" };

    inline const juce::StringArray padTypes { "Streicher", "Chor", "Orgel" };
    inline const juce::StringArray seqWaves { de ("Säge"), "Puls", "Glocke" };
    inline const juce::StringArray leadWaves { "Puls", de ("Säge"), "Glocke", de ("Flöte"), "Piepser" };
    inline const juce::StringArray drumKits { "Motorik 74", "Funk 75", "Schiene 77", "Maschine 78", "Rechner 81", "Digital 86" };
    inline const juce::StringArray textStyles { "Technik", "Zahlen", "Lautmalerei", "Eigener Text" };
    inline const juce::StringArray voiceTypes { "Vocoder", "Sprachchip", "Roboter" };

    // Echo-Teilungen in Vierteln
    inline const juce::StringArray echoDivisions { "1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2" };
    inline constexpr double echoBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };

    inline constexpr int numLanes = 7;
    inline const char* const laneIds[] { "pad", "seq", "bass", "lead", "vocal", "drums", "fx" };

    // Parameter, deren Änderung einen neuen Song berechnet
    inline const char* const generatorIds[] { "length", "intensity", "closeness", "variety", "era", "key", "scale", "tempo", "vocal_text" };

    // Klangvoreinstellung je Epoche (Flächen, Sequenz-Welle, Melodie-Welle, Kit, Stimme), wird beim Epochenwechsel gesetzt
    struct EraSound { int padType, seqWave, leadWave, drumKit, voiceType; float padVowel, seqCutoff, bassDrive, echoMix, revMix, revSize; };
    inline constexpr EraSound eraSounds[] {
        { 2, 0, 3, 0, 0, 0.4f, 0.5f, 0.15f, 0.3f, 0.3f, 0.6f },    // Motorik 1974: Orgel, Flöte, Vocoder
        { 1, 1, 0, 1, 0, 0.3f, 0.4f, 0.1f, 0.4f, 0.45f, 0.85f },   // Radiowellen 1975: Chor, langsame Pulse
        { 0, 0, 0, 2, 0, 0.5f, 0.5f, 0.2f, 0.3f, 0.35f, 0.75f },   // Schienen 1977: Streicher, Metall
        { 0, 0, 1, 3, 2, 0.5f, 0.55f, 0.25f, 0.25f, 0.25f, 0.6f }, // Maschinen 1978: Sechzehntel, Roboter
        { 0, 2, 4, 4, 1, 0.5f, 0.6f, 0.2f, 0.25f, 0.25f, 0.55f },  // Rechner 1981: Glocke, Piepser, Sprachchip
        { 1, 2, 2, 5, 0, 0.6f, 0.6f, 0.3f, 0.3f, 0.3f, 0.65f },    // Digital 1986: Glocken, harte Drums
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
        addChoice ("era", "Epoche", eras, 3);
        addChoice ("key", "Tonart", keys, 9);
        addChoice ("scale", "Skala", scales, 0);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo", NormalisableRange<float> (60.0f, 170.0f, 1.0f), 120.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " BPM"; })));
        addBool ("roll_all", de ("Tempo und Tonart mitwürfeln"), true);
        addBool ("era_sound", "Klang folgt der Epoche", true);
        addBool ("follow_host", "Host-Tempo", true);
        addBool ("loop", "Schleife", true);
        addBool ("midi_transpose", "MIDI transponiert", true);

        // Roboterstimme
        addBool ("vocal_on", "Stimme", true);
        addChoice ("vocal_text", "Text", textStyles, 0);
        addChoice ("voice_type", "Stimmart", voiceTypes, 2);
        addPercent ("vocal_melody", "Melodie", 0.5f);
        addPercent ("vocal_bands", de ("Auflösung"), 0.6f);
        addPercent ("vocal_carrier", de ("Träger"), 0.5f);
        addPercent ("vocal_crush", de ("Körnung"), 0.15f);
        addPercent ("vocal_metal", "Metall", 0.25f);
        addPercent ("vocal_double", "Doppelung", 0.4f);
        addPercent ("vocal_harmony", "Zweitstimme", 0.5f);
        addPercent ("vocal_reverb", "Stimmhall", 0.35f);
        addPercent ("vocal_echo", "Stimmecho", 0.3f);

        // Flächen
        addChoice ("pad_type", "Flächen-Klang", padTypes, 0);
        addPercent ("pad_bright", "Helligkeit", 0.5f);
        addPercent ("pad_attack", "Anschwellen", 0.4f);
        addPercent ("pad_ensemble", "Ensemble", 0.6f);
        addPercent ("pad_vowel", "Chor-Vokal", 0.5f);

        // Sequenz
        addPercent ("seq_cutoff", "Cutoff", 0.55f);
        addPercent ("seq_reso", "Resonanz", 0.45f);
        addPercent ("seq_env", "Filter-Kurve", 0.5f);
        addPercent ("seq_decay", "Abklingen", 0.3f);
        addChoice ("seq_wave", "Welle", seqWaves, 0);

        // Bass
        addPercent ("bass_cutoff", "Cutoff", 0.4f);
        addPercent ("bass_decay", "Abklingen", 0.35f);
        addPercent ("bass_sub", "Sub", 0.4f);
        addPercent ("bass_drive", "Biss", 0.2f);

        // Melodie
        addPercent ("lead_glide", "Gleiten", 0.2f);
        addPercent ("lead_vibrato", "Vibrato", 0.4f);
        addPercent ("lead_bright", "Helligkeit", 0.55f);
        addChoice ("lead_wave", "Welle", leadWaves, 1);

        // Drumcomputer
        addChoice ("drum_kit", "Kit", drumKits, 3);
        addPercent ("drum_tone", "Ton", 0.5f);
        addPercent ("drum_swing", "Swing", 0.0f);

        // Geräusche
        addPercent ("fx_noise", "Rauschen", 0.5f);
        addPercent ("fx_machine", "Maschinen", 0.6f);
        addPercent ("fx_signal", "Signale", 0.5f);

        // Mischpult
        const char* volNames[] { "Flächen", "Sequenz", "Bass", "Melodie", "Stimme", "Rhythmus", "Geräusche" };
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
