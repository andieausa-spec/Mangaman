#include "PluginEditor.h"

namespace
{
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    MangamanLook& lookOf (juce::Component& c) { return static_cast<MangamanLook&> (c.getLookAndFeel()); }

    // Versionsanzeige aus CMakeLists (0.6.0 -> "V0.6")
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
    // Tonhöhen-Pad eines Sequenzer-Schritts: Tippen schaltet an/aus, Ziehen ändert den Ton
    class PitchPad : public juce::Component
    {
    public:
        PitchPad (juce::AudioProcessorValueTreeState& s, int index)
        {
            pitchAttachment = std::make_unique<juce::ParameterAttachment> (*s.getParameter (Params::seqPitchId (index)),
                [this] (float v) { pitch = juce::roundToInt (v); repaint(); });
            gateAttachment = std::make_unique<juce::ParameterAttachment> (*s.getParameter (Params::seqGateId (index)),
                [this] (float v) { gate = v > 0.5f; repaint(); });
            rootAttachment = std::make_unique<juce::ParameterAttachment> (*s.getParameter ("sq_root"),
                [this] (float v) { root = juce::roundToInt (v); repaint(); });
            pitchAttachment->sendInitialUpdate();
            gateAttachment->sendInitialUpdate();
            rootAttachment->sendInitialUpdate();
            setRepaintsOnMouseActivity (true);
        }

        void setCurrent (bool c) { if (c != current) { current = c; repaint(); } }

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            auto r = getLocalBounds().toFloat().reduced (3.0f);
            if (pressed)
                r = r.withSizeKeepingCentre (r.getWidth() * 0.93f, r.getHeight() * 0.9f).translated (0.0f, 1.0f);

            if (current)
            {
                g.setColour (th.accent.withAlpha (0.45f));
                g.fillRoundedRectangle (r.expanded (3.0f), 9.0f);
            }
            g.setColour (juce::Colours::black.withAlpha (pressed ? 0.15f : 0.3f));
            g.fillRoundedRectangle (r.translated (0.0f, pressed ? 1.0f : 2.5f), 7.0f);

