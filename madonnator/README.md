# MADONNATOR V0.1

Dance-Pop-Song-Generator, eine Hommage an Madonna: würfelt komplette Songs mit Gesang im Stil des Dance-Pop von den Achtzigern bis in die Zweitausender
(Pulswellen-Stabs, Oktav-Bass, House-Klavier, Gospelchor, Trance-Arpeggios, Vierviertel-Bassdrum, Orchester-Hits, helle Popstimme).
Von Andreas Engel, Darmstadt, Deutschland. Instrument als AU, VST3 und Standalone (JUCE 8, CMake).
Gestaltung wie TRENDY ANDY: Design-Umschalter 1980 / 2100, Taster-Reiter, Querformat ohne Scrollen.

Alles wird nach Stilregeln neu erfunden und synthetisch erzeugt: keine Samples, keine Melodien, Akkordfolgen oder Texte aus Originalaufnahmen.
Die Stimme ist eine synthetische weibliche Popstimme aus Formantfiltern (hell, klar, etwas Hauch) und kein Nachbau oder Klon der Stimme einer echten Person.
Sie singt erfundene Silben, Lautmalerei oder summt. „Nähe zum Original“ steuert nur, wie oft der Generator zu den typischen Stilmitteln greift.

## Epochen

| Epoche | Tempo | Klang |
|---|---|---|
| Dance-Pop 1984 | 112–128 BPM | hell und tanzbar: Pulswellen-Stabs, Oktav-Bass, Drumcomputer mit Klatschen, Orchester-Hits |
| Gospel-Pop 1989 | 96–112 BPM | Kirche trifft Funk: Gospelchor, Septakkorde, Funk-Bass, Tamburin, gezupfte Gitarre |
| House 1990 | 116–124 BPM | Clubnacht: Klavier-Stabs, Vierviertel-Bassdrum, offene Hihat und Bass auf der Und |
| Electronica 1998 | 124–136 BPM | schwebend: Trance-Arpeggios, Breakbeats, weite Flächen, Moll und Phrygisch |
| Disco 2005 | 120–128 BPM | Disco-Revival: Oktav-Bass, Vierviertel mit Tamburin, Cowbell, Anläufe |

## Seiten

- **Song**: Länge (1 bis 10 Minuten), Intensität, Nähe zum Original, Abwechslung, Epoche, Klang folgt der Epoche, Tonart, Skala, Tempo,
  Mitwürfeln, Host-Tempo, Schleife. Rechts der Steckbrief: Titel, Tonart, Tempo, Länge, Refrain-Wort, Stiltreue, Form.
  MIDI SPEICHERN schreibt den Song als MIDI-Datei (8 Spuren, Gesangssilben als Liedtext, Rhythmus nach General MIDI auf Kanal 10).
- **Zeitleiste**: Abschnitte (Intro, Strophe, Vorrefrain, Refrain, Zwischenspiel, Bridge, Break, Outro), Intensitätskurve und alle Noten der sieben Spuren.
  Abschnittskopf anklicken = nur diesen Abschnitt neu würfeln; Punkte ziehen = Intensität (Doppelklick = automatisch); in die Spuren klicken = Position.
- **Klang**: Flächen (Streicher, Chor, Klavier), Riff (Säge, Puls, Glocke), Bass, Melodie (Puls, Säge, Glocke, Gitarre), Drumcomputer (5 Kits), Effekte (Schimmer, Anlauf, Orchester-Hit).
- **Gesang**: Gesang an/aus, Silben (Kunstwörter, Lautmalerei, Summen), Wärme, Hauch, Vibrato, Gleiten, Doppelung, Chorstimme, Hall, Echo, Pegel.
  Darunter die Textzeile mit den gesungenen Silben.
- **Mischpult**: Pegel, Stumm und Solo der sieben Spuren, Echo im Takt, Hall, Breite, Master, Bühnenlicht.

Immer sichtbar unten: Anzeige, Pegel, START/STOP, ANFANG, GESANG (an/aus), **NEUER SONG** und **VARIATION** (gleiche Form, neue Noten).
Im Host folgt der Song dem Transport und Tempo des Hosts. MIDI-Tasten rücken den ganzen Song (C3 = Originallage).

## Bauen

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Selbsttest (16 Prüfungen):

    cmake -B build -DMADONNATOR_TESTS=ON && cmake --build build --target MadonnatorRenderTest
    MADONNATOR_SNAPSHOT=Bilder ./build/MadonnatorRenderTest_artefacts/Release/MadonnatorRenderTest
    MADONNATOR_WAV=probe.wav MADONNATOR_WAV_ERA=2 ./build/MadonnatorRenderTest_artefacts/Release/MadonnatorRenderTest

Plugin-Codes: Hersteller `Andi`, Plugin `Mdnr`, Bundle `com.andi.madonnator`.
