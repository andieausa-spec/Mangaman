# KRAFT TRANS V0.1

Elektro-Pop-Song-Generator im Stil der deutschen Elektronik der Siebziger und Achtziger: würfelt komplette Songs mit Roboterstimme
(Sequenzer-Ostinati, Oktav-Bass, Streichermaschine und Orgel, Vocoder, Sprachchip, elektronische Percussion mit Metall und Syndrum,
Zug-, Auto-, Funk- und Datengeräusche). Von Andreas Engel, Darmstadt, Deutschland. Instrument als AU, VST3 und Standalone (JUCE 8, CMake).
Gestaltung wie TRENDY ANDY: Design-Umschalter 1980 / 2100, Gummipotis, Taster-Reiter, Querformat ohne Scrollen.

Alles wird nach Stilregeln neu erfunden und synthetisch erzeugt: keine Samples, keine Melodien, Akkordfolgen oder Texte aus Originalaufnahmen.
Statt Gesang spricht eine Roboterstimme: eine Formant-Sprachsynthese, die wahlweise durch einen Vocoder, einen Sprachchip oder einen
Ringmodulator läuft. Kein Nachbau einer echten Stimme. Die eingebauten Wörter sind neutrale Technikbegriffe, Zahlen oder Lautmalerei
(bum, tschak, boing …). **Eigener Text** wird so gesprochen, wie er eingetippt wird: deutsche Schreibweise wird in Silben und Laute zerlegt,
Ziffern werden als Zahlwörter gesprochen.
„Nähe zum Original“ steuert nur, wie oft der Generator zu den typischen Stilmitteln greift.

## Epochen

| Epoche | Tempo | Klang |
|---|---|---|
| Motorik 1974 | 84–104 BPM | unterwegs: hüpfender Oktav-Bass, Flöte und Orgel, sanfte Elektro-Drums, Autos ziehen vorbei |
| Radiowellen 1975 | 72–96 BPM | Funkwellen: Chorflächen, langsame Pulse, Zählrohr-Knacken und Morsezeichen |
| Schienen 1977 | 104–120 BPM | auf Schienen: Metall-Percussion im Fahrrhythmus, Streicherflächen, Zuggeräusche |
| Maschinen 1978 | 112–128 BPM | Maschinenpark: Sechzehntel-Sequenzen, Syndrum-Toms, Roboterstimme |
| Rechner 1981 | 112–132 BPM | Rechenzentrum: Piepser-Melodien, Sprachchip, Datengeräusche, knackige Snare |
| Digital 1986 | 108–124 BPM | digital: harte Drums mit Klatschen und Metall, Glocken, Sprechrhythmus |

## Seiten

- **Song**: Länge (1 bis 10 Minuten), Intensität, Nähe zum Original, Abwechslung, Epoche, Klang folgt der Epoche, Tonart, Skala, Tempo,
  Mitwürfeln, Host-Tempo, Schleife. Rechts der Steckbrief: Titel, Tonart, Tempo, Länge, Refrain-Wort, Stiltreue, Form.
  MIDI SPEICHERN schreibt den Song als MIDI-Datei (8 Spuren, Silben als Liedtext, Rhythmus nach General MIDI auf Kanal 10).
- **Zeitleiste**: Abschnitte (Intro, Strophe, Überleitung, Refrain, Thema, Bridge, Break, Outro), Intensitätskurve und alle Noten der sieben Spuren.
  Abschnittskopf anklicken = nur diesen Abschnitt neu würfeln; Punkte ziehen = Intensität (Doppelklick = automatisch); in die Spuren klicken = Position.
- **Klang**: Flächen (Streicher, Chor, Orgel), Sequenz (Säge, Puls, Glocke), Bass, Melodie (Puls, Säge, Glocke, Flöte, Piepser),
  Elektro-Drums (6 Kits), Geräusche (Rauschen, Maschinen, Signale).
- **Stimme**: Stimme an/aus, Stimmart (Vocoder, Sprachchip, Roboter), Text (Technik, Zahlen, Lautmalerei, Eigener Text), Melodie
  (0 % = monoton gesprochen), Auflösung (Vocoder-Bänder), Träger, Körnung, Metall, Doppelung, Zweitstimme, Hall, Echo.
  Feld **EIGENER TEXT** mit SPRECHEN; daneben die Textzeile mit den gesprochenen Silben.
- **Mischpult**: Pegel, Stumm und Solo der sieben Spuren, Echo im Takt, Hall, Breite, Master, Lichtraster.

Immer sichtbar unten: Anzeige, Pegel, START/STOP, ANFANG, STIMME (an/aus), **NEUER SONG** und **VARIATION** (gleiche Form, neue Noten).
Im Host folgt der Song dem Transport und Tempo des Hosts. MIDI-Tasten rücken den ganzen Song (C3 = Originallage).

## Bauen

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Selbsttest (19 Prüfungen):

    cmake -B build -DKRAFT_TRANS_TESTS=ON && cmake --build build --target KraftTransRenderTest
    KRAFT_TRANS_SNAPSHOT=Bilder ./build/KraftTransRenderTest_artefacts/Release/KraftTransRenderTest
    KRAFT_TRANS_WAV=probe.wav KRAFT_TRANS_WAV_ERA=3 ./build/KraftTransRenderTest_artefacts/Release/KraftTransRenderTest

Plugin-Codes: Hersteller `Andi`, Plugin `KrTr`, Bundle `com.andi.krafttrans`.
