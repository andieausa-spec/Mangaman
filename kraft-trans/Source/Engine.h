#pragma once

#include "Voices.h"

// Spielt einen generierten Song ab: holt die Noten des laufenden Blocks aus dem Song,
// startet die Stimmen sampelgenau und mischt sieben Spuren mit Echo und Hall. Die Roboterstimme bekommt die Laute der Silbe.
class Engine
{
public:
    struct Controls
    {
        double bpm = 120.0;
        bool loop = true;
        int transpose = 0;
        std::array<float, Kt::numLanes> laneGain { 1, 1, 1, 1, 1, 1, 1 };

        int padType = 0; float padBright = 0.5f, padAttack = 0.4f, padEnsemble = 0.6f, padVowel = 0.4f;
        float seqCutoff = 0.5f, seqReso = 0.45f, seqEnv = 0.5f, seqDecay = 0.3f; int seqWave = 0;
        float bassCutoff = 0.4f, bassDecay = 0.35f, bassSub = 0.4f, bassDrive = 0.3f;
        float leadGlide = 0.3f, leadVibrato = 0.4f, leadBright = 0.55f; int leadWave = 0;
        int drumKit = 0; float drumTone = 0.5f, drumSwing = 0.0f;
        float fxNoise = 0.5f, fxMachine = 0.6f, fxSignal = 0.5f;
        bool vocalOn = true;
        int voiceType = 0;
        float vocalMelody = 1.0f, vocalBands = 0.6f, vocalCarrier = 0.5f, vocalCrush = 0.2f, vocalMetal = 0.25f;
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
        voice.prepare (sr);
        harmony.prepare (sr);
        shownBands = -1.0f;
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
    void setSong (const Kt::Song* s) { song = s; jumped = true; }
    const Kt::Song* getSong() const { return song; }

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
            for (auto& f : fx) if (f.active && f.type == Kt::Static) f.total = juce::jmin (f.total, f.pos + (juce::int64) (sr * 1.5));
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

    std::array<std::atomic<float>, Kt::numLanes> laneLevel {};
    std::atomic<float> outLevel { 0.0f }, outLevelR { 0.0f };

private:
    struct Event { int offset; int lane; Kt::Note note; double remaining; };

    static constexpr int chunk = 256;

    void collect (double from, double to, double beatsPerSample, int baseOffset)
    {
        for (int lane = 0; lane < Kt::numLanes; ++lane)
        {
            const auto& v = song->notes[(size_t) lane];
            auto it = std::lower_bound (v.begin(), v.end(), from, [] (const Kt::Note& n, double b) { return n.start < b; });
            for (; it != v.end() && it->start < to; ++it)
                events.push_back ({ baseOffset + (int) ((it->start - from) / beatsPerSample), lane, *it, it->length });
        }
    }

    // Nach einem Sprung: lange Töne (Flächen, Melodie, Atmosphäre), die gerade klingen müssten, sofort nachholen
    void chase (double beat)
    {
        for (int lane : { (int) Kt::Pad, (int) Kt::Lead, (int) Kt::Fx })
            for (const auto& n : song->notes[(size_t) lane])
            {
                if (n.start >= beat) break;
                const double rest = n.start + n.length - beat;
                if (rest > 0.25 && (lane != Kt::Fx || n.pitch == Kt::Static || n.pitch == Kt::Train))
                    events.push_back ({ 0, lane, n, rest });
            }
    }

