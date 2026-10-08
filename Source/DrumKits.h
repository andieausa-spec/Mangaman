#pragma once

#include <JuceHeader.h>

// Rhythmus: zehn Drumcomputer im Stil der Klassiker, synthetisch nachgebaut (keine Samples).
// Jede Stimme wird aus Oszillatoren, Rauschen und Filtern erzeugt.
namespace Drums
{
    constexpr int numTracks = 12;
    constexpr int numSteps = 16;

    // Klangarten
    enum Type { DKick, DSnare, DClap, DRim, DHat, DTom, DBell, DClave, DShaker, DGuiro };

    // Bauplan einer Stimme. Kick/Tom: f Grundton, pr Tonhöhen-Sprung, pt Dauer des Sprungs, d Ausklang,
    // cl Klick, nz Fell-Rauschen, dr Sättigung. Snare: f/f2 Kesseltöne, td/nd Ausklang Ton/Rauschen,
    // nf/nq Rauschfilter, sn Rauschanteil. Hat/Becken: fr Metall-Oszillatoren, hc/hp Filter, nm Rauschanteil.
    struct VoiceDef
    {
        int type = DKick;
        float fv = 100, f2v = 200, prv = 1, ptv = 0.02f, dv = 0.2f, clv = 0, nzv = 0, drv = 0;
        float tdv = 0.05f, ndv = 0.1f, nfv = 1000, nqv = 0.7f, snv = 0.5f;
        float hcv = 8000, hpv = 2000, nmv = 0, attv = 0.005f, gv = 1;
        int numFr = 0, scrapeCount = 8;
        float frv[6] {};

        VoiceDef& f (float v)   { fv = v; return *this; }
        VoiceDef& f2 (float v)  { f2v = v; return *this; }
        VoiceDef& pr (float v)  { prv = v; return *this; }
        VoiceDef& pt (float v)  { ptv = v; return *this; }
        VoiceDef& d (float v)   { dv = v; return *this; }
        VoiceDef& cl (float v)  { clv = v; return *this; }
        VoiceDef& nz (float v)  { nzv = v; return *this; }
        VoiceDef& dr (float v)  { drv = v; return *this; }
        VoiceDef& td (float v)  { tdv = v; return *this; }
        VoiceDef& nd (float v)  { ndv = v; return *this; }
        VoiceDef& nf (float v)  { nfv = v; return *this; }
        VoiceDef& nq (float v)  { nqv = v; return *this; }
        VoiceDef& sn (float v)  { snv = v; return *this; }
        VoiceDef& hc (float v)  { hcv = v; return *this; }
        VoiceDef& hp (float v)  { hpv = v; return *this; }
        VoiceDef& nm (float v)  { nmv = v; return *this; }
        VoiceDef& att (float v) { attv = v; return *this; }
        VoiceDef& g (float v)   { gv = v; return *this; }
        VoiceDef& scrapes (int n) { scrapeCount = n; return *this; }
        VoiceDef& fr (std::initializer_list<float> list)
        {
            numFr = 0;
            for (float x : list)
                if (numFr < 6)
                    frv[numFr++] = x;
            return *this;
        }
    };

    inline VoiceDef V (int type) { VoiceDef v; v.type = type; return v; }

    // bits/hold: grobe Auflösung wie bei den frühen Sample-Maschinen, drive: Sättigung des Busses
    struct Kit
    {
        const char* name;
        int bits, hold;
        float drive;
        std::array<VoiceDef, numTracks> voices;
    };

