# TRENDY DELAY V0.1

Fettes Bandecho für modernen Dub. Von Andreas Engel, Darmstadt, Deutschland.
Audio-Effekt als AU, VST3 und Standalone (JUCE 8, CMake). Gestaltung wie TRENDY ANDY: Design-Umschalter 1980 / 2100, Taster-Reiter, Querformat ohne Scrollen.

## Seiten

- **Echo**: Modus (Einfach, Ping-Pong, Drei Koepfe 1:2:3, Breit), Sync mit Teilung (1/32 bis 1/1, triolisch und punktiert; Standard 1/8 punktiert), freie Zeit 10 ms bis 2 s.
  Schleife: Feedback bis 120 % (Selbstoszillation), Low Cut, High Cut mit Resonanz. Die Kurve rechts zeigt, wie viel pro Umlauf zurückkommt; über der Linie schwingt das Band selbst.
- **Band**: Saettigung (Bandkompression mit geraden Obertönen), Wow, Flutter, Alter (Rauschen und dumpfer werdende Wiederholungen), Bandtempo Halb / Normal / Doppelt mit Gleiten. Animiertes Laufwerk.
- **Hall**: Federhall mit Hall, Laenge, Ton, Boing (Dispersion); sitzt auf den Echos, in der Schleife oder auf allem.
- **Dub**: Send, Mix, Ausgang, Ducking (Echos weichen dem Signal aus), Stereobreite, Tipps.

Immer sichtbar unten: Anzeige (Echozeit, Teilung, Tempo), Pegel, Bandtempo-Taster, **THROW** (nur solange gedrückt ins Echo) und **ENDLOS** (Band kreist ohne neues Signal).

## Bauen

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Selbsttest (Echozeit, Sync, Ping-Pong, drei Köpfe, Abklingen, Selbstoszillation, Endlos, Throw, Federhall, Bandtempo, Zufallsregler, Speichern):

    cmake -B build -DTRENDY_DELAY_TESTS=ON && cmake --build build --target TrendyDelayRenderTest
    TRENDY_DELAY_SNAPSHOT=Bilder ./build/TrendyDelayRenderTest_artefacts/Release/TrendyDelayRenderTest

Plugin-Codes: Hersteller `Andi`, Plugin `TrDl`, Bundle `com.andi.trendydelay`.
