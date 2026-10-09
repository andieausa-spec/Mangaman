#include "PluginEditor.h"

namespace
{
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    TrendyLook& lookOf (juce::Component& c) { return static_cast<TrendyLook&> (c.getLookAndFeel()); }

    // Versionsanzeige aus CMakeLists (0.1.0 -> "V0.1")
    juce::String versionLabel()
    {
        auto v = juce::String (JucePlugin_VersionString);
        while (v.endsWith (".0") && v.containsChar ('.') && v.indexOfChar ('.') != v.lastIndexOfChar ('.'))
            v = v.dropLastCharacters (2);
        return "V" + v;
    }

    juce::String makerLabel() { return juce::String::fromUTF8 ("ANDREAS ENGEL \xc2\xb7 DARMSTADT \xc2\xb7 DEUTSCHLAND"); }

    //==========================================================================
    // Drehregler mit Gummi-Kappe: gibt beim Berühren nach, folgt dem Wert federnd
    class RubberSlider : public juce::Slider, private juce::Timer
    {
    public:
        explicit RubberSlider (bool isMini = false) : mini (isMini)
        {
            // 1970: jeder Schieberegler hat etwas Spiel, sitzt leicht schief und hat seine eigene Kappenfarbe
            static int counter = 0;
            capIndex = counter++;
            auto& rng = juce::Random::getSystemRandom();
            play = (rng.nextFloat() - 0.5f) * 0.14f;
            side = (rng.nextFloat() - 0.5f) * 3.0f;
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            setMouseDragSensitivity (220);
            if (mini)
            {
                setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                setPopupDisplayEnabled (true, true, nullptr);
            }
        }

        // Design 1970: Fader statt Drehregler (kleine liegen waagerecht), Wert darunter
        void setFaderMode (bool f)
        {
            fader = f;
            if (fader)
            {
                setSliderStyle (mini ? juce::Slider::LinearHorizontal : juce::Slider::LinearVertical);
                setSliderSnapsToMousePosition (false);
                if (! mini)
                    setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
            }
            else
            {
                setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                if (! mini)
                    setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
            }
            repaint();
        }
        bool isFader() const { return fader; }

        void paint (juce::Graphics& g) override
        {
            if (! started)
            {
                shown = (float) valueToProportionOfLength (getValue());
                started = true;
            }
            const auto layout = getLookAndFeel().getSliderLayout (*this);
            if (fader)
            {
                lookOf (*this).drawFader (g, layout.sliderBounds.toFloat(), shown, getMinimum() < 0.0 && getMaximum() > 0.0, mini, squish,
                                          juce::jlimit (-0.35f, 0.35f, play + tilt), side + juce::jlimit (-3.0f, 3.0f, tilt * 6.0f), capIndex);
                return;
            }
            const auto rp = getRotaryParameters();
            lookOf (*this).drawRubberKnob (g, layout.sliderBounds.toFloat(), shown, squish, dent,
                                           getMinimum() < 0.0 && getMaximum() > 0.0,
                                           rp.startAngleRadians, rp.endAngleRadians, mini);
        }

        void valueChanged() override { animate(); }

        void mouseEnter (const juce::MouseEvent& e) override { juce::Slider::mouseEnter (e); squishTarget = 0.35f; animate(); }
        void mouseExit (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseExit (e);
            if (! isMouseButtonDown())
                squishTarget = 0.0f;
            animate();
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseDown (e);
            const auto c = getLookAndFeel().getSliderLayout (*this).sliderBounds.toFloat();
            const float r = juce::jmax (1.0f, juce::jmin (c.getWidth(), c.getHeight()) * 0.5f);
            dent = { juce::jlimit (-1.0f, 1.0f, (e.position.x - c.getCentreX()) / r),
                     juce::jlimit (-1.0f, 1.0f, (e.position.y - c.getCentreY()) / r) };
            squishTarget = 1.0f;
            animate();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseDrag (e);
            if (fader)
            {
                tilt += (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.05f;   // der alte Regler ruckelt im Schlitz
                animate();
            }
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseUp (e);
            squishTarget = isMouseOver() ? 0.35f : 0.0f;
            animate();
        }

    private:
        void animate() { if (! isTimerRunning()) startTimerHz (60); }

        void timerCallback() override
        {
            const float target = (float) valueToProportionOfLength (getValue());
            const float err = target - shown;
            if (fader)
            {
                velocity = velocity * 0.8f + err * 0.16f;     // lockerer Schieber: schwingt nach und kippt dabei
                tilt = tilt * 0.86f + velocity * 2.6f;
            }
            else
                velocity = velocity * 0.5f + err * 0.35f;     // leichtes Überschwingen wie Gummi
            shown += velocity;

            const float sqErr = squishTarget - squish;
            squishVelocity = squishVelocity * 0.55f + sqErr * 0.3f;
            squish += squishVelocity;

            if (std::abs (err) < 1.0e-4f && std::abs (velocity) < 1.0e-4f && std::abs (tilt) < 2.0e-3f
                && std::abs (sqErr) < 1.0e-3f && std::abs (squishVelocity) < 1.0e-3f)
            {
                shown = target;
                squish = squishTarget;
                tilt = 0.0f;
                stopTimer();
            }
            repaint();
        }

        bool mini, started = false, fader = false;
        float shown = 0.0f, velocity = 0.0f, tilt = 0.0f, play = 0.0f, side = 0.0f;
        int capIndex = 0;
        float squish = 0.0f, squishVelocity = 0.0f, squishTarget = 0.0f;
        juce::Point<float> dent;
    };

    //==========================================================================
    // Bedienelement mit Beschriftung darüber
    class Labelled : public juce::Component
    {
    public:
        explicit Labelled (const juce::String& name)
        {
            label.setText (name, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            addAndMakeVisible (label);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (16));
            layoutControl (r);
        }

        virtual void layoutControl (juce::Rectangle<int>) = 0;
        virtual int preferredWidth() const = 0;
        virtual RubberSlider* getSlider() { return nullptr; }

        juce::Label label;
    };

    class KnobControl : public Labelled
    {
    public:
        KnobControl (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name, int size = 84)
            : Labelled (name), knobSize (size)
        {
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
            addAndMakeVisible (slider);
            attachment = std::make_unique<SliderAttachment> (s, id, slider);
            // Doppelklick setzt auf den Grundwert zurück
            if (auto* prm = s.getParameter (id))
                slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        }

        void layoutControl (juce::Rectangle<int> r) override
        {
            if (slider.isFader())
            {
                slider.setBounds (r.withSizeKeepingCentre (juce::jmin (64, r.getWidth()), r.getHeight()).withTrimmedBottom (2));
                return;
            }
            const int size = juce::jmin (r.getWidth(), r.getHeight() - 18, knobSize);
            slider.setBounds (r.withSizeKeepingCentre (r.getWidth(), size + 18).withY (r.getY()));
        }
        int preferredWidth() const override { return slider.isFader() ? (knobSize >= 100 ? 110 : 64) : juce::jmax (80, knobSize + 8); }

        RubberSlider* getSlider() override { return &slider; }

        RubberSlider slider;
        int knobSize;
        std::unique_ptr<SliderAttachment> attachment;
    };

    class ChoiceControl : public Labelled
    {
    public:
        ChoiceControl (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name, int width = 132)
            : Labelled (name), prefWidth (width)
        {
            box.addItemList (s.getParameter (id)->getAllValueStrings(), 1);
            addAndMakeVisible (box);
            attachment = std::make_unique<ComboAttachment> (s, id, box);
        }

        void layoutControl (juce::Rectangle<int> r) override
        {
            box.setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, 28).withY (r.getY() + 8));
        }
        int preferredWidth() const override { return prefWidth; }