            const auto base = gate ? th.accent.interpolatedWith (th.panel, 0.15f) : th.field.interpolatedWith (th.ink, 0.1f);
            juce::ColourGradient grad (base.brighter (0.12f), r.getX(), r.getY(), base.darker (0.12f), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (th.ink.withAlpha (isMouseOver() ? 0.45f : 0.2f));
            g.drawRoundedRectangle (r, 7.0f, 1.0f);

            g.setColour (gate ? juce::Colours::white : th.dim);
            g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            g.drawText (Params::noteName (juce::jlimit (0, 127, root + pitch)), r, juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            startY = e.position.y;
            startPitch = pitch;
            moved = false;
            pressed = true;
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const int d = juce::roundToInt ((startY - e.position.y) / (e.mods.isShiftDown() ? 24.0f : 9.0f));
            if (d != 0)
                moved = true;
            if (moved && juce::jlimit (-24, 24, startPitch + d) != pitch)
            {
                pitchAttachment->setValueAsCompleteGesture ((float) juce::jlimit (-24, 24, startPitch + d));
                if (! gate)
                    gateAttachment->setValueAsCompleteGesture (1.0f);
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (! moved)
                gateAttachment->setValueAsCompleteGesture (gate ? 0.0f : 1.0f);
            pressed = false;
            repaint();
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            const int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
            if (d != 0)
                pitchAttachment->setValueAsCompleteGesture ((float) juce::jlimit (-24, 24, pitch + d));
        }

    private:
        std::unique_ptr<juce::ParameterAttachment> pitchAttachment, gateAttachment, rootAttachment;
        int pitch = 0, root = 48, startPitch = 0;
        bool gate = false, current = false, pressed = false, moved = false;
        float startY = 0.0f;
    };

    //==========================================================================
    // Drumcomputer-Wahl: Taster mit Modellname und typischen Stilen
    class KitButton : public juce::Button
    {
    public:
        KitButton (int index) : juce::Button (Rhythm::kitInfo()[(size_t) index].name), kit (index)
        {
            setClickingTogglesState (false);
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto& th = lookOf (*this).getTheme();
            const bool on = getToggleState();
            auto r = getLocalBounds().toFloat().reduced (2.0f);
            const float rad = th.faders() ? 2.0f : 12.0f;
            g.setColour (juce::Colours::black.withAlpha (th.faders() ? 0.45f : 0.12f));
            g.fillRoundedRectangle (r.translated (0.0f, down ? 1.0f : 2.0f), rad);
            if (down) r.translate (0.0f, 1.0f);
            const auto base = on ? (th.faders() ? th.ink : th.accent)
                                 : th.field.interpolatedWith (th.ink, highlighted ? 0.08f : 0.03f);
            g.setColour (base);
            g.fillRoundedRectangle (r, rad);
            g.setColour (on ? base.darker (0.2f) : th.line);
            g.drawRoundedRectangle (r, rad, 1.0f);

            auto text = r.reduced (10.0f, 4.0f);
            const auto ink = on ? (th.faders() ? th.body : juce::Colours::white) : th.ink;
            g.setColour (ink);
            g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            g.drawText (juce::String (Rhythm::kitInfo()[(size_t) kit].name) + "-Stil", text.removeFromTop (text.getHeight() * 0.55f),
                        juce::Justification::bottomLeft);
            g.setColour (on ? ink.withAlpha (0.8f) : th.dim);
            g.setFont (juce::FontOptions (10.5f));
            g.drawText (Rhythm::kitInfo()[(size_t) kit].sub, text, juce::Justification::topLeft);
            if (th.faders() && on)
            {
                g.setColour (juce::Colour (0xffff3b24));
                g.fillEllipse (r.getRight() - 16.0f, r.getY() + 8.0f, 7.0f, 7.0f);
            }
        }

        const int kit;
    };

    //==========================================================================
    // Raster der Drums: 12 Spuren x 16 Schritte. Tippen schaltet an/aus, nach oben ziehen = Akzent, weiter = Wirbel.
    // Links die Spurnamen (anklicken = wählen und vorhören) mit Stumm-Taste.
    class StepGrid : public juce::Component
    {
    public:
        StepGrid (MangamanProcessor& p) : processor (p) {}

        std::function<void (int)> onSelect;
        int selected = 0, current = -1;
        juce::uint32 flash = 0;
        float flashAmount = 0.0f;
        static constexpr int labelW = 150, headerH = 20;

        float colW() const { return (float) (getWidth() - labelW) / (float) Drums::numSteps; }
        float rowH() const { return (float) (getHeight() - headerH) / (float) Drums::numTracks; }

        void paint (juce::Graphics& g) override
        {
            const auto& th = lookOf (*this).getTheme();
            const auto pat = processor.getPattern();
            const int len = juce::roundToInt (processor.apvts.getRawParameterValue ("rh_len")->load());
            const int kit = (int) processor.apvts.getRawParameterValue ("rh_kit")->load();
            const float cw = colW(), rh = rowH();
            static const juce::Colour groups1980[] { juce::Colour (0xffd8382c), juce::Colour (0xfff08a24),
                                                     juce::Colour (0xfff2c230), juce::Colour (0xffe9e6dc) };

            // Kopfzeile: Schrittnummern und Lauflicht
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            for (int i = 0; i < Drums::numSteps; ++i)
            {
                const auto cell = juce::Rectangle<float> ((float) labelW + i * cw, 0.0f, cw, (float) headerH);
                const bool lit = i == current;
                if (th.faders())
                {
                    auto led = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ cell.getX() + 9.0f, cell.getCentreY() });
                    g.setColour (lit ? juce::Colour (0xffff3b24) : juce::Colour (0xff4a1c18));
                    g.fillEllipse (led);
                    g.setColour (i < len ? th.ink.withAlpha (0.8f) : th.dim.withAlpha (0.35f));
                    g.drawText (juce::String (i + 1), cell.withTrimmedLeft (14.0f), juce::Justification::centredLeft);
                }
                else
                {
                    g.setColour (lit ? th.accent : (i < len ? th.dim : th.dim.withAlpha (0.35f)));
                    Design::drawText (g, th, juce::String (i + 1), cell, juce::Justification::centred, lit ? th.accent : (i < len ? th.dim : th.dim.withAlpha (0.35f)));
                }
            }

            for (int t = 0; t < Drums::numTracks; ++t)
            {
                const float y = (float) headerH + t * rh;
                const auto label = juce::Rectangle<float> (0.0f, y, (float) labelW - 6.0f, rh);
                const bool muted = processor.apvts.getRawParameterValue (Params::rhId ("mu", t))->load() > 0.5f;
                if (t == selected)
                {
                    g.setColour (th.accent.withAlpha (th.faders() ? 0.3f : 0.1f));
                    g.fillRoundedRectangle (label.reduced (0.0f, 1.0f), th.faders() ? 2.0f : 6.0f);
                }
                g.setColour (muted ? th.dim.withAlpha (0.5f) : th.ink);
                g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
                g.drawText (Rhythm::trackShort[t], label.withTrimmedLeft (6.0f).withWidth (28.0f), juce::Justification::centredLeft);
                g.setColour (muted ? th.dim.withAlpha (0.5f) : th.dim);
                g.setFont (juce::FontOptions (11.0f));
                g.drawText (Rhythm::trackLabel (kit, t), label.withTrimmedLeft (36.0f).withTrimmedRight (24.0f), juce::Justification::centredLeft);
                // Stumm-Taste
                auto m = muteBounds (t);
                g.setColour (muted ? th.accent : th.field.interpolatedWith (th.ink, 0.06f));
                g.fillRoundedRectangle (m, 3.0f);
                g.setColour (muted ? juce::Colours::white : th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                g.drawText ("M", m, juce::Justification::centred);

                for (int i = 0; i < Drums::numSteps; ++i)
                {
                    const int v = pat[(size_t) (t * Drums::numSteps + i)];
                    auto cell = juce::Rectangle<float> ((float) labelW + i * cw, y, cw, rh).reduced (2.0f, 1.5f);
                    const bool out = i >= len;
                    const bool hitNow = i == current && ((flash >> t) & 1u) != 0;
                    if (th.faders())
                    {
                        // Tasten in den Gruppenfarben wie bei den Roland-Maschinen, rote LED bei aktivem Schritt
                        auto key = groups1980[i / 4];
                        if (out) key = key.withMultipliedBrightness (0.45f);
                        if (i == current) key = key.brighter (0.25f);
                        g.setColour (juce::Colours::black.withAlpha (0.5f));
                        g.fillRoundedRectangle (cell.translated (0.0f, 1.5f), 2.0f);
                        juce::ColourGradient kg (key.brighter (0.1f), cell.getX(), cell.getY(), key.darker (0.25f), cell.getX(), cell.getBottom(), false);
                        g.setGradientFill (kg);
                        g.fillRoundedRectangle (cell, 2.0f);
                        // LED in dunkler Fassung, damit sie auch auf den roten Tasten sichtbar bleibt
                        const auto red = juce::Colour (0xffff2a18);
                        const auto c = cell.getCentre();
                        auto drawLed = [&] (juce::Point<float> at, float size, bool lit, bool strong)
                        {
                            auto led = juce::Rectangle<float> (size, size).withCentre (at);
                            g.setColour (juce::Colour (0xff141010));
                            g.fillEllipse (led.expanded (1.6f));
                            if (! lit)
                            {
                                g.setColour (juce::Colour (0xff4a1612));
                                g.fillEllipse (led);
                                return;
                            }
                            g.setColour (red.withAlpha (hitNow ? 0.75f : 0.45f));
                            g.fillEllipse (led.expanded (strong ? 4.5f : 3.0f));
                            g.setColour (strong ? juce::Colour (0xffff8a70) : red);
                            g.fillEllipse (led);
                            g.setColour (juce::Colours::white.withAlpha (0.55f));
                            g.fillEllipse (led.withSizeKeepingCentre (size * 0.35f, size * 0.35f).translated (-size * 0.15f, -size * 0.15f));
                        };
                        if (v == 3)
                        {
                            drawLed (c.translated (-5.0f, 0.0f), 6.0f, true, false);
                            drawLed (c.translated (5.0f, 0.0f), 6.0f, true, false);
                        }
                        else
                            drawLed (c, v == 2 ? 9.0f : 7.5f, v > 0, v == 2);
                    }
                    else
                    {
                        // 2100: leuchtende blaue Punkte im hellen Material
                        const auto c = cell.getCentre();
                        const float r0 = juce::jmin (cell.getWidth(), cell.getHeight()) * 0.36f;
                        if (i == current)
                        {
                            g.setColour (th.accent.withAlpha (0.07f));
                            g.fillRoundedRectangle (cell, 5.0f);
                        }
                        if (v == 0)
                        {
                            g.setColour (th.accent.withAlpha (out ? 0.1f : 0.22f));
                            g.drawEllipse (juce::Rectangle<float> (r0 * 1.2f, r0 * 1.2f).withCentre (c), 1.0f);
                            continue;
                        }
                        const float alpha = out ? 0.35f : 1.0f;
                        const float r1 = v == 2 ? r0 * 1.15f : r0 * 0.85f;
                        g.setColour (th.accent.withAlpha ((hitNow ? 0.45f : 0.2f) * alpha));
                        g.fillEllipse (juce::Rectangle<float> (r1 * 3.0f, r1 * 3.0f).withCentre (c));
                        g.setColour (th.accent.withAlpha (alpha));
                        if (v == 3)
                        {
                            g.fillEllipse (juce::Rectangle<float> (r0, r0).withCentre (c.translated (-r0 * 0.7f, 0.0f)));
                            g.fillEllipse (juce::Rectangle<float> (r0, r0).withCentre (c.translated (r0 * 0.7f, 0.0f)));
                        }
                        else
                            g.fillEllipse (juce::Rectangle<float> (r1 * 2.0f, r1 * 2.0f).withCentre (c));
                    }
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragTrack = dragStep = -1;
            const int t = trackAt (e.position.y);
            if (t < 0)
                return;
            if (e.position.x < (float) labelW)
            {
                if (muteBounds (t).expanded (2.0f).contains (e.position))
                {
                    if (auto* p = processor.apvts.getParameter (Params::rhId ("mu", t)))
                        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
                }
                else
                {
                    select (t);
                    audition (t, 1);
                }
                repaint();
                return;
            }
            const int i = stepAt (e.position.x);
            if (i < 0)
                return;
            select (t);
            dragTrack = t; dragStep = i; moved = false;
            startY = e.position.y;
            startValue = processor.getPattern()[(size_t) (t * Drums::numSteps + i)];
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragTrack < 0)
                return;
            const int d = juce::roundToInt ((startY - e.position.y) / 14.0f);
            if (d != 0)
                moved = true;
            if (moved)
            {
                const int nv = juce::jlimit (1, 3, juce::jmax (1, startValue) + d);
                if (nv != processor.getPattern()[(size_t) (dragTrack * Drums::numSteps + dragStep)])
                {
                    processor.setStep (dragTrack, dragStep, nv);
                    audition (dragTrack, nv);
                    repaint();
                }
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragTrack >= 0 && ! moved)
            {
                const int nv = startValue != 0 ? 0 : 1;
                processor.setStep (dragTrack, dragStep, nv);
                if (nv != 0)
                    audition (dragTrack, nv);
                repaint();
            }
            dragTrack = -1;
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            const int t = trackAt (e.position.y), i = stepAt (e.position.x);
            if (t < 0 || i < 0)
                return;
            const int d = w.deltaY > 0 ? 1 : w.deltaY < 0 ? -1 : 0;
            const int v = processor.getPattern()[(size_t) (t * Drums::numSteps + i)];
            processor.setStep (t, i, juce::jlimit (0, 3, v + d));
            repaint();
        }

    private:
        juce::Rectangle<float> muteBounds (int t) const
        {
            const float y = (float) headerH + t * rowH();
            return juce::Rectangle<float> (18.0f, juce::jmin (16.0f, rowH() - 3.0f)).withCentre ({ (float) labelW - 18.0f, y + rowH() * 0.5f });
        }
        int trackAt (float y) const
        {
            const int t = (int) std::floor ((y - (float) headerH) / rowH());
            return y < (float) headerH || t >= Drums::numTracks ? -1 : t;
        }
        int stepAt (float x) const
        {
            const int i = (int) std::floor ((x - (float) labelW) / colW());
            return x < (float) labelW || i >= Drums::numSteps ? -1 : i;
        }
        void select (int t)
        {
            if (t != selected)
            {
                selected = t;
                if (onSelect) onSelect (t);
                repaint();
            }
        }
        void audition (int t, int v)
        {
            if (processor.apvts.getRawParameterValue ("rh_run")->load() < 0.5f)
                processor.audition (t, v == 2 ? 1.0f : 0.7f);
        }

        MangamanProcessor& processor;
        int dragTrack = -1, dragStep = -1, startValue = 0;
        bool moved = false;
        float startY = 0.0f;
    };

    //==========================================================================
    // Eine Seite: zeichnet ihre Bedienfelder, die Regler liegen als Kinder darauf
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
    class Content : public juce::Component, private juce::Timer
    {
    public:
        Content (MangamanProcessor& p, MangamanLook& lf)
            : processor (p), look (lf),
              keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
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

            const char* tabTitles[] { "Klang", "Formen", "Modulation", "Sequenzer", "Rhythmus" };
            const char* tabSubs[]   { "Klangerzeugung", "Filter, Huellkurven, LFO", "Matrix, Arpeggiator", "16 Schritte", "Drumcomputer, Beats" };
            for (int i = 0; i < numPages; ++i)
            {
                auto* t = tabs.add (new TabButton (tabTitles[i], tabSubs[i]));
                t->onClick = [this, i] { showPage (i); };
                addAndMakeVisible (t);
                auto* pg = pages.add (new Page());
                addChildComponent (pg);
            }

            buildSoundPage();
            buildShapePage();
            buildModPage();
            buildSeqPage();
            buildRhythmPage();

            keyboard.setAvailableRange (24, 108);
            addAndMakeVisible (keyboard);

            setSize (MangamanEditor::designWidth, MangamanEditor::designHeight);
            applyDesign();
            tabs[0]->setToggleState (true, juce::dontSendNotification);
            showPage (0);
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
                g.drawText ("TRENDY ANDY", 20, 12, 300, 38, juce::Justification::centredLeft);
                g.setColour (th.accent);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.15f));
                g.drawText ("HYBRID SYNTHESIZER " + versionLabel(), 300, 14, 260, 18, juce::Justification::centredLeft);
                g.setColour (th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
                g.drawText (makerLabel(), 300, 32, 300, 16, juce::Justification::centredLeft);
            }
            else
            {
                g.setFont (juce::FontOptions (30.0f, juce::Font::bold).withKerningFactor (0.1f));
                Design::drawText (g, th, "TRENDY ANDY", { 20.0f, 12.0f, 290.0f, 38.0f }, juce::Justification::centredLeft, th.ink);
                g.setFont (juce::FontOptions (13.0f, juce::Font::bold).withKerningFactor (0.15f));
                Design::drawText (g, th, "HYBRID SYNTHESIZER " + versionLabel(), { 300.0f, 14.0f, 260.0f, 18.0f }, juce::Justification::centredLeft, th.dim);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.1f));
                Design::drawText (g, th, makerLabel(), { 300.0f, 32.0f, 300.0f, 16.0f }, juce::Justification::centredLeft, th.dim);
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

