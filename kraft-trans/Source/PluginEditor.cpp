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
            setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            setMouseDragSensitivity (220);
            if (mini)
            {
                setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                setPopupDisplayEnabled (true, true, nullptr);
            }
        }

        // Design 1980: Fader statt Drehregler (kleine liegen waagerecht)
        void setFaderMode (bool f)
        {
            fader = f;
            if (fader)
            {
                setSliderStyle (mini ? juce::Slider::LinearHorizontal : juce::Slider::LinearVertical);
                setSliderSnapsToMousePosition (false);
                if (! mini)
                    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
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
                lookOf (*this).drawFader (g, layout.sliderBounds.toFloat(), shown, getMinimum() < 0.0 && getMaximum() > 0.0, mini, squish);
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
            velocity = velocity * 0.5f + err * 0.35f;     // leichtes Überschwingen wie Gummi
            shown += velocity;

            const float sqErr = squishTarget - squish;
            squishVelocity = squishVelocity * 0.55f + sqErr * 0.3f;
            squish += squishVelocity;

            if (std::abs (err) < 1.0e-4f && std::abs (velocity) < 1.0e-4f
                && std::abs (sqErr) < 1.0e-3f && std::abs (squishVelocity) < 1.0e-3f)
            {
                shown = target;
                squish = squishTarget;
                stopTimer();
            }
            repaint();
        }

        bool mini, started = false, fader = false;
        float shown = 0.0f, velocity = 0.0f;
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
                slider.setBounds (r.withSizeKeepingCentre (44, r.getHeight()).withTrimmedBottom (2));
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
            const float rad = th.faders() ? 3.0f : 16.0f;
            const auto led1980 = juce::Colour (0xffff3b24);
            const auto ledColour = th.faders() ? led1980 : th.accent;
            auto r = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
            r.removeFromBottom (5.0f);

            // Sockel und Schatten
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (r.translated (0.0f, 6.0f).expanded (0.5f), rad);
            g.setColour (th.panel.interpolatedWith (th.ink, 0.45f));
            g.fillRoundedRectangle (r.translated (0.0f, 5.0f), rad);

            auto face = r.translated (0.0f, press);
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
            static const juce::Colour stripes[] { juce::Colour (0xffd8382c), juce::Colour (0xfff08a24),
                                                  juce::Colour (0xfff2c230), juce::Colour (0xff3f86d8) };
            int index = 0;
            for (auto& p : panels)
            {
                const auto r = p.bounds.toFloat();
                if (th.faders())
                {
                    // 1980: aufgedruckter Rahmen, weißes Titelschild mit farbigem Streifen
                    g.setColour (th.line.withAlpha (0.55f));
                    g.drawRect (r.reduced (0.5f), 1.0f);
                    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
                    const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), p.title) + 22.0f;
                    auto plate = juce::Rectangle<float> (r.getX(), r.getY(), juce::jmin (w, r.getWidth()), 22.0f);
                    g.setColour (th.ink);
                    g.fillRect (plate);
                    g.setColour (stripes[index % 4]);
                    g.fillRect (plate.removeFromBottom (3.0f));
                    g.setColour (th.body);
                    g.drawText (p.title, juce::Rectangle<float> (r.getX() + 10.0f, r.getY() + 1.0f, w - 14.0f, 18.0f),
                                juce::Justification::centredLeft);
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
    // Anzeige wie ein Gerätedisplay: 1980 rote Siebensegment-Ziffern, 2100 blau leuchtende Schrift
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
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 3.0f);
                g.setColour (juce::Colour (0xff0a0505));
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (juce::Colour (0xff3a3b40));
                g.drawRoundedRectangle (r, 3.0f, 1.5f);
                auto inner = r.reduced (12.0f, 6.0f);
                auto top = inner.removeFromTop (inner.getHeight() * 0.66f);
                const juce::FontOptions digits (juce::Font::getDefaultMonospacedFontName(), top.getHeight() * 0.9f, juce::Font::bold);
                g.setFont (digits);
                // nicht leuchtende Segmente schimmern durch
                g.setColour (juce::Colour (0xff2a0806));
                juce::String ghost;
                for (auto ch : bigText)
                    ghost << (juce::CharacterFunctions::isDigit (ch) ? juce::juce_wchar ('8') : juce::juce_wchar (' '));
                g.drawText (ghost, top, juce::Justification::centredLeft);
                g.setColour (juce::Colour (0x55ff2a14));
                g.drawText (bigText, top.translated (0.0f, 0.5f).expanded (1.0f), juce::Justification::centredLeft);
                g.setColour (juce::Colour (0xffff3b24));
                g.drawText (bigText, top, juce::Justification::centredLeft);
                g.setColour (juce::Colour (0xffd8402a));
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
        }

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            auto r = getLocalBounds().toFloat();
            const char* names[] { "L", "R" };
            const float colW = r.getWidth() / 2.0f;
            for (int c = 0; c < 2; ++c)
            {
                auto col = juce::Rectangle<float> (r.getX() + colW * (float) c, r.getY(), colW, r.getHeight()).reduced (6.0f, 0.0f);
                auto label = col.removeFromBottom (16.0f);
                g.setColour (th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
                g.drawText (names[c], label, juce::Justification::centred);
                col.removeFromBottom (4.0f);
                if (th.faders())
                {
                    // LED-Kette: grün, gelb, rot
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
        std::array<float, 2> levels {};
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
            const float rad = th.faders() ? 3.0f : 20.0f;
            const auto led = th.faders() ? juce::Colour (0xffff3b24) : th.accent;
            auto r = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
            r.removeFromBottom (6.0f);

            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (r.translated (0.0f, 7.0f).expanded (0.5f), rad);
            g.setColour (th.panel.interpolatedWith (th.ink, 0.45f));
            g.fillRoundedRectangle (r.translated (0.0f, 6.0f), rad);

            auto face = r.translated (0.0f, on ? 4.0f : highlighted ? 1.0f : 0.0f);
            if (th.faders())
            {
                // 1980: farbige Gummitaste wie bei den Drumcomputern
                const auto cap = colour1980.withMultipliedBrightness (on ? 1.15f : 0.9f);
                juce::ColourGradient grad (cap.brighter (0.25f), face.getX(), face.getY(), cap.darker (0.35f), face.getX(), face.getBottom(), false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (face, rad);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.drawRoundedRectangle (face, rad, 1.0f);
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
            if (on)
            {
                g.setColour (led.withAlpha (0.35f));
                g.fillEllipse (ledR.expanded (5.0f));
            }
            g.setColour (on ? led : juce::Colours::black.withAlpha (0.35f));
            g.fillEllipse (ledR);

            const auto textColour = th.faders() ? juce::Colour (0xff141416) : th.ink;
            auto area = face.reduced (14.0f, 8.0f);
            g.setColour (textColour);
            g.setFont (juce::FontOptions (big ? 24.0f : 18.0f, juce::Font::bold).withKerningFactor (0.08f));
            g.drawText (getButtonText(), area.removeFromTop (area.getHeight() * (subtitle.isEmpty() ? 1.0f : 0.6f)),
                        subtitle.isEmpty() ? juce::Justification::centred : juce::Justification::bottomLeft);
            g.setColour (th.faders() ? textColour.withAlpha (0.75f) : th.dim);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (subtitle, area, juce::Justification::topLeft);
        }

        juce::Colour colour1980 { 0xfff08a24 };

    private:
        juce::String subtitle;
        bool big;
    };

    //==========================================================================
    // Farben der Abschnitte: 1980 die farbigen Streifen der Geräte, 2100 Blautöne
    juce::Colour sectionColour (const Design::Theme& th, int type)
    {
        if (th.faders())
        {
            static const juce::uint32 c[] { 0xff5a6a80, 0xfff08a24, 0xffd8382c, 0xfff2c230, 0xff3f86d8, 0xff7a7c82, 0xffff5a3c, 0xff4a4c52 };
            return juce::Colour (c[juce::jlimit (0, 7, type)]);
        }
        static const float a[] { 0.18f, 0.35f, 0.65f, 0.3f, 0.55f, 0.2f, 0.9f, 0.15f };
        return th.accent.withAlpha (a[juce::jlimit (0, 7, type)]);
    }

    juce::String keyName (const Kt::Song& s)
    {
        return Params::keys[Kt::detail::mod (s.root, 12)] + "-" + Params::scales[s.scaleIndex + 1];
    }

    juce::String timeText (double seconds)
    {
        const int t = juce::jmax (0, (int) seconds);
        return juce::String (t / 60).paddedLeft ('0', 2) + ":" + juce::String (t % 60).paddedLeft ('0', 2);
    }

    juce::String dot() { return juce::String::fromUTF8 ("  \xc2\xb7  "); }

    //==========================================================================
    // Steckbrief des Songs: Titel, Epoche, Tonart, Tempo, Länge, Refrain-Wort, Stiltreue, Form
    class SongSummary : public juce::Component
    {
    public:
        explicit SongSummary (KraftTransProcessor& p) : processor (p) {}

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

            g.setFont (juce::FontOptions (28.0f, juce::Font::bold).withKerningFactor (0.03f));
            Design::drawText (g, th, Kt::upper (song->title), r.removeFromTop (38.0f), juce::Justification::centredLeft, th.ink);

            const double seconds = song->totalBeats() * 60.0 / song->tempo;
            g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            Design::drawText (g, th, keyName (*song) + dot() + juce::String (juce::roundToInt (song->tempo)) + " BPM" + dot()
                                     + timeText (seconds) + " min" + dot() + juce::String (song->totalBars) + " Takte",
                              r.removeFromTop (22.0f), juce::Justification::centredLeft, th.dim);
            const int era = juce::jlimit (0, (int) Kt::numEras - 1, song->settings.era);
            g.setFont (juce::FontOptions (12.5f));
            Design::drawText (g, th, Params::eras[era] + ": " + Kt::eraDescription (era), r.removeFromTop (20.0f),
                              juce::Justification::centredLeft, th.dim);
            g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
            Design::drawText (g, th, juce::String::fromUTF8 ("Refrain-Wort  \xc2\xbb") + song->hookWord + juce::String::fromUTF8 ("\xc2\xab"),
                              r.removeFromTop (20.0f), juce::Justification::centredLeft, th.ink);
            r.removeFromTop (8.0f);

            // Stiltreue: wie viele Entscheidungen der Generator stiltypisch getroffen hat
            auto row = r.removeFromTop (22.0f);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.1f));
            Design::drawText (g, th, "STILTREUE", row.removeFromLeft (96.0f), juce::Justification::centredLeft, th.dim);
            auto bar = row.removeFromLeft (row.getWidth() - 60.0f).withSizeKeepingCentre (row.getWidth() - 60.0f, 8.0f);
            g.setColour (th.knobTrack);
            g.fillRoundedRectangle (bar, 4.0f);
            g.setColour (th.faders() ? juce::Colour (0xffff3b24) : th.accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * song->typicality), 4.0f);
            g.setColour (th.ink);
            g.drawText (juce::String (juce::roundToInt (song->typicality * 100.0f)) + " %", row, juce::Justification::centredRight);
            r.removeFromTop (10.0f);

            // Form als Streifen
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.1f));
            Design::drawText (g, th, "FORM", r.removeFromTop (18.0f), juce::Justification::centredLeft, th.dim);
            auto strip = r.removeFromTop (28.0f);
            const double total = song->totalBeats();
            for (auto& sec : song->sections)
            {
                const float x0 = strip.getX() + strip.getWidth() * (float) (sec.startBeat() / total);
                const float x1 = strip.getX() + strip.getWidth() * (float) (sec.endBeat() / total);
                auto cell = juce::Rectangle<float> (x0, strip.getY(), x1 - x0, strip.getHeight()).reduced (1.0f, 0.0f);
                g.setColour (sectionColour (th, sec.type));
                g.fillRoundedRectangle (cell, th.faders() ? 1.0f : 6.0f);
            }
            r.removeFromTop (8.0f);

            // Abschnittsliste in zwei Spalten
            g.setFont (juce::FontOptions (12.5f));
            const int n = (int) song->sections.size();
            const int perCol = (n + 1) / 2;
            const float colW = r.getWidth() / 2.0f;
            const float rowH = juce::jmin (18.0f, r.getHeight() / (float) juce::jmax (1, perCol));
            for (int i = 0; i < n; ++i)
            {
                const auto& sec = song->sections[(size_t) i];
                auto cell = juce::Rectangle<float> (r.getX() + colW * (float) (i / perCol), r.getY() + rowH * (float) (i % perCol), colW - 10.0f, rowH);
                g.setColour (sectionColour (th, sec.type).withAlpha (1.0f));
                g.fillEllipse (cell.removeFromLeft (14.0f).withSizeKeepingCentre (8.0f, 8.0f));
                g.setColour (th.ink);
                g.drawText (juce::String (i + 1) + ". " + Kt::sectionName (sec.type) + (sec.rerolled ? " *" : ""), cell, juce::Justification::centredLeft);
                g.setColour (th.dim);
                g.drawText (juce::String (sec.bars) + " T  " + juce::String (juce::roundToInt (sec.intensity * 100.0f)) + " %"
                                + (sec.transpose != 0 ? "  +" + juce::String (sec.transpose) : juce::String()),
                            cell, juce::Justification::centredRight);
            }
        }

    private:
        KraftTransProcessor& processor;
        std::shared_ptr<const Kt::Song> song;
        int shownStyle = -1;
    };

    //==========================================================================
    // Zeitleiste: Abschnitte, Intensitätskurve und sieben Spuren mit allen Noten
    class Timeline : public juce::Component
    {
    public:
        explicit Timeline (KraftTransProcessor& p) : processor (p)
        {
            for (int i = 0; i < Kt::numLanes; ++i)
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

        static constexpr int headerW = 150, sectionH = 32, curveH = 64, helpH = 22;

        void resized() override
        {
            cache = {};
            const float laneH = laneHeight();
            for (int i = 0; i < Kt::numLanes; ++i)
            {
                const int y = (int) (sectionH + curveH + laneH * (float) i + laneH * 0.5f) - 10;
                buttons[i * 2]->setBounds (headerW - 66, y, 28, 20);
                buttons[i * 2 + 1]->setBounds (headerW - 34, y, 28, 20);
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
            g.setColour (th.faders() ? juce::Colour (0xffff3b24) : th.accent);
            g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            for (size_t i = 0; i < song->sections.size(); ++i)
            {
                const auto pt = handlePoint ((int) i);
                auto dot = juce::Rectangle<float> (12.0f, 12.0f).withCentre (pt);
                g.setColour (th.panel);
                g.fillEllipse (dot);
                g.setColour (th.faders() ? juce::Colour (0xffff3b24) : th.accent);
                if (song->sections[i].overridden) g.fillEllipse (dot.reduced (1.0f));
                else g.drawEllipse (dot.reduced (1.0f), 2.0f);
            }

            // Abspielposition
            const float x = xFor (shownBeat);
            g.setColour (th.faders() ? juce::Colour (0xffff3b24) : th.accent);
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
        float laneHeight() const { return ((float) getHeight() - sectionH - curveH - helpH) / (float) Kt::numLanes; }
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
                g.fillRoundedRectangle (cell, th.faders() ? 1.0f : 8.0f);
                g.setColour (sectionColour (th, s.type).withMultipliedAlpha (0.12f));
                g.fillRect (juce::Rectangle<float> (x0 + 1.0f, (float) sectionH, x1 - x0 - 2.0f, (float) getHeight() - sectionH - helpH));
                g.setColour (th.faders() ? juce::Colour (0xff111214) : sectionColour (th, s.type).getFloatAlpha() > 0.6f ? juce::Colours::white : th.ink);
                g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
                if (cell.getWidth() > 30.0f)
                    g.drawFittedText (Kt::sectionName (s.type) + (s.rerolled ? " *" : ""), cell.reduced (3.0f, 0.0f).toNearestInt(),
                                      juce::Justification::centred, 1, 0.6f);
            }

            // Kopf links
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.08f));
            Design::drawText (g, th, "ABSCHNITT", { 0.0f, 0.0f, (float) headerW - 8.0f, (float) sectionH }, juce::Justification::centredLeft, th.dim);
            Design::drawText (g, th, juce::String::fromUTF8 ("INTENSITÄT"), { 0.0f, (float) sectionH, (float) headerW - 8.0f, (float) curveH },
                              juce::Justification::centredLeft, th.dim);
            g.setColour (th.line.withAlpha (th.faders() ? 0.25f : 0.9f));
            for (float v : { 0.0f, 0.5f, 1.0f })
                g.drawHorizontalLine (juce::roundToInt ((float) sectionH + 8.0f + (curveH - 16.0f) * (1.0f - v)), (float) headerW, w);

            static const int ranges[][2] { { 34, 82 }, { 50, 86 }, { 26, 64 }, { 56, 90 }, { 40, 70 }, { 0, 8 }, { 0, 3 } };
            for (int lane = 0; lane < Kt::numLanes; ++lane)
            {
                const float top = (float) sectionH + curveH + laneH * (float) lane;
                auto row = juce::Rectangle<float> (0.0f, top, w, laneH);
                g.setColour (th.line.withAlpha (th.faders() ? 0.3f : 0.9f));
                g.drawHorizontalLine (juce::roundToInt (top), 0.0f, w);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
                Design::drawText (g, th, Kt::upper (Kt::laneName (lane)), row.withWidth ((float) headerW - 70.0f).reduced (0.0f, 2.0f),
                                  juce::Justification::centredLeft, ink);

                const auto inner = row.withTrimmedLeft ((float) headerW).reduced (0.0f, 4.0f);
                const auto col = th.faders() ? juce::Colour (0xfff1ede4) : th.accent;
                auto yFor = [&] (int pitch)
                {
                    const float t = (float) (pitch - ranges[lane][0]) / (float) (ranges[lane][1] - ranges[lane][0]);
                    return inner.getBottom() - inner.getHeight() * juce::jlimit (0.0f, 1.0f, t);
                };
                const auto& notes = song->notes[(size_t) lane];
                if (lane == Kt::Lead || lane == Kt::Vocal)
                {
                    for (auto& n : notes)
                    {
                        g.setColour (lane == Kt::Vocal && n.harmony ? col.withAlpha (0.4f) : col);
                        g.fillRect (juce::Rectangle<float> (xFor (n.start), yFor (n.pitch) - 1.5f, juce::jmax (1.5f, xFor (n.start + n.length) - xFor (n.start)), 3.0f));
                    }
                }
                else if (lane == Kt::Drums)
                {
                    for (auto& n : notes)
                    {
                        g.setColour (col.withAlpha (0.3f + 0.6f * n.velocity));
                        const float y = inner.getY() + inner.getHeight() * ((float) n.pitch + 0.5f) / (float) Kt::numDrums;
                        g.fillRect (juce::Rectangle<float> (xFor (n.start), y - 1.0f, 1.5f, 2.0f));
                    }
                }
                else if (lane == Kt::Fx)
                {
                    for (auto& n : notes)
                    {
                        const float x0 = xFor (n.start), x1 = xFor (n.start + n.length);
                        switch (n.pitch)
                        {
                            case Kt::Static:
                                g.setColour (col.withAlpha (0.18f));
                                g.fillRoundedRectangle (juce::Rectangle<float> (x0, inner.getY() + inner.getHeight() * 0.2f, x1 - x0, inner.getHeight() * 0.35f), 6.0f);
                                break;
                            case Kt::Swell:
                            {
                                juce::Path p;
                                p.addTriangle (x0, inner.getBottom(), x1, inner.getBottom(), x1, inner.getY());
                                g.setColour (col.withAlpha (0.45f));
                                g.fillPath (p);
                                break;
                            }
                            default:
                            {
                                // Maschinen und Signale: Balken mit Kürzel
                                auto bar = juce::Rectangle<float> (x0, inner.getY() + inner.getHeight() * 0.55f, juce::jmax (3.0f, x1 - x0), inner.getHeight() * 0.4f);
                                g.setColour (col.withAlpha (0.55f));
                                g.fillRoundedRectangle (bar, 3.0f);
                                if (bar.getWidth() > 22.0f)
                                {
                                    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
                                    g.setColour (th.faders() ? juce::Colours::black : th.panel);
                                    g.drawText (Kt::upper (Kt::fxName (n.pitch)).substring (0, 4), bar, juce::Justification::centred);
                                }
                                break;
                            }
                        }
                    }
                }
                else
                {
                    g.setColour (col.withAlpha (lane == Kt::Pad ? 0.55f : 0.8f));
                    const float h = lane == Kt::Pad ? 3.0f : 2.0f;
                    for (auto& n : notes)
                        g.fillRect (juce::Rectangle<float> (xFor (n.start), yFor (n.pitch) - h * 0.5f,
                                                            juce::jmax (1.0f, xFor (n.start + n.length) - xFor (n.start) - 0.5f), h));
                }
            }

            // Zeitmarken (Minuten) und Hilfe
            const double secondsPerBeat = 60.0 / song->tempo;
            const double totalSeconds = song->totalBeats() * secondsPerBeat;
            const double stepSec = totalSeconds > 360.0 ? 60.0 : 30.0;
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

        KraftTransProcessor& processor;
        std::shared_ptr<const Kt::Song> song;
        juce::Image cache;
        int shownStyle = -1, dragIndex = -1, rerollIndex = -1;
        float lastDragValue = -1.0f;
        double shownBeat = -1.0;
        juce::OwnedArray<juce::TextButton> buttons;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> attachments;
    };

    //==========================================================================
    // Textzeile der Roboterstimme: die laufende Phrase als Tonhöhen-Balken mit Silben, die gesprochene Silbe leuchtet
    class LyricView : public juce::Component
    {
    public:
        explicit LyricView (KraftTransProcessor& p) : processor (p) {}

        void refresh()
        {
            auto s = processor.getSong();
            const double beat = processor.shownBeat.load();
            const bool on = processor.apvts.getRawParameterValue ("vocal_on")->load() > 0.5f;
            const int style = lookOf (*this).getTheme().style;
            if (s != song || std::abs (beat - shownBeat) > 0.02 || on != vocalOn || style != shownStyle)
            {
                song = s;
                shownBeat = beat;
                vocalOn = on;
                shownStyle = style;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            auto r = getLocalBounds().toFloat();
            const auto lit = th.faders() ? juce::Colour (0xffff3b24) : th.accent;
            if (th.faders())
            {
                g.setColour (juce::Colour (0xff0a0505));
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (juce::Colour (0xff3a3b40));
                g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.5f);
            }
            else
            {
                g.setColour (th.field);
                g.fillRoundedRectangle (r, 16.0f);
            }
            r = r.reduced (20.0f, 12.0f);
            if (song == nullptr)
                return;

            // Phrase rund um die Abspielposition finden (Pausen ab knapp einem Viertel trennen Phrasen)
            const auto& v = song->notes[Kt::Vocal];
            std::vector<int> main;
            for (int i = 0; i < (int) v.size(); ++i)
                if (! v[(size_t) i].harmony) main.push_back (i);
            const auto& sec = song->sections[(size_t) song->sectionIndexAt (shownBeat)];
            auto header = r.removeFromTop (20.0f);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold).withKerningFactor (0.12f));
            g.setColour (th.faders() ? juce::Colour (0xffd8402a) : th.dim);
            g.drawText (Kt::upper (Kt::sectionName (sec.type)) + (sec.lanes[Kt::Vocal] ? juce::String() : juce::String::fromUTF8 ("  \xc2\xb7  STIMME PAUSIERT")),
                        header, juce::Justification::centredLeft);
            g.drawText (vocalOn ? juce::String::fromUTF8 ("REFRAIN-WORT \xc2\xbb") + Kt::upper (song->hookWord) + juce::String::fromUTF8 ("\xc2\xab") : "STIMME AUS",
                        header, juce::Justification::centredRight);
            if (main.empty())
                return;

            int at = -1;
            for (int k = 0; k < (int) main.size(); ++k)
                if (v[(size_t) main[(size_t) k]].start <= shownBeat + 0.01) at = k; else break;
            if (at < 0) at = 0;
            const auto& cur = v[(size_t) main[(size_t) at]];
            if (shownBeat > cur.start + cur.length + 0.5 && at + 1 < (int) main.size() && v[(size_t) main[(size_t) at + 1]].start - shownBeat < 3.0)
                ++at;
            auto gap = [&] (int a, int b) { const auto& x = v[(size_t) main[(size_t) a]]; return v[(size_t) main[(size_t) b]].start - (x.start + x.length); };
            int first = at, last = at;
            while (first > 0 && gap (first - 1, first) < 0.9) --first;
            while (last + 1 < (int) main.size() && gap (last, last + 1) < 0.9) ++last;

            const double t0 = v[(size_t) main[(size_t) first]].start;
            const double t1 = juce::jmax (t0 + 4.0, v[(size_t) main[(size_t) last]].start + v[(size_t) main[(size_t) last]].length);
            int lo = 127, hi = 0;
            for (int k = first; k <= last; ++k) { lo = juce::jmin (lo, v[(size_t) main[(size_t) k]].pitch); hi = juce::jmax (hi, v[(size_t) main[(size_t) k]].pitch); }
            lo -= 2; hi += 2;
            auto textRow = r.removeFromBottom (44.0f);
            r.removeFromBottom (6.0f);
            auto xFor = [&] (double beat) { return r.getX() + r.getWidth() * (float) ((beat - t0) / (t1 - t0)); };
            auto yFor = [&] (int p) { return r.getBottom() - r.getHeight() * (float) (p - lo) / (float) juce::jmax (1, hi - lo); };

            // Hilfslinien
            g.setColour ((th.faders() ? juce::Colour (0xff3a1a14) : th.line));
            for (int k = 0; k < 4; ++k)
                g.drawHorizontalLine (juce::roundToInt (r.getY() + r.getHeight() * (float) k / 3.0f), r.getX(), r.getRight());

            for (int k = first; k <= last; ++k)
            {
                const auto& n = v[(size_t) main[(size_t) k]];
                const bool now = vocalOn && shownBeat >= n.start - 0.01 && shownBeat < n.start + n.length + 0.1;
                const float x0 = xFor (n.start), x1 = juce::jmax (x0 + 6.0f, xFor (n.start + n.length) - 3.0f);
                auto bar = juce::Rectangle<float> (x0, yFor (n.pitch) - 6.0f, x1 - x0, 12.0f);
                if (now)
                {
                    g.setColour (lit.withAlpha (0.3f));
                    g.fillRoundedRectangle (bar.expanded (4.0f), 8.0f);
                }
                g.setColour (now ? lit : (th.faders() ? juce::Colour (0xff7a2a20) : th.dim.withAlpha (vocalOn ? 0.8f : 0.3f)));
                g.fillRoundedRectangle (bar, th.faders() ? 2.0f : 6.0f);

                // Silbe darunter; Wortenden bekommen Abstand, sonst Bindestrich
                juce::String text = song->syllableAt (n.syllable);
                if (! n.wordEnd && k < last) text << "-";
                g.setFont (juce::FontOptions (now ? 26.0f : 20.0f, juce::Font::bold));
                g.setColour (now ? lit : (th.faders() ? juce::Colour (0xffd8402a) : th.ink.withAlpha (vocalOn ? 0.75f : 0.3f)));
                g.drawText (text, juce::Rectangle<float> (x0 - 4.0f, textRow.getY(), juce::jmax (40.0f, x1 - x0 + 30.0f), textRow.getHeight()),
                            juce::Justification::centredLeft);
            }
            // Spielposition
            if (shownBeat >= t0 && shownBeat <= t1)
            {
                g.setColour (lit.withAlpha (0.6f));
                g.fillRect (juce::Rectangle<float> (xFor (shownBeat) - 1.0f, r.getY(), 2.0f, r.getHeight()));
            }
        }

    private:
        KraftTransProcessor& processor;
        std::shared_ptr<const Kt::Song> song;
        double shownBeat = -1.0;
        bool vocalOn = true;
        int shownStyle = -1;
    };

    //==========================================================================
    // Lichtraster: ein Neon-Gitter wie eine Bühnenwand aus Leuchtröhren. Jede Spur hat eine Spalte, die nach
    // ihrem Pegel aufleuchtet; darüber läuft ein Sechzehntel-Lauflicht im Takt, unten zeigt eine Laufschrift
    // die gerade gesprochene Silbe.
    class StageLights : public juce::Component
    {
    public:
        explicit StageLights (KraftTransProcessor& p) : processor (p) {}

        void refresh()
        {
            for (int i = 0; i < Kt::numLanes; ++i)
            {
                const float v = juce::jlimit (0.0f, 1.0f, std::sqrt (processor.engine.laneLevel[(size_t) i].load() * 2.5f));
                shown[(size_t) i] = juce::jmax (v, shown[(size_t) i] * 0.82f);
            }
            beat = processor.shownBeat.load();
            song = processor.getSong();
            repaint();
        }

    private:
        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            const bool retro = th.faders();
            auto r = getLocalBounds().toFloat();
            g.setColour (retro ? juce::Colour (0xff070809) : juce::Colour (0xff050608));
            g.fillRoundedRectangle (r, 3.0f);

            static const juce::uint32 neon[] { 0xff31e0ff, 0xff4cff6a, 0xffff3b24, 0xfff2e030, 0xffff4fd8, 0xffffffff, 0xff8a6cff };
            auto colourOf = [&] (int lane) { return juce::Colour (neon[lane]); };

            auto area = r.reduced (10.0f, 8.0f);
            auto legend = area.removeFromBottom (14.0f);
            auto ticker = area.removeFromBottom (30.0f);
            auto runner = area.removeFromTop (14.0f);
            area.removeFromTop (6.0f);

            // Lauflicht: 16 Felder, das aktuelle Sechzehntel leuchtet
            const int step = (int) std::floor (std::fmod (juce::jmax (0.0, beat) * 4.0, 16.0));
            const float sw = runner.getWidth() / 16.0f;
            for (int k = 0; k < 16; ++k)
            {
                auto cell = juce::Rectangle<float> (runner.getX() + sw * (float) k, runner.getY(), sw, runner.getHeight()).reduced (2.0f, 3.0f);
                const bool on = k == step;
                g.setColour (on ? juce::Colour (0xffff3b24) : juce::Colour (k % 4 == 0 ? 0xff3a1a14 : 0xff1c1010));
                g.fillRect (cell);
                if (on)
                {
                    g.setColour (juce::Colour (0x55ff3b24));
                    g.fillRect (cell.expanded (3.0f, 2.0f));
                }
            }

            // Gitter: Spalten = Spuren, Zeilen = Pegel
            constexpr int rows = 12;
            const float cw = area.getWidth() / (float) Kt::numLanes;
            const float rh = area.getHeight() / (float) rows;
            for (int lane = 0; lane < Kt::numLanes; ++lane)
            {
                const auto c = colourOf (lane);
                const int litRows = juce::roundToInt (shown[(size_t) lane] * rows);
                for (int k = 0; k < rows; ++k)
                {
                    auto cell = juce::Rectangle<float> (area.getX() + cw * (float) lane, area.getBottom() - rh * (float) (k + 1), cw, rh).reduced (3.0f, 2.0f);
                    const bool on = k < litRows;
                    if (on)
                    {
                        g.setColour (c.withAlpha (0.18f));
                        g.fillRect (cell.expanded (2.0f));
                    }
                    g.setColour (on ? c.withAlpha (0.45f + 0.55f * (float) k / rows) : c.withAlpha (0.07f));
                    g.drawRect (cell, on ? 2.0f : 1.0f);
                }
            }

            // Laufschrift: gesprochene Silbe
            juce::String word;
            if (song != nullptr)
            {
                const auto& v = song->notes[Kt::Vocal];
                for (const auto& n : v)
                {
                    if (n.start > beat) break;
                    if (! n.harmony && beat < n.start + n.length + 0.15) word = song->syllableAt (n.syllable);
                }
            }
            g.setColour (juce::Colour (0xff0e0a08));
            g.fillRect (ticker.reduced (0.0f, 2.0f));
            g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 20.0f, juce::Font::bold).withKerningFactor (0.25f));
            g.setColour (juce::Colour (word.isEmpty() ? 0x55ff3b24 : 0xffff3b24));
            g.drawText (word.isEmpty() ? juce::String ("- - -") : Kt::upper (word), ticker, juce::Justification::centred);

            // Legende
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold).withKerningFactor (0.06f));
            for (int i = 0; i < Kt::numLanes; ++i)
            {
                g.setColour (colourOf (i).withAlpha (0.4f + 0.6f * shown[(size_t) i]));
                g.drawFittedText (Kt::upper (Kt::laneName (i)), juce::Rectangle<float> (legend.getX() + cw * (float) i, legend.getY(), cw, legend.getHeight()).toNearestInt(),
                                  juce::Justification::centred, 1, 0.6f);
            }
        }

        KraftTransProcessor& processor;
        std::shared_ptr<const Kt::Song> song;
        std::array<float, Kt::numLanes> shown {};
        double beat = 0.0;
    };

    //==========================================================================
    class Content : public juce::Component, private juce::Timer
    {
    public:
        Content (KraftTransProcessor& p, TrendyLook& lf)
            : processor (p), look (lf), summary (p), timeline (p), lyrics (p), lights (p),
              startPad ("START", "eigene Uhr"), homePad ("ANFANG", "zum Songbeginn", false),
              vocalPad ("STIMME", "Roboter an / aus"),
              rollPad ("NEUER SONG", juce::String::fromUTF8 ("würfeln: Form, Noten, Tempo")),
              variationPad ("VARIATION", "gleiche Form, neue Noten")
        {
            setLookAndFeel (&lf);
            grain = Design::plasticGrain();

            // Design-Umschalter: zwei Taster
            const char* designTitles[] { "1980", "2100" };
            const char* designSubs[]   { "Plastik, Schieberegler", "Minimal, Gummipotis" };
            for (int i = 0; i < 2; ++i)
            {
                auto* b = designButtons.add (new TabButton (designTitles[i], designSubs[i], 1002));
                b->onClick = [this, i] { processor.design.store (i); applyDesign(); };
                addAndMakeVisible (b);
            }

            const char* tabTitles[] { "Song", "Zeitleiste", "Klang", "Stimme", "Mischpult" };
            const char* tabSubs[]   { "Würfel, Epoche, Länge", "Abschnitte, Spuren", "Synths, Drums, Geräusche", "Roboter, Text, Raum", "Pegel, Echo, Hall" };
            for (int i = 0; i < numPages; ++i)
            {
                auto* t = tabs.add (new TabButton (tabTitles[i], juce::String::fromUTF8 (tabSubs[i])));
                t->onClick = [this, i] { showPage (i); };
                addAndMakeVisible (t);
                auto* pg = pages.add (new Page());
                addChildComponent (pg);
            }

            buildSongPage();
            buildTimelinePage();
            buildSoundPage();
            buildVocalPage();
            buildMixPage();

            addAndMakeVisible (display);
            addAndMakeVisible (meter);

            startPad.colour1980 = juce::Colour (0xff4bd04b);
            startPad.onClick = [this] { processor.setRunning (! processor.running.load()); timerCallback(); };
            addAndMakeVisible (startPad);
            homePad.colour1980 = juce::Colour (0xffeeeae0);
            homePad.onClick = [this] { processor.seek (0.0); };
            addAndMakeVisible (homePad);
            vocalPad.colour1980 = juce::Colour (0xff3f86d8);
            vocalPad.setClickingTogglesState (true);
            vocalAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "vocal_on", vocalPad);
            addAndMakeVisible (vocalPad);
            rollPad.colour1980 = juce::Colour (0xffd8382c);
            rollPad.onClick = [this] { processor.rollNewTrack(); };
            addAndMakeVisible (rollPad);
            variationPad.colour1980 = juce::Colour (0xfff2c230);
            variationPad.onClick = [this] { processor.rollVariation(); };
            addAndMakeVisible (variationPad);

            setSize (KraftTransEditor::designWidth, KraftTransEditor::designHeight);
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
                g.setColour (th.ink);
                g.setFont (juce::FontOptions (31.0f, juce::Font::bold | juce::Font::italic).withKerningFactor (0.1f));
                g.drawText ("KRAFT TRANS", 20, 12, 380, 38, juce::Justification::centredLeft);
                g.setColour (th.accent);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.15f));
                g.drawText ("ELEKTRO-POP SONG GENERATOR " + versionLabel(), 410, 14, 360, 18, juce::Justification::centredLeft);
                g.setColour (th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
                g.drawText (makerLabel(), 410, 32, 360, 16, juce::Justification::centredLeft);
            }
            else
            {
                g.setFont (juce::FontOptions (29.0f, juce::Font::bold).withKerningFactor (0.08f));
                Design::drawText (g, th, "KRAFT TRANS", { 20.0f, 12.0f, 380.0f, 38.0f }, juce::Justification::centredLeft, th.ink);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.15f));
                Design::drawText (g, th, "ELEKTRO-POP SONG GENERATOR " + versionLabel(), { 410.0f, 14.0f, 360.0f, 18.0f }, juce::Justification::centredLeft, th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
                Design::drawText (g, th, makerLabel(), { 410.0f, 32.0f, 360.0f, 16.0f }, juce::Justification::centredLeft, th.dim);
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

            auto tabRow = juce::Rectangle<int> (16, 64, numPages * 182, 62);
            for (auto* t : tabs)
                t->setBounds (tabRow.removeFromLeft (182).withTrimmedRight (8));

            const auto pageArea = juce::Rectangle<int> (16, 138, getWidth() - 32, getHeight() - 138 - 132);
            for (auto* pg : pages)
                pg->setBounds (pageArea);

            // Transportleiste
            auto strip = juce::Rectangle<int> (24, getHeight() - 116, getWidth() - 48, 100);
            display.setBounds (strip.removeFromLeft (330).reduced (0, 6));
            strip.removeFromLeft (12);
            meter.setBounds (strip.removeFromLeft (60).reduced (0, 4));
            strip.removeFromLeft (14);
            startPad.setBounds (strip.removeFromLeft (132));
            strip.removeFromLeft (8);
            homePad.setBounds (strip.removeFromLeft (118));
            strip.removeFromLeft (8);
            vocalPad.setBounds (strip.removeFromLeft (152));
            variationPad.setBounds (strip.removeFromRight (172));
            strip.removeFromRight (10);
            rollPad.setBounds (strip.removeFromRight (206));

            layoutSongPage();
            layoutTimelinePage();
            layoutSoundPage();
            layoutVocalPage();
            layoutMixPage();
        }

    private:
        static constexpr int numPages = 5;

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
        void buildSongPage()
        {
            auto& pg = *pages[0];
            pg.panels = {
                { "GENERATOR", {}, { knob (pg, "length", de ("Länge"), 104), knob (pg, "intensity", de ("Intensität"), 104),
                                     knob (pg, "closeness", de ("Nähe zum Original"), 104), knob (pg, "variety", "Abwechslung", 104),
                                     choice (pg, "era", "Epoche", 156), toggle (pg, "era_sound", "Klang folgt") } },
                { "TONART & TEMPO", {}, { choice (pg, "key", "Tonart", 74), choice (pg, "scale", "Skala", 124), knob (pg, "tempo", "Tempo", 104),
                                          toggle (pg, "roll_all", de ("Mitwürfeln")), toggle (pg, "follow_host", "Host-Tempo"),
                                          toggle (pg, "loop", "Schleife") } },
                { "DER SONG", {}, {} },
            };
            transposeControl = toggle (pg, "midi_transpose", "MIDI transponiert");
            pg.addAndMakeVisible (summary);
            exportButton.setButtonText ("MIDI SPEICHERN");
            exportButton.onClick = [this] { exportMidi(); };
            pg.addAndMakeVisible (exportButton);
            pg.paintExtra = [this] (juce::Graphics& g) { paintSongHelp (g); };
        }

        void layoutSongPage()
        {
            auto& pg = *pages[0];
            const int h = (pg.getHeight() - 12) / 2;
            const int left = 760;
            pg.panels[0].bounds = { 0, 0, left, h };
            pg.panels[1].bounds = { 0, h + 12, left, h };
            pg.panels[2].bounds = { left + 12, 0, pg.getWidth() - left - 12, pg.getHeight() };
            Page::layout (pg.panels[0], 160);
            Page::layout (pg.panels[1], 160);
            auto r = pg.panels[2].bounds.reduced (20, 0).withTrimmedTop (38).withTrimmedBottom (14);
            auto bottom = r.removeFromBottom (64);
            summary.setBounds (r);
            transposeControl->setBounds (bottom.removeFromLeft (110));
            exportButton.setBounds (bottom.removeFromRight (170).withSizeKeepingCentre (170, 34).withY (bottom.getY() + 24));
        }

        void paintSongHelp (juce::Graphics& g)
        {
            const auto& th = look.getTheme();
            auto r = pages[0]->panels[2].bounds.toFloat().reduced (20.0f, 0.0f).withTrimmedBottom (14.0f);
            r = r.removeFromBottom (64.0f).withTrimmedLeft (118.0f).withTrimmedRight (176.0f);
            g.setFont (juce::FontOptions (11.0f));
            g.setColour (th.dim);
            g.drawFittedText (de ("Tasten im Host rücken den ganzen Song, C3 = Originallage."), r.toNearestInt().withTrimmedTop (18), juce::Justification::centredLeft, 3, 1.0f);
        }

        void exportMidi()
        {
            auto song = processor.getSong();
            if (song == nullptr) return;
            const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                   .getChildFile (juce::File::createLegalFileName (song->title) + ".mid");
            chooser = std::make_unique<juce::FileChooser> ("Song als MIDI speichern", start, "*.mid");
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
            timeline.setBounds (pg.getLocalBounds().reduced (16, 0).withTrimmedTop (36).withTrimmedBottom (8));
        }

        void buildSoundPage()
        {
            auto& pg = *pages[2];
            pg.panels = {
                { de ("FLÄCHEN"), {}, { choice (pg, "pad_type", "Klang", 112), knob (pg, "pad_bright", "Helligkeit", 72),
                                        knob (pg, "pad_attack", "Anschwellen", 72), knob (pg, "pad_ensemble", "Ensemble", 72),
                                        knob (pg, "pad_vowel", "Chor-Vokal", 72) } },
                { "SEQUENZ", {}, { knob (pg, "seq_cutoff", "Cutoff", 72), knob (pg, "seq_reso", "Resonanz", 72),
                                knob (pg, "seq_env", "Filterkurve", 72), knob (pg, "seq_decay", "Abklingen", 72),
                                choice (pg, "seq_wave", "Welle", 92) } },
                { de ("GERÄUSCHE"), {}, { knob (pg, "fx_noise", "Rauschen", 72), knob (pg, "fx_machine", "Maschinen", 72), knob (pg, "fx_signal", "Signale", 72) } },
                { "BASS", {}, { knob (pg, "bass_cutoff", "Cutoff", 72), knob (pg, "bass_decay", "Abklingen", 72), knob (pg, "bass_sub", "Sub", 72),
                                knob (pg, "bass_drive", "Biss", 72) } },
                { "MELODIE", {}, { knob (pg, "lead_glide", "Gleiten", 72), knob (pg, "lead_vibrato", "Vibrato", 72),
                                   knob (pg, "lead_bright", "Helligkeit", 72), choice (pg, "lead_wave", "Welle", 132) } },
                { "ELEKTRO-DRUMS", {}, { choice (pg, "drum_kit", "Kit", 140), knob (pg, "drum_tone", "Ton", 72), knob (pg, "drum_swing", "Swing", 72) } },
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

        void buildVocalPage()
        {
            auto& pg = *pages[3];
            pg.panels = {
                { "ROBOTERSTIMME", {}, { toggle (pg, "vocal_on", "Stimme"), choice (pg, "voice_type", "Stimmart", 128), choice (pg, "vocal_text", "Text", 132),
                                         knob (pg, "vocal_melody", "Melodie", 72), knob (pg, "vocal_bands", de ("Auflösung"), 72),
                                         knob (pg, "vocal_carrier", de ("Träger"), 72), knob (pg, "vocal_crush", de ("Körnung"), 72),
                                         knob (pg, "vocal_metal", "Metall", 72) } },
                { "RAUM", {}, { knob (pg, "vocal_double", "Doppelung", 72), knob (pg, "vocal_harmony", "Zweitstimme", 72),
                                knob (pg, "vocal_reverb", "Hall", 72), knob (pg, "vocal_echo", "Echo", 72) } },
                { "EIGENER TEXT", {}, {} },
                { "TEXTZEILE", {}, {} },
            };
            ownText.setMultiLine (true, true);
            ownText.setReturnKeyStartsNewLine (false);
            ownText.setFont (juce::FontOptions (16.0f, juce::Font::bold));
            ownText.setText (processor.getOwnText(), false);
            ownText.setTextToShowWhenEmpty (de ("Text eintippen, die Maschine spricht ihn"), juce::Colours::grey);
            ownText.onReturnKey = [this] { speakOwnText(); };
            pg.addAndMakeVisible (ownText);
            speakButton.setButtonText ("SPRECHEN");
            speakButton.onClick = [this] { speakOwnText(); };
            pg.addAndMakeVisible (speakButton);
            pg.addAndMakeVisible (lyrics);
            pg.paintExtra = [this] (juce::Graphics& g) { paintVocalHelp (g); };
        }

        // eigenen Text übernehmen und auf "Eigener Text" schalten
        void speakOwnText()
        {
            processor.setOwnText (ownText.getText());
            if (auto* p = processor.apvts.getParameter ("vocal_text"))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) Kt::OwnText));
                p->endChangeGesture();
            }
        }

        void layoutVocalPage()
        {
            auto& pg = *pages[3];
            const int h = 200;
            row (pg, { 0, 1 }, 0, h);
            const int left = 430;
            pg.panels[2].bounds = { 0, h + 12, left, pg.getHeight() - h - 12 };
            pg.panels[3].bounds = { left + 12, h + 12, pg.getWidth() - left - 12, pg.getHeight() - h - 12 };
            for (auto& p : pg.panels)
                Page::layout (p, 150);
            auto r = pg.panels[2].bounds.reduced (16, 0).withTrimmedTop (36).withTrimmedBottom (14);
            auto buttons = r.removeFromBottom (40);
            r.removeFromBottom (8);
            ownText.setBounds (r);
            speakButton.setBounds (buttons.removeFromRight (150));
            lyrics.setBounds (pg.panels[3].bounds.reduced (16, 0).withTrimmedTop (34).withTrimmedBottom (34));
        }

        void paintVocalHelp (juce::Graphics& g)
        {
            const auto& th = look.getTheme();
            g.setFont (juce::FontOptions (11.0f));
            g.setColour (th.dim);
            auto r = pages[3]->panels[3].bounds.toFloat().reduced (18.0f, 0.0f).removeFromBottom (30.0f);
            g.drawText (de ("Synthetische Roboterstimme: Formant-Sprachsynthese, wahlweise durch Vocoder, Sprachchip oder Ringmodulator. Kein Nachbau einer echten Stimme."),
                        r, juce::Justification::centredLeft);
            auto t = pages[3]->panels[2].bounds.toFloat().reduced (16.0f, 0.0f).withTrimmedBottom (14.0f).removeFromBottom (40.0f).withTrimmedRight (160.0f);
            g.drawFittedText (de ("Enter oder SPRECHEN: Text wird zu Silben und Lauten."), t.toNearestInt(), juce::Justification::centredLeft, 2, 1.0f);
        }

        void buildMixPage()
        {
            auto& pg = *pages[4];
            pg.panels = {
                { "SPUREN", {}, { knob (pg, "vol_pad", de ("Flächen"), 74), knob (pg, "vol_seq", "Sequenz", 74), knob (pg, "vol_bass", "Bass", 74),
                                  knob (pg, "vol_lead", "Melodie", 74), knob (pg, "vol_vocal", "Stimme", 74), knob (pg, "vol_drums", "Rhythmus", 74),
                                  knob (pg, "vol_fx", de ("Geräusche"), 74), knob (pg, "master", "Master", 74) } },
                { "ECHO", {}, { choice (pg, "echo_div", "Teilung", 90), knob (pg, "echo_fb", "Feedback", 72), knob (pg, "echo_mix", "Echo", 72) } },
                { "HALL", {}, { knob (pg, "rev_size", de ("Raumgröße"), 72), knob (pg, "rev_mix", "Hall", 72), knob (pg, "width", "Breite", 72) } },
                { "LICHTRASTER", {}, {} },
            };
            pg.addAndMakeVisible (lights);
        }

        void layoutMixPage()
        {
            auto& pg = *pages[4];
            const int h = (pg.getHeight() - 12) / 2;
            const int left = 780;
            pg.panels[0].bounds = { 0, 0, left, h };
            row (pg, { 1, 2 }, h + 12, h, 0, left);
            pg.panels[3].bounds = { left + 12, 0, pg.getWidth() - left - 12, pg.getHeight() };
            for (auto& p : pg.panels)
                Page::layout (p, 160);
            lights.setBounds (pg.panels[3].bounds.reduced (16).withTrimmedTop (26));
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
            ownText.applyColourToAllText (look.getTheme().ink);
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
                g.fillAll (th.body);
                g.setTiledImageFill (grain, 0, 0, 1.0f);
                g.fillAll();
                juce::ColourGradient shade (juce::Colours::white.withAlpha (0.05f), 0.0f, 0.0f,
                                            juce::Colours::black.withAlpha (0.15f), 0.0f, r.getBottom(), false);
                g.setGradientFill (shade);
                g.fillAll();
            }
            else
            {
                juce::ColourGradient light (juce::Colour (0xfffbfcfd), r.getCentreX(), 0.0f,
                                            th.body, r.getCentreX(), r.getHeight() * 0.7f, true);
                g.setGradientFill (light);
                g.fillAll();
            }

            g.setColour (th.faders() ? juce::Colour (0xff121315) : juce::Colour (0xffdde1e6));
            g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, (float) getHeight() - 124.0f, (float) getWidth() - 24.0f, 116.0f),
                                    th.faders() ? 3.0f : 18.0f);
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
                small << "TAKT " << bar << "/" << song->totalBars << "  " << Kt::upper (Kt::sectionName (sec.type))
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
            if (currentPage == 3) lyrics.refresh();
            if (currentPage == 4) lights.refresh();
        }

        KraftTransProcessor& processor;
        TrendyLook& look;
        juce::Image grain, background;
        juce::OwnedArray<TabButton> tabs, designButtons;
        juce::OwnedArray<Page> pages;
        juce::OwnedArray<Labelled> controls;
        int currentPage = 0;
        Labelled* transposeControl = nullptr;

        SongSummary summary;
        Timeline timeline;
        LyricView lyrics;
        StageLights lights;
        juce::TextEditor ownText;
        juce::TextButton speakButton;
        juce::TextButton exportButton;
        std::unique_ptr<juce::FileChooser> chooser;
        LedDisplay display;
        Meter meter;
        DubPad startPad, homePad, vocalPad, rollPad, variationPad;
        std::unique_ptr<ButtonAttachment> vocalAttachment;
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
    setColour (juce::TextEditor::backgroundColourId, theme.field);
    setColour (juce::TextEditor::textColourId, theme.ink);
    setColour (juce::TextEditor::outlineColourId, theme.line);
    setColour (juce::TextEditor::focusedOutlineColourId, theme.accent);
    setColour (juce::TextEditor::highlightColourId, theme.accent.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, theme.accent);
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

