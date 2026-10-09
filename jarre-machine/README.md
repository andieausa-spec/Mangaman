# JARRE MACHINE V0.1

Space-Track-Generator: würfelt komplette Instrumentaltracks im Stil der Synthesizer-Musik der späten Siebziger
(String-Maschine mit Phaser, sprudelnde Sequenzen, Rhythmusbox, gleitende Melodien, Wind, Brandung, Laser).
Von Andreas Engel, Darmstadt, Deutschland. Instrument als AU, VST3 und Standalone (JUCE 8, CMake).
Gestaltung wie TRENDY ANDY: Design-Umschalter 1980 / 2100, Taster-Reiter, Querformat ohne Scrollen.

Alles wird nach Stilregeln neu erfunden und synthetisch erzeugt: keine Samples, keine Melodien oder Akkordfolgen aus Originalaufnahmen.
„Nähe zum Original“ steuert nur, wie oft der Generator zu den typischen Stilmitteln greift.

## Seiten

- **Track**: Länge (1 bis 15 Minuten), Intensität, Nähe zum Original, Abwechslung, Epoche (Oxygène 1976, Équinoxe 1978, Magnetfelder 1981),
  Tonart, Skala, Tempo; Mitwürfeln (Tempo und Tonart beim Würfeln), Host-Tempo, Schleife. Rechts der Steckbrief: Titel, Tonart, Tempo, Länge,
  Stiltreue in Prozent, Form. MIDI SPEICHERN schreibt den Track als MIDI-Datei (6 Spuren, Rhythmus nach General MIDI auf Kanal 10).
- **Zeitleiste**: Abschnitte (Intro, Aufbau, Thema A, Zwischenspiel, Thema B, Break, Höhepunkt, Outro), Intensitätskurve und alle Noten der sechs Spuren.
  Abschnittskopf anklicken = nur diesen Abschnitt neu würfeln; Punkte ziehen = Intensität des Abschnitts (Doppelklick = automatisch);
  in die Spuren klicken = Position. Je Spur Stumm (M) und Solo (S).
- **Klang**: Flächen (Ensemble, Phaser, Phaser-Tempo, Helligkeit, Anschwellen), Sequenz (Cutoff, Resonanz, Filterkurve, Abklingen, Welle),
  Effekte (Wind, Laser, Brandung), Bass, Melodie (Gleiten, Vibrato, Helligkeit, Welle), Rhythmusbox (Kit, Ton, Swing).
- **Mischpult**: Pegel der sechs Spuren und Master, Echo im Takt, Hall, Breite, Laserharfe (ein Strahl je Spur).

Immer sichtbar unten: Anzeige (Zeit, Takt, Abschnitt, Tempo), Pegel, START/STOP (eigene Uhr), ANFANG, **NEUER TRACK** und **VARIATION** (gleiche Form, neue Noten).
Im Host folgt der Track dem Transport und Tempo des Hosts. MIDI-Tasten rücken den ganzen Track (C3 = Originallage, die letzte Taste bleibt stehen).

## Bauen

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Selbsttest (Seed wiederholbar, Länge, Intensität, Nähe zum Original, Töne in der Skala, Abschnitt neu würfeln, Mix, Spuren solo,
Host-Sync, MIDI-Transponieren, MIDI-Export, Speichern, Zufallsregler):

    cmake -B build -DJARRE_MACHINE_TESTS=ON && cmake --build build --target JarreMachineRenderTest
    JARRE_MACHINE_SNAPSHOT=Bilder ./build/JarreMachineRenderTest_artefacts/Release/JarreMachineRenderTest
    JARRE_MACHINE_WAV=probe.wav JARRE_MACHINE_WAV_ERA=1 ./build/JarreMachineRenderTest_artefacts/Release/JarreMachineRenderTest

Plugin-Codes: Hersteller `Andi`, Plugin `JrMc`, Bundle `com.andi.jarremachine`.
