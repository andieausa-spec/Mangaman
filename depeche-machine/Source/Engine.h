#pragma once

#include "Voices.h"

// Spielt einen generierten Song ab: holt die Noten des laufenden Blocks aus dem Song,
// startet die Stimmen sampelgenau und mischt sieben Spuren mit Echo und Hall.
class Engine
{
public:
    struct Controls
    {
        double bpm = 120.0;
        bool loop = true;
        int transpose = 0;
        std::array<float, Depeche::numLanes> laneGain { 1, 1, 1, 1, 1, 1, 1 };

        int padType = 0; float padBright = 0.5f, padAttack = 0.4f, padEnsemble = 0.6f, padVowel = 0.4f;
        float seqCutoff = 0.5f, seqReso = 0.45f, seqEnv = 0.5f, seqDecay = 0.3f; int seqWave = 0;
        float bassCutoff = 0.4f, bassDecay = 0.35f, bassSub = 0.4f, bassDrive = 0.3f;
        float leadGlide = 0.3f, leadVibrato = 0.4f, leadBright = 0.55f; int leadWave = 0;
        int drumKit = 0; float drumTone = 0.5f, drumSwing = 0.0f;
        float fxAtmo = 0.55f, fxSwell = 0.5f, fxMetal = 0.5f;
        bool vocalOn = true;
        float vocalDepth = 0.65f, vocalBreath = 0.3f, vocalVibrato = 0.45f, vocalGlide = 0.35f;
        float vocalDouble = 0.5f, vocalHarmony = 0.5f, vocalReverb = 0.45f, vocalEcho = 0.3f;
        double echoBeats = 0.75; float echoFeedback = 0.35f, echoMix = 0.35f;
        float reverbSize = 0.7f, reverbMix = 0.35f, width = 0.8f, master = 0.7f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ensemble.prepare (sr);
        choir.prepare (sr);
        doubler.prepare (sr);
        echo.prepare (sr);
        drums.prepare (sr);
        reverb.setSampleRate (sr);
        reverb.reset();
        revL.assign (chunk, 0.0f);
        revR.assign (chunk, 0.0f);
        events.reserve (1024);
        for (auto& g : gains) g = 0.0f;
        allOff (true);
        wasPlaying = false;
    }

    // nur aus dem Audio-Thread
    void setSong (const Depeche::Song* s) { song = s; jumped = true; }
    const Depeche::Song* getSong() const { return song; }

    void allOff (bool hard)
    {
        for (auto& v : pads) { if (hard) { v.env.kill(); v.active = false; } else v.env.noteOff(); v.gate = 0; }
        if (hard)
        {
            seq.active = bass.active = lead.active = false;
            voice.active = harmony.active = false;
            voice.fresh = harmony.fresh = true;
            for (auto& f : fx) f.active = false;
        }
        else
        {
            seq.amp.noteOff(); seq.gate = 0;
            bass.amp.noteOff(); bass.gate = 0;
            lead.release();
            voice.release();
            harmony.release();
            for (auto& f : fx) if (f.active && f.type == Depeche::Atmo) f.total = juce::jmin (f.total, f.pos + (juce::int64) (sr * 1.5));
        }
    }

    // beat: Position am Blockanfang in Vierteln (schon in den Song gefaltet), playing: Transport läuft
    void process (float* outL, float* outR, int numSamples, double beat, bool playing, const Controls& c)
    {
        int done = 0;
        while (done < numSamples)
        {
            const int n = juce::jmin (chunk, numSamples - done);
            processChunk (outL + done, outR != nullptr ? outR + done : nullptr, n, beat, playing, c);
            beat += n * c.bpm / 60.0 / sr;
            if (song != nullptr && c.loop && beat >= song->totalBeats())
                beat = std::fmod (beat, song->totalBeats());
            done += n;
        }
    }

    std::array<std::atomic<float>, Depeche::numLanes> laneLevel {};
    std::atomic<float> outLevel { 0.0f }, outLevelR { 0.0f };

private:
    struct Event { int offset; int lane; Depeche::Note note; double remaining; };

    static constexpr int chunk = 256;

