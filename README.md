# Hold 'em for Flipper Zero

Native single-player Texas Hold'em for Flipper Zero.

Release 1 is focused on stable gameplay, clear on-device controls, fair dealing, and a codebase that is easy for other developers to inspect and extend.

## Release 1 Features

- 1 human player versus 1 to 3 CPU opponents
- Configurable bot count from the in-game menu
- Full hand flow: Preflop, Flop, Turn, River, and Showdown
- Fold-win and showdown resolution with side-pot-aware payout handling
- Single-slot save/load flow
- In-game controls help, blind editor, bot-count editor, and new-game reset
- Startup splash and jingle
- Bot action pacing with readable on-screen decision messages
- Hand result screen with payout details and best-five-card summary
- Bitmap-style suit icons on the main table view and result-card summaries
- Big Win interstitial plus persistent win/lose end screens

## Controls

### In Hand
- `Left`: Fold
- `OK`: Commit the current action (`Check`, `Call`, or `Raise`)
- `Up/Down`: Increase or decrease the current bet amount
- `Right`: Reset the current bet amount to the default call/check value

### Global
- `Back` short:
  - From the game screen: open Controls Help
  - From menu screens: close or cancel the current menu
- `Back` hold (1.5s): open `Exit Hold 'em`

### Exit Menu
- `OK`: Save and exit
- `Back` short: Cancel
- `Back` hold (1.5s): Exit without saving

## Save Behavior

Save path:
- `/ext/apps_data/holdem/save.bin`

Startup behavior when a save exists:
- `OK`: Load save
- `Back`: Start a new game and delete the previous save

There is only one save slot by design.

## Fairness and RNG

Card dealing fairness is based on:
- Hardware RNG via `furi_hal_random_fill_buf`
- Fisher-Yates shuffle across all 52 cards before each hand

Current bounded random selection uses modulo reduction (`value % upper_bound`).

What this guarantees:
- No duplicate cards in a hand
- A full-deck shuffle every hand
- No AI influence over card distribution

What remains open for future improvement:
- Replace modulo reduction with rejection sampling to eliminate modulo bias entirely

## Build

```bash
ufbt update
ufbt
```

Build output:
- `dist/holdem.fap`

## Install

1. Connect Flipper Zero over USB.
2. Build locally with `ufbt update` and `ufbt`.
3. Copy `dist/holdem.fap` to `/ext/apps/Games/`.
4. Launch from `Apps -> Games -> Hold 'em`.

## Firmware Notes

Target/API:
- Target 7
- API 87.1

The app is intended for official firmware and compatible forks, including Momentum, as long as they preserve external-app API compatibility.

## Repository Layout

- `holdem.c`: app orchestration, rendering, and input/state handling
- `holdem_engine.c/.h`: gameplay flow, pot handling, and showdown logic
- `holdem_ai.c/.h`: bot decision logic
- `holdem_eval.c/.h`: hand evaluation and card formatting helpers
- `holdem_storage.c/.h`: save/load management
- `holdem_types.h`: shared types and constants
- `application.fam`: app metadata
- `holdem.png`: app icon
- `docs/architecture.md`: architecture and extension notes
- `docs/roadmap.md`: release follow-up and deferred work
- `docs/changelog.md`: placeholder until the first tagged release is cut
- `CONTRIBUTING.md`: contributor workflow

## Release Notes Discipline

- Source control should not carry release binaries long-term.
- Build artifacts should be generated locally or by release automation.
- The `dist/` directory is intentionally ignored again before the public Release 1 merge.

## Future Considerations

- Lifetime total hands completed counter remains intentionally deferred until after Release 1.

## Contributing

Contributions are welcome.

Please read:
- `CONTRIBUTING.md`

## License

MIT
