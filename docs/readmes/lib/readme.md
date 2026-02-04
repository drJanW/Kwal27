# Kwal26 - An Art Project (Jellyfish)

> Version: 260104M | Updated: 2026-01-04

## Project Overview
Hoofdfunctionaliteiten:
•       Audio Management - MP3 afspelen met random fragment
en en TTS (VoiceRSS)                                       •       LED Management - 160 WS2812B LEDs met complexe lich
tshows                                                     •       Web Interface - Bediening via browser (volume, lich
t, voting systeem)                                         •       WiFi & OTA - Netwerkconnectie en wireless updates  
•       Tijd & Sensoren - NTP tijd, zonsop/ondergang, senso
ren                                                        •       SD Kaart - Audio files en indexsysteem
Enkele interessante aspecten:
Audio Systeem
•       Gebruikt ESP8266Audio library met I2S output       
•       Ondersteunt zowel lokale MP3 files als TTS via Voic
eRSS API                                                   •       "Voting systeem" voor audiofragmenten (likes/dislik
es)                                                        •       Random audio interval: 6-18 minuten tussen fragment
en (was: vast 10 min)                                      Licht Shows
•       Complexe algoritmes voor cirkel-patronen, gradiente
n, en animaties                                            •       Positie-gebaseerde LED mapping (x,y coördinaten van
 SD kaart)                                                 •       Dynamische brightness gebaseerd op audio levels en 
web input                                                  •       Random pattern en color selectie bij boot (uit CSV 
bestanden)                                                 Web Interface
•       Moderne HTML/CSS/JS interface
•       Real-time sliders voor volume en brightness        
•       Voting buttons voor audio content

Context Controller becomes the runtime brain: gathers state
 from the environment, normalizes it, and surfaces “what’s happening now” to the rest of the system; timer-driven updates and sensor snapshots all feed into that shared context.RunManager is the decision layer: it consumes context plus 
request inputs (user/web/etc.), applies high-level rules, and emits requests toward subsystems (audio, light, OTA) without touching hardware details itself.                     Each Policy (AudioPolicy, LightPolicy, SDPolicy, etc.) is n
ow a focused ruleset: given a request and the current context, it enforces local constraints (context-driven brightness caps, playback arbitration) before delegating to the subsystem controllers.                                         Net effect: context collects facts, run chooses actions, po
licies enforce domain-specific guardrails—clean separation that keeps subsystems modular and easier to evolve.        
Status ownership rule: controllers only write status to Ale
rtState (or StatusBits). All status reads must come from AlertState (or StatusBits), not controller APIs.             
A TimerManager slot fires and runs its callback (e.g., hour
ly “say time”, periodic fragment shuffle).                 Each callback raises a request toward RunManager (or direct
ly queues a ContextController refresh), never touching hardware.                                                      RunManager combines the request with the current context sn
apshot, then consults the relevant policy (audio/light/SD/etc.).                                                      The policy enforces its domain rules—resource availability,
 safety thresholds—and either rejects or forwards the request.                                                        Approved requests go to the subsystem controller (AudioMana
ger, LightController, …), which executes the action; rejections are logged or deferred.                               
 producers → Context → Run → policies → requests → consumer
s (audio/light/serial)                                     
 layered controller-based architecture:

Modules: encapsulate specific functionality (audio, light, 
input, etc.).                                              
ContextController: decides current state/environment (time,
 rules, events).                                           
RunManager: applies behavior patterns given the context.   

Policy layer: enforces priorities, resolves conflicts betwe
en runs.                                                   
TimerManager: centralized timing, fade scheduling, FSM tick
s.                                                         
Each module has one global instance, and responsibilities a
re strictly separated. Dependencies flow top-down: context → run → policy → modules, with TimerManager as shared core 
Analogie:
Context (seizoen/zaal/programma): welk concert, begintijd, 
akoestiek, regels van de zaal.                             Plan (programmakeuze): Beethoven 5, tempi-doelen, dynamiekb
and.                                                       Policy (chef-dirigent/artistiek leider): prioriteiten bij c
onflicten, wie solo krijgt, no-phones.                     Run (partituur→gebaar): concrete inzetten, crescendi, balan
sbesluiten.                                                Do-requests (uitvoering): “strijkers p, hout in”, “trompet 
nu inzetten”.                                              Modules: secties en instrumenten.
TimerManager: maatsoort, metrum, cue-timing.
