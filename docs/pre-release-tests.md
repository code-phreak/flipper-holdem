# Pre-Release Test Checklist

Use this file to track high-risk gameplay and payout checks before the next public release.

## Payout Integrity

- [x] Heads-up all-in with one player covering the other returns uncalled excess chips before payout.
- [x] Heads-up all-in with one player covering the other does not create a fake side pot for the covering player.
- [x] Heads-up all-in with one player covering the other only shows showdown `*` markers for real payout recipients.
- [x] Heads-up showdown where one player is all in short and loses awards the full contested pot to the winner.
- [x] Heads-up showdown where one player is all in short and loses does not grant the loser a phantom partial payout.
- [x] Heads-up showdown where one player is all in short and loses shows a result-screen amount that matches the real stack change after confirmation.
- [ ] Three-player hand with one folded dead-money contributor and two players reaching showdown keeps folded money in the contested pot correctly.
- [ ] Three-player hand with one folded dead-money contributor and two players reaching showdown does not create an incorrect dead-money split.
- [ ] Genuine multi-way side-pot hand resolves main and side pots according to real contribution layers.
- [ ] Genuine multi-way side-pot hand reflects all positive payouts in final stack updates.
- [ ] Genuine multi-way side-pot hand only shows showdown `*` markers for players who actually receive payout.
- [ ] Multiple all-in players with different stack sizes still resolve contribution layers correctly with 4 to 5 active players.
- [ ] Multiple all-in players with different stack sizes never lose chips or pay out more than the total pot.
- [ ] A heads-up big-blind walk on 10/20 shows a +$30 result instead of incorrectly refunding the big blind down to a +$20 result.
- [ ] A heads-up small-blind shove into a folding big blind on 10/20 settles to a +$40 result after the uncalled raise portion is returned.
- [ ] When one player is all in from the blind and the hand continues, the next street shows normalized stack and pot values instead of carrying uncalled blind excess forward.
- [ ] Showdown stack and pot displays stay numerically consistent with the chips actually committed when a short stack goes all in on a later street.

## Rules Integrity

- [ ] A short all-in raise does not reopen raising to a player who already acted.
- [ ] A short all-in raise still gives affected players a correct call-or-fold response.
- [ ] The footer bet amount removes its arrow glyph in call/fold-only spots and stays centered cleanly.
- [ ] Holding Right while editing a bet snaps the selection to all in after the same hold length used by Back-hold.
- [ ] Holding Right while editing a bet fires the all-in snap once per hold and does not immediately reset before release.
- [ ] Short Right still resets the current bet cleanly after the hold-to-all-in change.
- [ ] Bot raise totals stay on small-blind increments instead of creating odd call amounts like `$22` on a `10/20` table.
- [x] A player forced all in by a blind still gets the folded-autoplay fast-forward affordance while bots finish the betting.
- [x] A bot that goes all in short to call a bet shows `Bot[n] is All In` on the action message line.
- [ ] A player who folds their last chips is marked eliminated only once the hand result is final.
- [ ] The status column changes to `X` only when the player is truly out of the game.
- [ ] A zero-chip player still active in showdown does not show `X` before the hand resolves.
- [ ] Previously busted players keep their busted cues during later hands and showdowns.

## Result and Showdown UI

- [x] Showdown `*` means "received some payout," not necessarily sole winner.
- [x] Busted inactive players keep their busted strike cue on the showdown screen.
- [x] Human fold wins show two hole cards on the result screen.
- [x] Bot fold wins show `?? ??` on the result screen.
- [x] Fold-win result amounts match the actual payout applied afterward.
- [ ] Heads-up blind-fold results show the full visible pot amount on the result screen, not the post-refund contested amount.
- [x] Showdown result screens show the correct best-five cards, hand label, and payout line.
- [x] Result-screen centered text remains visually centered across short and long strings.
- [x] Split-pot result screens prefer `You` when the human player receives any payout.
- [x] Bot-only split pots choose a real payout recipient to represent the hand.
- [ ] Bot-only split pots choose the largest paid bot when multiple bots receive different payouts.
- [x] `[Split Pot]` appears under the payout line whenever more than one player is paid.
- [x] The representative split-pot payout amount matches that selected player's actual award.
- [x] Three-line result screens keep their legacy centered vertical spacing after four-line split-pot centering changes.
- [x] Split-pot result screens keep the full four-line centered block vertically centered as a unit.
- [ ] When a raise selection reaches the player's full stack, the bottom-right action label switches from `Raise` to `All In`.
- [x] Game Over shows `Continue` plus the OK glyph bottom-right.
- [x] Game Over text is vertically centered on screen.
- [ ] Straight result screens order the five cards ascending for display.
- [ ] All visible money amounts use the new money glyph cleanly instead of a `$` character.
- [ ] Money-glyph kerning looks correct on the main footer, pot header, stack column, bot action text, result screens, and progressive blind notification.
- [x] The Game Menu blind row shows `P -` before the blind values whenever progressive blinds are enabled.

## Persistence

- [x] Player count survives save/load and startup resume.
- [x] Current blind values survive save/load and reopen correctly in Edit Blinds.
- [x] Bot difficulty survives save/load and reopens correctly in Edit Bots.
- [x] Progressive-blind enabled state, period, step, and next scheduled increase survive save/load.
- [x] Save/load during an in-progress hand that later reaches showdown preserves payout and winner display.
- [x] Save/load with progressive blinds active near an increase boundary only shows the blind increase interstitial when due.
- [x] Exiting without saving preserves the previously stored save instead of deleting it.
- [x] Custom non-progressive blinds persist across a user-initiated `Start new game?` reset.
- [x] Custom non-progressive blinds persist across automatic fresh games after a game win or loss.
- [x] Saving while progressive blinds are enabled preserves the configured base `SB/BB`, not just the currently active in-hand blind level.
- [x] Loading a game with progressive blinds enabled still restores the correct base `SB/BB` when progressive blinds are later turned off or a fresh game is started.

## Refactor Regression Sweep

- [x] Startup, load-save startup choice, and `Start Game` prompt still behave correctly after the UI split.
- [x] Help, Edit Blinds, Edit Bots, New Game confirm, and Exit screens all render and navigate correctly after the UI split.
- [x] Edit Blinds only shows `Reset blinds` when the staged fixed blind differs from the value present when the screen opened.
- [x] Edit Blinds reset returns to the blind value present when the screen opened.
- [x] Progressive-blinds `On` only shows the left-arrow affordance at the minimum `5`-hand period where pressing left would turn the feature back off.
- [x] Canceling menus restores the correct underlying table or action state instead of a blank or stale footer.
- [x] Result, showdown, big win, and progressive blind interstitial screens still use the expected centered text and glyph prompts.

## AI Behavior

- [ ] In heads-up Easy games, repeated oversized preflop opens do not let the human bully the bot into folding nearly every hand.
- [ ] In heads-up Medium games, repeated oversized preflop opens do not let the human bully the bot into folding nearly every hand.
- [ ] In heads-up Easy and Medium games, oversized flop continuation bets still get defended often enough to feel fair without making the bots jam unrealistically wide.
- [ ] Hold Right snaps to all in after a 1-second hold instead of the longer Back-hold timing.
