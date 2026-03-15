# Pre-Release Test Checklist

Use this file to track high-risk gameplay and payout checks before the next public release.

## Payout Integrity

- [ ] Heads-up all-in with one player covering the other:
  - Verify uncalled excess chips are returned before payout.
  - Verify the covering player cannot accidentally win a fake side pot.
  - Verify only real payout recipients get a showdown `*`.

- [ ] Heads-up showdown where one player is all in short and loses:
  - Verify the winner receives the full contested pot.
  - Verify the loser does not receive a phantom partial payout.
  - Verify the result screen amount matches the actual stack change after confirmation.

- [ ] Three-player hand with one folded dead-money contributor and two players reaching showdown:
  - Verify folded money remains in the contested pot correctly.
  - Verify dead money does not create an incorrect side-pot split.

- [ ] Genuine multi-way side-pot hand:
  - Example shape: Player A short all-in, Player B medium stack, Player C deep stack.
  - Verify main and side pots are split according to real contribution layers.
  - Verify all positive payouts are reflected in final stack updates.
  - Verify showdown `*` markers appear only for players who actually receive payout.

- [ ] Multiple all-in players with different stack sizes:
  - Verify contribution layers still resolve correctly with 4 to 5 active players.
  - Verify no chips disappear and no payout exceeds the total pot.

## Rules Integrity

- [ ] Short all-in raise reopening:
  - Verify a short all-in raise does not reopen raising to a player who already acted.
  - Verify action still allows call or fold appropriately.
  - Verify the footer bet amount removes its arrow glyph in call/fold-only spots and remains centered cleanly.

- [ ] Fold win with zero-chip loser:
  - Verify a player who folds their last chips is marked eliminated only once the hand result is final.
  - Verify the status column changes to `X` only when the player is truly out of the game.

- [ ] Showdown elimination:
  - Verify a zero-chip player still active in showdown is not shown as `X` before the hand resolves.
  - Verify previously busted players keep their busted cues during later hands and showdowns.

## Result and Showdown UI

- [ ] Showdown screen:
  - Verify `*` means "received some payout," not necessarily sole winner.
  - Verify busted inactive players keep their busted strike cue.

- [ ] Result screen, fold win:
  - Human fold win shows two hole cards.
  - Bot fold win shows `?? ??`.
  - Amount line matches actual payout applied afterward.

- [ ] Result screen, showdown:
  - Verify best-five cards, hand label, and payout line match the actual winning hand.
  - Verify centered text remains visually centered across short and long strings.

- [ ] Result screen, split pot:
  - Verify the result view prefers `You` when the human player receives any payout.
  - Verify bot-only split pots choose a real payout recipient to represent the hand.
  - Verify `[Split Pot]` appears under the payout line whenever more than one player is paid.
  - Verify the representative payout amount matches that selected player's actual award.

## Persistence

- [ ] Save/load settings matrix:
  - Verify player count survives save/load and startup resume.
  - Verify current blind values survive save/load and reopen correctly in Edit Blinds.
  - Verify bot difficulty survives save/load and reopens correctly in Edit Bots.
  - Verify progressive-blind enabled state, period, step, and next scheduled increase survive save/load.

- [ ] Save/load during an in-progress hand that later reaches showdown:
  - Verify payout and winner display remain correct after resume.

- [ ] Save/load with progressive blinds active near an increase boundary:
  - Verify the correct blind increase interstitial appears only when due.

## Refactor Regression Sweep

- [ ] Modularization regression pass:
  - Verify startup, load-save startup choice, and `Start Game` prompt still behave correctly.
  - Verify Help, Edit Blinds, Edit Bots, New Game confirm, and Exit screens all render and navigate correctly after the UI split.
  - Verify Edit Blinds only shows `Reset blinds` when the staged fixed blind differs from the value present when the screen opened, and that reset returns to that opening value.
  - Verify progressive-blinds `On` only shows the left-arrow affordance at the minimum `5`-hand period where pressing left would turn the feature back off.
  - Verify canceling menus restores the correct underlying table/action state instead of a blank or stale footer.
  - Verify result, showdown, big win, and progressive blind interstitial screens still use the expected centered text and glyph prompts.
