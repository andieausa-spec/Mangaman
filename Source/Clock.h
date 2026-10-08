#pragma once

#include <JuceHeader.h>

// Gemeinsamer Takt: Sequenzer, Drums und (wenn einer von beiden läuft) der Arpeggiator zählen auf derselben Viertel-Uhr.
// Spielt der Host, ist das seine Song-Position, sonst eine eigene Uhr, die beim Start des Ersten bei null beginnt.
namespace Clock
{
    struct Block
    {
        double ppq = 0.0;          // Position in Vierteln am Blockanfang
        double perSample = 0.0;    // Viertel pro Sample
        double bpm = 120.0;
        bool sync = false;         // läuft Sequenzer oder Drums?

        double sampleAt (double position) const { return perSample > 0.0 ? (position - ppq) / perSample : 1.0e12; }
    };

    inline long long floorDiv2 (long long k) { return k >= 0 ? k / 2 : -((1 - k) / 2); }

    // Schritt-Nummer mit Shuffle: jeder zweite Schritt eines Paares kommt später
    inline long long index (double b, double L, double sw)
    {
        const double pair = std::floor (b / (2.0 * L));
        const double f = b - pair * 2.0 * L;
        return (long long) pair * 2 + (f >= L * (1.0 + sw) ? 1 : 0);
    }
    inline double start (long long k, double L, double sw)
    {
        return (double) floorDiv2 (k) * 2.0 * L + ((k & 1) != 0 ? L * (1.0 + sw) : 0.0);
    }
    inline double duration (long long k, double L, double sw) { return (k & 1) != 0 ? L * (1.0 - sw) : L * (1.0 + sw); }

    // Folgt einem Raster aus Schritten der Länge L. k ist der zuletzt gespielte Schritt.
    struct Follower
    {
        long long k = 0;
        bool joined = false;

        // Einsteigen: ist der laufende Schritt noch frisch (unter 30 %), wird er sofort gespielt, sonst der nächste
        void join (double b, double L, double sw)
        {
            const auto k0 = index (b, L, sw);
            k = b - start (k0, L, sw) < 0.3 * L ? k0 - 1 : k0;
            joined = true;
        }

        // am Blockanfang: neu einsteigen, wenn noch nicht dabei oder der Host gesprungen ist
        void begin (const Block& blk, double L, double sw)
        {
            if (! joined)
            {
                join (blk.ppq, L, sw);
                return;
            }
            const auto kb = index (blk.ppq, L, sw);
            if (kb < k || kb > k + 1)
                join (blk.ppq, L, sw);
        }

        // Sample-Position des nächsten Schritts im Block
        double nextSample (const Block& blk, double L, double sw) const { return blk.sampleAt (start (k + 1, L, sw)); }
    };
}
