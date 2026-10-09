#pragma once

#include <JuceHeader.h>

// Zwei Gestaltungen der Oberfläche:
//  1980: Sampler-Gehäuse der Achtziger: kittfarbenes Blech, dunkle Bedienleiste, grünlich hinterleuchtete LCD-Anzeige,
//        Folientasten und Schieberegler, Beschriftung aufgedruckt (nachempfunden, ohne Logos)
//  2100: hell, steril, minimalistisch, große griffige Gummipotis, scharfe blaue Schrift, die von innen durchscheint
namespace Design
{
    enum Style { Retro1980 = 0, Future2100 = 1 };

    struct Theme
    {
        int style = Future2100;
        juce::Colour body, panel, line, knob, knobTrack, field, ink, dim, accent, glow;
        juce::Colour bezel, bezelInk, lcdBack, lcdInk, lcdGhost;   // dunkle Bedienleiste und LCD (nur 1980)
        bool faders() const { return style == Retro1980; }
    };

    inline Theme themeFor (int style)
    {
        Theme t;
        t.style = style == Retro1980 ? Retro1980 : Future2100;
        if (t.faders())
        {
            t.body = juce::Colour (0xffd3cdbf);   t.panel = juce::Colour (0xffe2dccf);
            t.line = juce::Colour (0xff4a4238);   t.knob = juce::Colour (0xff2a2826);
            t.knobTrack = juce::Colour (0xffaaa293); t.field = juce::Colour (0xffefeadf);
            t.ink = juce::Colour (0xff2a251f);    t.dim = juce::Colour (0xff6c6458);
            t.accent = juce::Colour (0xff2f6db5); t.glow = juce::Colours::transparentBlack;
            t.bezel = juce::Colour (0xff2b2926); t.bezelInk = juce::Colour (0xffe9e3d6);
            t.lcdBack = juce::Colour (0xffa6ba52); t.lcdInk = juce::Colour (0xff17200b);
            t.lcdGhost = juce::Colour (0x1c17200b);
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

    // Feine Körnung für das Gehäuse (nahtlos kachelbar)
    inline juce::Image plasticGrain()
    {
        const int size = 160;
        juce::Random rng (5);
        juce::Image img (juce::Image::ARGB, size, size, true);
        juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                data.setPixelColour (x, y, juce::Colours::black.withAlpha (rng.nextFloat() * 0.06f));
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
