#pragma once

#include <JuceHeader.h>

// Alle Parameter-IDs, Auswahllisten und das Parameter-Layout an einer Stelle.
namespace Params
{
    // Echo-Modus: ein Kopf, Ping-Pong, drei Köpfe (Bandecho-Art 1:2:3), breit (rechts 1,5-fach)
    inline const juce::StringArray modes { "Einfach", "Ping-Pong", "Drei Koepfe", "Breit" };
    enum Mode { Single, PingPong, ThreeHeads, Wide };

    // Notenwerte für Tempo-Sync, Länge in Vierteln
    inline const juce::StringArray divisions { "1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.",
                                               "1/4T", "1/4", "1/4.", "1/2", "1/2.", "1/1" };
    inline constexpr double divisionBeats[] { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75,
                                              2.0 / 3.0, 1.0, 1.5, 2.0, 3.0, 4.0 };

    // Bandgeschwindigkeit: halbe Geschwindigkeit = doppelte Echozeit, eine Oktave tiefer
    inline const juce::StringArray speeds { "Halb", "Normal", "Doppelt" };
    inline constexpr float speedFactors[] { 0.5f, 1.0f, 2.0f };

    // Wo der Federhall sitzt
    inline const juce::StringArray springPlaces { "Auf Echos", "In Schleife", "Auf alles" };
    enum SpringPlace { OnEchoes, InLoop, OnAll };

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto range = [] (float lo, float hi, float centre)
        {
            NormalisableRange<float> r (lo, hi);
            r.setSkewForCentre (centre);
            return r;
        };
        // Anzeige mit sinnvollen Stellen: "375 ms", "1.25 s", "140 Hz", "3.2 kHz", "-3.5 dB"
        auto addFloat = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit)
        {
            const String u (unit);
            auto text = [u] (float v, int)
            {
                if (u == "ms") return v < 1000.0f ? String (roundToInt (v)) + " ms" : String (v / 1000.0f, 2) + " s";
                if (u == "Hz") return v < 1000.0f ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 1) + " kHz";
                return (v > 0.05f ? "+" : "") + String (v, 1) + " " + u;
            };
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                               AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (text)));
        };
        auto addPercent = [&] (const char* id, const char* name, float def)
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
        };
        auto addChoice = [&] (const char* id, const char* name, const StringArray& items, int def)
        {
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
        };
        auto addBool = [&] (const char* id, const char* name, bool def)
        {
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
        };

        // Echo
        addChoice ("mode", "Modus", modes, Single);
        addBool   ("sync", "Sync", true);
        addChoice ("division", "Teilung", divisions, 6);   // 1/8 punktiert: der Dub-Klassiker
        addFloat  ("time", "Zeit", range (10.0f, 2000.0f, 350.0f), 375.0f, "ms");

        // Rückkopplungsschleife
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "feedback", 1 }, "Feedback", NormalisableRange<float> (0.0f, 1.2f), 0.6f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })));
        addFloat  ("low_cut", "Low Cut", range (20.0f, 2000.0f, 200.0f), 140.0f, "Hz");
        addFloat  ("high_cut", "High Cut", range (500.0f, 16000.0f, 3000.0f), 3200.0f, "Hz");
        addPercent ("resonance", "Resonanz", 0.25f);

        // Bandmaschine
        addPercent ("drive", "Saettigung", 0.45f);
        addPercent ("wow", "Wow", 0.3f);
        addPercent ("flutter", "Flutter", 0.25f);
        addPercent ("age", "Alter", 0.3f);
        addChoice ("speed", "Bandtempo", speeds, 1);
        addPercent ("glide", "Gleiten", 0.5f);

        // Federhall
        addPercent ("spring_mix", "Hall", 0.25f);
        addPercent ("spring_decay", "Laenge", 0.5f);
        addPercent ("spring_tone", "Ton", 0.5f);
        addPercent ("spring_boing", "Boing", 0.5f);
        addChoice ("spring_place", "Platz", springPlaces, OnEchoes);

        // Mischpult
        addPercent ("send", "Send", 1.0f);
        addPercent ("mix", "Mix", 0.35f);
        addFloat  ("output", "Ausgang", NormalisableRange<float> (-24.0f, 6.0f), 0.0f, "dB");
        addPercent ("ducking", "Ducking", 0.0f);
        addPercent ("width", "Breite", 0.7f);

        // Dub-Taster
        addBool ("throw", "Throw", false);
        addBool ("freeze", "Endlos", false);

        return layout;
    }

    // Schneller Zugriff auf die Rohwerte im Audio-Thread
    struct Reader
    {
        explicit Reader (juce::AudioProcessorValueTreeState& s)
        {
            for (auto* id : { "mode", "sync", "division", "time", "feedback", "low_cut", "high_cut", "resonance",
                              "drive", "wow", "flutter", "age", "speed", "glide",
                              "spring_mix", "spring_decay", "spring_tone", "spring_boing", "spring_place",
                              "send", "mix", "output", "ducking", "width", "throw", "freeze" })
                values.set (id, s.getRawParameterValue (id));
        }

        float operator[] (const char* id) const { return values[id]->load(); }

    private:
        juce::HashMap<juce::String, std::atomic<float>*> values;
    };
}
