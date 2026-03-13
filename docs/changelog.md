# Changelog

All notable changes to this project should be documented in this file.

This changelog follows a lightweight Keep a Changelog style.

## [Unreleased]

### Added
- Support for a fourth bot, expanding tables to five total players.
- Configurable bot difficulty tiers with Medium as the default baseline.
- Optional progressive blinds with period and increase tuning from the blind editor.

### Changed
- Main table and menu layout were revised to preserve readability for the larger five-player table.
- Result-screen prompts and in-hand footer layout were tightened up to free space for a fourth bot row.
- Player rows now use fixed columns so turn markers, role/status flags, stacks, and hidden-card placeholders stay visually aligned.
- Table header and board placeholder alignment are now measured dynamically so hand, stage, pot, and preflop board state stay visually centered and stable.
- Help, menu, footer, and cancel prompts now use compact bitmap control glyphs where screen space is tight.
- Bot decision logic now accounts for betting pressure, tones down low pocket-pair aggression preflop, and avoids weak stack-off lines.
- Extreme AI now plays a sharper heuristic game with stronger pressure handling, draw awareness, and more disciplined bluffing.
- Bot-count and difficulty settings now persist cleanly through restart, save/load, and new-game flow.
- Blind editing now supports a progressive-blinds mode that stays off by default and only advances at hand boundaries.
- Progressive blind periods now step in five-hand increments, with clearer blind-editor labeling.
- Progressive blind increases now surface a short centered notification before `Hand Start` when the saved schedule says a new level is due.
- Freshly dealt hands now pause briefly on a stable `Hand Start` beat before action begins.

### Fixed
- Folded autoplay no longer flashes the generic help footer, and `OK` now skips ahead to the next showdown or win step cleanly.
- Folded autoplay now keeps its dedicated fast-forward affordance through bot turns without leaking the generic idle hint before results.
- Live gameplay events no longer kick the player out of open menus before they choose to return to the table.

## Planned

### Added
- Support for a sixth total player with a major conditional UI revision is planned for a future release.

## [1.0] - 2026-03-11

### Added
- Initial public release of Hold 'em for Flipper Zero.
- Full single-player Texas Hold'em gameplay from preflop through showdown.
- Support for 1 to 3 CPU opponents.
- Single-slot save/load with full game-state persistence.
- In-game blind editor, bot-count editor, controls help, and new-game reset.
- Big Win celebration and persistent win/loss end screens.

### Changed
- README, architecture notes, roadmap, and contributor docs were aligned with the shipped release.
- Catalog submission assets were added under `.catalog/`.

### Fixed
- Final release icon clarity and screenshot presentation polish for public release materials.
