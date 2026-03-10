# Roadmap

This roadmap tracks the work that is intentionally deferred until after the first public Release 1 build of Hold 'em.

## Status Key

- Planned: agreed direction, not started
- In Progress: actively implemented
- Blocked: waiting on dependency, design decision, or validation
- Deferred: intentionally out of Release 1 scope
- Done: shipped and verified

## Release 1 Status

Release 1 implementation is feature-complete for current scope. Remaining work should be handled as release validation, bug fixing, documentation cleanup, or post-v1 planning rather than new feature expansion.

## Post-v1 Work

### F-001 Lifetime Hands Completed Counter
- Status: Deferred
- Summary: Track total completed hands for the currently installed app instance after Release 1.
- Future requirements:
  - Counter persists across app restarts.
  - Counter increments only when a hand reaches a completed result state.
  - Save format/versioning must allow future extension without data loss.
  - Behavior for save deletion, reinstall, and firmware migration must be documented.

### F-002 Configurable Progressive Blinds
- Status: Deferred
- Summary: Add optional progressive blind structures after Release 1.
- Future requirements:
  - Add enable/disable control and basic structure tuning.
  - Apply increases only at hand boundaries.
  - Preserve save/load correctness with an active structure.

### F-003 Configurable Bot Difficulty
- Status: Deferred
- Summary: Add a player-facing bot difficulty setting after Release 1.
- Future requirements:
  - Preserve the current default balance as the initial setting.
  - Persist the chosen difficulty in save/config state.
  - Keep the AI path extensible for richer future behavior.

### F-004 Winner Celebration Polish
- Status: Deferred
- Summary: Expand the winner presentation after Release 1 with audio, LED effects, and richer animation treatment.
- Future requirements:
  - Add an optional winner tune.
  - Add celebratory LED behavior.
  - Expand the current fireworks treatment without blocking core flow excessively.

### F-005 Game Over Polish
- Status: Deferred
- Summary: Expand the game-over presentation after Release 1 with audio, LED effects, and richer animation treatment.
- Future requirements:
  - Add an optional game-over tune distinct from the winner tune.
  - Add a subdued LED pattern for losses.
  - Expand the current game-over presentation without obscuring restart flow.

### F-006 Busted Player Strikethrough Experiment
- Status: Deferred
- Summary: Evaluate drawing a strike-through treatment for busted bot rows to improve scanability.
- Future requirements:
  - Prototype a lightweight strike-through overlay for busted bot names or full rows.
  - Confirm readability on-device with the current font and row spacing.
  - Keep the effect optional until the visual result is validated.
  - Avoid cluttering active-player rows or reducing card readability.

## Notes

- Keep this file concise and implementation-focused.
- Add new post-v1 items only when they are meaningfully defined.
- Revisit deferred items after Release 1 is tagged and stabilized.
