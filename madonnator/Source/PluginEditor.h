#pragma once

#include "PluginProcessor.h"
#include "DesignTheme.h"

// Aussehen nach gewähltem Design (wie TRENDY ANDY): 1980 mit Fadern und Aufdruck, 2100 mit großen Gummipotis und blauer Leuchtschrift
class TrendyLook : public juce::LookAndFeel_V4
{
public:
    TrendyLook();

    void setTheme (const Design::Theme&);
    const Design::Theme& getTheme() const { return theme; }

    // squish: 0 = ruhend, 1 = ganz eingedrückt; dent: Druckpunkt relativ zur Mitte (-1..1)
    void drawRubberKnob (juce::Graphics&, juce::Rectangle<float> area, float pos, float squish,
                         juce::Point<float> dent, bool bipolar, float startAngle, float endAngle, bool mini);

    // Fader im Stil 1980: Schlitz, aufgedruckte Skala, schwarze Kappe mit weißem Strich
    void drawFader (juce::Graphics&, juce::Rectangle<float> area, float pos, bool bipolar, bool horizontal, float squish);

    int getSliderThumbRadius (juce::Slider&) override { return faderIndent; }
    static constexpr int faderIndent = 9;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;

private:
    Design::Theme theme;
};

class MadonnatorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MadonnatorEditor (MadonnatorProcessor&);
    ~MadonnatorEditor() override;

    void resized() override;

    static constexpr int designWidth = 1280, designHeight = 760;

private:
    TrendyLook look;
    std::unique_ptr<juce::Component> content;   // die ganze Oberfläche in Entwurfsgröße, wird auf die Fenstergröße skaliert

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MadonnatorEditor)
};