    void trigger (const Event& e, const Controls& c, double beatsPerSample)
    {
        const auto len = (juce::int64) juce::jmax (1.0, e.remaining / beatsPerSample);
        const int pitch = e.note.pitch + c.transpose;
        switch (e.lane)
        {
            case Kt::Pad:
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
            case Kt::Seq:
                seq.wave = c.seqWave;
                seq.start (pitch, e.note.velocity, len, sr, 0.03f + c.seqDecay * c.seqDecay * 0.8f);
                break;
            case Kt::Bass:
                bass.start (pitch, e.note.velocity, len, sr, 0.04f + c.bassDecay * 0.5f);
                break;
            case Kt::Lead:
                lead.wave = c.leadWave;
                lead.start (pitch, e.note.velocity, len, sr);
                break;
            case Kt::Vocal:
            {
                if (song == nullptr || e.note.syllable < 0 || e.note.syllable >= (int) song->syllables.size()) break;
                auto& v = e.note.harmony ? harmony : voice;
                // Melodie: 0 = monoton gesprochen, 1 = volle Tonhöhen
                const float center = 64.0f + (float) c.transpose - (e.note.harmony ? 12.0f : 0.0f);
                const float note = center + ((float) pitch - center) * c.vocalMelody;
                v.start (note, e.note.velocity, len, sr, song->syllables[(size_t) e.note.syllable], (juce::uint32) (++counter * 2654435761u));
                break;
            }
            case Kt::Drums:
                drums.trigger (e.note.pitch, e.note.velocity, c.drumKit, c.drumTone);
                break;
            case Kt::Fx:
            {
                Dsp::FxVoice* best = nullptr;
                for (auto& f : fx) if (! f.active) { best = &f; break; }
                if (best == nullptr) best = &fx[0];
                const auto full = (juce::int64) (e.note.length / beatsPerSample);
                best->start (e.note.pitch, e.note.velocity, full, full - len, sr, (juce::uint32) (++counter * 2654435761u),
                             e.note.syllable + c.transpose, 1.0 / beatsPerSample);
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
                    if ((e.lane == Kt::Drums || e.lane == Kt::Bass) && std::fmod (e.note.start * 4.0 + 0.001, 2.0) >= 1.0)
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
        std::array<float, Kt::numLanes> target = c.laneGain;
        if (! c.vocalOn) target[Kt::Vocal] = 0.0f;
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
            v->type = c.voiceType;
            v->bands = c.vocalBands;
            v->carrierBright = c.vocalCarrier;
            v->crush = c.vocalCrush;
            v->metal = c.vocalMetal;
        }
        if (std::abs (c.vocalBands - shownBands) > 0.01f)
        {
            shownBands = c.vocalBands;
            voice.setupBands();
            harmony.setupBands();
        }
        const float fxAmounts[] { c.fxNoise, c.fxMachine, c.fxSignal };
        const float echoSamples = (float) (c.echoBeats * 60.0 / c.bpm * sr);
        const float harmonyGain = c.vocalHarmony * 0.9f;

        juce::Reverb::Parameters rp;
        rp.roomSize = 0.5f + 0.48f * c.reverbSize;
        rp.damping = 0.4f;
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        reverb.setParameters (rp);

        std::array<float, Kt::numLanes> peaks {};
        float outPeak = 0.0f, outPeakR = 0.0f;
        size_t ei = 0;
        for (int i = 0; i < n; ++i)
        {
            while (ei < events.size() && events[ei].offset <= i)
                trigger (events[ei++], c, bps);

            for (int l = 0; l < Kt::numLanes; ++l)
                gains[(size_t) l] += (target[(size_t) l] - gains[(size_t) l]) * 0.002f;

            float padMono = 0.0f;
            for (auto& v : pads) if (v.active) padMono += v.render();
            if (c.padType == 1)
                padMono = choir.process (padMono, c.padVowel) * 1.3f;
            float padL, padR;
            ensemble.process (padMono * 0.45f, c.padEnsemble, padL, padR);
            padL *= gains[Kt::Pad]; padR *= gains[Kt::Pad];

            const float s = seq.render() * 0.42f * gains[Kt::Seq];
            const float b = bass.render() * 0.6f * gains[Kt::Bass];
            const float ld = lead.render() * 0.36f * gains[Kt::Lead];
            const float vMono = (voice.render() + harmony.render() * harmonyGain) * 0.9f * gains[Kt::Vocal];
            float vl, vr;
            doubler.process (vMono, c.vocalDouble, vl, vr);
            float dl, dr;
            drums.render (dl, dr);
            dl *= 0.62f * gains[Kt::Drums]; dr *= 0.62f * gains[Kt::Drums];
            float fl = 0.0f, fr = 0.0f;
            for (auto& f : fx)
                if (f.active)
                {
                    float a, bb;
                    f.render (a, bb, fxAmounts);
                    fl += a; fr += bb;
                }
            fl *= 0.5f * gains[Kt::Fx]; fr *= 0.5f * gains[Kt::Fx];

            float L = padL + s * 1.0f + b + ld * 0.85f + vl + dl + fl;
            float R = padR + s * 0.8f + b + ld * 1.0f + vr + dr + fr;

            const float echoIn = (padL + padR) * 0.05f + s * 0.35f + ld * 0.5f + vMono * c.vocalEcho * 0.7f + (fl + fr) * 0.2f
                                 + dl * (c.drumKit == 2 || c.drumKit == 5 ? 0.06f : 0.02f);
            float el, er;
            echo.process (echoIn, echoSamples, 0.12f + c.echoFeedback * 0.75f, el, er);
            L += el * c.echoMix;
            R += er * c.echoMix;

            const float revIn = (padL + padR) * 0.3f + s * 0.2f + ld * 0.4f + vMono * c.vocalReverb * 0.9f
                                + (dl + dr) * (c.drumKit == 1 || c.drumKit == 2 ? 0.22f : 0.1f) + (fl + fr) * 0.45f + (el + er) * 0.2f;
            revL[(size_t) i] = revIn;
            revR[(size_t) i] = revIn;
            outL[i] = L;
            if (outR != nullptr) outR[i] = R;

            peaks[Kt::Pad] = juce::jmax (peaks[Kt::Pad], std::abs (padL));
            peaks[Kt::Seq] = juce::jmax (peaks[Kt::Seq], std::abs (s));
            peaks[Kt::Bass] = juce::jmax (peaks[Kt::Bass], std::abs (b));
            peaks[Kt::Lead] = juce::jmax (peaks[Kt::Lead], std::abs (ld));
            peaks[Kt::Vocal] = juce::jmax (peaks[Kt::Vocal], std::abs (vMono));
            peaks[Kt::Drums] = juce::jmax (peaks[Kt::Drums], std::abs (dl));
            peaks[Kt::Fx] = juce::jmax (peaks[Kt::Fx], std::abs (fl));
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

        for (int l = 0; l < Kt::numLanes; ++l)
            laneLevel[(size_t) l].store (juce::jmax (peaks[(size_t) l], laneLevel[(size_t) l].load() * 0.9f));
        outLevel.store (juce::jmax (outPeak, outLevel.load() * 0.9f));
        outLevelR.store (juce::jmax (outPeakR, outLevelR.load() * 0.9f));
    }

    double sr = 48000.0;
    const Kt::Song* song = nullptr;
    bool wasPlaying = false, jumped = true;
    double expectedBeat = 0.0;
    juce::int64 counter = 0;

    std::array<Dsp::PadVoice, 14> pads;
    Dsp::SeqVoice seq;
    Dsp::BassVoice bass;
    Dsp::LeadVoice lead;
    Dsp::RobotVoice voice, harmony;
    float shownBands = -1.0f;
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
    std::array<float, Kt::numLanes> gains {};
};