        juce::ComboBox box;
        std::unique_ptr<ComboAttachment> attachment;
        int prefWidth;
    };

    class ToggleControl : public Labelled
    {
    public:
        ToggleControl (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& name,
                       juce::String onText = "an", juce::String offText = "aus")
            : Labelled (name)
        {
            button.setClickingTogglesState (true);
            button.setButtonText (offText);
            button.onStateChange = [this, onText, offText] { button.setButtonText (button.getToggleState() ? onText : offText); };
            addAndMakeVisible (button);
            attachment = std::make_unique<ButtonAttachment> (s, id, button);
        }

        void layoutControl (juce::Rectangle<int> r) override
        {
            button.setBounds (r.withSizeKeepingCentre (juce::jmin (84, r.getWidth() - 4), 28).withY (r.getY() + 8));
        }
        int preferredWidth() const override { return 90; }

        juce::TextButton button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    //==========================================================================
    // Reiter als Hardware-Taster mit LED
    class TabButton : public juce::Button
    {
    public:
        TabButton (const juce::String& title, const juce::String& sub, int group = 1001) : juce::Button (title), subtitle (sub)
        {
            setClickingTogglesState (true);
            setRadioGroupId (group);
        }

        bool blink = false, blinkPhase = false;

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto& th = lookOf (*this).getTheme();
            const bool on = getToggleState();
            const float press = on ? 3.0f : down ? 4.0f : highlighted ? 1.0f : 0.0f;
            const float rad = th.faders() ? 4.0f : 16.0f;
            const auto ledColour = th.accent;
            auto r = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
            r.removeFromBottom (5.0f);

            // Sockel und Schatten
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (r.translated (0.0f, 6.0f).expanded (0.5f), rad);
            g.setColour (th.faders() ? juce::Colour (0xff0b0908) : th.panel.interpolatedWith (th.ink, 0.45f));
            g.fillRoundedRectangle (r.translated (0.0f, 5.0f), rad);

            auto face = r.translated (0.0f, press);
            if (th.faders())
            {
                // 1970: dunkler Taster mit Glühlampe
                juce::ColourGradient grad (on ? juce::Colour (0xff24201c) : juce::Colour (0xff2c2823), face.getX(), face.getY(),
                                           on ? juce::Colour (0xff2e2a25) : juce::Colour (0xff1a1714), face.getX(), face.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (face, rad);
                g.setColour (juce::Colour (0xff0b0908));
                g.drawRoundedRectangle (face, rad, 1.0f);
                Design::paintLamp (g, juce::Rectangle<float> (13.0f, 13.0f).withCentre ({ face.getX() + 17.0f, face.getCentreY() }), on || (blink && blinkPhase));
            }
            else
            {
            juce::ColourGradient grad (on ? th.panel.interpolatedWith (th.ink, 0.12f) : th.panel.brighter (0.06f),
                                       face.getX(), face.getY(),
                                       on ? th.panel : th.panel.interpolatedWith (th.ink, 0.14f),
                                       face.getX(), face.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (face, rad);
            g.setColour (th.ink.withAlpha (0.22f));
            g.drawRoundedRectangle (face, rad, 1.0f);
            if (! on)
            {
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.drawLine (face.getX() + 8.0f, face.getY() + 1.5f, face.getRight() - 8.0f, face.getY() + 1.5f, 1.0f);
            }

            // LED
            const bool lit = on || (blink && blinkPhase);
            auto led = juce::Rectangle<float> (10.0f, rad).withCentre ({ face.getX() + 17.0f, face.getCentreY() });
            if (lit)
            {
                g.setColour (ledColour.withAlpha (0.35f));
                g.fillEllipse (led.expanded (5.0f));
            }
            g.setColour (lit ? ledColour : th.knobTrack.overlaidWith (ledColour.withAlpha (0.25f)));
            g.fillEllipse (led);
            }

            auto area = face.withTrimmedLeft (32.0f).reduced (4.0f, 6.0f);
            g.setColour (th.ink);
            g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
            if (subtitle.isEmpty())
            {
                g.drawText (getButtonText(), area, juce::Justification::centredLeft);
                return;
            }
            g.drawText (getButtonText().toUpperCase(), area.removeFromTop (area.getHeight() * 0.56f), juce::Justification::bottomLeft);
            g.setColour (th.dim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (subtitle, area, juce::Justification::topLeft);
        }

    private:
        juce::String subtitle;
    };

    //==========================================================================
    struct Panel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        std::vector<Labelled*> items;
    };

    class Page : public juce::Component
    {
    public:
        std::vector<Panel> panels;
        std::function<void (juce::Graphics&)> paintExtra;

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            static const juce::Colour stripes[] { juce::Colour (0xffe8742a), juce::Colour (0xffdba534),
                                                  juce::Colour (0xff8aa23c), juce::Colour (0xff4f8f9a) };
            int index = 0;
            for (auto& p : panels)
            {
                const auto r = p.bounds.toFloat();
                if (th.faders())
                {
                    // 1970: gerundeter Siebdruck-Rahmen, Titel mittig in einem Schild auf der Linie
                    auto frame = r.reduced (0.75f).withTrimmedTop (10.0f);
                    g.setColour (juce::Colours::black.withAlpha (0.12f));
                    g.fillRoundedRectangle (frame, 10.0f);
                    g.setColour (th.line.withAlpha (0.7f));
                    g.drawRoundedRectangle (frame, 10.0f, 1.5f);
                    g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.25f));
                    const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), p.title) + 28.0f;
                    auto plate = juce::Rectangle<float> (juce::jmin (w, r.getWidth() - 20.0f), 20.0f).withCentre ({ r.getCentreX(), r.getY() + 10.0f });
                    g.setColour (th.plate);
                    g.fillRoundedRectangle (plate, 10.0f);
                    g.setColour (stripes[index % 4]);
                    g.drawRoundedRectangle (plate, 10.0f, 1.5f);
                    g.setColour (th.ink);
                    g.drawText (p.title, plate, juce::Justification::centred);
                }
                else
                {
                    // 2100: Fläche ohne Rahmen, Schrift scheint blau von innen durch
                    juce::DropShadow (juce::Colour (0x22283c5a), 22, { 0, 10 }).drawForRectangle (g, p.bounds);
                    g.setColour (th.panel);
                    g.fillRoundedRectangle (r, 18.0f);
                    g.setColour (th.accent);
                    g.fillEllipse ((float) p.bounds.getX() + 16.0f, (float) p.bounds.getY() + 16.0f, 6.0f, 6.0f);
                    g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.2f));
                    Design::drawText (g, th, p.title, juce::Rectangle<float> ((float) p.bounds.getX() + 30.0f, (float) p.bounds.getY() + 9.0f,
                                                                              (float) p.bounds.getWidth() - 40.0f, 20.0f),
                                      juce::Justification::centredLeft, th.ink);
                }
                ++index;
            }
            if (paintExtra)
                paintExtra (g);
        }

        // Regler eines Feldes nebeneinander, links beginnend
        static void layout (Panel& p, int controlHeight, int offsetY = 0)
        {
            auto inner = p.bounds.reduced (14, 0).withTrimmedTop (40 + offsetY);
            for (auto* c : p.items)
            {
                c->setBounds (inner.removeFromLeft (c->preferredWidth()).withHeight (controlHeight));
                inner.removeFromLeft (8);
            }
        }
    };

    //==========================================================================
    // Anzeige wie ein Gerätedisplay: 1970 orange Nixie-Röhren, 2100 blau leuchtende Schrift
    class LedDisplay : public juce::Component
    {
    public:
        void setText (const juce::String& big, const juce::String& small)
        {
            if (big != bigText || small != smallText)
            {
                bigText = big;
                smallText = small;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            auto r = getLocalBounds().toFloat().reduced (2.0f);
            if (th.faders())
            {
                // 1970: Nixie-Röhren hinter dunklem Glas mit Gitter
                g.setColour (juce::Colour (0xff0b0908));
                g.fillRoundedRectangle (r, 8.0f);
                auto glass = r.reduced (3.0f);
                juce::ColourGradient bg (juce::Colour (0xff2a1608), glass.getCentreX(), glass.getY() + glass.getHeight() * 0.4f,
                                         juce::Colour (0xff0b0603), glass.getRight(), glass.getBottom(), true);
                g.setGradientFill (bg);
                g.fillRoundedRectangle (glass, 6.0f);
                g.setColour (juce::Colours::white.withAlpha (0.03f));
                for (float x = glass.getX(); x < glass.getRight(); x += 3.0f) g.drawVerticalLine ((int) x, glass.getY(), glass.getBottom());
                for (float y = glass.getY(); y < glass.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, glass.getX(), glass.getRight());
                auto inner = glass.reduced (12.0f, 4.0f);
                auto top = inner.removeFromTop (inner.getHeight() * 0.66f);
                const juce::FontOptions digits (juce::Font::getDefaultMonospacedFontName(), top.getHeight() * 0.86f, juce::Font::plain);
                g.setFont (digits);
                // die nicht leuchtenden Ziffern stehen dunkel in der Röhre
                g.setColour (juce::Colour (0xff3a2210));
                juce::String ghost;
                for (auto ch : bigText)
                    ghost << (juce::CharacterFunctions::isDigit (ch) ? juce::juce_wchar ('8') : ch);
                g.drawText (ghost, top, juce::Justification::centredLeft);
                for (auto [d, a] : { std::pair<float, float> { 3.0f, 0.10f }, { 2.0f, 0.18f }, { 1.0f, 0.3f } })
                {
                    g.setColour (juce::Colour (0xffff7a1a).withAlpha (a));
                    for (auto o : { juce::Point<float> (-d, 0.0f), { d, 0.0f }, { 0.0f, -d }, { 0.0f, d } })
                        g.drawText (bigText, top + o, juce::Justification::centredLeft);
                }
                g.setColour (juce::Colour (0xffff9a3c));
                g.drawText (bigText, top, juce::Justification::centredLeft);
                g.setColour (juce::Colour (0xffe07b2e));
                g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
                g.drawText (smallText, inner, juce::Justification::centredLeft);
            }
            else
            {
                juce::DropShadow (juce::Colour (0x1a283c5a), 12, { 0, 4 }).drawForRectangle (g, r.toNearestInt());
                g.setColour (th.field);
                g.fillRoundedRectangle (r, 16.0f);
                auto inner = r.reduced (18.0f, 8.0f);
                auto top = inner.removeFromTop (inner.getHeight() * 0.66f);
                g.setFont (juce::FontOptions (top.getHeight() * 0.85f, juce::Font::bold).withKerningFactor (0.04f));
                Design::drawText (g, th, bigText, top, juce::Justification::centredLeft, th.ink);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.12f));
                Design::drawText (g, th, smallText, inner, juce::Justification::centredLeft, th.dim);
            }
        }

    private:
        juce::String bigText, smallText;
    };

    //==========================================================================
    // Pegelanzeige für Eingang und Ausgang
    class Meter : public juce::Component
    {
    public:
        void setLevels (float in, float out)
        {
            const auto toPos = [] (float v) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -60.0f) + 60.0f) / 66.0f); };
            const float a = toPos (in), b = toPos (out);
            if (std::abs (a - levels[0]) > 0.005f || std::abs (b - levels[1]) > 0.005f)
            {
                levels = { a, b };
                repaint();
            }
            // träger Zeiger der Zeigerinstrumente (1970)
            for (int c = 0; c < 2; ++c)
            {
                needleVel[(size_t) c] = needleVel[(size_t) c] * 0.55f + (levels[(size_t) c] - needle[(size_t) c]) * 0.3f;
                needle[(size_t) c] += needleVel[(size_t) c];
            }
            if (std::abs (needleVel[0]) + std::abs (needleVel[1]) > 0.002f)
                repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            auto r = getLocalBounds().toFloat();
            const char* names[] { "L", "R" };
            if (th.faders())
            {
                // 1970: zwei Zeigerinstrumente übereinander, cremefarbene Skala im Lampenlicht
                const float h = (r.getHeight() - 4.0f) * 0.5f;
                for (int c = 0; c < 2; ++c)
                {
                    auto face = juce::Rectangle<float> (r.getX() + 2.0f, r.getY() + (h + 4.0f) * (float) c, r.getWidth() - 4.0f, h);
                    g.setColour (juce::Colour (0xff0b0908));
                    g.fillRoundedRectangle (face.expanded (1.0f), 4.0f);
                    juce::ColourGradient bg (juce::Colour (0xfffff1c8), face.getCentreX(), face.getBottom(), juce::Colour (0xffd9c08a), face.getX(), face.getY(), true);
                    g.setGradientFill (bg);
                    g.fillRoundedRectangle (face, 3.0f);
                    const juce::Point<float> pivot { face.getCentreX(), face.getBottom() + 8.0f };
                    const float rad = face.getHeight() - 2.0f;
                    for (int t = 0; t <= 10; ++t)
                    {
                        const float a = -0.8f + 1.6f * (float) t / 10.0f;
                        g.setColour (t >= 8 ? juce::Colour (0xffb8321c) : juce::Colour (0xff2a2016));
                        g.drawLine ({ pivot.getPointOnCircumference (rad - 4.0f, a), pivot.getPointOnCircumference (rad - 9.0f, a) }, 1.0f);
                    }
                    juce::Path red;
                    red.addCentredArc (pivot.x, pivot.y, rad - 4.0f, rad - 4.0f, 0.0f, 0.48f, 0.8f, true);
                    g.setColour (juce::Colour (0xffb8321c));
                    g.strokePath (red, juce::PathStrokeType (2.0f));
                    g.setColour (juce::Colour (0xff2a2016));
                    g.setFont (juce::FontOptions (7.5f, juce::Font::bold));
                    g.drawText (names[c], face.reduced (4.0f, 2.0f), juce::Justification::bottomLeft);
                    g.drawText ("VU", face.reduced (4.0f, 2.0f), juce::Justification::bottomRight);
                    const float a = -0.8f + 1.6f * juce::jlimit (0.0f, 1.05f, needle[(size_t) c]);
                    juce::Graphics::ScopedSaveState clip (g);
                    g.reduceClipRegion (face.toNearestInt());
                    g.setColour (juce::Colour (0xff1a120a));
                    g.drawLine ({ pivot, pivot.getPointOnCircumference (rad - 2.0f, a) }, 1.3f);
                }
                return;
            }
            const float colW = r.getWidth() / 2.0f;
            for (int c = 0; c < 2; ++c)
            {
                auto col = juce::Rectangle<float> (r.getX() + colW * (float) c, r.getY(), colW, r.getHeight()).reduced (6.0f, 0.0f);
                auto label = col.removeFromBottom (16.0f);
                g.setColour (th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
                g.drawText (names[c], label, juce::Justification::centred);
                col.removeFromBottom (4.0f);
                if (false)
                {
                    // (früher: LED-Kette)
                    const int segs = 12;
                    const float segH = col.getHeight() / (float) segs;
                    for (int i = 0; i < segs; ++i)
                    {
                        auto seg = juce::Rectangle<float> (col.getX(), col.getBottom() - segH * (float) (i + 1), col.getWidth(), segH).reduced (1.0f, 1.2f);
                        const auto on = i >= 10 ? juce::Colour (0xffff3b24) : i >= 8 ? juce::Colour (0xfff2c230) : juce::Colour (0xff4bd04b);
                        const bool lit = levels[(size_t) c] * (float) segs > (float) i + 0.5f;
                        g.setColour (lit ? on : on.withAlpha (0.12f));
                        g.fillRect (seg);
                    }
                }
                else
                {
                    g.setColour (th.knobTrack);
                    g.fillRoundedRectangle (col.withSizeKeepingCentre (8.0f, col.getHeight()), 4.0f);
                    const float h = col.getHeight() * levels[(size_t) c];
                    g.setColour (levels[(size_t) c] > 0.92f ? juce::Colour (0xffff4d6d) : th.accent);
                    g.fillRoundedRectangle (juce::Rectangle<float> (col.getCentreX() - 4.0f, col.getBottom() - h, 8.0f, h), 4.0f);
                }
            }
        }

    private:
        std::array<float, 2> levels {}, needle {}, needleVel {};
    };

    //==========================================================================
    // Große Dub-Taster: THROW (nur solange gedrückt), ENDLOS (rastet ein), Bandtempo
    class DubPad : public juce::Button
    {
    public:
        DubPad (const juce::String& title, const juce::String& sub, bool bigText = true)
            : juce::Button (title), subtitle (sub), big (bigText) {}

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto& th = lookOf (*this).getTheme();
            const bool on = getToggleState() || down;
            const float rad = th.faders() ? 6.0f : 20.0f;
            const auto led = th.accent;
            auto r = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
            r.removeFromBottom (6.0f);

            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (r.translated (0.0f, 7.0f).expanded (0.5f), rad);
            g.setColour (th.faders() ? juce::Colour (0xff0b0908) : th.panel.interpolatedWith (th.ink, 0.45f));
            g.fillRoundedRectangle (r.translated (0.0f, 6.0f), rad);

            auto face = r.translated (0.0f, on ? 4.0f : highlighted ? 1.0f : 0.0f);
            if (th.faders())
            {
                // 1970: große farbige Taste mit schwarzem Rand
                const auto cap = colour1970.withMultipliedBrightness (on ? 1.1f : 1.0f);
                juce::ColourGradient grad (cap.brighter (0.3f), face.getX(), face.getY(), cap.darker (0.35f), face.getX(), face.getBottom(), false);
                grad.addColour (0.3, cap);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (face, rad);
                g.setColour (juce::Colour (0xff0b0908));
                g.drawRoundedRectangle (face, rad, 2.0f);
            }
            else
            {
                juce::ColourGradient grad (on ? th.panel.interpolatedWith (th.accent, 0.10f) : th.panel.brighter (0.06f), face.getX(), face.getY(),
                                           on ? th.panel.interpolatedWith (th.accent, 0.16f) : th.panel.interpolatedWith (th.ink, 0.12f),
                                           face.getX(), face.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (face, rad);
                g.setColour (on ? th.accent : th.ink.withAlpha (0.22f));
                g.drawRoundedRectangle (face, rad, on ? 2.0f : 1.0f);
            }

            // LED oben rechts
            auto ledR = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ face.getRight() - 16.0f, face.getY() + 15.0f });
            if (th.faders())
                Design::paintLamp (g, ledR.expanded (1.0f), on);
            else if (on)
            {
                g.setColour (led.withAlpha (0.35f));
                g.fillEllipse (ledR.expanded (5.0f));
            }
            if (! th.faders())
            {
                g.setColour (on ? led : juce::Colours::black.withAlpha (0.35f));
                g.fillEllipse (ledR);
            }

            const auto textColour = th.faders() ? juce::Colour (0xff1a1208) : th.ink;
            auto area = face.reduced (14.0f, 8.0f);
            g.setColour (textColour);
            g.setFont (juce::FontOptions (big ? 24.0f : 18.0f, juce::Font::bold).withKerningFactor (0.08f));
            g.drawText (getButtonText(), area.removeFromTop (area.getHeight() * (subtitle.isEmpty() ? 1.0f : 0.6f)),
                        subtitle.isEmpty() ? juce::Justification::centred : juce::Justification::bottomLeft);
            g.setColour (th.faders() ? textColour.withAlpha (0.75f) : th.dim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (subtitle, area, juce::Justification::topLeft);
        }

        juce::Colour colour1970 { 0xffe8742a };

    private:
        juce::String subtitle;
        bool big;
    };

    //==========================================================================
    // Farben der Abschnitte: 1970 erdige Töne der Zeit, 2100 Blautöne
    juce::Colour sectionColour (const Design::Theme& th, int type)
    {
        if (th.faders())
        {
            static const juce::uint32 c[] { 0xff6b5843, 0xffc98a2e, 0xffe2622a, 0xffdba534, 0xff4f8f9a, 0xff7c7466, 0xfff0803a, 0xff54493c };
            return juce::Colour (c[juce::jlimit (0, 7, type)]);
        }
        static const float a[] { 0.18f, 0.35f, 0.65f, 0.3f, 0.55f, 0.2f, 0.9f, 0.15f };
        return th.accent.withAlpha (a[juce::jlimit (0, 7, type)]);
    }

    juce::String keyName (const Jarre::Song& s)
    {
        return Params::keys[Jarre::detail::mod (s.root, 12)] + "-" + Params::scales[s.scaleIndex + 1];
    }

    juce::String timeText (double seconds)
    {
        const int t = juce::jmax (0, (int) seconds);
        return juce::String (t / 60).paddedLeft ('0', 2) + ":" + juce::String (t % 60).paddedLeft ('0', 2);
    }

    //==========================================================================
    // Steckbrief des Tracks: Titel, Tonart, Tempo, Länge, Stiltreue, Form
    class TrackSummary : public juce::Component
    {
    public:
        explicit TrackSummary (JarreMachineProcessor& p) : processor (p) {}

        void refresh()
        {
            auto s = processor.getSong();
            const int style = lookOf (*this).getTheme().style;
            if (s != song || style != shownStyle)
            {
                song = s;
                shownStyle = style;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            if (song == nullptr)
                return;
            const auto& th = lookOf (*this).getTheme();
            auto r = getLocalBounds().toFloat();

            g.setFont (juce::FontOptions (30.0f, juce::Font::bold).withKerningFactor (0.03f));
            Design::drawText (g, th, Jarre::upper (song->title), r.removeFromTop (40.0f), juce::Justification::centredLeft, th.ink);

            const double seconds = song->totalBeats() * 60.0 / song->tempo;
            g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            Design::drawText (g, th, keyName (*song) + juce::String::fromUTF8 ("  \xc2\xb7  ") + juce::String (juce::roundToInt (song->tempo)) + " BPM"
                                     + juce::String::fromUTF8 ("  \xc2\xb7  ") + timeText (seconds) + " min" + juce::String::fromUTF8 ("  \xc2\xb7  ")
                                     + juce::String (song->totalBars) + " Takte",
                              r.removeFromTop (24.0f), juce::Justification::centredLeft, th.dim);
            r.removeFromTop (12.0f);

            // Stiltreue: wie viele Entscheidungen der Generator stiltypisch getroffen hat
            auto row = r.removeFromTop (22.0f);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.1f));
            Design::drawText (g, th, "STILTREUE", row.removeFromLeft (96.0f), juce::Justification::centredLeft, th.dim);
            auto bar = row.removeFromLeft (row.getWidth() - 60.0f);
            bar = bar.withSizeKeepingCentre (bar.getWidth(), 8.0f);
            g.setColour (th.knobTrack);
            g.fillRoundedRectangle (bar, 4.0f);
            g.setColour (th.faders() ? juce::Colour (0xffff9a3c) : th.accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * song->typicality), 4.0f);
            g.setColour (th.ink);
            g.drawText (juce::String (juce::roundToInt (song->typicality * 100.0f)) + " %", row, juce::Justification::centredRight);
            r.removeFromTop (16.0f);

            // Form als Streifen
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.1f));
            Design::drawText (g, th, "FORM", r.removeFromTop (18.0f), juce::Justification::centredLeft, th.dim);
            auto strip = r.removeFromTop (34.0f);
            const double total = song->totalBeats();
            for (auto& sec : song->sections)
            {
                const float x0 = strip.getX() + strip.getWidth() * (float) (sec.startBeat() / total);
                const float x1 = strip.getX() + strip.getWidth() * (float) (sec.endBeat() / total);
                auto cell = juce::Rectangle<float> (x0, strip.getY(), x1 - x0, strip.getHeight()).reduced (1.0f, 0.0f);
                g.setColour (sectionColour (th, sec.type));
                g.fillRoundedRectangle (cell, th.faders() ? 3.0f : 6.0f);
            }
            r.removeFromTop (8.0f);

            // Abschnittsliste in zwei Spalten
            g.setFont (juce::FontOptions (13.0f));
            const int n = (int) song->sections.size();
            const int perCol = (n + 1) / 2;
            const float colW = r.getWidth() / 2.0f;
            const float rowH = juce::jmin (19.0f, (r.getHeight() - 70.0f) / (float) juce::jmax (1, perCol));
            for (int i = 0; i < n; ++i)
            {
                const auto& sec = song->sections[(size_t) i];
                auto cell = juce::Rectangle<float> (r.getX() + colW * (float) (i / perCol), r.getY() + rowH * (float) (i % perCol), colW - 10.0f, rowH);
                g.setColour (sectionColour (th, sec.type).withAlpha (1.0f));
                g.fillEllipse (cell.removeFromLeft (14.0f).withSizeKeepingCentre (8.0f, 8.0f));
                g.setColour (th.ink);
                g.drawText (juce::String (i + 1) + ". " + Jarre::sectionName (sec.type) + (sec.rerolled ? " *" : ""), cell, juce::Justification::centredLeft);
                g.setColour (th.dim);
                g.drawText (juce::String (sec.bars) + " T  " + juce::String (juce::roundToInt (sec.intensity * 100.0f)) + " %"
                                + (sec.transpose != 0 ? "  +" + juce::String (sec.transpose) : juce::String()),
                            cell, juce::Justification::centredRight);
            }
        }

    private:
        JarreMachineProcessor& processor;
        std::shared_ptr<const Jarre::Song> song;
        int shownStyle = -1;
    };

    //==========================================================================
    // Zeitleiste: Abschnitte, Intensitätskurve und sechs Spuren mit allen Noten
    class Timeline : public juce::Component
    {
    public:
        explicit Timeline (JarreMachineProcessor& p) : processor (p)
        {
            for (int i = 0; i < Jarre::numLanes; ++i)
            {
                const juce::String id (Params::laneIds[i]);
                for (int k = 0; k < 2; ++k)
                {
                    auto* b = buttons.add (new juce::TextButton (k == 0 ? "M" : "S"));
                    b->setClickingTogglesState (true);
                    b->setTooltip (k == 0 ? "stumm" : "solo");
                    addAndMakeVisible (b);
                    attachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (p.apvts, (k == 0 ? "mute_" : "solo_") + id, *b));
                }
            }
        }

        static constexpr int headerW = 150, sectionH = 32, curveH = 70, helpH = 22;

        void resized() override
        {
            cache = {};
            const float laneH = laneHeight();
            for (int i = 0; i < Jarre::numLanes; ++i)
            {
                const int y = (int) (sectionH + curveH + laneH * (float) i + laneH * 0.5f) - 11;
                buttons[i * 2]->setBounds (headerW - 66, y, 28, 22);
                buttons[i * 2 + 1]->setBounds (headerW - 34, y, 28, 22);
            }
        }

        void refresh()
        {
            auto s = processor.getSong();
            const int style = lookOf (*this).getTheme().style;
            if (s != song || style != shownStyle)
            {
                song = s;
                shownStyle = style;
                cache = {};
                repaint();
            }
            const double beat = processor.shownBeat.load();
            if (std::abs (beat - shownBeat) > 1.0e-3)
            {
                shownBeat = beat;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            if (song == nullptr)
                return;
            if (! cache.isValid() || cache.getWidth() != getWidth() || cache.getHeight() != getHeight())
                renderCache();
            g.drawImageAt (cache, 0, 0);

            // Intensitätskurve mit Griffen
            const auto& th = lookOf (*this).getTheme();
            juce::Path curve;
            for (size_t i = 0; i < song->sections.size(); ++i)
            {
                const auto pt = handlePoint ((int) i);
                if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
            }
            g.setColour (th.faders() ? juce::Colour (0xffff9a3c) : th.accent);
            g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            for (size_t i = 0; i < song->sections.size(); ++i)
            {
                const auto pt = handlePoint ((int) i);
                auto dot = juce::Rectangle<float> (12.0f, 12.0f).withCentre (pt);
                g.setColour (th.panel);
                g.fillEllipse (dot);
                g.setColour (th.faders() ? juce::Colour (0xffff9a3c) : th.accent);
                if (song->sections[i].overridden) g.fillEllipse (dot.reduced (1.0f));
                else g.drawEllipse (dot.reduced (1.0f), 2.0f);
            }

            // Abspielposition
            const float x = xFor (shownBeat);
            g.setColour (th.faders() ? juce::Colour (0xffff9a3c) : th.accent);
            g.fillRect (juce::Rectangle<float> (x - 1.0f, 0.0f, 2.0f, (float) getHeight() - helpH));
            juce::Path tri;
            tri.addTriangle (x - 6.0f, 0.0f, x + 6.0f, 0.0f, x, 8.0f);
            g.fillPath (tri);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragIndex = rerollIndex = -1;
            if (song == nullptr || e.x < headerW)
                return;
            if (e.y < sectionH)
            {
                rerollIndex = sectionAtX ((float) e.x);
                return;
            }
            if (e.y < sectionH + curveH)
            {
                dragIndex = sectionAtX ((float) e.x);
                mouseDrag (e);
                return;
            }
            if (e.y < getHeight() - helpH)
                processor.seek (beatFor ((float) e.x));
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragIndex < 0)
                return;
            const float v = juce::jlimit (0.0f, 1.0f, 1.0f - ((float) e.y - sectionH - 8.0f) / (curveH - 16.0f));
            if (std::abs (v - lastDragValue) > 0.01f)
            {
                lastDragValue = v;
                processor.setSectionIntensity (dragIndex, v);
            }
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (rerollIndex >= 0 && e.y < sectionH && sectionAtX ((float) e.x) == rerollIndex)
                processor.rerollSection (rerollIndex);
            dragIndex = rerollIndex = -1;
            lastDragValue = -1.0f;
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (song != nullptr && e.x >= headerW && e.y >= sectionH && e.y < sectionH + curveH)
                processor.setSectionIntensity (sectionAtX ((float) e.x), -1.0f);
        }

    private:
        float laneHeight() const { return ((float) getHeight() - sectionH - curveH - helpH) / (float) Jarre::numLanes; }
        float xFor (double beat) const
        {
            const double total = song != nullptr ? juce::jmax (1.0, song->totalBeats()) : 1.0;
            return (float) headerW + (float) (getWidth() - headerW) * (float) (beat / total);
        }
        double beatFor (float x) const
        {
            return song == nullptr ? 0.0 : juce::jlimit (0.0, song->totalBeats(), (double) (x - (float) headerW) / (double) (getWidth() - headerW) * song->totalBeats());
        }
        int sectionAtX (float x) const { return song == nullptr ? -1 : song->sectionIndexAt (beatFor (x)); }
        juce::Point<float> handlePoint (int i) const
        {
            const auto& s = song->sections[(size_t) i];
            return { (xFor (s.startBeat()) + xFor (s.endBeat())) * 0.5f,
                     (float) sectionH + 8.0f + (curveH - 16.0f) * (1.0f - s.intensity) };
        }

        void renderCache()
        {
            const auto& th = lookOf (*this).getTheme();
            cache = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
            juce::Graphics g (cache);
            const float w = (float) getWidth();
            const float laneH = laneHeight();
            const auto ink = th.ink;

            // Abschnitte
            for (size_t i = 0; i < song->sections.size(); ++i)
            {
                const auto& s = song->sections[i];
                const float x0 = xFor (s.startBeat()), x1 = xFor (s.endBeat());
                auto cell = juce::Rectangle<float> (x0, 0.0f, x1 - x0, (float) sectionH).reduced (1.0f, 2.0f);
                g.setColour (sectionColour (th, s.type));
                g.fillRoundedRectangle (cell, th.faders() ? 3.0f : 8.0f);
                // ganze Spalte zart hinterlegen
                g.setColour (sectionColour (th, s.type).withMultipliedAlpha (0.12f));
                g.fillRect (juce::Rectangle<float> (x0 + 1.0f, (float) sectionH, x1 - x0 - 2.0f, (float) getHeight() - sectionH - helpH));
                g.setColour (th.faders() ? juce::Colour (0xff111214) : sectionColour (th, s.type).getFloatAlpha() > 0.6f ? juce::Colours::white : th.ink);
                g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
                if (cell.getWidth() > 34.0f)
                    g.drawFittedText (Jarre::sectionName (s.type) + (s.rerolled ? " *" : ""), cell.reduced (4.0f, 0.0f).toNearestInt(),
                                      juce::Justification::centred, 1, 0.7f);
            }

            // Kopf links
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.08f));
            Design::drawText (g, th, "ABSCHNITT", { 0.0f, 0.0f, (float) headerW - 8.0f, (float) sectionH }, juce::Justification::centredLeft, th.dim);
            Design::drawText (g, th, juce::String::fromUTF8 ("INTENSITÄT"), { 0.0f, (float) sectionH, (float) headerW - 8.0f, (float) curveH },
                              juce::Justification::centredLeft, th.dim);
            // Raster der Intensität
            g.setColour (th.line.withAlpha (th.faders() ? 0.25f : 0.9f));
            for (float v : { 0.0f, 0.5f, 1.0f })
                g.drawHorizontalLine (juce::roundToInt ((float) sectionH + 8.0f + (curveH - 16.0f) * (1.0f - v)), (float) headerW, w);

            static const int ranges[][2] { { 34, 82 }, { 38, 82 }, { 30, 66 }, { 58, 92 }, { 0, 8 }, { 0, 4 } };
            for (int lane = 0; lane < Jarre::numLanes; ++lane)
            {
                const float top = (float) sectionH + curveH + laneH * (float) lane;
                auto row = juce::Rectangle<float> (0.0f, top, w, laneH);
                g.setColour (th.line.withAlpha (th.faders() ? 0.3f : 0.9f));
                g.drawHorizontalLine (juce::roundToInt (top), 0.0f, w);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
                Design::drawText (g, th, Jarre::upper (Jarre::laneName (lane)), row.withWidth ((float) headerW - 70.0f).reduced (0.0f, 2.0f),
                                  juce::Justification::centredLeft, ink);

                const auto inner = row.withTrimmedLeft ((float) headerW).reduced (0.0f, 5.0f);
                const auto col = th.faders() ? juce::Colour (0xfff1ede4) : th.accent;
                auto yFor = [&] (int pitch)
                {
                    const float t = (float) (pitch - ranges[lane][0]) / (float) (ranges[lane][1] - ranges[lane][0]);
                    return inner.getBottom() - inner.getHeight() * juce::jlimit (0.0f, 1.0f, t);
                };
                const auto& notes = song->notes[(size_t) lane];
                if (lane == Jarre::Lead)
                {
                    g.setColour (col);
                    for (auto& n : notes)
                        g.fillRect (juce::Rectangle<float> (xFor (n.start), yFor (n.pitch) - 1.5f, juce::jmax (1.5f, xFor (n.start + n.length) - xFor (n.start)), 3.0f));
                }
                else if (lane == Jarre::Drums)
                {
                    for (auto& n : notes)
                    {
                        g.setColour (col.withAlpha (0.3f + 0.6f * n.velocity));
                        const float y = inner.getY() + inner.getHeight() * ((float) n.pitch + 0.5f) / (float) Jarre::numDrums;
                        g.fillRect (juce::Rectangle<float> (xFor (n.start), y - 1.0f, 1.5f, 2.0f));
                    }
                }
                else if (lane == Jarre::Fx)
                {
                    for (auto& n : notes)
                    {
                        const float x0 = xFor (n.start), x1 = xFor (n.start + n.length);
                        switch (n.pitch)
                        {
                            case Jarre::Wind:
                                g.setColour (col.withAlpha (0.18f));
                                g.fillRoundedRectangle (juce::Rectangle<float> (x0, inner.getY() + inner.getHeight() * 0.15f, x1 - x0, inner.getHeight() * 0.35f), 6.0f);
                                break;
                            case Jarre::Surf:
                            {
                                juce::Path p;
                                for (float x = x0; x < x1; x += 2.0f)
                                {
                                    const float y = inner.getY() + inner.getHeight() * (0.72f + 0.12f * std::sin ((x - x0) * 0.12f));
                                    if (p.isEmpty()) p.startNewSubPath (x, y); else p.lineTo (x, y);
                                }
                                g.setColour (col.withAlpha (0.6f));
                                g.strokePath (p, juce::PathStrokeType (1.2f));
                                break;
                            }
                            case Jarre::Laser:
                                g.setColour (col);
                                g.drawLine (x0, inner.getY(), x0 + juce::jmax (3.0f, x1 - x0), inner.getBottom(), 1.5f);
                                break;
                            default:
                            {
                                juce::Path p;
                                p.addTriangle (x0, inner.getBottom(), x1, inner.getBottom(), x1, inner.getY());
                                g.setColour (col.withAlpha (0.45f));
                                g.fillPath (p);
                                break;
                            }
                        }
                    }
                }
                else
                {
                    g.setColour (col.withAlpha (lane == Jarre::Pad ? 0.55f : 0.8f));
                    const float h = lane == Jarre::Pad ? 3.0f : 2.0f;
                    for (auto& n : notes)
                        g.fillRect (juce::Rectangle<float> (xFor (n.start), yFor (n.pitch) - h * 0.5f,
                                                            juce::jmax (1.0f, xFor (n.start + n.length) - xFor (n.start) - 0.5f), h));
                }
            }

            // Zeitmarken (Minuten) und Hilfe
            const double secondsPerBeat = 60.0 / song->tempo;
            const double totalSeconds = song->totalBeats() * secondsPerBeat;
            const double stepSec = totalSeconds > 480.0 ? 120.0 : totalSeconds > 180.0 ? 60.0 : 30.0;
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            for (double t = stepSec; t < totalSeconds; t += stepSec)
            {
                const float x = xFor (t / secondsPerBeat);
                g.setColour (th.line.withAlpha (th.faders() ? 0.35f : 1.0f));
                g.drawVerticalLine (juce::roundToInt (x), (float) sectionH + curveH, (float) getHeight() - helpH);
                g.setColour (th.dim);
                g.drawText (timeText (t), juce::Rectangle<float> (x - 30.0f, (float) getHeight() - helpH, 60.0f, 12.0f), juce::Justification::centred);
            }
            g.setFont (juce::FontOptions (11.0f));
            g.setColour (th.dim);
            g.drawText (juce::String::fromUTF8 ("Abschnittskopf anklicken: neu würfeln   \xc2\xb7   Punkte ziehen: Intensität (Doppelklick: automatisch)   \xc2\xb7   In die Spuren klicken: Position"),
                        juce::Rectangle<float> (0.0f, (float) getHeight() - 12.0f, w, 12.0f), juce::Justification::centredRight);
        }

        JarreMachineProcessor& processor;
        std::shared_ptr<const Jarre::Song> song;
        juce::Image cache;
        int shownStyle = -1, dragIndex = -1, rerollIndex = -1;
        float lastDragValue = -1.0f;
        double shownBeat = -1.0;
        juce::OwnedArray<juce::TextButton> buttons;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> attachments;
    };

    //==========================================================================
    // Laserharfe als Punktmatrix: dunkles Raster, jede Spur ein Strahl aus Lichtpunkten in eigener Farbe,
    // jede gespielte Note blitzt als Punkt auf ihrem Strahl auf (hohe Töne weiter außen), Punkte glimmen nach
    class LaserHarp : public juce::Component
    {
    public:
        explicit LaserHarp (JarreMachineProcessor& p) : processor (p)
        {
            setOpaque (false);
            for (int i = 0; i < Jarre::numLanes; ++i)
                seen[(size_t) i] = processor.engine.hitCount[(size_t) i].load();
        }

        static constexpr float pitchStep = 6.0f;

        void resized() override
        {
            cols = juce::jmax (1, (int) ((float) getWidth() / pitchStep));
            rows = juce::jmax (1, (int) (((float) getHeight() - 30.0f) / pitchStep));
            light.assign ((size_t) (cols * rows), 0.0f);
            owner.assign ((size_t) (cols * rows), 0);
            grid = {};
        }

        void refresh()
        {
            if (light.empty()) return;
            for (auto& v : light) v *= 0.66f;
            float total = 0.0f;
            const float bc = (float) (cols - 1) * 0.5f, br = (float) rows - 2.0f;
            for (int i = 0; i < Jarre::numLanes; ++i)
            {
                const float v = juce::jlimit (0.0f, 1.0f, std::sqrt (processor.engine.laneLevel[(size_t) i].load() * 2.5f));
                shown[(size_t) i] = shown[(size_t) i] * 0.5f + v * 0.5f;
                total += shown[(size_t) i];
                const float lvl = shown[(size_t) i];
                if (lvl > 0.02f)
                {
                    const float a = angle (i), len = reach (i, bc, br) * (0.35f + 0.65f * lvl);
                    for (float d = 1.0f; d < len; d += 0.5f)
                        add (bc + std::sin (a) * d, br - std::cos (a) * d, 0.14f * lvl * (1.0f - 0.5f * d / len), i);
                    add (bc + std::sin (a) * len, br - std::cos (a) * len, 0.55f * lvl, i);
                }
                // neue Noten seit dem letzten Bild
                const int count = processor.engine.hitCount[(size_t) i].load();
                const int fresh = juce::jlimit (0, 4, count - seen[(size_t) i]);
                seen[(size_t) i] = count;
                for (int k = 0; k < fresh; ++k)
                {
                    const float fr = 0.3f + 0.62f * juce::jlimit (0.0f, 1.0f, ((float) processor.engine.hitPitch[(size_t) i].load() - 28.0f) / 64.0f);
                    const float d = reach (i, bc, br) * fr + (float) k * 1.5f, a = angle (i);
                    const float c = bc + std::sin (a) * d, r = br - std::cos (a) * d;
                    add (c, r, 1.1f, i);
                    for (auto o : { juce::Point<float> (1.0f, 0.0f), { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f } })
                        add (c + o.x, r + o.y, 0.4f, i);
                    for (int s = 0; s < 3; ++s)
                        add (c + (rng.nextFloat() - 0.5f) * 8.0f, r + (rng.nextFloat() - 0.5f) * 8.0f, 0.25f * rng.nextFloat(), i);
                }
            }
            if (processor.shownPlaying.load())
                for (int s = 0; s < juce::roundToInt (total * 3.0f); ++s)
                    if (rng.nextFloat() < 0.5f)
                        add (rng.nextFloat() * (float) cols, rng.nextFloat() * (float) rows * 0.8f, 0.18f + 0.2f * rng.nextFloat(), rng.nextInt (Jarre::numLanes));
            add (bc, br, 0.6f + 0.4f * juce::jmin (1.0f, total * 0.5f), Jarre::numLanes);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            const bool retro = th.faders();
            auto r = getLocalBounds().toFloat();
            g.setColour (retro ? juce::Colour (0xff070504) : juce::Colour (0xff070a12));
            g.fillRoundedRectangle (r, retro ? 8.0f : 14.0f);

            const float ox = (r.getWidth() - (float) (cols - 1) * pitchStep) * 0.5f;
            const float oy = ((r.getHeight() - 30.0f) - (float) (rows - 1) * pitchStep) * 0.5f + 2.0f;

            // ruhendes Raster einmal vorzeichnen
            if (! grid.isValid() || gridRetro != retro)
            {
                gridRetro = retro;
                grid = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
                juce::Graphics gg (grid);
                gg.setColour (retro ? juce::Colour (0x14ffab3d) : juce::Colour (0x148cb4ff));
                for (int y = 0; y < rows; ++y)
                    for (int x = 0; x < cols; ++x)
                        gg.fillRect (ox + (float) x * pitchStep - 0.75f, oy + (float) y * pitchStep - 0.75f, 1.5f, 1.5f);
            }
            g.drawImageAt (grid, 0, 0);

            for (int y = 0; y < rows; ++y)
                for (int x = 0; x < cols; ++x)
                {
                    const float b = light[(size_t) (y * cols + x)];
                    if (b < 0.04f) continue;
                    const float w = juce::jmin (1.0f, b), px = ox + (float) x * pitchStep, py = oy + (float) y * pitchStep;
                    const auto col = laneColour (owner[(size_t) (y * cols + x)]);
                    if (b > 0.35f)
                    {
                        g.setColour (col.withAlpha (0.12f * w));
                        g.fillEllipse (px - 5.5f, py - 5.5f, 11.0f, 11.0f);
                    }
                    const float hot = juce::jmax (0.0f, b - 0.9f) * 2.0f;
                    g.setColour (col.interpolatedWith (juce::Colours::white, juce::jmin (1.0f, hot)).withAlpha (0.25f + 0.75f * w));
                    const float rad = 0.9f + 1.5f * w;
                    g.fillRect (px - rad, py - rad, rad * 2.0f, rad * 2.0f);
                }

            // Spurnamen als kleine Leuchtschrift in ihrer Farbe
            g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 9.5f, juce::Font::bold));
            for (int i = 0; i < Jarre::numLanes; ++i)
            {
                const float x = 30.0f + (float) i * (r.getWidth() - 60.0f) / (float) (Jarre::numLanes - 1);
                g.setColour (laneColour (i).withAlpha (0.3f + 0.7f * shown[(size_t) i]));
                g.drawText (Jarre::upper (Jarre::laneName (i)), juce::Rectangle<float> (80.0f, 14.0f).withCentre ({ x, r.getBottom() - 14.0f }), juce::Justification::centred);
            }
        }

        static juce::Colour laneColour (int lane)
        {
            static const juce::uint32 c[] { 0xffb06bff, 0xff2ee6ff, 0xffff3b4e, 0xff39ff6a, 0xffffc23a, 0xffff4fd8, 0xffffffff };
            return juce::Colour (c[juce::jlimit (0, 6, lane)]);
        }

    private:
        static float angle (int i) { return juce::degreesToRadians (-50.0f + 100.0f * (float) i / (float) (Jarre::numLanes - 1)); }
        static float reach (int i, float bc, float br)
        {
            return juce::jmin (br - 3.0f, (bc - 3.0f) / juce::jmax (0.1f, std::abs (std::sin (angle (i)))));
        }

        // Punkt aufhellen; er nimmt die Farbe der Spur an, die ihn am stärksten anstrahlt
        void add (float c, float r, float v, int lane)
        {
            const int x = juce::roundToInt (c), y = juce::roundToInt (r);
            if (x < 0 || y < 0 || x >= cols || y >= rows) return;
            auto& b = light[(size_t) (y * cols + x)];
            if (v >= b * 0.5f) owner[(size_t) (y * cols + x)] = (juce::uint8) lane;
            b = juce::jmin (1.4f, b + v);
        }

        JarreMachineProcessor& processor;
        std::array<float, Jarre::numLanes> shown {};
        std::array<int, Jarre::numLanes> seen {};
        std::vector<float> light;
        std::vector<juce::uint8> owner;
        int cols = 1, rows = 1;
        juce::Image grid;
        bool gridRetro = false;
        juce::Random rng { 7 };
    };

    //==========================================================================
    class Content : public juce::Component, private juce::Timer
    {
    public:
        Content (JarreMachineProcessor& p, TrendyLook& lf)
            : processor (p), look (lf), summary (p), timeline (p), harp (p),
              startPad ("START", "eigene Uhr"), homePad ("ANFANG", "zum Trackbeginn"),
              rollPad ("NEUER TRACK", juce::String::fromUTF8 ("würfeln: Form, Noten, Tempo")),
              variationPad ("VARIATION", "gleiche Form, neue Noten")
        {
            setLookAndFeel (&lf);
            grain = Design::plasticGrain();

            // Design-Umschalter: zwei Taster
            const char* designTitles[] { "1970", "2100" };
            const char* designSubs[]   { "Holz, Schieberegler", "Minimal, Gummipotis" };
            for (int i = 0; i < 2; ++i)
            {
                auto* b = designButtons.add (new TabButton (designTitles[i], designSubs[i], 1002));
                b->onClick = [this, i] { processor.design.store (i); applyDesign(); };
                addAndMakeVisible (b);
            }

            const char* tabTitles[] { "Track", "Zeitleiste", "Klang", "Mischpult" };
            const char* tabSubs[]   { "Würfel, Länge, Stil", "Abschnitte, Spuren", "Flächen, Sequenz, Rhythmus", "Pegel, Echo, Hall" };
            for (int i = 0; i < numPages; ++i)
            {
                auto* t = tabs.add (new TabButton (tabTitles[i], juce::String::fromUTF8 (tabSubs[i])));
                t->onClick = [this, i] { showPage (i); };
                addAndMakeVisible (t);
                auto* pg = pages.add (new Page());
                addChildComponent (pg);
            }

            buildTrackPage();
            buildTimelinePage();
            buildSoundPage();
            buildMixPage();

            addAndMakeVisible (display);
            addAndMakeVisible (meter);

            startPad.colour1970 = juce::Colour (0xff8aa23c);
            startPad.onClick = [this] { processor.setRunning (! processor.running.load()); timerCallback(); };
            addAndMakeVisible (startPad);
            homePad.colour1970 = juce::Colour (0xffe9dfc6);
            homePad.onClick = [this] { processor.seek (0.0); };
            addAndMakeVisible (homePad);
            rollPad.colour1970 = juce::Colour (0xffe2622a);
            rollPad.onClick = [this] { processor.rollNewTrack(); };
            addAndMakeVisible (rollPad);
            variationPad.colour1970 = juce::Colour (0xffdba534);
            variationPad.onClick = [this] { processor.rollVariation(); };
            addAndMakeVisible (variationPad);

            setSize (JarreMachineEditor::designWidth, JarreMachineEditor::designHeight);
            applyDesign();
            tabs[0]->setToggleState (true, juce::dontSendNotification);
            showPage (0);
            timerCallback();
            startTimerHz (30);
        }

        ~Content() override { setLookAndFeel (nullptr); }

        void paint (juce::Graphics& g) override
        {
            if (background.isValid())
                g.drawImageAt (background, 0, 0);

            const auto& th = look.getTheme();
            if (th.faders())
            {
                g.setColour (th.accent);
                g.setFont (juce::FontOptions (32.0f, juce::Font::bold).withKerningFactor (0.1f));
                g.drawText ("JARRE MACHINE", 24, 12, 340, 38, juce::Justification::centredLeft);
                g.setColour (th.accent);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.15f));
                g.drawText ("SPACE TRACK GENERATOR " + versionLabel(), 366, 14, 320, 18, juce::Justification::centredLeft);
                g.setColour (th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
                g.drawText (makerLabel(), 366, 32, 320, 16, juce::Justification::centredLeft);
            }
            else
            {
                g.setFont (juce::FontOptions (30.0f, juce::Font::bold).withKerningFactor (0.1f));
                Design::drawText (g, th, "JARRE MACHINE", { 20.0f, 12.0f, 340.0f, 38.0f }, juce::Justification::centredLeft, th.ink);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.15f));
                Design::drawText (g, th, "SPACE TRACK GENERATOR " + versionLabel(), { 366.0f, 14.0f, 320.0f, 18.0f }, juce::Justification::centredLeft, th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
                Design::drawText (g, th, makerLabel(), { 366.0f, 32.0f, 320.0f, 16.0f }, juce::Justification::centredLeft, th.dim);
            }
            g.setColour (th.dim);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.15f));
            g.drawText ("DESIGN", 944, 42, 160, 18, juce::Justification::centredLeft);
        }

        void resized() override
        {
            renderBackground();

            designButtons[0]->setBounds (940, 64, 156, 62);
            designButtons[1]->setBounds (1100, 64, 164, 62);

            auto tabRow = juce::Rectangle<int> (16, 64, 4 * 200, 62);
            for (auto* t : tabs)
                t->setBounds (tabRow.removeFromLeft (200).withTrimmedRight (8));

            const auto pageArea = juce::Rectangle<int> (16, 138, getWidth() - 32, getHeight() - 138 - 132);
            for (auto* pg : pages)
                pg->setBounds (pageArea);

            // Transportleiste
            auto strip = juce::Rectangle<int> (24, getHeight() - 116, getWidth() - 48, 100);
            display.setBounds (strip.removeFromLeft (330).reduced (0, 6));
            strip.removeFromLeft (16);
            meter.setBounds (strip.removeFromLeft (70).reduced (0, 4));
            strip.removeFromLeft (20);
            startPad.setBounds (strip.removeFromLeft (170));
            strip.removeFromLeft (10);
            homePad.setBounds (strip.removeFromLeft (170));
            variationPad.setBounds (strip.removeFromRight (210));
            strip.removeFromRight (12);
            rollPad.setBounds (strip.removeFromRight (240));

            layoutTrackPage();
            layoutTimelinePage();
            layoutSoundPage();
            layoutMixPage();
        }

    private:
        static constexpr int numPages = 4;

        //----------------------------------------------------------------------
        Labelled* add (Page& page, Labelled* c)
        {
            controls.add (c);
            page.addAndMakeVisible (c);
            return c;
        }
        Labelled* knob (Page& pg, const juce::String& id, const juce::String& name, int size = 84)
        {
            return add (pg, new KnobControl (processor.apvts, id, name, size));
        }
        Labelled* choice (Page& pg, const juce::String& id, const juce::String& name, int width = 132)
        {
            return add (pg, new ChoiceControl (processor.apvts, id, name, width));
        }
        Labelled* toggle (Page& pg, const juce::String& id, const juce::String& name,
                          juce::String onText = "an", juce::String offText = "aus")
        {
            return add (pg, new ToggleControl (processor.apvts, id, name, onText, offText));
        }
        static juce::String de (const char* t) { return juce::String::fromUTF8 (t); }

        // Felder einer Reihe nebeneinander; jedes bekommt seinen Platzbedarf, der Rest wird verteilt
        static void row (Page& pg, std::initializer_list<int> indices, int y, int h, int x0 = 0, int width = -1)
        {
            if (width < 0) width = pg.getWidth() - x0;
            int need = 0;
            for (int i : indices)
            {
                int w = 28;
                for (auto* c : pg.panels[(size_t) i].items) w += c->preferredWidth() + 8;
                need += w;
            }
            const int gaps = 12 * ((int) indices.size() - 1);
            const float extra = juce::jmax (0.0f, (float) (width - gaps - need) / (float) indices.size());
            int x = x0;
            for (int i : indices)
            {
                int w = 28;
                for (auto* c : pg.panels[(size_t) i].items) w += c->preferredWidth() + 8;
                const int wdt = (int) ((float) w + extra);
                pg.panels[(size_t) i].bounds = { x, y, wdt, h };
                x += wdt + 12;
            }
        }

        //----------------------------------------------------------------------
        void buildTrackPage()
        {
            auto& pg = *pages[0];
            pg.panels = {
                { "GENERATOR", {}, { knob (pg, "length", de ("Länge"), 112), knob (pg, "intensity", de ("Intensität"), 112),
                                     knob (pg, "closeness", de ("Nähe zum Original"), 112), knob (pg, "variety", "Abwechslung", 112),
                                     choice (pg, "era", "Epoche", 150) } },
                { "TONART & TEMPO", {}, { choice (pg, "key", "Tonart", 74), choice (pg, "scale", "Skala", 124), knob (pg, "tempo", "Tempo", 104),
                                          toggle (pg, "roll_all", de ("Mitwürfeln")), toggle (pg, "follow_host", "Host-Tempo"),
                                          toggle (pg, "loop", "Schleife") } },
                { "DER TRACK", {}, {} },
            };
            transposeControl = toggle (pg, "midi_transpose", "MIDI transponiert");
            pg.addAndMakeVisible (summary);
            exportButton.setButtonText ("MIDI SPEICHERN");
            exportButton.onClick = [this] { exportMidi(); };
            pg.addAndMakeVisible (exportButton);
            pg.paintExtra = [this] (juce::Graphics& g) { paintTrackHelp (g); };
        }

        void layoutTrackPage()
        {
            auto& pg = *pages[0];
            const int h = (pg.getHeight() - 12) / 2;
            pg.panels[0].bounds = { 0, 0, 700, h };
            pg.panels[1].bounds = { 0, h + 12, 700, h };
            pg.panels[2].bounds = { 712, 0, pg.getWidth() - 712, pg.getHeight() };
            Page::layout (pg.panels[0], 160);
            Page::layout (pg.panels[1], 160);
            auto r = pg.panels[2].bounds.reduced (22, 0).withTrimmedTop (40).withTrimmedBottom (16);
            auto bottom = r.removeFromBottom (64);
            summary.setBounds (r);
            transposeControl->setBounds (bottom.removeFromLeft (110));
            exportButton.setBounds (bottom.removeFromRight (190).withSizeKeepingCentre (190, 34).withY (bottom.getY() + 24));
        }

        void paintTrackHelp (juce::Graphics& g)
        {
            const auto& th = look.getTheme();
            auto r = pages[0]->panels[2].bounds.toFloat().reduced (22.0f, 0.0f).withTrimmedBottom (16.0f);
            r = r.removeFromBottom (64.0f).withTrimmedLeft (122.0f).withTrimmedRight (196.0f);
            g.setFont (juce::FontOptions (11.5f));
            g.setColour (th.dim);
            g.drawFittedText (de ("Tasten im Host rücken den ganzen Track, C3 = Originallage."), r.toNearestInt().withTrimmedTop (18), juce::Justification::centredLeft, 3, 1.0f);
        }

        void exportMidi()
        {
            auto song = processor.getSong();
            if (song == nullptr) return;
            const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                   .getChildFile (juce::File::createLegalFileName (song->title) + ".mid");
            chooser = std::make_unique<juce::FileChooser> ("Track als MIDI speichern", start, "*.mid");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      auto f = fc.getResult();
                                      if (f != juce::File())
                                          processor.exportMidi (f.withFileExtension ("mid"));
                                  });
        }

        void buildTimelinePage()
        {
            auto& pg = *pages[1];
            pg.panels = { { "ZEITLEISTE", {}, {} } };
            pg.addAndMakeVisible (timeline);
        }

        void layoutTimelinePage()
        {
            auto& pg = *pages[1];
            pg.panels[0].bounds = pg.getLocalBounds();
            timeline.setBounds (pg.getLocalBounds().reduced (16, 0).withTrimmedTop (38).withTrimmedBottom (10));
        }

        void buildSoundPage()
        {
            auto& pg = *pages[2];
            pg.panels = {
                { de ("FLÄCHEN"), {}, { knob (pg, "pad_ensemble", "Ensemble", 72), knob (pg, "pad_phaser", "Phaser", 72),
                                        knob (pg, "pad_phaser_rate", "Ph.-Tempo", 72), knob (pg, "pad_bright", "Helligkeit", 72),
                                        knob (pg, "pad_attack", "Anschwellen", 72) } },
                { "SEQUENZ", {}, { knob (pg, "seq_cutoff", "Cutoff", 72), knob (pg, "seq_reso", "Resonanz", 72),
                                   knob (pg, "seq_env", "Filterkurve", 72), knob (pg, "seq_decay", "Abklingen", 72),
                                   choice (pg, "seq_wave", "Welle", 84) } },
                { "EFFEKTE", {}, { knob (pg, "fx_wind", "Wind", 72), knob (pg, "fx_laser", "Laser", 72), knob (pg, "fx_surf", "Brandung", 72) } },
                { "BASS", {}, { knob (pg, "bass_cutoff", "Cutoff", 72), knob (pg, "bass_decay", "Abklingen", 72), knob (pg, "bass_sub", "Sub", 72) } },
                { "MELODIE", {}, { knob (pg, "lead_glide", "Gleiten", 72), knob (pg, "lead_vibrato", "Vibrato", 72),
                                   knob (pg, "lead_bright", "Helligkeit", 72), choice (pg, "lead_wave", "Welle", 100) } },
                { "RHYTHMUSBOX", {}, { choice (pg, "drum_kit", "Kit", 160), knob (pg, "drum_tone", "Ton", 72), knob (pg, "drum_swing", "Swing", 72) } },
            };
        }

        void layoutSoundPage()
        {
            auto& pg = *pages[2];
            const int h = (pg.getHeight() - 12) / 2;
            row (pg, { 0, 1, 2 }, 0, h);
            row (pg, { 3, 4, 5 }, h + 12, h);
            for (auto& p : pg.panels)
                Page::layout (p, 160);
        }

        void buildMixPage()
        {
            auto& pg = *pages[3];
            pg.panels = {
                { "SPUREN", {}, { knob (pg, "vol_pad", de ("Flächen"), 84), knob (pg, "vol_seq", "Sequenz", 84), knob (pg, "vol_bass", "Bass", 84),
                                  knob (pg, "vol_lead", "Melodie", 84), knob (pg, "vol_drums", "Rhythmus", 84), knob (pg, "vol_fx", "Effekte", 84),
                                  knob (pg, "master", "Master", 84) } },
                { "ECHO", {}, { choice (pg, "echo_div", "Teilung", 90), knob (pg, "echo_fb", "Feedback", 72), knob (pg, "echo_mix", "Echo", 72) } },
                { "HALL", {}, { knob (pg, "rev_size", de ("Raumgröße"), 72), knob (pg, "rev_mix", "Hall", 72), knob (pg, "width", "Breite", 72) } },
                { "LASERHARFE", {}, {} },
            };
            pg.addAndMakeVisible (harp);
        }

        void layoutMixPage()
        {
            auto& pg = *pages[3];
            const int h = (pg.getHeight() - 12) / 2;
            const int left = 760;
            pg.panels[0].bounds = { 0, 0, left, h };
            row (pg, { 1, 2 }, h + 12, h, 0, left);
            pg.panels[3].bounds = { left + 12, 0, pg.getWidth() - left - 12, pg.getHeight() };
            for (auto& p : pg.panels)
                Page::layout (p, 160);
            harp.setBounds (pg.panels[3].bounds.reduced (16).withTrimmedTop (26));
        }

        //----------------------------------------------------------------------
        void showPage (int index)
        {
            currentPage = index;
            for (int i = 0; i < numPages; ++i)
            {
                pages[i]->setVisible (i == index);
                tabs[i]->setToggleState (i == index, juce::dontSendNotification);
            }
            timerCallback();
        }

        // Design anwenden: Farben, Fader oder Drehregler, Anordnung
        void applyDesign()
        {
            const int d = processor.design.load();
            look.setTheme (Design::themeFor (d));
            const bool faders = look.getTheme().faders();
            for (auto* c : controls)
                if (auto* sl = c->getSlider())
                    sl->setFaderMode (faders);
            for (int i = 0; i < designButtons.size(); ++i)
                designButtons[i]->setToggleState (i == d, juce::dontSendNotification);
            sendLookAndFeelChange();
            resized();
            repaint();
        }

        void renderBackground()
        {
            if (getWidth() <= 0)
                return;
            const auto& th = look.getTheme();
            background = juce::Image (juce::Image::RGB, getWidth(), getHeight(), false);
            juce::Graphics g (background);
            const auto r = getLocalBounds().toFloat();
            if (th.faders())
            {
                // 1970: Nussbaumrahmen, schwarze Frontplatte mit vier Schrauben, Transportleiste als eingelassenes Feld
                Design::paintWood (g, r);
                const auto plate = r.reduced (6.0f);
                g.setColour (juce::Colour (0xff0c0a08));
                g.fillRoundedRectangle (plate.expanded (1.5f), 7.0f);
                g.setColour (th.plate);
                g.fillRoundedRectangle (plate, 6.0f);
                g.setTiledImageFill (grain, 0, 0, 1.0f);
                g.fillRoundedRectangle (plate, 6.0f);
                for (auto p : { juce::Point<float> (15.0f, 15.0f), { r.getRight() - 15.0f, 15.0f }, { 15.0f, r.getBottom() - 15.0f }, { r.getRight() - 15.0f, r.getBottom() - 15.0f } })
                {
                    juce::ColourGradient screw (juce::Colour (0xffd8d2c4), p.x - 2.0f, p.y - 2.0f, juce::Colour (0xff3a3732), p.x + 4.5f, p.y + 4.5f, true);
                    g.setGradientFill (screw);
                    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (p));
                    g.setColour (juce::Colour (0xff2a2622));
                    g.drawLine (p.x - 3.0f, p.y + 1.0f, p.x + 3.0f, p.y - 1.0f, 1.2f);
                }
                auto deck = juce::Rectangle<float> (24.0f, (float) getHeight() - 124.0f, (float) getWidth() - 48.0f, 112.0f);
                g.setColour (juce::Colour (0xff15120f));
                g.fillRoundedRectangle (deck, 8.0f);
                g.setColour (th.line.withAlpha (0.35f));
                g.drawRoundedRectangle (deck, 8.0f, 1.5f);
                return;
            }

            juce::ColourGradient light (juce::Colour (0xfffbfcfd), r.getCentreX(), 0.0f,
                                        th.body, r.getCentreX(), r.getHeight() * 0.7f, true);
            g.setGradientFill (light);
            g.fillAll();
            g.setColour (juce::Colour (0xffdde1e6));
            g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, (float) getHeight() - 124.0f, (float) getWidth() - 24.0f, 116.0f), 18.0f);
        }

        void timerCallback() override
        {
            auto song = processor.getSong();
            const double beat = processor.shownBeat.load();
            const bool playing = processor.shownPlaying.load(), host = processor.shownHost.load();
            const double bpm = processor.shownBpm.load();
            if (song != nullptr)
            {
                const int bar = (int) (beat / 4.0) + 1;
                const auto& sec = song->sections[(size_t) song->sectionIndexAt (beat)];
                juce::String small;
                small << "TAKT " << bar << "/" << song->totalBars << "  " << Jarre::upper (Jarre::sectionName (sec.type))
                      << "  " << juce::roundToInt (bpm) << " BPM" << (host ? "  HOST" : "");
                const int tr = processor.transpose.load();
                if (tr != 0 && processor.apvts.getRawParameterValue ("midi_transpose")->load() > 0.5f)
                    small << "  " << (tr > 0 ? "+" : "") << tr;
                display.setText (timeText (beat * 60.0 / juce::jmax (1.0, bpm)) + " / " + timeText (song->totalBeats() * 60.0 / juce::jmax (1.0, bpm)), small);
            }
            meter.setLevels (processor.engine.outLevel.load(), processor.engine.outLevelR.load());
            startPad.setToggleState (playing, juce::dontSendNotification);
            startPad.setButtonText (host ? "HOST" : playing ? "STOP" : "START");

            if (currentPage == 0) summary.refresh();
            if (currentPage == 1) timeline.refresh();
            if (currentPage == 3) harp.refresh();
        }

        JarreMachineProcessor& processor;
        TrendyLook& look;
        juce::Image grain, background;
        juce::OwnedArray<TabButton> tabs, designButtons;
        juce::OwnedArray<Page> pages;
        juce::OwnedArray<Labelled> controls;
        int currentPage = 0;
        Labelled* transposeControl = nullptr;

        TrackSummary summary;
        Timeline timeline;
        LaserHarp harp;
        juce::TextButton exportButton;
        std::unique_ptr<juce::FileChooser> chooser;
        LedDisplay display;
        Meter meter;
        DubPad startPad, homePad, rollPad, variationPad;
    };
}