    inline const std::array<Kit, 10>& kits()
    {
        static const std::array<Kit, 10> list {{
        { "808", 0, 1, 0.15f, {
            V (DKick).f (47.0f).pr (2.4f).pt (0.03f).d (0.62f).cl (0.12f).g (3.25f),
            V (DSnare).f (238.0f).f2 (476.0f).pr (1.15f).td (0.06f).nd (0.11f).nf (3200.0f).nq (0.6f).sn (0.55f).g (2.89f),
            V (DClap).nf (1100.0f).nq (1.6f).d (0.16f).g (3.2f),
            V (DRim).f (1720.0f).f2 (490.0f).d (0.018f).g (1.5f),
            V (DHat).fr ({ 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f }).hc (9000.0f).hp (7000.0f).d (0.04f).nm (0.1f).g (3.6f),
            V (DHat).fr ({ 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f }).hc (8500.0f).hp (6500.0f).d (0.32f).nm (0.1f).g (2.67f),
            V (DTom).f (88.0f).pr (1.25f).pt (0.06f).d (0.42f).nz (0.04f).g (2.35f),
            V (DTom).f (128.0f).pr (1.25f).pt (0.05f).d (0.36f).nz (0.04f).g (2.25f),
            V (DTom).f (176.0f).pr (1.25f).pt (0.045f).d (0.3f).nz (0.04f).g (2.15f),
            V (DBell).fr ({ 540.0f, 800.0f }).hc (2400.0f).d (0.3f).g (0.67f),
            V (DHat).fr ({ 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f }).hc (6500.0f).hp (4500.0f).d (1.4f).nm (0.25f).g (1.83f),
            V (DClave).f (2500.0f).d (0.028f).g (1.12f) } },
        { "909", 0, 1, 0.3f, {
            V (DKick).f (52.0f).pr (4.2f).pt (0.028f).d (0.3f).cl (0.55f).dr (0.45f).g (2.6f),
            V (DSnare).f (190.0f).f2 (335.0f).pr (1.35f).td (0.05f).nd (0.17f).nf (5200.0f).nq (0.5f).sn (0.68f).g (2.89f),
            V (DClap).nf (1250.0f).nq (1.3f).d (0.22f).g (3.0f),
            V (DRim).f (1580.0f).f2 (455.0f).d (0.025f).nz (0.2f).g (1.31f),
            V (DHat).fr ({ 263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f }).hc (10500.0f).hp (8000.0f).d (0.05f).nm (0.35f).g (1.97f),
            V (DHat).fr ({ 263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f }).hc (10000.0f).hp (7500.0f).d (0.45f).nm (0.35f).g (2.07f),
            V (DTom).f (82.0f).pr (1.6f).pt (0.05f).d (0.36f).nz (0.12f).g (2.35f),
            V (DTom).f (118.0f).pr (1.6f).pt (0.045f).d (0.32f).nz (0.12f).g (2.25f),
            V (DTom).f (165.0f).pr (1.6f).pt (0.04f).d (0.28f).nz (0.12f).g (2.15f),
            V (DBell).fr ({ 560.0f, 835.0f }).hc (2800.0f).d (0.22f).g (0.73f),
            V (DHat).fr ({ 263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f }).hc (7000.0f).hp (5000.0f).d (1.6f).nm (0.55f).g (1.18f),
            V (DHat).fr ({ 3100.0f, 3480.0f, 4125.0f, 5200.0f, 6030.0f, 7400.0f }).hc (5600.0f).hp (3500.0f).d (0.9f).nm (0.2f).g (0.8f) } },
        { "606", 0, 1, 0.1f, {
            V (DKick).f (64.0f).pr (2.0f).pt (0.018f).d (0.16f).cl (0.25f).g (2.84f),
            V (DSnare).f (260.0f).f2 (440.0f).pr (1.1f).td (0.035f).nd (0.09f).nf (6000.0f).nq (0.5f).sn (0.75f).g (3.05f),
            V (DClap).nf (1500.0f).nq (1.2f).d (0.12f).g (2.93f),
            V (DRim).f (1900.0f).f2 (620.0f).d (0.015f).g (1.23f),
            V (DHat).fr ({ 245.0f, 306.0f, 365.0f, 415.0f, 437.0f, 619.0f }).hc (11000.0f).hp (8500.0f).d (0.035f).nm (0.25f).g (2.88f),
            V (DHat).fr ({ 245.0f, 306.0f, 365.0f, 415.0f, 437.0f, 619.0f }).hc (10500.0f).hp (8000.0f).d (0.24f).nm (0.25f).g (2.4f),
            V (DTom).f (110.0f).pr (1.15f).pt (0.03f).d (0.22f).g (2.36f),
            V (DTom).f (150.0f).pr (1.15f).pt (0.03f).d (0.2f).g (2.27f),
            V (DTom).f (205.0f).pr (1.15f).pt (0.03f).d (0.18f).g (2.17f),
            V (DBell).fr ({ 610.0f, 920.0f }).hc (3000.0f).d (0.15f).g (0.6f),
            V (DHat).fr ({ 245.0f, 306.0f, 365.0f, 415.0f, 437.0f, 619.0f }).hc (8000.0f).hp (6000.0f).d (0.9f).nm (0.4f).g (1.5f),
            V (DShaker).hc (7500.0f).att (0.008f).d (0.06f).g (0.8f) } },
        { "707", 7, 2, 0.2f, {
            V (DKick).f (56.0f).pr (3.2f).pt (0.022f).d (0.24f).cl (0.7f).nz (0.1f).dr (0.3f).g (2.3f),
            V (DSnare).f (205.0f).f2 (360.0f).pr (1.25f).td (0.05f).nd (0.15f).nf (4500.0f).nq (0.55f).sn (0.6f).g (3.12f),
            V (DClap).nf (1350.0f).nq (1.5f).d (0.2f).g (3.07f),
            V (DRim).f (1650.0f).f2 (520.0f).d (0.022f).nz (0.15f).g (1.24f),
            V (DHat).fr ({ 263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f }).hc (9500.0f).hp (7000.0f).d (0.045f).nm (0.65f).g (1.32f),
            V (DHat).fr ({ 263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f }).hc (9000.0f).hp (6500.0f).d (0.4f).nm (0.65f).g (1.2f),
            V (DTom).f (90.0f).pr (1.5f).pt (0.04f).d (0.32f).nz (0.2f).g (2.35f),
            V (DTom).f (125.0f).pr (1.5f).pt (0.04f).d (0.3f).nz (0.2f).g (2.12f),
            V (DTom).f (172.0f).pr (1.5f).pt (0.035f).d (0.27f).nz (0.2f).g (2.02f),
            V (DBell).fr ({ 585.0f, 870.0f }).hc (2700.0f).d (0.2f).g (0.68f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (7200.0f).hp (4800.0f).d (1.5f).nm (0.6f).g (1.03f),
            V (DShaker).hc (8500.0f).att (0.002f).d (0.12f).fr ({ 4200.0f, 5600.0f, 7300.0f, 8100.0f, 9300.0f, 10500.0f }).g (1.11f) } },
        { "CR-78", 0, 1, 0.05f, {
            V (DKick).f (62.0f).pr (1.25f).pt (0.012f).d (0.18f).cl (0.05f).g (3.4f),
            V (DSnare).f (300.0f).f2 (520.0f).pr (1.0f).td (0.04f).nd (0.08f).nf (2600.0f).nq (0.9f).sn (0.5f).g (2.53f),
            V (DClap).nf (900.0f).nq (2.2f).d (0.08f).g (2.86f),
            V (DRim).f (2100.0f).f2 (760.0f).d (0.012f).g (1.09f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (9500.0f).hp (7500.0f).d (0.03f).nm (0.85f).g (1.15f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (9000.0f).hp (7000.0f).d (0.18f).nm (0.85f).g (0.95f),
            V (DTom).f (180.0f).pr (1.08f).pt (0.02f).d (0.16f).g (2.38f),
            V (DTom).f (240.0f).pr (1.08f).pt (0.02f).d (0.14f).g (2.29f),
            V (DTom).f (330.0f).pr (1.08f).pt (0.02f).d (0.12f).g (2.19f),
            V (DBell).fr ({ 720.0f, 1080.0f }).hc (3200.0f).d (0.12f).g (0.68f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (7500.0f).hp (5500.0f).d (0.7f).nm (0.8f).g (0.86f),
            V (DGuiro).hc (3200.0f).d (0.2f).scrapes (9).g (0.48f) } },
        { "LinnDrum", 9, 1, 0.15f, {
            V (DKick).f (58.0f).pr (2.2f).pt (0.02f).d (0.22f).cl (0.8f).nz (0.35f).dr (0.2f).g (2.24f),
            V (DSnare).f (180.0f).f2 (290.0f).pr (1.2f).td (0.08f).nd (0.24f).nf (3600.0f).nq (0.4f).sn (0.62f).g (3.0f),
            V (DClap).nf (1050.0f).nq (1.0f).d (0.24f).g (3.38f),
            V (DRim).f (1450.0f).f2 (380.0f).d (0.03f).nz (0.35f).g (1.32f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (9000.0f).hp (6000.0f).d (0.06f).nm (0.75f).g (1.09f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (8500.0f).hp (5500.0f).d (0.55f).nm (0.75f).g (1.02f),
            V (DTom).f (78.0f).pr (1.4f).pt (0.06f).d (0.45f).nz (0.25f).g (2.22f),
            V (DTom).f (108.0f).pr (1.4f).pt (0.055f).d (0.4f).nz (0.25f).g (2.12f),
            V (DTom).f (150.0f).pr (1.4f).pt (0.05f).d (0.35f).nz (0.25f).g (1.9f),
            V (DBell).fr ({ 565.0f, 845.0f }).hc (2600.0f).d (0.25f).g (0.73f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (6000.0f).hp (3800.0f).d (1.8f).nm (0.7f).g (0.86f),
            V (DShaker).hc (6800.0f).att (0.012f).d (0.08f).g (0.82f) } },
        { "DMX", 8, 1, 0.4f, {
            V (DKick).f (55.0f).pr (2.8f).pt (0.025f).d (0.26f).cl (0.9f).nz (0.25f).dr (0.55f).g (2.58f),
            V (DSnare).f (195.0f).f2 (310.0f).pr (1.3f).td (0.07f).nd (0.2f).nf (3000.0f).nq (0.45f).sn (0.58f).g (2.65f),
            V (DClap).nf (1150.0f).nq (1.2f).d (0.2f).g (3.29f),
            V (DRim).f (1500.0f).f2 (420.0f).d (0.026f).nz (0.3f).g (1.4f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (8500.0f).hp (6000.0f).d (0.05f).nm (0.7f).g (1.2f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (8000.0f).hp (5500.0f).d (0.45f).nm (0.7f).g (1.13f),
            V (DTom).f (82.0f).pr (1.5f).pt (0.05f).d (0.38f).nz (0.2f).g (2.35f),
            V (DTom).f (112.0f).pr (1.5f).pt (0.05f).d (0.34f).nz (0.2f).g (2.25f),
            V (DTom).f (156.0f).pr (1.5f).pt (0.045f).d (0.3f).nz (0.2f).g (2.02f),
            V (DBell).fr ({ 550.0f, 820.0f }).hc (2500.0f).d (0.24f).g (0.68f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (6200.0f).hp (4000.0f).d (1.6f).nm (0.65f).g (0.96f),
            V (DShaker).hc (7800.0f).att (0.002f).d (0.14f).fr ({ 4100.0f, 5300.0f, 6900.0f, 7700.0f, 8800.0f, 9900.0f }).g (0.9f) } },
        { "Drumulator", 8, 2, 0.25f, {
            V (DKick).f (60.0f).pr (2.0f).pt (0.02f).d (0.25f).cl (0.75f).nz (0.4f).dr (0.3f).g (2.41f),
            V (DSnare).f (175.0f).f2 (280.0f).pr (1.15f).td (0.09f).nd (0.28f).nf (3300.0f).nq (0.4f).sn (0.66f).g (3.0f),
            V (DClap).nf (1000.0f).nq (1.1f).d (0.22f).g (3.67f),
            V (DRim).f (1400.0f).f2 (360.0f).d (0.032f).nz (0.4f).g (1.49f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (8000.0f).hp (5500.0f).d (0.07f).nm (0.8f).g (1.04f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (7600.0f).hp (5000.0f).d (0.6f).nm (0.8f).g (0.93f),
            V (DTom).f (72.0f).pr (1.35f).pt (0.07f).d (0.5f).nz (0.3f).g (2.22f),
            V (DTom).f (100.0f).pr (1.35f).pt (0.065f).d (0.44f).nz (0.3f).g (2.0f),
            V (DTom).f (140.0f).pr (1.35f).pt (0.06f).d (0.38f).nz (0.3f).g (1.9f),
            V (DBell).fr ({ 575.0f, 860.0f }).hc (2600.0f).d (0.26f).g (0.63f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (5800.0f).hp (3600.0f).d (2.0f).nm (0.75f).g (0.91f),
            V (DClave).f (2300.0f).d (0.03f).g (1.05f) } },
        { "SDS-V", 0, 1, 0.35f, {
            V (DKick).f (52.0f).pr (3.0f).pt (0.07f).d (0.4f).cl (0.4f).nz (0.15f).dr (0.35f).g (2.82f),
            V (DSnare).f (210.0f).f2 (320.0f).pr (1.9f).td (0.12f).nd (0.2f).nf (2400.0f).nq (0.7f).sn (0.5f).g (2.81f),
            V (DClap).nf (1300.0f).nq (1.4f).d (0.2f).g (2.75f),
            V (DRim).f (1300.0f).f2 (600.0f).d (0.04f).nz (0.2f).g (1.24f),
            V (DHat).fr ({ 263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f }).hc (9000.0f).hp (6500.0f).d (0.06f).nm (0.4f).g (1.71f),
            V (DHat).fr ({ 263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f }).hc (8500.0f).hp (6000.0f).d (0.45f).nm (0.4f).g (1.7f),
            V (DTom).f (75.0f).pr (2.6f).pt (0.2f).d (0.6f).nz (0.25f).g (2.33f),
            V (DTom).f (110.0f).pr (2.6f).pt (0.18f).d (0.55f).nz (0.25f).g (2.11f),
            V (DTom).f (160.0f).pr (2.6f).pt (0.16f).d (0.5f).nz (0.25f).g (2.01f),
            V (DBell).fr ({ 640.0f, 960.0f }).hc (2900.0f).d (0.2f).g (0.6f),
            V (DHat).fr ({ 263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f }).hc (6500.0f).hp (4200.0f).d (1.5f).nm (0.5f).g (1.18f),
            V (DTom).f (240.0f).pr (3.2f).pt (0.12f).d (0.35f).nz (0.2f).g (1.2f) } },
        { "Mini Pops", 0, 1, 0.05f, {
            V (DKick).f (72.0f).pr (1.15f).pt (0.01f).d (0.14f).cl (0.02f).g (3.54f),
            V (DSnare).f (330.0f).f2 (560.0f).pr (1.0f).td (0.03f).nd (0.07f).nf (2200.0f).nq (1.0f).sn (0.45f).g (2.39f),
            V (DClap).nf (800.0f).nq (2.5f).d (0.07f).g (2.92f),
            V (DRim).f (2300.0f).f2 (900.0f).d (0.01f).g (1.07f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (8000.0f).hp (6500.0f).d (0.028f).nm (0.95f).g (0.98f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (7500.0f).hp (6000.0f).d (0.14f).nm (0.95f).g (0.98f),
            V (DTom).f (200.0f).pr (1.05f).pt (0.02f).d (0.14f).g (2.4f),
            V (DTom).f (265.0f).pr (1.05f).pt (0.02f).d (0.12f).g (2.31f),
            V (DTom).f (350.0f).pr (1.05f).pt (0.02f).d (0.1f).g (2.22f),
            V (DBell).fr ({ 800.0f, 1200.0f }).hc (3600.0f).d (0.1f).g (0.6f),
            V (DHat).fr ({ 317.0f, 421.0f, 553.0f, 709.0f, 797.0f, 1123.0f }).hc (7000.0f).hp (5000.0f).d (0.5f).nm (0.9f).g (0.72f),
            V (DShaker).hc (6000.0f).att (0.015f).d (0.05f).g (0.74f) } }
        }};
        return list;
    }

    inline float softclip (float x)
    {
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float sine (double phase) { return (float) std::sin (phase * juce::MathConstants<double>::twoPi); }

    // Zustandsvariables Filter (TPT), liefert Tief-, Band- und Hochpass
    struct SVF
    {
        float s1 = 0, s2 = 0, lp = 0, bp = 0, hp = 0, g = 0.1f, k = 1.4f, a1 = 0.5f;

        void set (float fc, float q, double sr)
        {
            g = (float) std::tan (juce::MathConstants<double>::pi * juce::jmin ((double) fc, sr * 0.45) / sr);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
        }
        void run (float x)
        {
            const float v3 = x - s2, v1 = a1 * (s1 + g * v3), v2 = s2 + g * v1;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
        void reset() { s1 = s2 = 0.0f; }
    };

    // Eine Spur: spielt ihren Klang nach dem Bauplan
    class Voice
    {
    public:
        bool on = false;
        float vel = 1.0f, gain = 1.0f;

        // tune in Halbtönen, decay −1…1 (Ausklang ×¼ … ×4)
        void trigger (const VoiceDef& def, float tune, float decay, float velocity, double sr)
        {
            sampleRate = sr;
            const float rt = std::pow (2.0f, tune / 12.0f), dm = std::pow (2.0f, decay * 2.0f);
            type = def.type; vel = velocity; on = true; s = 0; gain = def.gv;
            a = 1.0f;
            ka = coef (juce::jmax (0.004f, def.dv * dm));
            switch (def.type)
            {
                case DKick: case DTom:
                    f = def.fv * rt; pr = def.prv; pe = 1.0f; kp = coef (def.ptv);
                    cl = def.clv; ce = 1.0f; kc = coef (0.0025f); nz = def.nzv; dr = def.drv;
                    ph[0] = 0.25; f1.set (juce::jmin (9000.0f, def.fv * rt * 40.0f), 0.8f, sr); f1.reset();
                    break;
                case DSnare:
                    f = def.fv * rt; f2 = def.f2v * rt; pr = def.prv; pe = 1.0f; kp = coef (0.012f);
                    ka = coef (def.tdv * dm); b = 1.0f; kb = coef (def.ndv * dm);
                    sn = def.snv; f1.set (def.nfv * rt, def.nqv, sr); f1.reset(); ph[0] = ph[1] = 0.0;
                    break;
                case DClap:
                    f1.set (def.nfv * rt, def.nqv, sr); f1.reset(); ce = 1.0f; kc = coef (0.0032f);
                    spacing = juce::roundToInt (0.0105 * sr); bursts = 4;
                    break;
                case DRim:
                    f = def.fv * rt; f2 = def.f2v * rt; ph[0] = ph[1] = 0.0; nz = def.nzv; f2f.set (380.0f, 0.7f, sr); f2f.reset();
                    break;
                case DHat: case DBell:
                {
                    const float sq = std::sqrt (rt);
                    numFr = def.numFr;
                    for (int i = 0; i < numFr; ++i) { fr[i] = def.frv[i] * rt / sr; ph[i] = rng.nextDouble(); }
                    nm = def.nmv;
                    f1.set (def.hcv * sq, def.type == DBell ? 3.0f : 1.1f, sr); f1.reset();
                    f2f.set (def.hpv * sq, 0.7f, sr); f2f.reset();
                    if (def.type == DBell) { b = 1.0f; kb = coef (0.012f); }
                    break;
                }
                case DClave:
                    f = def.fv * rt; ph[0] = 0.0;
                    break;
                case DShaker:
                    f1.set (def.hcv * std::sqrt (rt), 0.9f, sr); f1.reset();
                    attack = juce::jmax (1.0f, def.attv * (float) sr); a = 0.0f;
                    kat = coef (juce::jmax (0.004f, def.dv * dm));
                    numFr = def.numFr;
                    for (int i = 0; i < numFr; ++i) { fr[i] = def.frv[i] * rt / sr; ph[i] = rng.nextDouble(); }
                    break;
                case DGuiro:
                    f1.set (def.hcv * rt, 4.0f, sr); f1.reset(); length = def.dv * dm * (float) sr; scrapes = def.scrapeCount;
                    break;
                default: break;
            }
        }

        // Hi-Hat zu würgt die offene ab
        void choke() { if (on) ka = juce::jmin (ka, coef (0.006f)); }

        float next()
        {
            const int i = s++;
            float x = 0.0f;
            switch (type)
            {
                case DKick: case DTom:
                {
                    const float fr0 = f * (1.0f + (pr - 1.0f) * pe); pe *= kp;
                    ph[0] += fr0 / sampleRate; if (ph[0] >= 1.0) ph[0] -= 1.0;
                    float y = sine (ph[0]);
                    if (dr > 0.0f) y = softclip (y * (1.0f + dr * 3.0f)) / (1.0f + dr * 0.5f);
                    x = y * a;
                    if (nz > 0.0f) { f1.run (noise()); x += f1.lp * nz * a * a; }
                    if (cl > 0.0f && ce > 0.001f) { x += (noise() * 0.6f + (i < 24 ? 0.8f : 0.0f)) * cl * ce; ce *= kc; }
                    a *= ka;
                    break;
                }
                case DSnare:
                {
                    const float bendNow = 1.0f + (pr - 1.0f) * pe; pe *= kp;
                    ph[0] += f * bendNow / sampleRate; ph[1] += f2 * (1.0f + (pr - 1.0f) * pe) / sampleRate;
                    const float tone = sine (ph[0]) + 0.55f * sine (ph[1]);
                    f1.run (noise());
                    x = tone * (1.0f - sn) * a * 0.8f + (f1.hp * 0.6f + f1.bp * 0.7f) * sn * b * 1.3f;
                    a *= ka; b *= kb;
                    if (a < 1.0e-4f && b < 1.0e-4f) on = false;
                    return x;
                }
                case DClap:
                {
                    f1.run (noise());
                    const int burst = spacing * bursts;
                    float e;
                    if (i < burst) { if (i % spacing == 0) ce = 1.0f; e = ce; ce *= kc; }
                    else { e = a * 0.85f; a *= ka; }
                    x = f1.bp * e * 1.6f;
                    if (i >= burst && a < 1.0e-4f) on = false;
                    return x;
                }
                case DRim:
                {
                    ph[0] += f / sampleRate; ph[1] += f2 / sampleRate;
                    const float y = sine (ph[0]) * 0.8f + sine (ph[1]) * 0.6f + (i < 40 ? noise() * 0.5f : 0.0f) + noise() * nz * 0.4f;
                    f2f.run (y); x = f2f.hp * a; a *= ka;
                    break;
                }
                case DHat: case DBell:
                {
                    float m = 0.0f;
                    for (int k = 0; k < numFr; ++k) { ph[k] += fr[k]; if (ph[k] >= 1.0) ph[k] -= 1.0; m += ph[k] < 0.5 ? 1.0f : -1.0f; }
                    m /= (float) juce::jmax (1, numFr);
                    m = m * (1.0f - nm) + noise() * nm;
                    f1.run (m);
                    if (type == DBell) { x = f1.bp * (a * 0.55f + b * 0.6f) * 1.6f; b *= kb; }
                    else { f2f.run (f1.bp); x = f2f.hp * a * 2.2f; }
                    a *= ka;
                    break;
                }
                case DClave:
                    ph[0] += f / sampleRate;
                    x = (sine (ph[0]) + 0.2f * sine (ph[0] * 2.7)) * a; a *= ka;
                    break;
                case DShaker:
                {
                    float src = noise();
                    if (numFr > 0)
                    {
                        float m = 0.0f;
                        for (int k = 0; k < numFr; ++k) { ph[k] += fr[k]; if (ph[k] >= 1.0) ph[k] -= 1.0; m += ph[k] < 0.5 ? 1.0f : -1.0f; }
                        src = src * 0.4f + m / (float) numFr * 0.8f;
                    }
                    f1.run (src);
                    if ((float) i < attack) a = (float) i / attack; else a *= kat;
                    x = f1.hp * a * 1.4f;
                    if ((float) i > attack && a < 1.0e-4f) on = false;
                    return x;
                }
                case DGuiro:
                {
                    f1.run (noise());
                    const float u = (float) i / length;
                    if (u >= 1.0f) { on = false; return 0.0f; }
                    const float sc = std::fmod (u * (float) scrapes, 1.0f);
                    return f1.bp * (sc < 0.35f ? 1.0f : 0.15f) * (1.0f - u) * 2.2f;
                }
                default: break;
            }
            if (a < 1.0e-4f) on = false;
            return x;
        }

        // verzögerter Einsatz (Humanize) und Wirbel (Ratschen)
        int wait = -1;
        float waitVel = 0.0f;
        int rollEvery = 0, rollLeft = 0, rollIn = 0;
        float rollVel = 0.0f;

    private:
        float coef (float seconds) const { return std::exp (-1.0f / (seconds * (float) sampleRate)); }
        float noise()
        {
            seed = seed * 1664525u + 1013904223u;
            return (float) seed / 2147483648.0f - 1.0f;
        }

        double sampleRate = 44100.0;
        int type = DKick, s = 0, numFr = 0, spacing = 400, bursts = 4, scrapes = 8;
        double ph[6] {}, fr[6] {};
        float f = 100, f2 = 200, pr = 1, pe = 1, kp = 0.99f, a = 1, ka = 0.999f, b = 1, kb = 0.999f;
        float cl = 0, ce = 1, kc = 0.99f, nz = 0, dr = 0, sn = 0.5f, nm = 0, attack = 1, kat = 0.999f, length = 1000;
        SVF f1, f2f;
        juce::uint32 seed = (juce::uint32) juce::Random::getSystemRandom().nextInt();
        juce::Random rng;
    };
}
