# Roadmap

This roadmap tracks the remaining work between the current branch and the next public release.

## Release 1.1 Finalization

### R-001 Full Pre-Release Test Pass
- Status: In Progress
- Summary: Run the full device-focused validation pass before shipping v1.1.
- Scope:
  - Work through [docs/pre-release-tests.md](pre-release-tests.md) end to end.
  - Re-check payout integrity, short-all-in rules, split-pot presentation, and money-glyph spacing.
  - Confirm the final menu, interstitial, startup, and result flows on real hardware.

### R-002 Screenshot Refresh
- Status: Planned
- Summary: Replace the current README and release-facing screenshots with captures from the final v1.1 UI.
- Scope:
  - Capture the final startup, main table, controls, settings, showdown, result, and win/loss states.
  - Keep the GitHub gallery aligned with the actual release build.
  - Reuse the existing padded README presentation workflow after the final raw captures are approved.

## Post-1.1 Ideas

### F-010 Cursor-Based Menu Navigation
- Status: Deferred
- Summary: Update dense settings menus to use a highlighted row with inline `L/R` controls.

### F-012 Configurable Bot Pause Length
- Status: Deferred
- Summary: Let players choose how long bot action messages pause between decisions.

### F-014 Descriptive Split-Pot Result Details
- Status: Deferred
- Summary: Expand split-pot result screens so they explain who won which share.

### F-001 Lifetime Hands Completed Counter
- Status: Deferred
- Summary: Track total completed hands for the currently installed app instance.

### F-004 Winner Celebration Polish
- Status: Deferred
- Summary: Expand winner presentation with richer audio, LED, or animation treatment.

### F-005 Game Over Polish
- Status: Deferred
- Summary: Expand the loss presentation with richer audio, LED, or animation treatment.

### F-007 New Startup Art
- Status: Deferred
- Summary: Replace or expand the current startup art with a more distinctive title-screen presentation.

### F-008 Sub-GHz Multiplayer Investigation
- Status: Deferred
- Summary: Evaluate whether synchronous multiplayer across multiple Flipper devices is feasible and worth pursuing.

## Notes

- Release-blocking validation lives in [docs/pre-release-tests.md](pre-release-tests.md).
- New feature work beyond v1.1 should stay out of the release branch until the final test pass and screenshot refresh are complete.
