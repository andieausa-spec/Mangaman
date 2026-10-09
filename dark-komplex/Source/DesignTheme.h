#pragma once

#include <JuceHeader.h>

// Zwei Gestaltungen der Oberfläche:
//  1980: dunkles Plastik, Schieberegler statt Potis, Beschriftung und Skalen aufgedruckt (Roland-Geräte jener Zeit)
//  2100: hell, steril, minimalistisch, große griffige Gummipotis, scharfe blaue Schrift, die von innen durchscheint
namespace Design
{
    enum Style { Retro1980 = 0, Future2100 = 1 };

    struct Theme
    {
        int style = Future2100;
        juce::Colour body, panel, line, knob, knobTrack, field, ink, dim, accent, glow;
        bool faders() const { return style == Retro1980; }
    };

    inline Theme themeFor (int style)
    {
        Theme t;
        t.style = style == Retro1980 ? Retro1980 : Future2100;
        if (t.faders())
        {
            t.body = juce::Colour (0xff1c1d20);   t.panel = juce::Colour (0xff27282c);
            t.line = juce::Colour (0xffbdbdb6);   t.knob = juce::Colour (0xff121315);
            t.knobTrack = juce::Colour (0xff44464c); t.field = juce::Colour (0xff151619);
            t.ink = juce::Colour (0xffefeee8);    t.dim = juce::Colour (0xffa4a6ab);
            t.accent = juce::Colour (0xfff2672a); t.glow = juce::Colours::transparentBlack;
        }
        else
        {
            t.body = juce::Colour (0xffeceef1);   t.panel = juce::Colour (0xfff8f9fa);
            t.line = juce::Colour (0xffe2e5e9);   t.knob = juce::Colour (0xffd5d9de);
            t.knobTrack = juce::Colour (0xffe3e6ea); t.field = juce::Colour (0xffffffff);
            t.ink = juce::Colour (0xff1450ff);    t.dim = juce::Colour (0xff6f93ff);
            t.accent = juce::Colour (0xff1450ff); t.glow = juce::Colour (0x161450ff);
        }
        return t;
    }

    // Feine Körnung für das dunkle Plastik (nahtlos kachelbar)
    inline juce::Image plasticGrain()
    {
        const int size = 160;
        juce::Random rng (5);
        juce::Image img (juce::Image::ARGB, size, size, true);
        juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                data.setPixelColour (x, y, juce::Colours::white.withAlpha (rng.nextFloat() * 0.05f));
        return img;
    }

    // Schrift, die von innen durch das Material scheint (2100); bei 1980 einfach gedruckt
    inline void drawText (juce::Graphics& g, const Theme& t, const juce::String& text, juce::Rectangle<float> r,
                          juce::Justification just, juce::Colour colour)
    {
        if (! t.faders())
        {
            g.setColour (t.glow);
            for (auto o : { juce::Point<float> (-1.0f, 0.0f), { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
                g.drawText (text, r + o, just);
        }
        g.setColour (colour);
        g.drawText (text, r, just);
    }
}
