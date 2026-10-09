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
        int preferredWidth() const override { return slider.isFader() ? 64 : juce::jmax (92, knobSize + 8); }

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
            const char* names[] { "IN", "OUT" };
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
    // Kurve der Schleife: wie stark jede Frequenz pro Umlauf zurückkommt; über 0 dB schwingt das Band selbst
    class LoopCurve : public juce::Component
    {
    public:
        explicit LoopCurve (TrendyDelayProcessor& p) : processor (p) {}

        void refresh()
        {
            const std::array<float, 6> now { value ("feedback"), value ("low_cut"), value ("high_cut"), value ("resonance"),
                                             value ("age"), value ("freeze") };
            if (now != last)
            {
                last = now;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            auto r = getLocalBounds().toFloat().reduced (4.0f);
            const float fb = last[5] > 0.5f ? 1.0f : last[0];
            const float lowCut = last[1], highCut = juce::jmax (last[2], last[1] * 1.2f);
            const float q = 0.707f + last[3] * 2.6f;
            const float ageHz = 18000.0f - 13000.0f * last[4];
            const float dbTop = 12.0f, dbBottom = -36.0f;
            auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (dbBottom, dbTop, db), dbTop, dbBottom, r.getY(), r.getBottom()); };
            auto xFor = [&] (float hz) { return juce::jmap (std::log10 (hz / 20.0f) / 3.0f, r.getX(), r.getRight()); };

            // Raster
            g.setColour (th.line.withAlpha (th.faders() ? 0.25f : 0.9f));
            for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
                g.drawVerticalLine (juce::roundToInt (xFor (hz)), r.getY(), r.getBottom());
            for (float db : { -24.0f, -12.0f })
                g.drawHorizontalLine (juce::roundToInt (yFor (db)), r.getX(), r.getRight());
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.setColour (th.dim);
            for (auto [hz, name] : { std::pair<float, const char*> { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
                g.drawText (name, juce::Rectangle<float> (xFor (hz) + 3.0f, r.getBottom() - 14.0f, 40.0f, 12.0f), juce::Justification::centredLeft);

            // 0 dB = Grenze zur Selbstoszillation
            const float zero = yFor (0.0f);
            const auto hot = th.faders() ? juce::Colour (0xffff3b24) : juce::Colour (0xffff4d6d);
            g.setColour (hot.withAlpha (0.08f));
            g.fillRect (juce::Rectangle<float> (r.getX(), r.getY(), r.getWidth(), zero - r.getY()));
            g.setColour (hot.withAlpha (0.7f));
            const float dashes[] { 5.0f, 4.0f };
            g.drawDashedLine ({ r.getX(), zero, r.getRight(), zero }, dashes, 2, 1.0f);
            g.drawText (juce::String::fromUTF8 ("SELBSTOSZILLATION"), juce::Rectangle<float> (r.getX() + 6.0f, zero - 16.0f, 200.0f, 14.0f), juce::Justification::centredLeft);

            juce::Path curve, fill;
            const int n = 200;
            for (int i = 0; i <= n; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / (float) n;
                const float hz = 20.0f * std::pow (10.0f, 3.0f * (float) i / (float) n);
                const float wh = hz / lowCut, wl = hz / highCut;
                const float hp = wh * wh / std::sqrt ((1.0f - wh * wh) * (1.0f - wh * wh) + (wh / 0.707f) * (wh / 0.707f));
                const float lp = 1.0f / std::sqrt ((1.0f - wl * wl) * (1.0f - wl * wl) + (wl / q) * (wl / q));
                const float ag = 1.0f / std::sqrt (1.0f + (hz / ageHz) * (hz / ageHz));
                const float y = yFor (juce::Decibels::gainToDecibels (fb * hp * lp * ag, -100.0f));
                if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, r.getBottom()); }
                else curve.lineTo (x, y);
                fill.lineTo (x, y);
            }
            fill.lineTo (r.getRight(), r.getBottom());
            fill.closeSubPath();
            g.setColour (th.accent.withAlpha (0.12f));
            g.fillPath (fill);
            g.setColour (th.accent);
            g.strokePath (curve, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

    private:
        float value (const char* id) const { return processor.apvts.getRawParameterValue (id)->load(); }
        TrendyDelayProcessor& processor;
        std::array<float, 6> last { -1, -1, -1, -1, -1, -1 };
    };

    //==========================================================================
    // Laufwerk: zwei Spulen, Band über Lösch-, Aufnahme- und Wiedergabeköpfe
    class TapeDeck : public juce::Component
    {
    public:
        explicit TapeDeck (TrendyDelayProcessor& p) : processor (p) {}

        void refresh()
        {
            const float pos = processor.echo.tapePosition.load();
            const int mode = (int) processor.apvts.getRawParameterValue ("mode")->load();
            if (std::abs (pos - shownPos) > 1.0e-4f || mode != shownMode)
            {
                shownPos = pos;
                shownMode = mode;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            auto r = getLocalBounds().toFloat();
            const float reelR = juce::jmin (r.getHeight() * 0.44f, r.getWidth() * 0.14f);
            const juce::Point<float> c1 { r.getX() + reelR + 30.0f, r.getCentreY() - 6.0f };
            const juce::Point<float> c2 { r.getRight() - reelR - 30.0f, r.getCentreY() - 6.0f };
            const float angle = shownPos * juce::MathConstants<float>::twoPi * 0.8f;

            const auto tapeColour = th.faders() ? juce::Colour (0xff4a2c1c) : juce::Colour (0xff2b2f3a);
            const float headY = r.getBottom() - 26.0f;
            const float packR = reelR * 0.78f;

            // Bandweg
            juce::Path tape;
            tape.startNewSubPath (c1.x, c1.y + packR);
            tape.lineTo (c1.x + reelR * 0.6f, headY);
            tape.lineTo (c2.x - reelR * 0.6f, headY);
            tape.lineTo (c2.x, c2.y + packR * 0.62f);
            g.setColour (tapeColour);
            g.strokePath (tape, juce::PathStrokeType (3.0f));

            // Köpfe
            const char* headNames[] { "LOESCHEN", "AUFNAHME", "WIEDERG. 1", "WIEDERG. 2", "WIEDERG. 3" };
            const float span = (c2.x - reelR * 0.6f) - (c1.x + reelR * 0.6f);
            const auto lit = th.faders() ? juce::Colour (0xffff3b24) : th.accent;
            for (int i = 0; i < 5; ++i)
            {
                const float x = c1.x + reelR * 0.6f + span * (0.18f + 0.16f * (float) i);
                const bool active = i == 1 || i == 4 || (shownMode == Params::ThreeHeads && i >= 2);
                auto head = juce::Rectangle<float> (26.0f, 22.0f).withCentre ({ x, headY - 9.0f });
                g.setColour (th.faders() ? juce::Colour (0xffb8b9bd) : th.knob);
                g.fillRoundedRectangle (head, 4.0f);
                g.setColour (active ? lit : juce::Colours::black.withAlpha (0.25f));
                g.fillRect (head.withSizeKeepingCentre (4.0f, head.getHeight()).withY (head.getBottom() - 6.0f).withHeight (6.0f));
                if (active)
                {
                    g.setColour (lit.withAlpha (0.25f));
                    g.fillEllipse (juce::Rectangle<float> (16.0f, 10.0f).withCentre ({ x, headY }));
                }
                g.setColour (th.dim);
                g.setFont (juce::FontOptions (9.5f, juce::Font::bold).withKerningFactor (0.08f));
                g.drawText (headNames[i], juce::Rectangle<float> (x - 40.0f, headY + 4.0f, 80.0f, 14.0f), juce::Justification::centred);
            }

            // Spulen
            for (auto [c, pack] : { std::pair<juce::Point<float>, float> { c1, packR }, { c2, packR * 0.62f } })
            {
                g.setColour (tapeColour);
                g.fillEllipse (juce::Rectangle<float> (pack * 2.0f, pack * 2.0f).withCentre (c));
                juce::Path flange;
                flange.addEllipse (juce::Rectangle<float> (reelR * 2.0f, reelR * 2.0f).withCentre (c));
                for (int i = 0; i < 3; ++i)
                {
                    const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 3.0f;
                    juce::Path window;
                    window.addPieSegment (juce::Rectangle<float> (reelR * 1.7f, reelR * 1.7f).withCentre (c), a + 0.25f, a + 1.55f, 0.32f);
                    flange.addPath (window);
                }
                flange.setUsingNonZeroWinding (false);
                const auto metal = th.faders() ? juce::Colour (0xffc9cacd) : juce::Colour (0xccffffff);
                juce::ColourGradient grad (metal.brighter (0.2f), c.x - reelR, c.y - reelR, metal.darker (0.25f), c.x + reelR, c.y + reelR, false);
                g.setGradientFill (grad);
                g.fillPath (flange);
                g.setColour (th.faders() ? juce::Colours::black.withAlpha (0.5f) : th.line.darker (0.1f));
                g.strokePath (flange, juce::PathStrokeType (1.0f));
                g.setColour (th.faders() ? juce::Colour (0xff1c1d20) : th.accent);
                g.fillEllipse (juce::Rectangle<float> (reelR * 0.22f, reelR * 0.22f).withCentre (c));
            }
        }

    private:
        TrendyDelayProcessor& processor;
        float shownPos = -1.0f;
        int shownMode = -1;
    };

    //==========================================================================
    // Zwei Hallfedern, die mit dem Ausgangspegel zittern
    class SpringView : public juce::Component
    {
    public:
        explicit SpringView (TrendyDelayProcessor& p) : processor (p) {}

        void refresh()
        {
            const float mix = processor.apvts.getRawParameterValue ("spring_mix")->load();
            const float level = processor.echo.outLevel.load() * mix;
            if (level > 0.002f || std::abs (level - shownLevel) > 0.001f)
            {
                shownLevel = level;
                phase += 0.9f;
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            auto r = getLocalBounds().toFloat().reduced (20.0f, 10.0f);
            const float rowH = r.getHeight() / 2.0f;
            for (int s = 0; s < 2; ++s)
            {
                auto row = juce::Rectangle<float> (r.getX(), r.getY() + rowH * (float) s, r.getWidth(), rowH).reduced (0.0f, rowH * 0.18f);
                // Halterungen
                g.setColour (th.faders() ? juce::Colour (0xff8c8e93) : th.knob);
                g.fillRoundedRectangle (row.withWidth (18.0f), 3.0f);
                g.fillRoundedRectangle (row.withX (row.getRight() - 18.0f).withWidth (18.0f), 3.0f);
                auto coil = row.reduced (24.0f, 0.0f);
                juce::Path p;
                const int turns = 46;
                const float amp = coil.getHeight() * 0.36f;
                const float wobble = juce::jmin (1.0f, shownLevel * 3.0f) * amp * 0.35f;
                for (int i = 0; i <= turns * 12; ++i)
                {
                    const float t = (float) i / (float) (turns * 12);
                    const float x = coil.getX() + coil.getWidth() * t;
                    const float bend = wobble * std::sin (juce::MathConstants<float>::pi * t * (3.0f + (float) s) + phase * (1.0f + 0.3f * (float) s));
                    const float y = coil.getCentreY() + bend + amp * std::sin (juce::MathConstants<float>::twoPi * t * (float) turns);
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (th.faders() ? juce::Colour (0xffd2d3d6) : th.accent.withAlpha (0.85f));
                g.strokePath (p, juce::PathStrokeType (1.6f));
            }
        }

    private:
        TrendyDelayProcessor& processor;
        float shownLevel = 0.0f, phase = 0.0f;
    };

    //==========================================================================
    class Content : public juce::Component, private juce::Timer
    {
    public:
        Content (TrendyDelayProcessor& p, TrendyLook& lf)
            : processor (p), look (lf), curve (p), deck (p), springs (p),
              throwPad ("THROW", "gedrueckt halten: ab ins Echo"),
              freezePad ("ENDLOS", "Band laeuft allein weiter")
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

            const char* tabTitles[] { "Echo", "Band", "Hall", "Dub" };
            const char* tabSubs[]   { "Zeit, Schleife, Filter", "Saettigung, Wow, Tempo", "Federhall", "Mischpult, Ducking" };
            for (int i = 0; i < numPages; ++i)
            {
                auto* t = tabs.add (new TabButton (tabTitles[i], tabSubs[i]));
                t->onClick = [this, i] { showPage (i); };
                addAndMakeVisible (t);
                auto* pg = pages.add (new Page());
                addChildComponent (pg);
            }

            buildEchoPage();
            buildTapePage();
            buildSpringPage();
            buildDubPage();

            // Dub-Leiste unten: Anzeige, Pegel, Bandtempo, THROW, ENDLOS
            addAndMakeVisible (display);
            addAndMakeVisible (meter);

            throwPad.colour1980 = juce::Colour (0xffd8382c);
            throwPad.onStateChange = [this]
            {
                const bool down = throwPad.isDown();
                if (down == throwHeld)
                    return;
                throwHeld = down;
                if (auto* prm = processor.apvts.getParameter ("throw"))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (down ? 1.0f : 0.0f);
                    prm->endChangeGesture();
                }
            };
            addAndMakeVisible (throwPad);

            freezePad.colour1980 = juce::Colour (0xfff2c230);
            freezePad.setClickingTogglesState (true);
            freezeAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "freeze", freezePad);
            addAndMakeVisible (freezePad);

            const juce::String speedTitles[] { juce::String::fromUTF8 ("\xc2\xbd"), "x1", "x2" };
            for (int i = 0; i < 3; ++i)
            {
                auto* b = speedPads.add (new DubPad (speedTitles[i], {}, false));
                b->colour1980 = juce::Colour (0xffeeeae0);
                b->onClick = [this, i]
                {
                    if (auto* prm = processor.apvts.getParameter ("speed"))
                    {
                        prm->beginChangeGesture();
                        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
                        prm->endChangeGesture();
                    }
                };
                addAndMakeVisible (b);
            }

            setSize (TrendyDelayEditor::designWidth, TrendyDelayEditor::designHeight);
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
                g.setFont (juce::FontOptions (32.0f, juce::Font::bold | juce::Font::italic).withKerningFactor (0.12f));
                g.drawText ("TRENDY DELAY", 20, 12, 300, 38, juce::Justification::centredLeft);
                g.setColour (th.accent);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.15f));
                g.drawText ("DUB TAPE ECHO " + versionLabel(), 316, 14, 260, 18, juce::Justification::centredLeft);
                g.setColour (th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
                g.drawText (makerLabel(), 316, 32, 300, 16, juce::Justification::centredLeft);
            }
            else
            {
                g.setFont (juce::FontOptions (30.0f, juce::Font::bold).withKerningFactor (0.1f));
                Design::drawText (g, th, "TRENDY DELAY", { 20.0f, 12.0f, 300.0f, 38.0f }, juce::Justification::centredLeft, th.ink);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.15f));
                Design::drawText (g, th, "DUB TAPE ECHO " + versionLabel(), { 316.0f, 14.0f, 260.0f, 18.0f }, juce::Justification::centredLeft, th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
                Design::drawText (g, th, makerLabel(), { 316.0f, 32.0f, 300.0f, 16.0f }, juce::Justification::centredLeft, th.dim);
            }
            g.setColour (th.dim);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.15f));
            g.drawText ("DESIGN", 844, 42, 160, 18, juce::Justification::centredLeft);
            g.drawText ("BANDTEMPO", speedLabel.getX(), speedLabel.getY(), speedLabel.getWidth(), 16, juce::Justification::centredLeft);
        }

        void resized() override
        {
            renderBackground();

            designButtons[0]->setBounds (840, 64, 156, 62);
            designButtons[1]->setBounds (1000, 64, 164, 62);

            auto tabRow = juce::Rectangle<int> (16, 64, 4 * 190, 62);
            for (auto* t : tabs)
                t->setBounds (tabRow.removeFromLeft (190).withTrimmedRight (8));

            const auto pageArea = juce::Rectangle<int> (16, 138, getWidth() - 32, getHeight() - 138 - 132);
            for (auto* pg : pages)
                pg->setBounds (pageArea);

            // Dub-Leiste
            auto strip = juce::Rectangle<int> (24, getHeight() - 116, getWidth() - 48, 100);
            display.setBounds (strip.removeFromLeft (300).reduced (0, 6));
            strip.removeFromLeft (16);
            meter.setBounds (strip.removeFromLeft (80).reduced (0, 4));
            strip.removeFromLeft (24);
            auto speed = strip.removeFromLeft (264);
            speedLabel = speed.removeFromTop (20);
            for (auto* b : speedPads)
                b->setBounds (speed.removeFromLeft (88).withTrimmedRight (8));
            strip.removeFromLeft (16);
            freezePad.setBounds (strip.removeFromRight (220));
            strip.removeFromRight (12);
            throwPad.setBounds (strip.removeFromRight (240));

            layoutEchoPage();
            layoutTapePage();
            layoutSpringPage();
            layoutDubPage();
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
        Labelled* knob (Page& pg, const juce::String& id, const juce::String& name, int size = 96)
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

        // Zwei Reihen: links oben/unten Regler, rechts ein großes Feld
        void twoRows (Page& pg, int split, bool rightFullHeight)
        {
            const int w = pg.getWidth(), h = (pg.getHeight() - 12) / 2;
            pg.panels[0].bounds = { 0, 0, split, h };
            if (rightFullHeight)
            {
                pg.panels[1].bounds = { 0, h + 12, split, h };
                pg.panels[2].bounds = { split + 12, 0, w - split - 12, pg.getHeight() };
            }
            else
            {
                pg.panels[1].bounds = { split + 12, 0, w - split - 12, h };
                pg.panels[2].bounds = { 0, h + 12, w, h };
            }
            for (auto& p : pg.panels)
                Page::layout (p, 150);
        }

        void buildEchoPage()
        {
            auto& pg = *pages[0];
            pg.panels = {
                { "ZEIT", {}, { choice (pg, "mode", "Modus", 150), toggle (pg, "sync", "Sync"),
                                divisionControl = choice (pg, "division", "Teilung", 100), timeControl = knob (pg, "time", "Zeit (frei)") } },
                { "SCHLEIFE", {}, { knob (pg, "feedback", "Feedback"), knob (pg, "low_cut", "Low Cut"),
                                    knob (pg, "high_cut", "High Cut"), knob (pg, "resonance", "Resonanz") } },
                { "SCHLEIFENFILTER", {}, {} },
            };
            pg.addAndMakeVisible (curve);
        }

        void layoutEchoPage()
        {
            auto& pg = *pages[0];
            twoRows (pg, 620, true);
            curve.setBounds (pg.panels[2].bounds.reduced (16).withTrimmedTop (28));
        }

        void buildTapePage()
        {
            auto& pg = *pages[1];
            pg.panels = {
                { "BANDMASCHINE", {}, { knob (pg, "drive", "Saettigung"), knob (pg, "wow", "Wow"),
                                        knob (pg, "flutter", "Flutter"), knob (pg, "age", "Alter") } },
                { "BANDSPRUNG", {}, { choice (pg, "speed", "Bandtempo", 120), knob (pg, "glide", "Gleiten") } },
                { "LAUFWERK", {}, {} },
            };
            pg.addAndMakeVisible (deck);
        }

        void layoutTapePage()
        {
            auto& pg = *pages[1];
            twoRows (pg, 620, false);
            deck.setBounds (pg.panels[2].bounds.reduced (16).withTrimmedTop (24));
        }

        void buildSpringPage()
        {
            auto& pg = *pages[2];
            pg.panels = {
                { "FEDERHALL", {}, { knob (pg, "spring_mix", "Hall"), knob (pg, "spring_decay", "Laenge"),
                                     knob (pg, "spring_tone", "Ton"), knob (pg, "spring_boing", "Boing") } },
                { "PLATZ", {}, { choice (pg, "spring_place", "Hall sitzt", 160) } },
                { "FEDERN", {}, {} },
            };
            pg.addAndMakeVisible (springs);
            pg.paintExtra = [this] (juce::Graphics& g) { paintPlaceHelp (g); };
        }

        void layoutSpringPage()
        {
            auto& pg = *pages[2];
            twoRows (pg, 620, false);
            springs.setBounds (pg.panels[2].bounds.reduced (16).withTrimmedTop (24));
        }

        void paintPlaceHelp (juce::Graphics& g)
        {
            const auto& th = look.getTheme();
            auto r = pages[2]->panels[1].bounds.toFloat().withTrimmedLeft (200.0f).reduced (8.0f, 44.0f);
            g.setFont (juce::FontOptions (12.0f));
            g.setColour (th.dim);
            g.drawFittedText (juce::String::fromUTF8 ("Auf Echos: nur die Wiederholungen hallen.\n"
                                                      "In Schleife: jedes Echo hallt weiter, wird dichter und dunkler.\n"
                                                      "Auf alles: auch das trockene Signal bekommt Feder."),
                              r.toNearestInt(), juce::Justification::topLeft, 6);
        }

        void buildDubPage()
        {
            auto& pg = *pages[3];
            pg.panels = {
                { "MISCHPULT", {}, { knob (pg, "send", "Send"), knob (pg, "mix", "Mix"), knob (pg, "output", "Ausgang") } },
                { "DUCKING & STEREO", {}, { knob (pg, "ducking", "Ducking"), knob (pg, "width", "Breite") } },
                { "DUB-TRICKS", {}, {} },
            };
            pg.paintExtra = [this] (juce::Graphics& g) { paintTricks (g); };
        }

        void layoutDubPage()
        {
            twoRows (*pages[3], 620, false);
        }

        void paintTricks (juce::Graphics& g)
        {
            const auto& th = look.getTheme();
            auto r = pages[3]->panels[2].bounds.toFloat().reduced (20.0f, 0.0f).withTrimmedTop (44.0f).withTrimmedBottom (14.0f);
            const char* tricks[][2] {
                { "THROW", "Send auf 0 drehen und THROW nur bei einzelnen Snares oder Worten halten." },
                { "ENDLOS", "friert das Band ein: nichts Neues kommt dazu, die Schleife kreist weiter." },
                { "FEEDBACK > 100 %", "das Band schwingt selbst; mit High Cut und Resonanz die Tonhoehe formen." },
                { "BANDTEMPO", "Halb oder Doppelt waehrend des Echos: Sirenen-Rutscher, Gleiten bestimmt das Tempo." },
                { "DUCKING", "Echos weichen dem Gesang aus und kommen in den Pausen hoch." },
            };
            const float rowH = r.getHeight() / 5.0f;
            for (int i = 0; i < 5; ++i)
            {
                auto row = r.removeFromTop (rowH);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.08f));
                Design::drawText (g, th, tricks[i][0], row.removeFromLeft (170.0f), juce::Justification::centredLeft, th.accent);
                g.setFont (juce::FontOptions (13.0f));
                g.setColour (th.ink);
                g.drawText (tricks[i][1], row, juce::Justification::centredLeft);
            }
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

            // Fläche der Dub-Leiste
            g.setColour (th.faders() ? juce::Colour (0xff121315) : juce::Colour (0xffdde1e6));
            g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, (float) getHeight() - 124.0f, (float) getWidth() - 24.0f, 116.0f),
                                    th.faders() ? 3.0f : 18.0f);
        }

        void timerCallback() override
        {
            const float ms = processor.echo.shownMs.load();
            const bool sync = processor.apvts.getRawParameterValue ("sync")->load() > 0.5f;
            const int div = (int) processor.apvts.getRawParameterValue ("division")->load();
            const int speed = juce::jlimit (0, 2, (int) processor.apvts.getRawParameterValue ("speed")->load());
            const bool frozen = processor.apvts.getRawParameterValue ("freeze")->load() > 0.5f;
            juce::String small = sync ? Params::divisions[div] + "  " + juce::String (juce::roundToInt (processor.shownBpm.load())) + " BPM"
                                      : juce::String ("FREI");
            if (speed != 1)
                small << "  BAND " << Params::speeds[speed].toUpperCase();
            if (frozen)
                small << "  ENDLOS";
            display.setText (juce::String (juce::roundToInt (ms)).paddedLeft ('0', 4) + " ms", small);
            meter.setLevels (processor.echo.inLevel.load(), processor.echo.outLevel.load());
            for (int i = 0; i < speedPads.size(); ++i)
                speedPads[i]->setToggleState (i == speed, juce::dontSendNotification);
            // was gerade nicht zählt, wird blass
            timeControl->setAlpha (sync ? 0.4f : 1.0f);
            divisionControl->setAlpha (sync ? 1.0f : 0.4f);

            if (currentPage == 0) curve.refresh();
            if (currentPage == 1) deck.refresh();
            if (currentPage == 2) springs.refresh();
        }

        TrendyDelayProcessor& processor;
        TrendyLook& look;
        juce::Image grain, background;
        juce::OwnedArray<TabButton> tabs, designButtons;
        juce::OwnedArray<Page> pages;
        juce::OwnedArray<Labelled> controls;
        int currentPage = 0;
        Labelled* timeControl = nullptr;
        Labelled* divisionControl = nullptr;

        LoopCurve curve;
        TapeDeck deck;
        SpringView springs;
        LedDisplay display;
        Meter meter;
        DubPad throwPad, freezePad;
        juce::OwnedArray<DubPad> speedPads;
        juce::Rectangle<int> speedLabel;
        std::unique_ptr<ButtonAttachment> freezeAttachment;
        bool throwHeld = false;
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
TrendyDelayEditor::TrendyDelayEditor (TrendyDelayProcessor& p)
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

TrendyDelayEditor::~TrendyDelayEditor()
{
    content.reset();
    setLookAndFeel (nullptr);
}

void TrendyDelayEditor::resized()
{
    if (content != nullptr)
        content->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth));
}
