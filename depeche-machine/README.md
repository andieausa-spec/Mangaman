# DEPECHE MACHINE V0.1

Synthpop-Song-Generator: würfelt komplette Songs mit Gesang im Stil des dunklen britischen Synthpop der Achtziger und frühen Neunziger
(Pulswellen-Riffs, Oktav-Bass, Metall-Percussion, Chorflächen, Slide-Gitarre, tiefe Baritonstimme).
Von Andreas Engel, Darmstadt, Deutschland. Instrument als AU, VST3 und Standalone (JUCE 8, CMake).
Gestaltung wie TRENDY ANDY: Design-Umschalter 1980 / 2100, Taster-Reiter, Querformat ohne Scrollen.

Alles wird nach Stilregeln neu erfunden und synthetisch erzeugt: keine Samples, keine Melodien, Akkordfolgen oder Texte aus Originalaufnahmen.
Die Stimme ist eine synthetische Baritonstimme aus Formantfiltern (tief, dunkel, nah) und kein Nachbau einer echten Stimme.
Sie singt erfundene Silben, Lautmalerei oder summt. „Nähe zum Original“ steuert nur, wie oft der Generator zu den typischen Stilmitteln greift.

## Epochen

| Epoche | Tempo | Klang |
|---|---|---|
| Synthpop 1981 | 118–136 BPM | hell und hüpfend: Pulswellen, Oktav-Bass, einfacher Drumcomputer |
| Industrial 1984 | 100–122 BPM | dunkel und hart: Metallschläge, Hammer-Snare, Sechzehntel-Bass, Chor |
| Synth-Noir 1987 | 94–116 BPM | groß und düster: weite Hallräume, Glas-Flächen, getragene Linien |
| Elektro-Blues 1990 | 104–124 BPM | Slide-Gitarre, treibender Bass, Tamburin |
| Dunkel 1993 | 78–98 BPM | schwer und langsam: tiefer Puls, viel Raum, Gospel-Moll |

## Seiten

- **Song**: Länge (1 bis 10 Minuten), Intensität, Nähe zum Original, Abwechslung, Epoche, Klang folgt der Epoche, Tonart, Skala, Tempo,
  Mitwürfeln, Host-Tempo, Schleife. Rechts der Steckbrief: Titel, Tonart, Tempo, Länge, Refrain-Wort, Stiltreue, Form.
  MIDI SPEICHERN schreibt den Song als MIDI-Datei (8 Spuren, Gesangssilben als Liedtext, Rhythmus nach General MIDI auf Kanal 10).
- **Zeitleiste**: Abschnitte (Intro, Strophe, Vorrefrain, Refrain, Zwischenspiel, Bridge, Break, Outro), Intensitätskurve und alle Noten der sieben Spuren.
  Abschnittskopf anklicken = nur diesen Abschnitt neu würfeln; Punkte ziehen = Intensität (Doppelklick = automatisch); in die Spuren klicken = Position.
- **Klang**: Flächen (Streicher, Chor, Glas), Riff, Bass, Melodie (Puls, Säge, Metall, Slide-Gitarre), Drumcomputer (4 Kits), Effekte.
- **Gesang**: Gesang an/aus, Silben (Kunstwörter, Lautmalerei, Summen), Tiefe, Hauch, Vibrato, Gleiten, Doppelung, Zweitstimme, Hall, Echo, Pegel.
  Darunter die Textzeile mit den gesungenen Silben.
- **Mischpult**: Pegel, Stumm und Solo der sieben Spuren, Echo im Takt, Hall, Breite, Master, Bühnenlicht.

Immer sichtbar unten: Anzeige, Pegel, START/STOP, ANFANG, GESANG (an/aus), **NEUER SONG** und **VARIATION** (gleiche Form, neue Noten).
Im Host folgt der Song dem Transport und Tempo des Hosts. MIDI-Tasten rücken den ganzen Song (C3 = Originallage).

## Bauen

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Selbsttest (16 Prüfungen):

    cmake -B build -DDEPECHE_MACHINE_TESTS=ON && cmake --build build --target DepecheMachineRenderTest
    DEPECHE_MACHINE_SNAPSHOT=Bilder ./build/DepecheMachineRenderTest_artefacts/Release/DepecheMachineRenderTest
    DEPECHE_MACHINE_WAV=probe.wav DEPECHE_MACHINE_WAV_ERA=2 ./build/DepecheMachineRenderTest_artefacts/Release/DepecheMachineRenderTest

Plugin-Codes: Hersteller `Andi`, Plugin `DpMc`, Bundle `com.andi.depechemachine`.