    void collect (double from, double to, double beatsPerSample, int baseOffset)
    {
        for (int lane = 0; lane < Depeche::numLanes; ++lane)
        {
            const auto& v = song->notes[(size_t) lane];
            auto it = std::lower_bound (v.begin(), v.end(), from, [] (const Depeche::Note& n, double b) { return n.start < b; });
            for (; it != v.end() && it->start < to; ++it)
                events.push_back ({ baseOffset + (int) ((it->start - from) / beatsPerSample), lane, *it, it->length });
        }
    }

    // Nach einem Sprung: lange Töne (Flächen, Melodie, Atmosphäre), die gerade klingen müssten, sofort nachholen
    void chase (double beat)
    {
        for (int lane : { (int) Depeche::Pad, (int) Depeche::Lead, (int) Depeche::Fx })
            for (const auto& n : song->notes[(size_t) lane])
            {
                if (n.start >= beat) break;
                const double rest = n.start + n.length - beat;
                if (rest > 0.25 && (lane != Depeche::Fx || n.pitch == Depeche::Atmo))
                    events.push_back ({ 0, lane, n, rest });
            }
    }

    void trigger (const Event& e, const Controls& c, double beatsPerSample)
    {
        const auto len = (juce::int64) juce::jmax (1.0, e.remaining / beatsPerSample);
        const int pitch = e.note.pitch + c.transpose;
        switch (e.lane)
        {
            case Depeche::Pad:
            {
                Dsp::PadVoice* best = nullptr;
                for (auto& v : pads) if (! v.active) { best = &v; break; }
                if (best == nullptr)
                    for (auto& v : pads) if (best == nullptr || v.order < best->order) best = &v;
                best->start (pitch, e.note.velocity, len, sr, 0.02f + c.padAttack * c.padAttack * 2.5f, 0.6f + c.padAttack * 2.0f,
                             c.padType, noise, ++counter);
                best->setCutoff (padCutoff (c), sr);
                break;
            }
            case Depeche::Seq:
                seq.wave = c.seqWave;
                seq.start (pitch, e.note.velocity, len, sr, 0.03f + c.seqDecay * c.seqDecay * 0.8f);
                break;
            case Depeche::Bass:
                bass.start (pitch, e.note.velocity, len, sr, 0.04f + c.bassDecay * 0.5f);
                break;
            case Depeche::Lead:
                lead.wave = c.leadWave;
                lead.start (pitch, e.note.velocity, len, sr);
                break;
            case Depeche::Vocal:
            {
                auto& v = e.note.harmony ? harmony : voice;
                v.start (pitch, e.note.velocity, len, sr, e.note.syllable, (juce::uint32) (++counter * 2654435761u));
                break;
            }
            case Depeche::Drums:
                drums.trigger (e.note.pitch, e.note.velocity, c.drumKit, c.drumTone);
                break;
            case Depeche::Fx:
            {
                Dsp::FxVoice* best = nullptr;
                for (auto& f : fx) if (! f.active) { best = &f; break; }
                if (best == nullptr) best = &fx[0];
                const auto full = (juce::int64) (e.note.length / beatsPerSample);
                best->start (e.note.pitch, e.note.velocity, full, full - len, sr, (juce::uint32) (++counter * 2654435761u));
                break;
            }
            default: break;
        }
    }

    static double padCutoff (const Controls& c) { return 420.0 * std::pow (2.0, c.padBright * 5.2); }

