# Roadmap

This roadmap tracks the next planned work beyond the shipped v1.0 baseline of Hold 'em.

## Status Key

- Planned: agreed direction, not started
- In Progress: actively implemented
- Blocked: waiting on dependency, design decision, or validation
- Deferred: intentionally out of the current release scope
- Done: shipped and verified

## Branch Status

The public v1.0 release is stable. Current branch work is focused on the next release, including larger table support, UI tightening, and future-facing AI improvements.

## Next Release Candidates

### F-001 Lifetime Hands Completed Counter
- Status: Deferred
- Summary: Track total completed hands for the currently installed app instance after the current release.
- Future requirements:
  - Counter persists across app restarts.
  - Counter increments only when a hand reaches a completed result state.
  - Save format/versioning must allow future extension without data loss.
  - Behavior for save deletion, reinstall, and firmware migration must be documented.

### F-002 Configurable Progressive Blinds
- Status: In Progress
- Summary: Optional progressive blind structures are now wired into the blind editor and save state, with final on-device validation still pending.
- Current scope:
  - Keep the feature off by default.
  - Apply increases only at hand boundaries.
  - Show a short blind-level notification before the next hand when an increase is due.
  - Preserve save/load correctness with an active structure.

### F-010 Cursor-Based Menu Navigation
- Status: Planned
- Summary: Update dense settings menus to use a highlighted row with inline `L/R` increment and decrement controls.
- Future requirements:
  - Preserve the current compact three-button mental model on the Flipper screen.
  - Keep edit state readable without crowding rows with too much instructional text.
  - Reconcile the new icon-led control language with row selection so prompts stay terse.
  - Reuse the interaction pattern across bot, blind, and future settings menus.

### F-011 Screenshot Refresh Before Next Release
- Status: Planned
- Summary: Refresh README and release-facing screenshots once the current v1.1 UI work stabilizes.
- Future requirements:
  - Replace stale captures that still reflect the older four-seat layout and earlier menu wording.
  - Keep the GitHub gallery and future release assets aligned with the actual shipped UI.
  - Reuse the existing padded README presentation workflow after the final raw captures are approved.

### F-012 Configurable Bot Pause Length
- Status: Planned
- Summary: Let players choose how long bot action messages pause between decisions.
- Future requirements:
  - Preserve the current default pacing as the recommended baseline.
  - Allow fast autoplay without removing the ability to read betting flow.
  - Keep the setting persistent across save/load and new-game flow if it becomes user-facing.

### F-003 Configurable Bot Difficulty
- Status: Done
- Summary: Added player-facing bot difficulty settings on top of the shipped v1.0 baseline.
- Future requirements:
  - Preserve the current default balance as the initial setting.
  - Persist the chosen difficulty in save/config state.
  - Keep the AI path extensible for richer future behavior.

### F-009 Fifth-Seat Table Expansion
- Status: Done
- Summary: Expanded the live table from four total players to five total players with a fourth bot and tightened table/menu layout.
- Current scope:
  - Preserve row readability on the 128x64 display.
  - Keep footer, result, and help flows readable with the denser table.
  - Mark folded pre-showdown bot rows with a lightweight cue on the `XX XX` placeholders without exposing cards or cluttering the full row.
  - Maintain save/load correctness and bot-count restart behavior.

### F-004 Winner Celebration Polish
- Status: Deferred
- Summary: Expand the winner presentation after the current release with audio, LED effects, and richer animation treatment.
- Future requirements:
  - Add an optional winner tune.
  - Add celebratory LED behavior.
  - Expand the current fireworks treatment without blocking core flow excessively.

### F-005 Game Over Polish
- Status: Deferred
- Summary: Expand the game-over presentation after the current release with audio, LED effects, and richer animation treatment.
- Future requirements:
  - Add an optional game-over tune distinct from the winner tune.
  - Add a subdued LED pattern for losses.
  - Expand the current game-over presentation without obscuring restart flow.

### F-007 New Startup Art
- Status: Deferred
- Summary: Replace or expand the current startup art with a more distinctive title-screen presentation.
- Future requirements:
  - Preserve legibility on the 128x64 display.
  - Keep startup flow fast and input-safe.
  - Revalidate spacing around prompt text and title lockup on device.

### F-008 Sub-GHz Multiplayer Investigation
- Status: Deferred
- Summary: Evaluate whether synchronous multiplayer across multiple Flipper devices is feasible and worth pursuing.
- Future requirements:
  - Define a connection model that is reliable within Flipper hardware and regional radio constraints.
  - Determine how to synchronize shuffles, betting actions, and reconnect behavior without trust issues.
  - Confirm whether latency, packet loss, and user setup burden are acceptable for Texas Hold'em pacing.
  - Keep the investigation separate from the single-device core game until feasibility is proven.

## Notes

- Keep this file concise and implementation-focused.
- Add new post-v1 items only when they are meaningfully defined.
- Revisit deferred items as the next release scope becomes clear.
