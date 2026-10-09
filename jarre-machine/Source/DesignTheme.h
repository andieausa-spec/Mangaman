#pragma once

#include <JuceHeader.h>

// Zwei Gestaltungen der Oberfläche:
//  1970: Holzrahmen, schwarze Frontplatte mit cremefarbenem Siebdruck, bunte Schieberegler mit Spiel,
//        Glühlampen, Nixie-Röhren und Zeigerinstrumente (Synthesizer jener Zeit)
//  2100: hell, steril, minimalistisch, große griffige Gummipotis, scharfe blaue Schrift, die von innen durchscheint
namespace Design
{
    enum Style { Retro1970 = 0, Future2100 = 1 };

    struct Theme
    {
        int style = Future2100;
        juce::Colour body, panel, plate, line, knob, knobTrack, field, ink, dim, accent, glow, lamp;
        bool faders() const { return style == Retro1970; }
    };

    inline Theme themeFor (int style)
    {
        Theme t;
        t.style = style == Retro1970 ? Retro1970 : Future2100;
        if (t.faders())
        {
            t.body = juce::Colour (0xff3b2416);   t.panel = juce::Colour (0xff1e1b18);
            t.plate = juce::Colour (0xff23201c);  t.line = juce::Colour (0xffe9dfc6);
            t.knob = juce::Colour (0xff191715);   t.knobTrack = juce::Colour (0xff4a4339);
            t.field = juce::Colour (0xff120d09);  t.ink = juce::Colour (0xffefe5cc);
            t.dim = juce::Colour (0xffb9a98a);    t.accent = juce::Colour (0xffe8742a);
            t.glow = juce::Colours::transparentBlack; t.lamp = juce::Colour (0xffffab3d);
        }
        else
        {
            t.body = juce::Colour (0xffeceef1);   t.panel = juce::Colour (0xfff8f9fa);
            t.plate = t.body;                     t.line = juce::Colour (0xffe2e5e9);
            t.knob = juce::Colour (0xffd5d9de);   t.knobTrack = juce::Colour (0xffe3e6ea);
            t.field = juce::Colour (0xffffffff);  t.ink = juce::Colour (0xff1450ff);
            t.dim = juce::Colour (0xff6f93ff);    t.accent = juce::Colour (0xff1450ff);
            t.glow = juce::Colour (0x161450ff);   t.lamp = t.accent;
        }
        return t;
    }

    // Feine Körnung für die Frontplatte (nahtlos kachelbar)
    inline juce::Image plasticGrain()
    {
        const int size = 160;
        juce::Random rng (5);
        juce::Image img (juce::Image::ARGB, size, size, true);
        juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                data.setPixelColour (x, y, juce::Colours::white.withAlpha (rng.nextFloat() * 0.04f));
        return img;
    }

    // Nussbaum-Maserung für den Holzrahmen
    inline void paintWood (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::ColourGradient base (juce::Colour (0xff4a2c19), r.getX(), 0.0f, juce::Colour (0xff43271a), r.getRight(), 0.0f, false);
        base.addColour (0.4, juce::Colour (0xff5a3820));
        g.setGradientFill (base);
        g.fillRect (r);
        juce::Random rng (19);
        for (int i = 0; i < 420; ++i)
        {
            const float x = r.getX() + rng.nextFloat() * r.getWidth();
            const float slope = (rng.nextFloat() - 0.5f) * 0.08f;
            const bool dark = rng.nextFloat() < 0.7f;
            g.setColour (dark ? juce::Colours::black.withAlpha (0.05f + rng.nextFloat() * 0.12f)
                              : juce::Colour (0xffffc88c).withAlpha (0.03f + rng.nextFloat() * 0.04f));
            g.drawLine (x, r.getY(), x + slope * r.getHeight(), r.getBottom(), 0.6f + rng.nextFloat() * 1.6f);
        }
    }

    // Glühlampe: aus = dunkles Glas, an = warm leuchtend mit Hof
    inline void paintLamp (juce::Graphics& g, juce::Rectangle<float> r, bool lit)
    {
        if (lit)
        {
            g.setColour (juce::Colour (0xffffa032).withAlpha (0.35f));
            g.fillEllipse (r.expanded (5.0f));
        }
        juce::ColourGradient glass (lit ? juce::Colour (0xfffff3c4) : juce::Colour (0xff6a5236), r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.35f,
                                    lit ? juce::Colour (0xffc25a12) : juce::Colour (0xff2c2116), r.getRight(), r.getBottom(), true);
        if (lit) glass.addColour (0.45, juce::Colour (0xffffab3d));
        g.setGradientFill (glass);
        g.fillEllipse (r);
        g.setColour (juce::Colour (0xff0b0908));
        g.drawEllipse (r, 1.5f);
    }

    // Schrift, die von innen durch das Material scheint (2100); bei 1970 einfach gedruckt
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
