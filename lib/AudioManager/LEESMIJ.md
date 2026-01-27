Audio Subsystem 

Deze module beschrijft de gescheiden verantwoordelijkheden en samenwerking tussen AudioManager, PlayFragment, en PlaySentence. Het ontwerp is geoptimaliseerd voor betrouwbare, niet-blockerende MP3-weergave op ESP32, met fade-ondersteuning voor fragmenten, en sequentiële weergave van losse woorden via een intern woordenboek.

1. AudioManager – Dispatcher en Resourcebeheer
AudioManager is verantwoordelijk voor:

Initialisatie van I2S-output en geluidsniveau (begin())

Aanroepen van PlayFragment::start() en PlaySentence::start()

Doorroepen van update() naar de juiste module (PlayFragment::update() of PlaySentence::update())

Beheer van gedeelde audio-resources:

audioFile

audioMp3Decoder

audioOutput

helixDecoder

Verhinderen van gelijktijdige audio via globale statusvlaggen:

setFragmentPlaying(bool)

setSentencePlaying(bool)

Veilig stoppen van actieve audio via stop()

Wat AudioManager niet meer doet:

Geen fading-logica

Geen gain-aanpassingen

Geen decoder-looping

Geen timing van afspeelduur

Alle audio-uitvoering is volledig gedelegeerd aan PlayFragment of PlaySentence.

2. PlayFragment – Afspeelmodule voor MP3-fragmenten
PlayFragment beheert de volledige uitvoerlogica van fragmenten uit willekeurige subdirectories van de SD-kaart. Elk fragment wordt afgespeeld vanaf een opgegeven starttijd en met opgegeven duur, optioneel met fading.

Verantwoordelijkheden:

Bepalen van het pad via getMP3Path(dir, file)

Initialiseren van audioFile, audioMp3Decoder en helixDecoder

Uitvoeren van seekToValidFrame() met Helix decoder

Starten van drie timers:

Fade-in timer bij t=0

Fade-out timer bij t = durationMs - fadeMs

Fragment-einde timer op t = durationMs

Starten van een vierde, repeterende timer die periodiek:

getFFade() × getBaseGain() berekent

en SetGain() aanroept met dat resultaat

De volledige fading en volume-aanpassing is dus losgekoppeld van playbackstatus. Playback stopt automatisch zodra de fragment-einde timer afloopt. Geen afhankelijkheid van loop() of isRunning().

3. PlaySentence – Sequentiële woordspeler
PlaySentence speelt een reeks losse MP3-bestanden uit /000/, gebaseerd op vaste wordIDs. Deze woorden worden één voor één afgespeeld via een intern schuifmechanisme.

Eigenschappen:

Intern wordt een vaste array sentence[MAX_WORDS_PER_SENTENCE] gebruikt

Woorden worden naar links geschoven zodra het vorige woord klaar is

END_OF_SENTENCE markeert het einde van de zin

Elk woord wordt afgespeeld via PlayWord(wordID), een interne functie

PlayWord():

Bereidt het pad voor

Start het MP3-bestand

Berekent de geschatte duur via (fileSize / 16) - HEADER_MS

Start een timer die setSentencePlaying(false) zet zodra het woord klaar is

Geen gebruik van loop() of decoderstatus

Er is slechts één actieve zin tegelijk

De afspeeltijd van woorden is volledig tijdgestuurd. Volgende woorden worden alleen gestart nadat de vorige volledig is afgelopen.

4. Gemeenschappelijke principes
Alle MP3-bestanden zijn mono, 128 kbps

Afspeelduur wordt berekend via durationMs = (fileSize / 16) - HEADER_MS

Audiostatus wordt uitsluitend bepaald via gemuxte vlaggen in Globals:

isFragmentPlaying()

isSentencePlaying()

isAudioPlaying() = afgeleide getter

Fading wordt geregeld via fFade en fadeArray[], extern beheerd

Volume wordt steeds gezet via SetGain(fFade × baseGain)

5. Bestanden
AudioManager.h/.cpp: centrale dispatcher en resourcebeheer

PlayFragment.h/.cpp: playback van willekeurige MP3-fragmenten met fading

PlaySentence.h/.cpp: sequentiële afspeler voor woordreeksen via vaste ID’s

Deze structuur is ontworpen voor betrouwbaarheid, minimale blocking, en nauwkeurige timing — zonder afhankelijkheid van externe decoderstatus of blocking calls.







Ask ChatGPT