//==============================================================================
TrendyLook::TrendyLook()
{
    setTheme (Design::themeFor (Design::Future2100));
}

void TrendyLook::setTheme (const Design::Theme& t)
{
    theme = t;
    setColour (juce::Label::textColourId, theme.ink);
    setColour (juce::Slider::textBoxTextColourId, theme.ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, theme.field);
    setColour (juce::ComboBox::outlineColourId, theme.line);
    setColour (juce::ComboBox::textColourId, theme.ink);
    setColour (juce::ComboBox::arrowColourId, theme.accent);
    setColour (juce::PopupMenu::backgroundColourId, theme.panel);
    setColour (juce::PopupMenu::textColourId, theme.ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, theme.field);
    setColour (juce::TextButton::buttonOnColourId, theme.accent);
    setColour (juce::TextButton::textColourOffId, theme.ink);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::BubbleComponent::backgroundColourId, theme.panel);
    setColour (juce::BubbleComponent::outlineColourId, theme.accent);
    setColour (juce::TooltipWindow::textColourId, theme.ink);
    setColour (juce::TooltipWindow::backgroundColourId, theme.panel);
}

void TrendyLook::drawRubberKnob (juce::Graphics& g, juce::Rectangle<float> area, float pos, float squish,
                                   juce::Point<float> dent, bool bipolar, float startAngle, float endAngle, bool mini)
{
    auto bounds = area.reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius < 4.0f)
        return;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + juce::jlimit (-0.05f, 1.05f, pos) * (endAngle - startAngle);

    // Wertebogen (aufgedruckt, bleibt ruhig)
    const float lineW = juce::jmax (2.0f, radius * (mini ? 0.13f : 0.11f));
    const float arcR = radius - lineW * 0.5f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (theme.knobTrack);
    g.strokePath (track, { lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const float to = startAngle + juce::jlimit (0.0f, 1.0f, pos) * (endAngle - startAngle);
    if (std::abs (to - from) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (from, to), juce::jmax (from, to), true);
        g.setColour (theme.accent);
        g.strokePath (value, { lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }

    // Gummi-Kappe: wird beim Drücken breiter und flacher und gibt am Druckpunkt nach
    const float capR = radius * (mini ? 0.7f : 0.74f);
    const auto squeeze = juce::AffineTransform::scale (1.0f + 0.07f * squish, 1.0f - 0.09f * squish, centre.x, centre.y)
                             .translated (dent.x * squish * capR * 0.06f, squish * capR * 0.05f + dent.y * squish * capR * 0.04f);
    juce::Path cap;
    cap.addEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre));
    cap.applyTransform (squeeze);

    juce::DropShadow (juce::Colours::black.withAlpha (0.5f - 0.15f * squish), juce::roundToInt (capR * (0.35f - 0.15f * squish)) + 1,
                      { 0, juce::roundToInt (capR * (0.16f - 0.08f * squish)) + 1 }).drawForPath (g, cap);

    const auto capBounds = cap.getBounds();
    juce::ColourGradient body (theme.knob.brighter (0.55f), capBounds.getX() + capBounds.getWidth() * 0.36f, capBounds.getY() + capBounds.getHeight() * 0.26f,
                               theme.knob.darker (0.45f), capBounds.getRight(), capBounds.getBottom(), true);
    body.addColour (0.45, theme.knob);
    g.setGradientFill (body);
    g.fillPath (cap);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.addTransform (squeeze);

        // Griffrillen am Rand, drehen mit
        const int ribs = mini ? 14 : 24;
        for (int i = 0; i < ribs; ++i)
        {
            const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) ribs;
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.drawLine ({ centre.getPointOnCircumference (capR * 0.8f, a), centre.getPointOnCircumference (capR * 0.97f, a) }, 1.2f);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawLine ({ centre.getPointOnCircumference (capR * 0.8f, a + 0.05f), centre.getPointOnCircumference (capR * 0.97f, a + 0.05f) }, 0.8f);
        }

        // matter Glanz oben
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.16f), centre.x, centre.y - capR,
                                    juce::Colours::white.withAlpha (0.0f), centre.x, centre.y + capR * 0.1f, false);
        g.setGradientFill (sheen);
        g.fillEllipse (juce::Rectangle<float> (capR * 1.5f, capR * 1.1f).withCentre ({ centre.x, centre.y - capR * 0.35f }));

        // Delle am Druckpunkt
        if (squish > 0.02f)
        {
            const auto dp = centre + dent * capR * 0.45f;
            juce::ColourGradient dentGrad (juce::Colours::black.withAlpha (0.28f * juce::jmin (1.0f, squish)), dp.x, dp.y,
                                           juce::Colours::transparentBlack, dp.x + capR * 0.55f, dp.y, true);
            g.setGradientFill (dentGrad);
            g.fillEllipse (juce::Rectangle<float> (capR * 1.1f, capR * 1.1f).withCentre (dp));
        }

        // Zeiger
        g.setColour (theme.faders() ? juce::Colour (0xfff1ede4) : theme.accent);
        g.drawLine ({ centre.getPointOnCircumference (capR * 0.2f, angle), centre.getPointOnCircumference (capR * 0.74f, angle) },
                    juce::jmax (2.0f, capR * 0.12f));
    }
}

void TrendyLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                     float startAngle, float endAngle, juce::Slider& slider)
{
    drawRubberKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat(), pos, 0.0f, {},
                    slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0, startAngle, endAngle, false);
}

void TrendyLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                         bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f, 1.0f);
    const bool on = b.getToggleState();
    const float press = down ? 2.0f : on ? 1.5f : 0.0f;
    r.removeFromBottom (2.0f);
    const float rad = theme.faders() ? 2.0f : r.getHeight() * 0.5f;
    g.setColour (juce::Colours::black.withAlpha (theme.faders() ? 0.45f : 0.12f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), rad);
    const auto fill = on ? theme.accent : theme.field.interpolatedWith (theme.ink, highlighted ? 0.08f : 0.03f);
    g.setColour (fill);
    g.fillRoundedRectangle (r.translated (0.0f, press), rad);
    g.setColour (on ? theme.accent.darker (0.3f) : theme.line);
    g.drawRoundedRectangle (r.translated (0.0f, press), rad, 1.0f);
}

void TrendyLook::drawFader (juce::Graphics& g, juce::Rectangle<float> area, float pos, bool bipolar, bool horizontal, float squish,
                            float tilt, float side, int capIndex)
{
    // 1970: Schlitz in der Frontplatte, cremefarbener Siebdruck, farbige Kappe mit Spiel (kippt und versetzt sich etwas)
    pos = juce::jlimit (-0.03f, 1.03f, pos);
    const float indent = (float) faderIndent;
    const auto printed = theme.ink;
    static const juce::uint32 caps[] { 0xffe9dfc6, 0xffe8742a, 0xff2a2622, 0xffdba534, 0xff4f8f9a, 0xffe9dfc6, 0xffc7432a };
    const auto capColour = juce::Colour (caps[(size_t) (capIndex % 7)]);
    const auto drawCap = [&] (juce::Rectangle<float> cap, juce::Point<float> centre, bool vertical)
    {
        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::rotation (tilt, centre.x, centre.y).translated (vertical ? side : 0.0f, vertical ? 0.0f : side * 0.5f));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (cap.translated (1.0f, 4.0f), 3.0f);
        g.setColour (juce::Colour (0xff0b0908));
        g.fillRoundedRectangle (cap.expanded (1.0f), 4.0f);
        juce::ColourGradient grad (capColour.brighter (0.35f), cap.getX(), cap.getY(), capColour.darker (0.4f), cap.getX(), cap.getBottom(), false);
        grad.addColour (0.45, capColour);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (cap, 3.0f);
        const auto mark = capColour.getPerceivedBrightness() < 0.3f ? printed : juce::Colour (0xff1a1208);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        if (vertical)
        {
            g.fillRect (cap.getX() + 3.0f, centre.y - 5.5f, cap.getWidth() - 6.0f, 1.0f);
            g.fillRect (cap.getX() + 3.0f, centre.y + 4.5f, cap.getWidth() - 6.0f, 1.0f);
            g.setColour (mark);
            g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 4.0f, 2.0f));
        }
        else
        {
            g.setColour (mark);
            g.fillRect (cap.withSizeKeepingCentre (2.0f, cap.getHeight() - 6.0f));
        }
    };

    if (! horizontal)
    {
        const float cx = area.getCentreX() + 6.0f;
        const float top = area.getY() + indent, bottom = area.getBottom() - indent, len = bottom - top;

        // Siebdruck-Skala: Striche (lange bei 0, 5, 10) und Ziffern
        g.setColour (printed.withAlpha (0.55f));
        for (int i = 0; i <= 10; ++i)
        {
            const float y = bottom - len * (float) i / 10.0f, w = i % 5 == 0 ? 7.0f : 4.0f;
            g.drawLine (cx - 10.0f - w, y, cx - 10.0f, y, 1.0f);
            g.drawLine (cx + 10.0f, y, cx + 10.0f + w, y, 1.0f);
        }
        g.setColour (printed.withAlpha (0.85f));
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 9.5f, juce::Font::plain));
        const char* marks[] { "0", "5", "10" };
        const char* bmarks[] { "-", "0", "+" };
        for (int i = 0; i < 3; ++i)
            g.drawText (bipolar ? bmarks[i] : marks[i], juce::Rectangle<float> (cx - 34.0f, bottom - len * (float) i * 0.5f - 6.0f, 15.0f, 12.0f),
                        juce::Justification::centredRight);

        // Schlitz
        g.setColour (juce::Colour (0xff050403));
        g.fillRoundedRectangle (juce::Rectangle<float> (5.0f, len + 10.0f).withCentre ({ cx, (top + bottom) * 0.5f }), 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawLine (cx + 3.0f, top - 4.0f, cx + 3.0f, bottom + 4.0f, 1.0f);

        const float y = bottom - len * pos;
        auto cap = juce::Rectangle<float> (28.0f * (1.0f - 0.05f * squish), 20.0f * (1.0f - 0.1f * squish)).withCentre ({ cx, y });
        drawCap (cap, { cx, y }, true);
        return;
    }

    // waagerechter Mini-Fader
    const float cy = area.getCentreY();
    const float left = area.getX() + indent, right = area.getRight() - indent, len = right - left;
    g.setColour (juce::Colour (0xff050403));
    g.fillRoundedRectangle (juce::Rectangle<float> (len + 8.0f, 4.0f).withCentre ({ (left + right) * 0.5f, cy }), 2.0f);
    if (bipolar)
    {
        g.setColour (printed.withAlpha (0.5f));
        g.drawLine ((left + right) * 0.5f, cy - 7.0f, (left + right) * 0.5f, cy - 4.0f, 1.0f);
    }
    const float x = left + len * pos;
    auto cap = juce::Rectangle<float> (11.0f, 18.0f * (1.0f - 0.1f * squish)).withCentre ({ x, cy });
    drawCap (cap, { x, cy }, false);
}


//==============================================================================
JarreMachineEditor::JarreMachineEditor (JarreMachineProcessor& p)
    : AudioProcessorEditor (&p)
{
    setLookAndFeel (&look);
    content = std::make_unique<Content> (p, look);
    addAndMakeVisible (*content);

    // Fenster frei skalierbar, Seitenverhältnis fest: alles bleibt ohne Scrollen sichtbar
    setResizable (true, true);
    setResizeLimits (designWidth * 6 / 10, designHeight * 6 / 10, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (designWidth, designHeight);
}

JarreMachineEditor::~JarreMachineEditor()
{
    content.reset();
    setLookAndFeel (nullptr);
}

void JarreMachineEditor::resized()
{
    if (content != nullptr)
        content->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth));
}