    void processChunk (float* outL, float* outR, int n, double beat, bool playing, const Controls& c)
    {
        const double bps = c.bpm / 60.0 / sr;
        events.clear();
        const bool active = playing && song != nullptr && song->totalBeats() > 0.0;
        if (active)
        {
            const double total = song->totalBeats();
            if (! wasPlaying || jumped || std::abs (beat - expectedBeat) > 0.02)
            {
                allOff (false);
                if (beat < total) chase (beat);
                jumped = false;
            }
            const double b1 = beat + n * bps;
            if (beat < total)
                collect (beat, juce::jmin (b1, total), bps, 0);
            if (b1 > total && c.loop)
                collect (0.0, b1 - total, bps, (int) ((total - beat) / bps));
            // Swing: jede zweite Sechzehntel des Rhythmus etwas später
            if (c.drumSwing > 0.001f)
                for (auto& e : events)
                    if ((e.lane == Depeche::Drums || e.lane == Depeche::Bass) && std::fmod (e.note.start * 4.0 + 0.001, 2.0) >= 1.0)
                        e.offset += (int) (c.drumSwing * 0.33 * 0.25 / bps);
            std::sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.offset < b.offset; });
            expectedBeat = b1;
            if (c.loop && expectedBeat >= total) expectedBeat = std::fmod (expectedBeat, total);
        }
        else if (wasPlaying)
        {
            allOff (false);
        }
        wasPlaying = active;

        // Spurlautstärke mal Blende des Abschnitts
        std::array<float, Depeche::numLanes> target = c.laneGain;
        if (! c.vocalOn) target[Depeche::Vocal] = 0.0f;
        if (active)
        {
            const float fade = song->automation (beat, false);
            for (auto& t : target) t *= fade;
        }

        // Klangparameter je Block
        const double padCut = padCutoff (c);
        for (auto& v : pads) if (v.active) v.setCutoff (padCut, sr);
        const float filterAuto = active ? song->automation (beat, true) : 0.6f;
        seq.cutoff = (float) (90.0 * std::pow (2.0, c.seqCutoff * 7.0) * std::pow (2.0, (filterAuto - 0.55) * 3.0));
        seq.envAmount = c.seqEnv;
        seq.resonance = c.seqReso;
        bass.cutoff = (float) (60.0 * std::pow (2.0, c.bassCutoff * 6.0));
        bass.sub = c.bassSub;
        bass.drive = c.bassDrive;
        lead.glide = c.leadGlide;
        lead.vibrato = c.leadVibrato;
        lead.bright = c.leadBright;
        for (auto* v : { &voice, &harmony })
        {
            v->depth = c.vocalDepth;
            v->breath = c.vocalBreath;
            v->vibrato = c.vocalVibrato;
            v->glide = c.vocalGlide;
        }
        const float fxAmounts[] { c.fxAtmo, c.fxSwell, c.fxMetal };
        const float echoSamples = (float) (c.echoBeats * 60.0 / c.bpm * sr);
        const float harmonyGain = c.vocalHarmony * 0.9f;

        juce::Reverb::Parameters rp;
        rp.roomSize = 0.5f + 0.48f * c.reverbSize;
        rp.damping = 0.4f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        reverb.setParameters (rp);

        std::array<float, Depeche::numLanes> peaks {};
        float outPeak = 0.0f, outPeakR = 0.0f;
        size_t ei = 0;
        for (int i = 0; i < n; ++i)
        {
            while (ei < events.size() && events[ei].offset <= i)
                trigger (events[ei++], c, bps);

            for (int l = 0; l < Depeche::numLanes; ++l)
                gains[(size_t) l] += (target[(size_t) l] - gains[(size_t) l]) * 0.002f;

            float padMono = 0.0f;
            for (auto& v : pads) if (v.active) padMono += v.render();
            if (c.padType == 1)
                padMono = choir.process (padMono, c.padVowel) * 1.3f;
            float padL, padR;
            ensemble.process (padMono * 0.45f, c.padEnsemble, padL, padR);
            padL *= gains[Depeche::Pad]; padR *= gains[Depeche::Pad];

            const float s = seq.render() * 0.42f * gains[Depeche::Seq];
            const float b = bass.render() * 0.6f * gains[Depeche::Bass];
            const float ld = lead.render() * 0.36f * gains[Depeche::Lead];
            const float vMono = (voice.render() + harmony.render() * harmonyGain) * 0.9f * gains[Depeche::Vocal];
            float vl, vr;
            doubler.process (vMono, c.vocalDouble, vl, vr);
            float dl, dr;
            drums.render (dl, dr);
            dl *= 0.62f * gains[Depeche::Drums]; dr *= 0.62f * gains[Depeche::Drums];
            float fl = 0.0f, fr = 0.0f;
            for (auto& f : fx)
                if (f.active)
                {
                    float a, bb;
                    f.render (a, bb, fxAmounts);
                    fl += a; fr += bb;
                }
            fl *= 0.5f * gains[Depeche::Fx]; fr *= 0.5f * gains[Depeche::Fx];

            float L = padL + s * 1.0f + b + ld * 0.85f + vl + dl + fl;
            float R = padR + s * 0.8f + b + ld * 1.0f + vr + dr + fr;

            const float echoIn = (padL + padR) * 0.05f + s * 0.35f + ld * 0.5f + vMono * c.vocalEcho * 0.7f + (fl + fr) * 0.2f
                                 + dl * (c.drumKit == 2 ? 0.08f : 0.02f);
            float el, er;
            echo.process (echoIn, echoSamples, 0.12f + c.echoFeedback * 0.75f, el, er);
            L += el * c.echoMix;
            R += er * c.echoMix;

            const float revIn = (padL + padR) * 0.3f + s * 0.2f + ld * 0.4f + vMono * c.vocalReverb * 0.9f
                                + (dl + dr) * (c.drumKit == 2 ? 0.3f : 0.1f) + (fl + fr) * 0.45f + (el + er) * 0.2f;
            revL[(size_t) i] = revIn;
            revR[(size_t) i] = revIn;
            outL[i] = L;
            if (outR != nullptr) outR[i] = R;

            peaks[Depeche::Pad] = juce::jmax (peaks[Depeche::Pad], std::abs (padL));
            peaks[Depeche::Seq] = juce::jmax (peaks[Depeche::Seq], std::abs (s));
            peaks[Depeche::Bass] = juce::jmax (peaks[Depeche::Bass], std::abs (b));
            peaks[Depeche::Lead] = juce::jmax (peaks[Depeche::Lead], std::abs (ld));
            peaks[Depeche::Vocal] = juce::jmax (peaks[Depeche::Vocal], std::abs (vMono));
            peaks[Depeche::Drums] = juce::jmax (peaks[Depeche::Drums], std::abs (dl));
            peaks[Depeche::Fx] = juce::jmax (peaks[Depeche::Fx], std::abs (fl));
        }

        reverb.processStereo (revL.data(), revR.data(), n);
        const float width = c.width * 1.4f;
        for (int i = 0; i < n; ++i)
        {
            float L = outL[i] + revL[(size_t) i] * c.reverbMix * 0.6f;
            float R = (outR != nullptr ? outR[i] : outL[i]) + revR[(size_t) i] * c.reverbMix * 0.6f;
            const float mid = 0.5f * (L + R), side = 0.5f * (L - R) * width;
            L = std::tanh ((mid + side) * c.master);
            R = std::tanh ((mid - side) * c.master);
            if (! std::isfinite (L) || ! std::isfinite (R)) L = R = 0.0f;
            outPeak = juce::jmax (outPeak, std::abs (L));
            outPeakR = juce::jmax (outPeakR, std::abs (R));
            if (outR != nullptr) { outL[i] = L; outR[i] = R; }
            else outL[i] = 0.5f * (L + R);
        }

        for (int l = 0; l < Depeche::numLanes; ++l)
            laneLevel[(size_t) l].store (juce::jmax (peaks[(size_t) l], laneLevel[(size_t) l].load() * 0.9f));
        outLevel.store (juce::jmax (outPeak, outLevel.load() * 0.9f));
        outLevelR.store (juce::jmax (outPeakR, outLevelR.load() * 0.9f));
    }

    double sr = 48000.0;
    const Depeche::Song* song = nullptr;
    bool wasPlaying = false, jumped = true;
    double expectedBeat = 0.0;
    juce::int64 counter = 0;

    std::array<Dsp::PadVoice, 14> pads;
    Dsp::SeqVoice seq;
    Dsp::BassVoice bass;
    Dsp::LeadVoice lead;
    Dsp::VocalVoice voice, harmony;
    Dsp::DrumKit drums;
    std::array<Dsp::FxVoice, 8> fx;
    Dsp::Ensemble ensemble;
    Dsp::ChoirFilter choir;
    Dsp::Doubler doubler;
    Dsp::Echo echo;
    Dsp::Noise noise;
    juce::Reverb reverb;
    std::vector<float> revL, revR;
    std::vector<Event> events;
    std::array<float, Depeche::numLanes> gains {};
};
