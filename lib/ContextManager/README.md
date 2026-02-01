# ContextController

> Version: 251218A | Updated: 2025-12-17

The runtime brain of the system: gathers state from the environment, normalizes it, and surfaces "what's happening now" to the rest of the system. Timer-driven updates and sensor snapshots all feed into that shared context.

## Key Components

- **Calendar**: Calendar CSV parsing and theme lookup
- **CalendarSelector**: Scheduled calendar selection
- **ContextFlags**: System-wide boolean flags
- **LightColors/LightPatterns**: LED show configuration
- **ThemeBoxTable**: Theme box audio/visual mapping
- **TimeOfDay**: Time-based context (dawn, day, dusk, night)
- **TodayContext**: Daily context snapshot