void TrendyLook::drawFader (juce::Graphics& g, juce::Rectangle<float> area, float pos, bool bipolar, bool horizontal, float squish)
{
    pos = juce::jlimit (0.0f, 1.0f, pos);
    const float indent = (float) faderIndent;
    const auto printed = theme.ink.withAlpha (0.8f);
    const auto cap1980 = [&] (juce::Rectangle<float> cap)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (cap.translated (0.0f, 3.0f), 2.0f);
        juce::ColourGradient grad (juce::Colour (0xff46474c), cap.getX(), cap.getY(), juce::Colour (0xff0d0e10), cap.getX(), cap.getBottom(), false);
        grad.addColour (0.4, juce::Colour (0xff232427));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (cap, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawLine (cap.getX() + 2.0f, cap.getY() + 0.8f, cap.getRight() - 2.0f, cap.getY() + 0.8f, 1.0f);
    };

    if (! horizontal)
    {
        const float cx = area.getCentreX() + 6.0f;
        const float top = area.getY() + indent, bottom = area.getBottom() - indent, len = bottom - top;

        // aufgedruckte Skala: Striche und 10 / 5 / 0
        g.setColour (printed.withAlpha (0.45f));
        for (int i = 0; i <= 10; ++i)
        {
            const float y = bottom - len * (float) i / 10.0f;
            g.drawLine (cx - 15.0f, y, cx - 10.0f, y, 1.0f);
            g.drawLine (cx + 10.0f, y, cx + 15.0f, y, 1.0f);
        }
        g.setColour (printed);
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        const char* marks[] { "0", "5", "10" };
        const char* bmarks[] { "-", "0", "+" };
        for (int i = 0; i < 3; ++i)
            g.drawText (bipolar ? bmarks[i] : marks[i], juce::Rectangle<float> (cx - 32.0f, bottom - len * (float) i * 0.5f - 6.0f, 15.0f, 12.0f),
                        juce::Justification::centredRight);

        // Schlitz und Füllung
        g.setColour (juce::Colour (0xff070708));
        g.fillRoundedRectangle (juce::Rectangle<float> (6.0f, len + 8.0f).withCentre ({ cx, (top + bottom) * 0.5f }), 3.0f);
        const float from = bipolar ? 0.5f : 0.0f;
        const float ya = bottom - len * juce::jmax (from, pos), yb = bottom - len * juce::jmin (from, pos);
        g.setColour (theme.accent);
        g.fillRect (juce::Rectangle<float> (cx - 1.0f, ya, 2.0f, yb - ya));

        // Kappe: schwarz mit weißem Strich, gibt beim Drücken etwas nach
        const float y = bottom - len * pos;
        auto cap = juce::Rectangle<float> (30.0f * (1.0f - 0.05f * squish), 17.0f * (1.0f - 0.1f * squish)).withCentre ({ cx, y });
        cap1980 (cap);
        g.setColour (juce::Colour (0xfff1f0ea));
        g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6.0f, 2.0f));
        return;
    }

    // waagerechter Mini-Fader (Matrix, Sequenzer-Reihen)
    const float cy = area.getCentreY();
    const float left = area.getX() + indent, right = area.getRight() - indent, len = right - left;
    g.setColour (juce::Colour (0xff070708));
    g.fillRoundedRectangle (juce::Rectangle<float> (len + 8.0f, 4.0f).withCentre ({ (left + right) * 0.5f, cy }), 2.0f);
    if (bipolar)
    {
        g.setColour (printed.withAlpha (0.5f));
        g.drawLine ((left + right) * 0.5f, cy - 7.0f, (left + right) * 0.5f, cy - 4.0f, 1.0f);
    }
    const float from = bipolar ? 0.5f : 0.0f;
    const float xa = left + len * juce::jmin (from, pos), xb = left + len * juce::jmax (from, pos);
    g.setColour (theme.accent);
    g.fillRect (juce::Rectangle<float> (xa, cy - 1.0f, xb - xa, 2.0f));
    auto cap = juce::Rectangle<float> (10.0f, 18.0f * (1.0f - 0.1f * squish)).withCentre ({ left + len * pos, cy });
    cap1980 (cap);
    g.setColour (juce::Colour (0xfff1f0ea));
    g.fillRect (cap.withSizeKeepingCentre (2.0f, cap.getHeight() - 6.0f));
}


//==============================================================================
KraftTransEditor::KraftTransEditor (KraftTransProcessor& p)
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

KraftTransEditor::~KraftTransEditor()
{
    content.reset();
    setLookAndFeel (nullptr);
}

void KraftTransEditor::resized()
{
    if (content != nullptr)
        content->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth));
}