            auto tabRow = juce::Rectangle<int> (16, 64, 910, 62);
            for (auto* t : tabs)
                t->setBounds (tabRow.removeFromLeft (182).withTrimmedRight (8));

            keyboard.setBounds (20, getHeight() - 112, getWidth() - 40, 96);
            keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f);

            const auto pageArea = juce::Rectangle<int> (16, 138, getWidth() - 32, getHeight() - 138 - 124);
            for (auto* pg : pages)
                pg->setBounds (pageArea);

            layoutSoundPage();
            layoutShapePage();
            layoutModPage();
            layoutSeqPage();
            layoutRhythmPage();
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

        void buildSoundPage()
        {
            auto& pg = *pages[0];
            pg.panels = {
                { "KLANGERZEUGUNG", {}, { choice (pg, "osc_type", "Oszillator", 190), knob (pg, "osc_wave", "Wave", 140),
                                          knob (pg, "osc_timbre", "Timbre", 140), knob (pg, "osc_shape", "Shape", 140),
                                          knob (pg, "glide", "Glide", 100) } },
                { "MASTER", {}, { choice (pg, "voice_mode", "Stimmen", 120), knob (pg, "master_gain", "Volume", 100) } },
            };
        }

        void layoutSoundPage()
        {
            auto& pg = *pages[0];
            const int w = pg.getWidth();
            const int h = pg.getHeight();
            pg.panels[0].bounds = { 0, 0, w - 320, h };
            pg.panels[1].bounds = { w - 308, 0, 308, h };
            for (auto& p : pg.panels)
                Page::layout (p, 200, (h - 40 - 200) / 2);
        }

        void buildShapePage()
        {
            auto& pg = *pages[1];
            pg.panels = {
                { "FILTER", {}, { choice (pg, "flt_type", "Typ", 110), knob (pg, "flt_cutoff", "Cutoff"),
                                  knob (pg, "flt_res", "Resonanz"), knob (pg, "flt_env", "Env-Menge") } },
                { "HUELLKURVE", {}, { knob (pg, "env_attack", "Attack"), knob (pg, "env_decay", "Decay/Rel"),
                                      knob (pg, "env_sustain", "Sustain") } },
                { "ZYKLISCHE HUELLKURVE", {}, { choice (pg, "cyc_mode", "Modus", 100), knob (pg, "cyc_rise", "Rise"),
                                                knob (pg, "cyc_hold", "Hold"), knob (pg, "cyc_fall", "Fall"),
                                                knob (pg, "cyc_amount", "Menge") } },
                { "LFO", {}, { choice (pg, "lfo_shape", "Form", 120), knob (pg, "lfo_rate", "Rate"),
                               toggle (pg, "lfo_sync", "Sync"), choice (pg, "lfo_div", "Teilung", 90) } },
            };
        }

        void layoutShapePage()
        {
            auto& pg = *pages[1];
            const int w = pg.getWidth(), h = (pg.getHeight() - 12) / 2;
            pg.panels[0].bounds = { 0, 0, 470, h };
            pg.panels[1].bounds = { 482, 0, w - 482, h };
            pg.panels[2].bounds = { 0, h + 12, 590, h };
            pg.panels[3].bounds = { 602, h + 12, w - 602, h };
            for (auto& p : pg.panels)
                Page::layout (p, 140);
        }

        void buildModPage()
        {
            auto& pg = *pages[2];
            pg.panels = {
                { "MOD-MATRIX", {}, {} },
                { "ARPEGGIATOR", {}, { toggle (pg, "arp_on", "Arp"), choice (pg, "arp_mode", "Modus", 110),
                                       choice (pg, "arp_rate", "Rate", 90) } },
                { "ARPEGGIATOR  -  UMFANG", {}, { knob (pg, "arp_oct", "Oktaven"), toggle (pg, "arp_hold", "Hold") } },
            };

            for (int s = 0; s < Params::numSources; ++s)
                for (int d = 0; d < Params::numDests; ++d)
                {
                    auto* k = matrixKnobs.add (new RubberSlider (true));
                    k->setDoubleClickReturnValue (true, 0.0);
                    k->setTooltip (Params::modSources[s] + " > " + Params::modDests[d]);
                    pg.addAndMakeVisible (k);
                    matrixAttachments.add (new SliderAttachment (processor.apvts, Params::modId (s, d), *k));
                }

            pg.paintExtra = [this, &pg] (juce::Graphics& g)
            {
                const auto& th = look.getTheme();
                const auto area = pg.panels[0].bounds;
                g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
                g.setColour (th.dim);
                for (int d = 0; d < Params::numDests; ++d)
                    g.drawText (Params::modDests[d].toUpperCase(), area.getX() + matrixLabelW + d * matrixCellW, area.getY() + 36,
                                matrixCellW, 16, juce::Justification::centred);
                for (int s = 0; s < Params::numSources; ++s)
                {
                    g.setColour (s >= Params::firstSeqSource ? th.accent : th.dim);
                    g.drawText (Params::modSources[s].toUpperCase(), area.getX() + 16, area.getY() + 56 + s * matrixCellH,
                                matrixLabelW - 16, matrixCellH, juce::Justification::centredLeft);
                }
            };
        }

        void layoutModPage()
        {
            auto& pg = *pages[2];
            const int w = pg.getWidth(), h = pg.getHeight();
            pg.panels[0].bounds = { 0, 0, 760, h };
            pg.panels[1].bounds = { 772, 0, w - 772, (h - 12) / 2 };
            pg.panels[2].bounds = { 772, (h - 12) / 2 + 12, w - 772, h - (h - 12) / 2 - 12 };
            Page::layout (pg.panels[1], 60);
            Page::layout (pg.panels[2], 140);

            const auto area = pg.panels[0].bounds;
            matrixLabelW = 110;
            matrixCellW = (area.getWidth() - matrixLabelW - 16) / Params::numDests;
            matrixCellH = (area.getHeight() - 66) / Params::numSources;
            for (int s = 0; s < Params::numSources; ++s)
                for (int d = 0; d < Params::numDests; ++d)
                    matrixKnobs[s * Params::numDests + d]->setBounds (
                        juce::Rectangle<int> (area.getX() + matrixLabelW + d * matrixCellW, area.getY() + 56 + s * matrixCellH,
                                              matrixCellW, matrixCellH).withSizeKeepingCentre (matrixCellH - 4, matrixCellH - 4));
        }

        void buildSeqPage()
        {
            auto& pg = *pages[3];
            pg.panels = {
                { "SEQUENZER  -  16 SCHRITTE, TONHOEHEN-PADS UND DREI FREIE REIHEN UEBER DIE MOD-MATRIX", {},
                  { toggle (pg, "sq_run", "Lauf", "Stopp", "Start"), toggle (pg, "sq_rec", "Aufnahme"),
                    toggle (pg, "sq_trans", "Transpon."), choice (pg, "sq_dir", "Richtung", 100),
                    choice (pg, "sq_rate", "Schritt", 84), knob (pg, "sq_len", "Laenge"), knob (pg, "sq_gate", "Gate"),
                    knob (pg, "sq_swing", "Swing"), knob (pg, "sq_slew", "Glaetten"), knob (pg, "sq_root", "Grundton") } },
            };

            for (int i = 0; i < Params::seqSteps; ++i)
            {
                pg.addAndMakeVisible (pads.add (new PitchPad (processor.apvts, i)));
                for (int r = 0; r < Params::seqRows; ++r)
                {
                    auto* k = rowKnobs.add (new RubberSlider (true));
                    k->setDoubleClickReturnValue (true, 0.0);
                    pg.addAndMakeVisible (k);
                    rowAttachments.add (new SliderAttachment (processor.apvts, Params::seqRowId (r, i), *k));
                }
            }

            dice.setButtonText ("Wuerfeln");
            dice.onClick = [this] { randomiseSequence(); };
            reset.setButtonText ("Zuruecksetzen");
            reset.onClick = [this] { resetSequence(); };
            pg.addAndMakeVisible (dice);
            pg.addAndMakeVisible (reset);

            pg.paintExtra = [this, &pg] (juce::Graphics& g)
            {
                const auto& th = look.getTheme();
                const int x0 = seqGrid.getX();
                // aktiver Bereich (Länge) hinterlegen
                const int len = juce::roundToInt (processor.apvts.getRawParameterValue ("sq_len")->load());
                g.setColour (th.ink.withAlpha (0.05f));
                g.fillRoundedRectangle ((float) (x0 + seqLabelW), (float) seqGrid.getY(), (float) (seqColW * len),
                                        (float) seqGrid.getHeight(), 8.0f);

                g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
                for (int i = 0; i < Params::seqSteps; ++i)
                {
                    const int cx = x0 + seqLabelW + i * seqColW;
                    g.setColour (i < len ? th.dim : th.dim.withAlpha (0.4f));
                    g.drawText (juce::String (i + 1), cx, seqGrid.getY() + 2, seqColW, 14, juce::Justification::centred);
                    const bool lit = i == shownStep;
                    auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ (float) cx + seqColW * 0.5f, (float) seqGrid.getY() + 24.0f });
                    if (lit)
                    {
                        g.setColour (th.accent.withAlpha (0.4f));
                        g.fillEllipse (led.expanded (4.0f));
                    }
                    g.setColour (lit ? th.accent : th.knobTrack);
                    g.fillEllipse (led);
                }

                const char* rowNames[] { "TONHOEHE", "REIHE A", "REIHE B", "REIHE C" };
                const char* rowSubs[]  { "tippen an/aus, ziehen = Ton", "Matrix: Seq A", "Matrix: Seq B", "Matrix: Seq C" };
                for (int r = 0; r < 4; ++r)
                {
                    const auto row = seqRowBounds (r);
                    g.setColour (th.ink);
                    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                    g.drawText (rowNames[r], x0, row.getY() + row.getHeight() / 2 - 15, seqLabelW - 8, 16, juce::Justification::centredLeft);
                    g.setColour (r == 0 ? th.dim : th.accent);
                    g.setFont (juce::FontOptions (10.5f));
                    g.drawText (rowSubs[r], x0, row.getY() + row.getHeight() / 2 + 1, seqLabelW - 8, 14, juce::Justification::centredLeft);
                }
                juce::ignoreUnused (pg);
            };
        }

        juce::Rectangle<int> seqRowBounds (int r) const
        {
            const int top = seqGrid.getY() + 36;
            const int padH = 58;
            const int rowH = (seqGrid.getBottom() - top - padH) / 3;
            if (r == 0)
                return { seqGrid.getX(), top, seqGrid.getWidth(), padH };
            return { seqGrid.getX(), top + padH + (r - 1) * rowH, seqGrid.getWidth(), rowH };
        }

        void layoutSeqPage()
        {
            auto& pg = *pages[3];
            pg.panels[0].bounds = pg.getLocalBounds();
            Page::layout (pg.panels[0], 120);

            auto& last = *pg.panels[0].items.back();
            dice.setBounds (last.getRight() + 24, 64, 120, 30);
            reset.setBounds (last.getRight() + 24, 104, 120, 30);

            seqGrid = pg.getLocalBounds().reduced (16, 0).withTrimmedTop (168).withTrimmedBottom (12);
            seqLabelW = 150;
            seqColW = (seqGrid.getWidth() - seqLabelW) / Params::seqSteps;
            for (int i = 0; i < Params::seqSteps; ++i)
            {
                const int cx = seqGrid.getX() + seqLabelW + i * seqColW;
                const auto padRow = seqRowBounds (0);
                pads[i]->setBounds (cx, padRow.getY(), seqColW, padRow.getHeight());
                for (int r = 0; r < Params::seqRows; ++r)
                {
                    const auto row = seqRowBounds (r + 1);
                    const int size = juce::jmin (seqColW - 6, row.getHeight() - 6);
                    rowKnobs[i * Params::seqRows + r]->setBounds (
                        juce::Rectangle<int> (cx, row.getY(), seqColW, row.getHeight()).withSizeKeepingCentre (size, size));
                }
            }
        }

        //----------------------------------------------------------------------
        // Rhythmus: Drumcomputer-Wahl und Beat aus Text links, Regler und Raster rechts
        void buildRhythmPage()
        {
            auto& pg = *pages[4];
            pg.panels = {
                { "DRUMCOMPUTER", {}, {} },
                { "RHYTHMUS  -  12 SPUREN, 16 SCHRITTE", {},
                  { toggle (pg, "rh_run", "Lauf", "Stopp", "Start"), toggle (pg, "rh_link", "Seq koppeln"),
                    choice (pg, "rh_style", "Stil", 160), choice (pg, "rh_fill", "Fill", 96), choice (pg, "rh_rate", "Raster", 78),
                    toggle (pg, "rh_midi", "MIDI-Kanal 10") } },
            };
            for (auto* id : { "rh_rand", "rh_dens", "rh_var", "rh_swing", "rh_hum", "rh_acc", "rh_len", "rh_vol", "tempo" })
            {
                static const std::map<juce::String, juce::String> names { { "rh_rand", "Zufall" }, { "rh_dens", "Dichte" },
                    { "rh_var", "Variation" }, { "rh_swing", "Shuffle" }, { "rh_hum", "Humanize" }, { "rh_acc", "Akzent" },
                    { "rh_len", "Laenge" }, { "rh_vol", "Drums" }, { "tempo", "Tempo" } };
                rhythmKnobs.push_back (knob (pg, id, names.at (id), 56));
            }

            for (int i = 0; i < 10; ++i)
            {
                auto* b = kitButtons.add (new KitButton (i));
                b->onClick = [this, i]
                {
                    setParam ("rh_kit", (float) i);
                    if (processor.apvts.getRawParameterValue ("rh_run")->load() < 0.5f)
                    {
                        juce::Component::SafePointer<Content> safe (this);
                        processor.audition (0, 1.0f);
                        juce::Timer::callAfterDelay (120, [safe] { if (safe) safe->processor.audition (4, 0.8f); });
                        juce::Timer::callAfterDelay (250, [safe] { if (safe) safe->processor.audition (1, 0.9f); });
                    }
                };
                pg.addAndMakeVisible (b);
            }

            rhythmText.setMultiLine (true, true);
            rhythmText.setReturnKeyStartsNewLine (false);
            rhythmText.setTextToShowWhenEmpty ("Beat beschreiben, z. B. \"lockerer 808 Hip-Hop mit Shuffle\" oder \"bum tschak bum bum tschak\"",
                                               juce::Colours::grey);
            rhythmText.setText (processor.getRhythmText(), false);
            rhythmText.onReturnKey = [this] { applyText(); };
            pg.addAndMakeVisible (rhythmText);
            makeBeat.setButtonText ("Beat machen");
            makeBeat.onClick = [this] { applyText(); };
            pg.addAndMakeVisible (makeBeat);
            ideas.setTextWhenNothingSelected ("Ideen ...");
            for (int i = 0; i < Rhythm::ideas().size(); ++i)
                ideas.addItem (Rhythm::ideas()[i].upToFirstOccurrenceOf ("\n", false, false), i + 1);
            ideas.onChange = [this]
            {
                const int i = ideas.getSelectedItemIndex();
                if (i < 0) return;
                rhythmText.setText (Rhythm::ideas()[i], false);
                ideas.setSelectedItemIndex (-1, juce::dontSendNotification);
                applyText();
            };
            pg.addAndMakeVisible (ideas);
            readLabel.setJustificationType (juce::Justification::topLeft);
            readLabel.setFont (juce::FontOptions (11.5f));
            readLabel.setMinimumHorizontalScale (1.0f);
            pg.addAndMakeVisible (readLabel);

            dice2.setButtonText ("Wuerfeln");
            dice2.onClick = [this] { setParam ("rh_rand", (float) juce::Random::getSystemRandom().nextInt (100) / 100.0f); };
            clear.setButtonText ("Leeren");
            clear.onClick = [this] { processor.setPattern ({}); };
            pg.addAndMakeVisible (dice2);
            pg.addAndMakeVisible (clear);

            grid = std::make_unique<StepGrid> (processor);
            grid->onSelect = [this] (int t) { showTrack (t); };
            pg.addAndMakeVisible (*grid);

            // Klang der gewählten Spur
            static const char* what[] { "lv", "tu", "de", "pn" };
            for (int t = 0; t < Drums::numTracks; ++t)
                for (int k = 0; k < 4; ++k)
                {
                    auto* sl = trackKnobs.add (new RubberSlider (true));
                    sl->setDoubleClickReturnValue (true, k == 0 ? 0.8 : 0.0);
                    pg.addChildComponent (sl);
                    trackAttachments.add (new SliderAttachment (processor.apvts, Params::rhId (what[k], t), *sl));
                }
            showTrack (0);
            shownKit = (int) processor.apvts.getRawParameterValue ("rh_kit")->load();
            for (int i = 0; i < kitButtons.size(); ++i)
                kitButtons[i]->setToggleState (i == shownKit, juce::dontSendNotification);

            pg.paintExtra = [this, &pg] (juce::Graphics& g)
            {
                const auto& th = look.getTheme();
                const auto& right = pg.panels[1].bounds;
                g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                Design::drawText (g, th, processor.getRhythmTag(), juce::Rectangle<float> ((float) right.getRight() - 330.0f, (float) right.getY() + 9.0f, 314.0f, 20.0f),
                                  juce::Justification::centredRight, th.accent);

                const auto& left = pg.panels[0].bounds;
                g.setColour (th.dim);
                g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.12f));
                g.drawText ("BEAT AUS TEXT", left.getX() + 14, rhythmText.getY() - 20, 200, 16, juce::Justification::centredLeft);

                // Klang der gewählten Spur
                g.setColour (th.ink);
                g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                const int kit = (int) processor.apvts.getRawParameterValue ("rh_kit")->load();
                g.drawText (juce::String ("KLANG  ") + Rhythm::trackShort[grid->selected] + "  " + Rhythm::trackLabel (kit, grid->selected).toUpperCase(),
                            trackRow.getX(), trackRow.getY(), 220, trackRow.getHeight(), juce::Justification::centredLeft);
                static const char* names[] { "PEGEL", "STIMMUNG", "AUSKLANG", "PANORAMA" };
                g.setColour (th.dim);
                g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
                for (int k = 0; k < 4; ++k)
                {
                    const auto* sl = trackKnobs[grid->selected * 4 + k];
                    g.drawText (names[k], sl->getX() - 80, sl->getY(), 76, sl->getHeight(), juce::Justification::centredRight);
                }
            };
        }

        void layoutRhythmPage()
        {
            auto& pg = *pages[4];
            const int w = pg.getWidth(), h = pg.getHeight();
            const int leftW = 300;
            pg.panels[0].bounds = { 0, 0, leftW, h };
            pg.panels[1].bounds = { leftW + 12, 0, w - leftW - 12, h };

            // Modelle: zwei Spalten
            const int bw = (leftW - 28) / 2, bh = 46;
            for (int i = 0; i < kitButtons.size(); ++i)
                kitButtons[i]->setBounds (14 + (i % 2) * bw, 38 + (i / 2) * bh, bw, bh);
            const int textTop = 38 + 5 * bh + 30;
            rhythmText.setBounds (14, textTop, leftW - 28, 76);
            ideas.setBounds (14, textTop + 84, (leftW - 28) / 2 - 4, 28);
            makeBeat.setBounds (14 + (leftW - 28) / 2 + 4, textTop + 84, (leftW - 28) / 2 - 4, 28);
            readLabel.setBounds (14, textTop + 118, leftW - 28, h - textTop - 124);

            auto& right = pg.panels[1];
            Page::layout (right, 50, -8);
            auto& last = *right.items.back();
            dice2.setBounds (last.getRight() + 10, 46, 92, 28);
            clear.setBounds (last.getRight() + 108, 46, 80, 28);

            const bool faders = look.getTheme().faders();
            auto knobRow = juce::Rectangle<int> (right.bounds.getX() + 14, 88, right.bounds.getWidth() - 28, faders ? 104 : 100);
            const int kw = knobRow.getWidth() / (int) rhythmKnobs.size();
            for (auto* k : rhythmKnobs)
                k->setBounds (knobRow.removeFromLeft (kw).withTrimmedRight (6));

            trackRow = { right.bounds.getX() + 14, h - 46, right.bounds.getWidth() - 28, 36 };
            grid->setBounds (right.bounds.getX() + 14, 88 + (faders ? 108 : 104), right.bounds.getWidth() - 28,
                             trackRow.getY() - (88 + (faders ? 108 : 104)) - 6);

            const int knobW = (trackRow.getWidth() - 240) / 4;
            for (int t = 0; t < Drums::numTracks; ++t)
                for (int k = 0; k < 4; ++k)
                {
                    auto r = juce::Rectangle<int> (trackRow.getX() + 240 + k * knobW + 84, trackRow.getY(), knobW - 92, trackRow.getHeight());
                    trackKnobs[t * 4 + k]->setBounds (faders ? r : r.withSizeKeepingCentre (trackRow.getHeight(), trackRow.getHeight()).withX (r.getX()));
                }
        }

        void showTrack (int t)
        {
            for (int i = 0; i < trackKnobs.size(); ++i)
                trackKnobs[i]->setVisible (i / 4 == t);
            pages[4]->repaint();
        }

        void applyText()
        {
            const auto read = processor.applyRhythmText (rhythmText.getText());
            readLabel.setText (read.isEmpty() ? juce::String() : "Gelesen:  " + read.joinIntoString ("  /  "), juce::dontSendNotification);
            pages[4]->repaint();
        }

        void setParam (const juce::String& id, float value)
        {
            if (auto* p = processor.apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
        }

        void randomiseSequence()
        {
            static constexpr int scale[] { 0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24 };
            juce::Random rng;
            for (int i = 0; i < Params::seqSteps; ++i)
            {
                setParam (Params::seqPitchId (i), (float) scale[rng.nextInt (juce::numElementsInArray (scale))] - (rng.nextFloat() < 0.2f ? 12.0f : 0.0f));
                setParam (Params::seqGateId (i), rng.nextFloat() < 0.75f ? 1.0f : 0.0f);
                for (int r = 0; r < Params::seqRows; ++r)
                    setParam (Params::seqRowId (r, i), (float) juce::roundToInt ((rng.nextFloat() * 2.0f - 1.0f) * 100.0f) / 100.0f);
            }
        }

        void resetSequence()
        {
            for (int i = 0; i < Params::seqSteps; ++i)
            {
                for (auto id : { Params::seqPitchId (i), Params::seqGateId (i), Params::seqRowId (0, i),
                                 Params::seqRowId (1, i), Params::seqRowId (2, i) })
                    if (auto* p = processor.apvts.getParameter (id))
                        p->setValueNotifyingHost (p->getDefaultValue());
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
            for (auto* k : matrixKnobs) k->setFaderMode (faders);
            for (auto* k : rowKnobs) k->setFaderMode (faders);
            for (auto* k : trackKnobs) k->setFaderMode (faders);
            rhythmText.applyColourToAllText (look.getTheme().ink);
            rhythmText.setColour (juce::TextEditor::backgroundColourId, look.getTheme().field);
            rhythmText.setColour (juce::TextEditor::outlineColourId, look.getTheme().line);
            rhythmText.setColour (juce::TextEditor::focusedOutlineColourId, look.getTheme().accent);
            readLabel.setColour (juce::Label::textColourId, look.getTheme().accent);
            for (int i = 0; i < designButtons.size(); ++i)
                designButtons[i]->setToggleState (i == d, juce::dontSendNotification);
            const auto& th = look.getTheme();
            keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, th.accent.withAlpha (0.75f));
            keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, th.accent.withAlpha (0.3f));
            keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff6a6d74));
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

            // Spielfläche der Tastatur
            g.setColour (th.faders() ? juce::Colour (0xff121315) : juce::Colour (0xffdde1e6));
            g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, (float) getHeight() - 120.0f, (float) getWidth() - 24.0f, 112.0f),
                                    th.faders() ? 3.0f : 18.0f);
        }

        void timerCallback() override
        {
            const int step = processor.getSeqStep();
            if (step != shownStep)
            {
                shownStep = step;
                for (int i = 0; i < pads.size(); ++i)
                    pads[i]->setCurrent (i == step);
                if (currentPage == 3)
                    pages[3]->repaint();
            }

            // Drums: Lauflicht, Muster-Änderungen, gewähltes Modell
            const int drumStep = processor.getDrumStep();
            const int version = processor.getPatternVersion();
            const int kit = (int) processor.apvts.getRawParameterValue ("rh_kit")->load();
            if (drumStep != grid->current || version != shownVersion || kit != shownKit)
            {
                grid->current = drumStep;
                grid->flash = processor.getDrumHits();
                shownVersion = version;
                if (kit != shownKit)
                {
                    shownKit = kit;
                    for (int i = 0; i < kitButtons.size(); ++i)
                        kitButtons[i]->setToggleState (i == kit, juce::dontSendNotification);
                }
                if (currentPage == 4)
                    pages[4]->repaint();
            }

            // läuft Sequenzer oder Drums auf einer anderen Seite, blinkt die Taster-LED
            const bool phase = (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
            for (auto [tabIndex, active] : { std::pair<int, bool> { 3, step >= 0 }, { 4, drumStep >= 0 } })
            {
                auto* tab = tabs[tabIndex];
                const bool running = active && currentPage != tabIndex;
                if (running != tab->blink || (running && phase != tab->blinkPhase))
                {
                    tab->blink = running;
                    tab->blinkPhase = phase;
                    tab->repaint();
                }
            }
        }

        MangamanProcessor& processor;
        MangamanLook& look;

        juce::Image background, grain;
        juce::OwnedArray<TabButton> designButtons;
        juce::OwnedArray<TabButton> tabs;
        juce::OwnedArray<Page> pages;
        juce::OwnedArray<Labelled> controls;
        int currentPage = 0;

        juce::OwnedArray<RubberSlider> matrixKnobs;
        juce::OwnedArray<SliderAttachment> matrixAttachments;
        int matrixLabelW = 110, matrixCellW = 80, matrixCellH = 50;

        juce::OwnedArray<PitchPad> pads;
        juce::OwnedArray<RubberSlider> rowKnobs;
        juce::OwnedArray<SliderAttachment> rowAttachments;
        juce::TextButton dice, reset;
        juce::Rectangle<int> seqGrid;
        int seqLabelW = 150, seqColW = 60, shownStep = -1;

        juce::OwnedArray<KitButton> kitButtons;
        std::vector<Labelled*> rhythmKnobs;
        std::unique_ptr<StepGrid> grid;
        juce::OwnedArray<RubberSlider> trackKnobs;
        juce::OwnedArray<SliderAttachment> trackAttachments;
        juce::TextEditor rhythmText;
        juce::TextButton makeBeat, dice2, clear;
        juce::ComboBox ideas;
        juce::Label readLabel;
        juce::Rectangle<int> trackRow;
        int shownVersion = -1, shownKit = -1;

        juce::MidiKeyboardComponent keyboard;
    };
}

//==============================================================================
MangamanLook::MangamanLook()
{
    setTheme (Design::themeFor (Design::Future2100));
}

void MangamanLook::setTheme (const Design::Theme& t)
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

void MangamanLook::drawRubberKnob (juce::Graphics& g, juce::Rectangle<float> area, float pos, float squish,
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

void MangamanLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                     float startAngle, float endAngle, juce::Slider& slider)
{
    drawRubberKnob (g, juce::Rectangle<int> (x, y, w, h).toFloat(), pos, 0.0f, {},
                    slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0, startAngle, endAngle, false);
}

void MangamanLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
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

void MangamanLook::drawFader (juce::Graphics& g, juce::Rectangle<float> area, float pos, bool bipolar, bool horizontal, float squish)
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
MangamanEditor::MangamanEditor (MangamanProcessor& p)
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

MangamanEditor::~MangamanEditor()
{
    content.reset();
    setLookAndFeel (nullptr);
}

void MangamanEditor::resized()
{
    if (content != nullptr)
        content->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth));
}
