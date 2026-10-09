#pragma once

#include "Voices.h"

// Spielt einen generierten Track ab: holt die Noten des laufenden Blocks aus dem Song,
// startet die Stimmen sampelgenau und mischt sechs Spuren mit Echo und Hall.
class Engine
{
public:
    struct Controls
    {
        double bpm = 108.0;
        bool loop = true;
        int transpose = 0;
        std::array<float, Jarre::numLanes> laneGain { 1, 1, 1, 1, 1, 1 };

        float padEnsemble = 0.6f, padPhaser = 0.6f, padPhaserRate = 0.3f, padBright = 0.5f, padAttack = 0.5f;
        float seqCutoff = 0.45f, seqReso = 0.5f, seqEnv = 0.5f, seqDecay = 0.35f; int seqWave = 0;
        float bassCutoff = 0.35f, bassDecay = 0.5f, bassSub = 0.5f;
        float leadGlide = 0.45f, leadVibrato = 0.5f, leadBright = 0.5f; int leadWave = 0;
        int drumKit = 0; float drumTone = 0.5f, drumSwing = 0.0f;
        float fxWind = 0.6f, fxLaser = 0.5f, fxSurf = 0.45f;
        double echoBeats = 0.75; float echoFeedback = 0.45f, echoMix = 0.45f;
        float reverbSize = 0.75f, reverbMix = 0.45f, width = 0.8f, master = 0.7f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ensemble.prepare (sr);
        phaser.prepare (sr);
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
    void setSong (const Jarre::Song* s) { song = s; jumped = true; }
    const Jarre::Song* getSong() const { return song; }

    void allOff (bool hard)
    {
        for (auto& v : pads) { if (hard) { v.env.kill(); v.active = false; } else v.env.noteOff(); v.gate = 0; }
        if (hard) { seq.active = bass.active = lead.active = false; for (auto& f : fx) f.active = false; }
        else
        {
            seq.amp.noteOff(); seq.gate = 0;
            bass.amp.noteOff(); bass.gate = 0;
            lead.release();
            for (auto& f : fx) if (f.active && f.type != Jarre::Laser) f.total = juce::jmin (f.total, f.pos + (juce::int64) (sr * 1.5));
        }
    }

    // beat: Position am Blockanfang in Vierteln (schon in den Track gefaltet), playing: Transport läuft
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

    std::array<std::atomic<float>, Jarre::numLanes> laneLevel {};
    // für die Laser-Punktmatrix: Zähler der angeschlagenen Noten je Spur und Tonhöhe der letzten
    std::array<std::atomic<int>, Jarre::numLanes> hitCount {}, hitPitch {};
    std::atomic<float> outLevel { 0.0f }, outLevelR { 0.0f };

private:
    struct Event { int offset; int lane; Jarre::Note note; double remaining; };

    static constexpr int chunk = 256;

    void collect (double from, double to, double beatsPerSample, int baseOffset)
    {
        for (int lane = 0; lane < Jarre::numLanes; ++lane)
        {
            const auto& v = song->notes[(size_t) lane];
            auto it = std::lower_bound (v.begin(), v.end(), from, [] (const Jarre::Note& n, double b) { return n.start < b; });
            for (; it != v.end() && it->start < to; ++it)
                events.push_back ({ baseOffset + (int) ((it->start - from) / beatsPerSample), lane, *it, it->length });
        }
    }

    // Nach einem Sprung: lange Töne (Flächen, Melodie, Wind), die gerade klingen müssten, sofort nachholen
    void chase (double beat)
    {
        for (int lane : { (int) Jarre::Pad, (int) Jarre::Lead, (int) Jarre::Fx })
            for (const auto& n : song->notes[(size_t) lane])
            {
                if (n.start >= beat) break;
                const double rest = n.start + n.length - beat;
                if (rest > 0.25 && (lane != Jarre::Fx || n.pitch == Jarre::Wind || n.pitch == Jarre::Surf))
                    events.push_back ({ 0, lane, n, rest });
            }
    }

