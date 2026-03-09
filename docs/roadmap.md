# Roadmap

This roadmap tracks post-Release-1 enhancements and design work for Hold 'em.

## Status Key

- Planned: agreed direction, not started
- In Progress: actively implemented
- Blocked: waiting on dependency, design decision, or validation
- Done: shipped and verified

## Future Enhancements

### R-001 Card Suit Visuals
- Status: Planned
- Summary: Evaluate text-based suit characters versus icon-based suit rendering.
- Functional requirements:
  - Compare readability at 128x64 resolution for both approaches.
  - Keep card parsing logic independent from rendering representation.
  - Ensure no memory regressions in draw/update path.

### R-002 Winner Celebration Effects
- Status: Planned
- Summary: Add a winner-screen jingle and celebratory LED sequence.
- Functional requirements:
  - Trigger only on table-win outcome.
  - Keep audio+LED sequence non-blocking or tightly bounded in duration.
  - Provide a feature flag to disable effects in future low-power mode.

### R-003 Game Over Effects
- Status: Planned
- Summary: Add game-over tune and "sad" LED sequence when player busts.
- Functional requirements:
  - Trigger only on player elimination game-over screen.
  - Distinct tone and LED pattern from winner celebration.
  - Respect global effect-disable setting once introduced.

### R-004 Lifetime Hands Completed Counter
- Status: Planned
- Summary: Track total completed hands for the currently installed app instance.
- Functional requirements:
  - Counter persists across app restarts.
  - Counter increments only when a hand reaches a completed result state.
  - Save format/versioning must allow future extension without data loss.
  - Behavior for save deletion, reinstall, and firmware migration must be documented.

## Notes

- Keep this file concise and implementation-focused.
- Update status and requirements before starting any roadmap item.
