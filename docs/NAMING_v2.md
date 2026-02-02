# Naming canon (firmware/Arduino repo)

Doel: kleine, vaste woordenschat. Geen “enterprise” termen. Leesbaar in C++/Arduino.

## Actie-woorden (werkwoorden)

- **Request**  
  Een expliciete vraag/actie die uitgevoerd moet worden. (Noun als type/struct, verb als functie)
  - Voorbeelden: `SpeakRequest`, `LightSceneRequest`, `requestQueue`

- **runX / doX**  
  Uitvoeren. `run*` voor een complete handeling, `do*` voor een directe stap.
  - Voorbeelden: `runRequest(req)`, `doBootStep()`

- **selectX**  
  Kiezen op basis van input/flags/rules.
  - Voorbeelden: `selectProfile(...)`, `selectScene(...)`

## Rol-woorden (zelfstandige naamwoorden)

- **Selector**  
  Component dat iets kiest (en niets “managet”).  
  - Voorbeelden: `ProfileSelector`, `SceneSelector`, `AudioGroupSelector`

- **Rules**  
  Toelatings-/prioriteitsregels: “mag dit nu?” / “wat wint bij conflict?”  
  - Voorbeelden: `Rules::allow(req)`, `Rules::priority(req)`

- **Flags**  
  Gelijktijdig actieve eigenschappen (bitset / mask). Niet exclusief, geen voortgang.  
  - Voorbeelden: `TimeFlags`, `TodayFlags`, `AudioFlags`

- **Catalog**  
  Beheerde collectie/config-set (CRUD/selectie, evt. SD write-back).  
  - Voorbeelden: `ColorsCatalog`, `PatternCatalog`

- **Table**  
  Lookup/mapping data (bijv. CSV → multiplier/waarde). Geen CRUD, geen selectie-UI.  
  - Voorbeelden: `ShiftTable`, `AudioShiftTable`

- **Player**  
  Afspelen met begin/einde (audio/fragment/sentence).  
  - Voorbeelden: `SentencePlayer`, `FragmentPlayer`

- **Controller**  
  Actief aansturen/regelen (licht/motor/volume), mogelijk continu.  
  - Voorbeelden: `LightController`, `MotorController`

- **Core**  
  Centrale kernlogica van een domein (geen “manager”-connotatie).  
  - Voorbeelden: `AudioCore`, `SystemCore`

- **Driver**  
  Hardware/IO interface, zo dicht mogelijk bij device/protocol.  
  - Voorbeelden: `Bh1750Driver`, `I2sDriver`

## Woorden die we vermijden

Vermijd in nieuwe code en bij hernoemen (tenzij expliciet toegestaan):

- **Intent** (gebruik `Request`)
- **Conduct** (gebruik `Run` / `do*`)
- **Context** als containerwoord (gebruik `Flags` of een concreet noun)
- **Orchestrator / Engine / Service / Strategy** (te vaag / enterprise)
- **State** (impliceert voortgang/FSM; alleen gebruiken als het écht voortgang is)
- **Preset/Profile** (vermijden; als het toch moet: liever `Profile`/`Preset`)
- **Manager** (alleen toestaan als het echt resources/lifecycle beheert)


## Notify is verboden (geen 1:1 vervanging)

`notify` is semantisch te vaag (kan betekenen: waarschuwen, mededelen, aankondigen).

**Regel:** er bestaat expres **geen** automatische `Notify → X` mapping.
Elke `notify*`/`Notify*` moet handmatig worden omgezet naar exact één van:

- **Inform*** — neutrale mededeling/status/logging (geen urgentie)
- **Alert*** — waarschuwing/fout/aandacht vereist (urgent)
- **Announce*** — aankondiging/proclamatie (tijd, event-start, “nu gebeurt X”)

Voorbeelden:
- `notifyError()` → `alertError()`
- `notifyWifiUp()` → `informWifiUp()`
- `notifyTime()` → `announceTime()`

### Toegestane uitzonderingen (Manager)

- **TimerManager**: beheert timers/resources → ok
- **AudioManager**: beheert audio resources/overlap/stop → ok

## Mini-voorbeeld: hernoem-mapping

- `*Conduct*` → `*Run*`
- `*Intent*` → `*Request*`
- `*Store*` (CRUD) → `*Catalog*`
- `*Store*` (lookup/mapping) → `*Table*`
- `Context*` (gelijktijdige bits) → `*Flags*`