    void trigger (const Event& e, const Controls& c, double beatsPerSample)
    {
        const auto len = (juce::int64) juce::jmax (1.0, e.remaining / beatsPerSample);
        const int pitch = e.note.pitch + c.transpose;
        hitPitch[(size_t) e.lane].store (e.note.pitch);
        hitCount[(size_t) e.lane].fetch_add (1);
        switch (e.lane)
        {
            case Jarre::Pad:
            {
                Dsp::PadVoice* best = nullptr;
                for (auto& v : pads) if (! v.active) { best = &v; break; }
                if (best == nullptr)
                    for (auto& v : pads) if (best == nullptr || v.order < best->order) best = &v;
                best->start (pitch, e.note.velocity, len, sr, 0.03f + c.padAttack * c.padAttack * 3.5f, 1.2f + c.padAttack * 2.5f, noise, ++counter);
                best->setCutoff (380.0 * std::pow (2.0, c.padBright * 5.6), sr);
                break;
            }
            case Jarre::Seq:
                seq.pulse = c.seqWave == 1;
                seq.start (pitch, e.note.velocity, len, sr, 0.04f + c.seqDecay * c.seqDecay * 0.9f);
                break;
            case Jarre::Bass:
                bass.start (pitch, e.note.velocity, len, sr, 0.05f + c.bassDecay * 0.6f);
                break;
            case Jarre::Lead:
                lead.start (pitch, e.note.velocity, len, sr);
                break;
            case Jarre::Drums:
                drums.trigger (e.note.pitch, e.note.velocity, c.drumKit, c.drumTone);
                break;
            case Jarre::Fx:
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
            // Swing: jede zweite Sechzehntel der Rhythmusbox etwas später
            const double b1 = beat + n * bps;
            if (beat < total)
                collect (beat, juce::jmin (b1, total), bps, 0);
            if (b1 > total && c.loop)
                collect (0.0, b1 - total, bps, (int) ((total - beat) / bps));
            if (c.drumSwing > 0.001f)
                for (auto& e : events)
                    if (e.lane == Jarre::Drums && std::fmod (e.note.start * 4.0 + 0.001, 2.0) >= 1.0)
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
        std::array<float, Jarre::numLanes> target = c.laneGain;
        if (active)
        {
            const float fade = song->automation (beat, false);
            for (auto& t : target) t *= fade;
        }

        // Klangparameter je Block
        const double padCut = 380.0 * std::pow (2.0, c.padBright * 5.6);
        for (auto& v : pads) if (v.active) v.setCutoff (padCut, sr);
        const float filterAuto = active ? song->automation (beat, true) : 0.6f;
        seq.cutoff = (float) (70.0 * std::pow (2.0, c.seqCutoff * 7.2) * std::pow (2.0, (filterAuto - 0.55) * 3.5));
        seq.envAmount = c.seqEnv;
        seq.resonance = c.seqReso;
        bass.cutoff = (float) (60.0 * std::pow (2.0, c.bassCutoff * 6.0));
        bass.sub = c.bassSub;
        lead.glide = c.leadGlide;
        lead.vibrato = c.leadVibrato;
        lead.bright = c.leadBright;
        lead.wave = c.leadWave;
        const float fxAmounts[] { c.fxWind, c.fxLaser, c.fxSurf };
        const float echoSamples = (float) (c.echoBeats * 60.0 / c.bpm * sr);

        juce::Reverb::Parameters rp;
        rp.roomSize = 0.55f + 0.44f * c.reverbSize;
        rp.damping = 0.45f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        reverb.setParameters (rp);

        std::array<float, Jarre::numLanes> peaks {};
        float outPeak = 0.0f, outPeakR = 0.0f;
        size_t ei = 0;
        for (int i = 0; i < n; ++i)
        {
            while (ei < events.size() && events[ei].offset <= i)
                trigger (events[ei++], c, bps);

            for (int l = 0; l < Jarre::numLanes; ++l)
                gains[(size_t) l] += (target[(size_t) l] - gains[(size_t) l]) * 0.002f;

            float padMono = 0.0f;
            for (auto& v : pads) if (v.active) padMono += v.render();
            float padL, padR;
            ensemble.process (padMono * 0.5f, c.padEnsemble, padL, padR);
            phaser.process (padL, padR, c.padPhaserRate, c.padPhaser);
            padL *= gains[Jarre::Pad]; padR *= gains[Jarre::Pad];

            const float s = seq.render() * 0.5f * gains[Jarre::Seq];
            const float b = bass.render() * 0.55f * gains[Jarre::Bass];
            const float ld = lead.render() * 0.4f * gains[Jarre::Lead];
            float dl, dr;
            drums.render (dl, dr);
            dl *= 0.6f * gains[Jarre::Drums]; dr *= 0.6f * gains[Jarre::Drums];
            float fl = 0.0f, fr = 0.0f;
            for (auto& f : fx)
                if (f.active)
                {
                    float a, bb;
                    f.render (a, bb, fxAmounts);
                    fl += a; fr += bb;
                }
            fl *= 0.5f * gains[Jarre::Fx]; fr *= 0.5f * gains[Jarre::Fx];

            float L = padL + s * 0.8f + b + ld * 0.9f + dl + fl;
            float R = padR + s * 1.0f + b + ld * 1.0f + dr + fr;

            const float echoIn = (padL + padR) * 0.08f + s * 0.45f + ld * 0.5f + (dl + dr) * 0.04f + (fl + fr) * 0.3f;
            float el, er;
            echo.process (echoIn, echoSamples, 0.15f + c.echoFeedback * 0.75f, el, er);
            L += el * c.echoMix;
            R += er * c.echoMix;

            const float revIn = (padL + padR) * 0.3f + s * 0.25f + ld * 0.45f + (dl + dr) * 0.1f + (fl + fr) * 0.4f + (el + er) * 0.2f;
            revL[(size_t) i] = revIn;
            revR[(size_t) i] = revIn;
            outL[i] = L;
            if (outR != nullptr) outR[i] = R;

            peaks[Jarre::Pad] = juce::jmax (peaks[Jarre::Pad], std::abs (padL));
            peaks[Jarre::Seq] = juce::jmax (peaks[Jarre::Seq], std::abs (s));
            peaks[Jarre::Bass] = juce::jmax (peaks[Jarre::Bass], std::abs (b));
            peaks[Jarre::Lead] = juce::jmax (peaks[Jarre::Lead], std::abs (ld));
            peaks[Jarre::Drums] = juce::jmax (peaks[Jarre::Drums], std::abs (dl));
            peaks[Jarre::Fx] = juce::jmax (peaks[Jarre::Fx], std::abs (fl));
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

        for (int l = 0; l < Jarre::numLanes; ++l)
            laneLevel[(size_t) l].store (juce::jmax (peaks[(size_t) l], laneLevel[(size_t) l].load() * 0.9f));
        outLevel.store (juce::jmax (outPeak, outLevel.load() * 0.9f));
        outLevelR.store (juce::jmax (outPeakR, outLevelR.load() * 0.9f));
    }

    double sr = 48000.0;
    const Jarre::Song* song = nullptr;
    bool wasPlaying = false, jumped = true;
    double expectedBeat = 0.0;
    juce::int64 counter = 0;

    std::array<Dsp::PadVoice, 14> pads;
    Dsp::SeqVoice seq;
    Dsp::BassVoice bass;
    Dsp::LeadVoice lead;
    Dsp::DrumKit drums;
    std::array<Dsp::FxVoice, 6> fx;
    Dsp::Ensemble ensemble;
    Dsp::Phaser phaser;
    Dsp::Echo echo;
    Dsp::Noise noise;
    juce::Reverb reverb;
    std::vector<float> revL, revR;
    std::vector<Event> events;
    std::array<float, Jarre::numLanes> gains {};
};
