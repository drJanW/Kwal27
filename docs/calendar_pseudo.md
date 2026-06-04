# CalendarSelector — Next Event Cache (Pseudo)

Standalone uitbreiding van `lib/ContextController/Calendar.{h,cpp}`.
Los van demo-feature: bedoeld als algemene voorziening voor status/UI/demo.

---

## Doel

Bij boot één keer de eerstvolgende kalender-event vooruit opzoeken en cachen
in RAM. Daarna kan elke caller (status-balk, web-UI, demo) zonder SD-I/O
opvragen welke event-dag er aankomt.

Schaalt vrijwel gratis: ~80 bytes RAM, geen extra timers, geen kosten in
de hot path.

---

## API toevoegen aan `CalendarSelector` (Calendar.h)

```
struct NextEventInfo {
    bool       valid;          // false = geen event gevonden (einde CSV)
    uint16_t   year;
    uint8_t    month;
    uint8_t    day;
    uint16_t   daysFromToday;  // 0 = vandaag, 1 = morgen, ...
    String     ttsSentence;
    uint8_t    themeBoxId;
    uint8_t    patternId;
    uint8_t    colorId;
};

class CalendarSelector {
public:
    // existing API ...

    // Force refresh of cache (used after CSV reload, or by boot)
    bool refreshNextEvent(uint16_t fromYear, uint8_t fromMonth, uint8_t fromDay);

    // Read cached next event (no SD I/O); returns false if cache invalid
    bool getNextEvent(NextEventInfo& out) const;

    // Invalidate cache (after CSV upload, manual clear)
    void clearNextEvent();

private:
    NextEventInfo nextEvent_{};
};
```

---

## Implementatie (Calendar.cpp) — pseudo

```
bool CalendarSelector::refreshNextEvent(uint16_t y, uint8_t m, uint8_t d):
    nextEvent_.valid = false

    if not ready_ or not fs_:
        return false

    open calendar.csv
    if open failed: return false

    bestRow.valid = false
    bestRow.daysFromToday = UINT16_MAX

    for each row in CSV (skip header/comments):
        parse row → (year, month, day, sentence, themeBoxId, patternId, colorId)

        days = daysBetween(today=(y,m,d), eventDate=(row.year,row.month,row.day))
        if days < 0: continue        # event ligt in verleden
        if days > 730: continue       # cap op 2 jaar vooruit
        if days >= bestRow.daysFromToday: continue   # niet beter

        bestRow = {row, days}

    close file

    if not bestRow.valid:
        return false

    nextEvent_ = {
        valid:          true,
        year/month/day: bestRow.date,
        daysFromToday:  bestRow.days,
        ttsSentence:    bestRow.sentence,
        themeBoxId:     bestRow.themeBoxId,
        patternId:      bestRow.patternId,
        colorId:        bestRow.colorId
    }
    return true


bool CalendarSelector::getNextEvent(NextEventInfo& out) const:
    if not nextEvent_.valid: return false
    out = nextEvent_
    return true


void CalendarSelector::clearNextEvent():
    nextEvent_.valid = false
```

### Helper: daysBetween

```
int32_t daysBetween((y1,m1,d1), (y2,m2,d2)):
    # Convert both dates to Julian day number, subtract
    return julianDayNumber(y2,m2,d2) - julianDayNumber(y1,m1,d1)
```

`julianDayNumber()` is een standaard formule (Fliegel/Van Flandern, ~5 regels).

---

## Refresh-momenten

Eén nieuwe call vanuit RunManager — geen nieuwe timer.

```
CalendarBoot::plan()              // after successful first loadToday()
    → calendarSelector.refreshNextEvent(today)

CalendarRun::cb_loadCalendar()    // runs at midnight + on demand
    after loadToday() succeeds:
        if (nextEvent.valid AND today >= nextEvent.date)
            calendarSelector.refreshNextEvent(today)

WebInterfaceController on CSV upload:
    calendarSelector.clearNextEvent()
    (next loadToday will trigger refresh via the check above)
```

Zo blijft het verzonken in bestaande callbacks; geen aparte timer.

---

## Concurrency / SD-lock

`refreshNextEvent()` doet SD-I/O. Zelfde regels als `loadToday()`:
- Caller is verantwoordelijk voor `lockSD()`/`unlockSD()` als die elders
  worden gebruikt (huidige `loadCalendarRow` volgt zelfde conventie)
- Boot: SD is exclusief beschikbaar, lock niet kritiek
- Midnight refresh: gebeurt in timer-callback, `cb_loadCalendar` lockt al

---

## Geheugen / footprint

| Veld           | Bytes |
|----------------|-------|
| valid          | 1     |
| year           | 2     |
| month/day      | 2     |
| daysFromToday  | 2     |
| themeBoxId etc | 3     |
| ttsSentence    | ~32 (Arduino String, gemiddeld) |
| **Totaal**     | ~42 + overhead |

Eenmalig, statische instance. Geen impact op heap-fragmentatie na boot.

---

## Edge cases

1. **CSV leeg of corrupt** → refresh returns false, getNextEvent returns false.
   Callers handelen "geen volgende event" af (bv. demo: skip dat zinnetje).

2. **Vandaag is zelf een event-dag** → cache slaat vandaag op met
   `daysFromToday = 0`. Caller die "volgende" wil moet zelf filteren
   (`if days==0` → óók refresh +1 dag, of accepteer "vandaag" als antwoord).
   **Voorstel: NextEventInfo bevat strict > 0 dagen**, dus refresh skipt today.
   Vandaag heeft eigen API (`loadToday`).

3. **Jaar-grens** — `daysBetween` werkt over jaargrenzen (Julian day).

4. **CSV groeit ver vooruit** — cap op 730 dagen voorkomt onbedoeld scannen
   tot 2035.

---

## Test-checklist (handmatig)

- [ ] Boot op normale dag → log toont "next event in X days: <naam>"
- [ ] Verzet RTC naar event-dag → middernacht-refresh slaat nieuw next event op
- [ ] Upload nieuwe calendar.csv via web → cache invalideert, herstart bij volgende loadToday
- [ ] CSV zonder events na vandaag → getNextEvent returns false, demo gedraagt zich correct

---

## Niet in scope

- Geen nieuwe CSV-velden
- Geen wijzigingen aan bestaande `loadToday()` of `loadCalendarRow()` semantiek
- Geen demo-specifieke logica (demo gebruikt deze API maar weet niets van implementatie)
- Geen tweede cache voor "event daarna" — alleen de eerstvolgende
