# Hold 'em for Flipper Zero

Native single-player Texas Hold'em for Flipper Zero.

Release 1 focuses on stable gameplay, clear controls, and maintainable open-source code.

## Release 1 Scope

- 1 human player vs 2 CPU opponents
- Full hand progression: Preflop, Flop, Turn, River, Showdown
- Fold-win and showdown resolution with side-pot-aware payouts
- In-game controls help, blind editor, and new-game reset
- Single-slot save/load flow
- Startup splash and jingle
- Hand result screen with payout and best 5 cards

## Controls

### In-hand
- `Left`: Fold
- `OK`: Commit action (`Check`, `Call`, or `Raise` depending on current bet)
- `Up/Down`: Increase/decrease current bet amount
- `Right`: Reset current bet amount to default call/check value

### Global
- `Back` short:
  - From game screen: open Controls Help
  - From menu screens: close/cancel current menu
- `Back` hold (1.5s): open `Exit Hold 'em`

### Exit menu
- `OK`: Save + Exit
- `Back` short: Cancel
- `Back` hold (1.5s): Exit without saving

## Save Behavior

Save path:
- `/ext/apps_data/holdem/save.bin`

Startup behavior when save exists:
- `OK`: Load save
- `Back`: Start new game and delete previous save

There is only one save slot by design.

## Fairness and RNG

Dealing fairness is based on:
- Hardware RNG (`furi_hal_random_fill_buf`)
- Fisher-Yates shuffle over all 52 cards

Current bounded random helper uses modulo reduction (`value % upper_bound`).

What this guarantees:
- No duplicate cards in a hand
- Full-deck shuffle every hand
- No AI influence over card distribution

What remains open for improvement:
- Replace modulo reduction with rejection sampling to remove modulo bias completely.

## Build

```bash
ufbt update
ufbt
```

Build output:
- `dist/holdem.fap`

## Install (Quick UAT from Repo)

If the repo already includes a built artifact:

1. Connect Flipper Zero over USB.
2. Drag-and-drop `dist/holdem.fap` into `/ext/apps/Games/`.
3. Launch from `Apps -> Games -> Hold 'em`.

## Install (Local Build)

1. Connect Flipper Zero over USB.
2. Build locally (`ufbt update`, `ufbt`).
3. Copy `dist/holdem.fap` to `/ext/apps/Games/`.
4. Launch from `Apps -> Games -> Hold 'em`.

## Firmware Notes

Target/API:
- Target 7
- API 87.1

Designed for official firmware and common compatible forks (including Momentum) that preserve external-app API compatibility.

## Repository Layout

- `holdem.c`: app orchestration, UI, input state handling
- `holdem_engine.c/.h`: core gameplay mechanics
- `holdem_ai.c/.h`: bot decision logic
- `holdem_eval.c/.h`: hand evaluation and card formatting
- `holdem_storage.c/.h`: save/load management
- `holdem_types.h`: shared types and constants
- `application.fam`: app metadata
- `holdem.png`: app icon
- `dist/holdem.fap`: current shared UAT artifact (temporary workflow)
- `docs/architecture.md`: architecture and extension notes
- `docs/roadmap.md`: planned future enhancements
- `docs/changelog.md`: placeholder until Release 1 is finalized
- `CONTRIBUTING.md`: contributor workflow

## Contributing

Contributions are welcome.

Please read:
- `CONTRIBUTING.md`

## License

MIT
