#include "holdem_types.h"
#include "holdem_storage.h"
#include "holdem_eval.h"
#include "holdem_ai.h"
#include "holdem_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Stage labels are rendered in the footer and result views, so keep them short and stable.
static const char* k_stage_name[] = {"PREFLOP", "FLOP", "TURN", "RIVER", "SHOWDOWN"};
static bool process_global_event(HoldemApp* app, const InputEvent* ev);
static void flush_input_queue(HoldemApp* app);
static bool holdem_can_skip_folded_autoplay(const HoldemApp* app);
static void set_ai_level(HoldemApp* app, uint8_t level_pct);
static void holdem_reset_progressive_schedule(HoldemApp* app);
static void holdem_apply_blind_values_now(HoldemApp* app, int32_t new_small_blind);

static void holdem_draw_line(Canvas* canvas, uint8_t y, const char* text) {
    canvas_draw_str(canvas, 0, y, text);
}

static void holdem_draw_line_right(Canvas* canvas, uint8_t y, const char* text) {
    canvas_draw_str_aligned(canvas, 127, y, AlignRight, AlignBottom, text);
}

static const char* holdem_ai_label(uint8_t level_pct) {
    if(level_pct >= HOLDEM_AI_EXTREME_LEVEL) return "Extreme";
    if(level_pct >= HOLDEM_AI_HARD_LEVEL) return "Hard";
    if(level_pct <= HOLDEM_AI_EASY_LEVEL) return "Easy";
    return "Medium";
}

static uint8_t holdem_prev_ai_level(uint8_t level_pct) {
    if(level_pct >= HOLDEM_AI_EXTREME_LEVEL) return HOLDEM_AI_HARD_LEVEL;
    if(level_pct >= HOLDEM_AI_HARD_LEVEL) return HOLDEM_AI_MEDIUM_LEVEL;
    if(level_pct > HOLDEM_AI_EASY_LEVEL) return HOLDEM_AI_EASY_LEVEL;
    return HOLDEM_AI_EASY_LEVEL;
}

static uint8_t holdem_next_ai_level(uint8_t level_pct) {
    if(level_pct < HOLDEM_AI_MEDIUM_LEVEL) return HOLDEM_AI_MEDIUM_LEVEL;
    if(level_pct < HOLDEM_AI_HARD_LEVEL) return HOLDEM_AI_HARD_LEVEL;
    if(level_pct < HOLDEM_AI_EXTREME_LEVEL) return HOLDEM_AI_EXTREME_LEVEL;
    return HOLDEM_AI_EXTREME_LEVEL;
}

static bool holdem_is_menu_mode(UiMode mode) {
    return mode == UiModeHelp || mode == UiModeBlindEdit || mode == UiModeBotCountEdit ||
           mode == UiModeRestartConfirm || mode == UiModeExitPrompt;
}

// Gameplay events should not tear the player out of an open menu. When a menu is
// active, queue the next foreground screen in return_mode and let the player back
// out naturally into it.
static void holdem_set_foreground_mode(HoldemApp* app, UiMode mode) {
    if(holdem_is_menu_mode(app->mode)) {
        app->return_mode = mode;
    } else {
        app->mode = mode;
    }
}

static const char* holdem_display_name(const Player* player) {
    if(!player->is_bot) return "You";
    return player->name;
}

static char holdem_role_char(const HoldemGame* game, int player_index) {
    if(player_index == game->button) return 'D';
    if(player_index == game->sb_idx) return 'S';
    if(player_index == game->bb_idx) return 'B';
    return '-';
}

static char holdem_status_char(const Player* player) {
    if(player->stack <= 0) return 'X';
    if(!player->in_hand) return 'F';
    if(player->all_in) return 'A';
    return 'P';
}

static void holdem_build_display_order(const HoldemGame* game, size_t order[HOLDEM_MAX_PLAYERS]) {
    size_t write_index = 0;

    // Keep all-in players in the active cluster until the hand fully resolves.
    // They only sink to the bottom once they have zero chips and are out of the hand.
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        const Player* player = &game->players[player_index];
        if(player->stack > 0 || player->in_hand) order[write_index++] = player_index;
    }

    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        const Player* player = &game->players[player_index];
        if(player->stack <= 0 && !player->in_hand) order[write_index++] = player_index;
    }
}

#define HOLDEM_CARD_SLOT_WIDTH 16u
#define HOLDEM_SUIT_ICON_WIDTH 8u
#define HOLDEM_SUIT_BITMAP_SIZE 7u
#define HOLDEM_CARD_AREA_START_X 96u
#define HOLDEM_HIDDEN_TOKEN_EXTRA_GAP 1u
#define HOLDEM_TURN_MARK_X 0u
#define HOLDEM_NAME_X 8u
#define HOLDEM_ROLE_X 40u
#define HOLDEM_STATUS_X 46u
#define HOLDEM_STACK_DOLLAR_X 58u
#define HOLDEM_STACK_VALUE_X 64u

// Bitmap suit pips let us keep compact card rendering without sacrificing readability.
// The glyphs were tuned around the current table spacing and should be treated as a stable asset.
static const uint8_t k_holdem_suit_bitmap[4][HOLDEM_SUIT_BITMAP_SIZE] = {
    {0x1Cu, 0x1Cu, 0x6Bu, 0x7Fu, 0x6Bu, 0x08u, 0x1Cu},
    {0x08u, 0x14u, 0x22u, 0x41u, 0x22u, 0x14u, 0x08u},
    {0x22u, 0x55u, 0x49u, 0x41u, 0x22u, 0x14u, 0x08u},
    {0x08u, 0x1Cu, 0x3Eu, 0x7Fu, 0x6Bu, 0x08u, 0x1Cu},
};

// FontSecondary is effectively fixed-width, so a simple width estimate keeps the layout fast.
static uint8_t holdem_text_width(const char* text) {
    return (uint8_t)(strlen(text) * 6u);
}

// Footer controls need to stay readable as table density increases, so we anchor
// the left, center, and right segments independently and only tighten them if the
// measured widths would collide.
static void holdem_draw_action_footer(
    Canvas* canvas,
    int32_t bet_total,
    const char* ok_action,
    uint8_t baseline_y) {
    const char* left_text = "L Fold";
    char center_text[16];
    char right_text[24];
    int left_x = 0;
    int center_x;
    int left_width;
    int center_width;
    int right_width;
    const int min_gap = 2;

    snprintf(center_text, sizeof(center_text), "$%d", (int)bet_total);
    snprintf(right_text, sizeof(right_text), "%s OK", ok_action);

    left_width = holdem_text_width(left_text);
    center_width = holdem_text_width(center_text);
    right_width = holdem_text_width(right_text);

    center_x = 64 - (center_width / 2);

    if(center_x < (left_x + left_width + min_gap)) {
        center_x = left_x + left_width + min_gap;
    }

    if((center_x + center_width + min_gap) > (127 - right_width)) {
        center_x = (127 - right_width) - center_width - min_gap;
    }

    if(center_x < (left_x + left_width + min_gap)) {
        center_x = left_x + left_width + min_gap;
    }

    canvas_draw_str(canvas, left_x, baseline_y, left_text);
    canvas_draw_str(canvas, (uint8_t)center_x, baseline_y, center_text);
    canvas_draw_str_aligned(canvas, 127, baseline_y, AlignRight, AlignBottom, right_text);
}

static void holdem_draw_table_header(Canvas* canvas, const HoldemGame* game) {
    char left_text[20];
    char center_text[16];
    char right_text[24];
    int left_width;
    int center_width;
    int right_width;
    int center_x;
    const float min_gap = 6.0f;

    snprintf(left_text, sizeof(left_text), "Hand %d", (int)(game->hand_no + 1));
    snprintf(center_text, sizeof(center_text), "%s", k_stage_name[game->stage]);
    snprintf(right_text, sizeof(right_text), "Pot: $%d", (int)game->pot);

    left_width = canvas_string_width(canvas, left_text);
    center_width = canvas_string_width(canvas, center_text);
    right_width = canvas_string_width(canvas, right_text);

    float left_end = (float)left_width;
    float right_start = 127.0f - (float)right_width;
    float available_gap = right_start - left_end;

    if(available_gap < (float)center_width + (min_gap * 2.0f)) {
        snprintf(left_text, sizeof(left_text), "H%d", (int)(game->hand_no + 1));
        left_width = canvas_string_width(canvas, left_text);
        left_end = (float)left_width;
        available_gap = right_start - left_end;
    }

    if(available_gap < (float)center_width + (min_gap * 2.0f)) {
        snprintf(right_text, sizeof(right_text), "P: $%d", (int)game->pot);
        right_width = canvas_string_width(canvas, right_text);
        right_start = 127.0f - (float)right_width;
        available_gap = right_start - left_end;
    }

    center_x = (int)(left_end + ((right_start - left_end - (float)center_width) / 2.0f) + 0.5f);

    if((float)center_x < (left_end + min_gap)) {
        center_x = (int)(left_end + min_gap + 0.5f);
    }

    if(((float)center_x + (float)center_width + min_gap) > right_start) {
        center_x = (int)(right_start - (float)center_width - min_gap + 0.5f);
    }

    if((float)center_x < (left_end + min_gap)) {
        center_x = (int)(left_end + min_gap + 0.5f);
    }

    canvas_draw_str(canvas, 0, 8, left_text);
    canvas_draw_str(canvas, (uint8_t)center_x, 8, center_text);
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, right_text);
}

static void holdem_draw_suit_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y, uint8_t suit) {
    int icon_x = (int)x;
    int icon_y = (int)baseline_y - 7;

    for(uint8_t row = 0; row < HOLDEM_SUIT_BITMAP_SIZE; row++) {
        uint8_t row_bits = k_holdem_suit_bitmap[suit & 0x03u][row];
        for(uint8_t col = 0; col < HOLDEM_SUIT_BITMAP_SIZE; col++) {
            if(row_bits & (uint8_t)(1u << (HOLDEM_SUIT_BITMAP_SIZE - 1u - col))) {
                canvas_draw_dot(canvas, icon_x + col, icon_y + row);
            }
        }
    }
}

static uint8_t holdem_draw_card_token(Canvas* canvas, uint8_t x, uint8_t baseline_y, const char* token_text) {
    size_t token_len = strlen(token_text);
    uint8_t token_width = holdem_text_width(token_text);
    uint8_t token_x =
        (uint8_t)(x + ((HOLDEM_CARD_SLOT_WIDTH - token_width) / 2u) - 2u);

    // Hidden/folded placeholders benefit from a tiny extra gap so the repeated
    // glyphs do not visually merge next to the larger suit-icon card slots.
    if(token_len == 2 && token_text[0] == token_text[1]) {
        char left_char[2] = {token_text[0], '\0'};
        char right_char[2] = {token_text[1], '\0'};
        canvas_draw_str(canvas, token_x, baseline_y, left_char);
        canvas_draw_str(
            canvas,
            (uint8_t)(token_x + 6u + HOLDEM_HIDDEN_TOKEN_EXTRA_GAP),
            baseline_y,
            right_char);
    } else {
        canvas_draw_str(canvas, token_x, baseline_y, token_text);
    }
    return (uint8_t)(x + HOLDEM_CARD_SLOT_WIDTH);
}

static uint8_t holdem_draw_card(Canvas* canvas, uint8_t x, uint8_t baseline_y, Card card) {
    char rank_text[2] = {0};
    char card_text[3];
    uint8_t suit_x;

    card_to_str(card, card_text);
    rank_text[0] = card_text[0];
    canvas_draw_str(canvas, x, baseline_y, rank_text);

    suit_x = (uint8_t)(x + 6u + ((HOLDEM_SUIT_ICON_WIDTH - HOLDEM_SUIT_BITMAP_SIZE) / 2u));
    holdem_draw_suit_icon(canvas, suit_x, baseline_y, card.suit);
    return (uint8_t)(x + HOLDEM_CARD_SLOT_WIDTH);
}

static void holdem_draw_board_row(Canvas* canvas, const HoldemGame* game) {
    const char* table_prefix = "Table:";
    uint8_t draw_x = 0;

    canvas_draw_str(canvas, draw_x, 16, table_prefix);
    draw_x = (uint8_t)(holdem_text_width(table_prefix) + 6u);

    if(game->board_count == 0) {
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignBottom, "--");
        return;
    }

    for(size_t board_index = 0; board_index < game->board_count; board_index++) {
        draw_x = holdem_draw_card(canvas, draw_x, 16, game->board[board_index]);
    }
}


static void holdem_draw_pause_cards(Canvas* canvas, uint8_t baseline_y, const char* label, const Card* cards, size_t card_count) {
    uint8_t draw_x = 0;

    canvas_draw_str(canvas, draw_x, baseline_y, label);
    draw_x = (uint8_t)(holdem_text_width(label) + 2u);

    for(size_t card_index = 0; card_index < card_count; card_index++) {
        draw_x = holdem_draw_card(canvas, draw_x, baseline_y, cards[card_index]);
    }
}


static void holdem_draw_firework(Canvas* canvas, int cx, int cy, uint8_t phase) {
    static const int8_t burst_dx[8] = {0, 5, 7, 5, 0, -5, -7, -5};
    static const int8_t burst_dy[8] = {-7, -5, 0, 5, 7, 5, 0, -5};
    uint8_t radius = (uint8_t)(2u + (phase % 4u));

    canvas_draw_dot(canvas, cx, cy);
    for(size_t ray = 0; ray < 8; ray++) {
        int x = cx + ((burst_dx[ray] * (int)radius) / 4);
        int y = cy + ((burst_dy[ray] * (int)radius) / 4);
        if(x >= 0 && x < 128 && y >= 0 && y < 64) canvas_draw_dot(canvas, x, y);
    }
}

static void holdem_draw_interstitial_screen(Canvas* canvas, const HoldemApp* app) {
    uint32_t anim_tick = furi_get_tick() / 120u;

    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, app->interstitial_text);
    if(app->interstitial_fireworks) {
        holdem_draw_firework(canvas, 24, 18, (uint8_t)(anim_tick & 0x03u));
        holdem_draw_firework(canvas, 104, 18, (uint8_t)((anim_tick + 1u) & 0x03u));
        holdem_draw_firework(canvas, 20, 48, (uint8_t)((anim_tick + 2u) & 0x03u));
        holdem_draw_firework(canvas, 108, 48, (uint8_t)((anim_tick + 3u) & 0x03u));
    }
}

static int32_t holdem_total_bankroll(const HoldemGame* game) {
    int32_t total = game->pot;
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        total += game->players[player_index].stack;
    }
    return total;
}

static bool qualifies_for_big_win(const HoldemApp* app, const PayoutResult* payout) {
    int32_t total_bankroll = holdem_total_bankroll(&app->game);
    if(total_bankroll <= 0 || payout->count == 0) return false;

    int you_index = -1;
    for(size_t player_index = 0; player_index < app->game.player_count; player_index++) {
        if(!app->game.players[player_index].is_bot) {
            you_index = (int)player_index;
            break;
        }
    }
    if(you_index < 0) return false;

    return payout->payout[you_index] * 5 >= total_bankroll;
}

static bool show_interstitial_screen(HoldemApp* app, const char* text, bool fireworks, uint32_t duration_ms) {
    // Reuse the same mode for timed celebration cards and persistent endgame screens.
    // `duration_ms == 0` means "wait for explicit dismissal."
    holdem_set_foreground_mode(app, UiModeBigWin);
    snprintf(app->interstitial_text, sizeof(app->interstitial_text), "%s", text);
    app->interstitial_fireworks = fireworks;
    view_port_update(app->view_port);
    flush_input_queue(app);

    uint32_t start_tick = furi_get_tick();
    InputEvent ev;
    while(app->running) {
        if(duration_ms > 0 && (furi_get_tick() - start_tick) >= duration_ms) break;
        view_port_update(app->view_port);
        if(furi_message_queue_get(app->input_queue, &ev, 50) != FuriStatusOk) continue;
        if(app->mode == UiModeBigWin && ev.type == InputTypeShort && (ev.key == InputKeyOk || ev.key == InputKeyBack)) break;
        if(process_global_event(app, &ev)) {
            if(app->exit_requested) return false;
        }
    }

    holdem_set_foreground_mode(app, UiModeTable);
    return !app->exit_requested;
}

static bool show_big_win_screen(HoldemApp* app) {
    return show_interstitial_screen(app, "Big Win!", true, 2000u);
}

static void holdem_draw_player_row(
    Canvas* canvas,
    const HoldemGame* game,
    const Player* player,
    int player_index,
    int active_idx,
    bool showdown_waiting,
    uint8_t showdown_winner_mask,
    bool reveal_all,
    uint8_t baseline_y) {
    char stack_text[7];
    const char* turn_mark = "  ";
    uint8_t draw_x = HOLDEM_CARD_AREA_START_X;
    char role_text[2] = {0};
    char status_text[2] = {0};

    if(showdown_waiting && ((showdown_winner_mask >> player_index) & 0x1u)) {
        turn_mark = "* ";
    } else if(player_index == active_idx) {
        turn_mark = "> ";
    }

    snprintf(stack_text, sizeof(stack_text), "%d", (int)player->stack);
    role_text[0] = holdem_role_char(game, player_index);
    status_text[0] = holdem_status_char(player);

    // Each row uses fixed columns so the pointer, name, role/status, stack, and
    // card area remain stable as turns change and stack widths grow.
    canvas_draw_str(canvas, HOLDEM_TURN_MARK_X, baseline_y, turn_mark);
    canvas_draw_str(canvas, HOLDEM_NAME_X, baseline_y, holdem_display_name(player));
    canvas_draw_str(canvas, HOLDEM_ROLE_X, baseline_y, role_text);
    canvas_draw_str(canvas, HOLDEM_STATUS_X, baseline_y, status_text);
    canvas_draw_str(canvas, HOLDEM_STACK_DOLLAR_X, baseline_y, "$");
    canvas_draw_str(canvas, HOLDEM_STACK_VALUE_X, baseline_y, stack_text);

    if(!player->is_bot) {
        draw_x = holdem_draw_card(canvas, draw_x, baseline_y, player->hole[0]);
        holdem_draw_card(canvas, draw_x, baseline_y, player->hole[1]);
    } else if(game->stage == StageShowdown) {
        if(player->in_hand) {
            draw_x = holdem_draw_card(canvas, draw_x, baseline_y, player->hole[0]);
            holdem_draw_card(canvas, draw_x, baseline_y, player->hole[1]);
        } else {
            draw_x = holdem_draw_card_token(canvas, draw_x, baseline_y, "XX");
            holdem_draw_card_token(canvas, draw_x, baseline_y, "XX");
        }
    } else if(!player->in_hand) {
        draw_x = holdem_draw_card_token(canvas, draw_x, baseline_y, "XX");
        holdem_draw_card_token(canvas, draw_x, baseline_y, "XX");
    } else if(reveal_all) {
        draw_x = holdem_draw_card(canvas, draw_x, baseline_y, player->hole[0]);
        holdem_draw_card(canvas, draw_x, baseline_y, player->hole[1]);
    } else {
        draw_x = holdem_draw_card_token(canvas, draw_x, baseline_y, "??");
        holdem_draw_card_token(canvas, draw_x, baseline_y, "??");
    }
}
static void holdem_draw_callback(Canvas* canvas, void* ctx) {
    HoldemApp* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(app->mode == UiModeSplash) {
        // Splash art: dolphin at a card table while startup jingle plays.
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Hold 'em");
        canvas_set_font(canvas, FontSecondary);

        // Two simple card outlines near the right side.
        canvas_draw_frame(canvas, 96, 16, 14, 18);
        canvas_draw_frame(canvas, 108, 20, 14, 18);
        canvas_draw_str(canvas, 100, 28, "A");
        canvas_draw_str(canvas, 112, 32, "K");

        // Dolphin line art sized for 128x64 monochrome display.
        canvas_draw_line(canvas, 14, 34, 30, 28);
        canvas_draw_line(canvas, 30, 28, 50, 30);
        canvas_draw_line(canvas, 50, 30, 62, 36);
        canvas_draw_line(canvas, 62, 36, 48, 40);
        canvas_draw_line(canvas, 48, 40, 26, 40);
        canvas_draw_line(canvas, 26, 40, 14, 34);
        canvas_draw_line(canvas, 40, 29, 44, 22);
        canvas_draw_line(canvas, 44, 22, 48, 30);
        canvas_draw_line(canvas, 14, 34, 6, 30);
        canvas_draw_line(canvas, 14, 34, 5, 38);
        canvas_draw_dot(canvas, 53, 34);

        // Table rail under the dolphin.
        canvas_draw_line(canvas, 8, 44, 88, 44);
        canvas_draw_line(canvas, 10, 45, 86, 45);
        holdem_draw_line_right(canvas, 64, "v1.1");
        return;
    }

    if(app->mode == UiModeStartChoice) {
        holdem_draw_line(canvas, 8, "Saved game found");
        holdem_draw_line(canvas, 16, "");
        holdem_draw_line(canvas, 24, "OK: Load save");
        holdem_draw_line(canvas, 32, "Back: New game");
        return;
    }

    if(app->mode == UiModeStartReady) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to Start");
        return;
    }

    if(app->mode == UiModeHelp) {
        // Help is intentionally split across 3 focused pages for readability on 128x64.
        char help_line[40];
        int32_t menu_small_blind = app->pending_blinds_dirty ? app->pending_small_blind : app->game.small_blind;
        int32_t menu_big_blind = menu_small_blind * 2;
        snprintf(help_line, sizeof(help_line), "SB/BB: %d/%d", (int)menu_small_blind, (int)menu_big_blind);

        if(app->help_page == 0) {
            holdem_draw_line(canvas, 8, "Controls Help");
            holdem_draw_line_right(canvas, 8, "1/3");
            holdem_draw_line(canvas, 16, "");
            holdem_draw_line(canvas, 24, "OK: Check/Call/Raise");
            holdem_draw_line(canvas, 32, "L: Fold   R: Reset Bet");
            holdem_draw_line(canvas, 40, "Up/Down: Bet +/-");
            holdem_draw_line(canvas, 48, "");
            holdem_draw_line(canvas, 53, "Up/Down: Next/Prev page");
            holdem_draw_line(canvas, 62, "OK/Back: Close help");
        } else if(app->help_page == 1) {
            char blind_line[40];
            char bot_line[40];
            snprintf(
                blind_line,
                sizeof(blind_line),
                "R: Edit blinds (%s%d/%d)",
                app->progressive_blinds_enabled ? "P - " : "",
                (int)menu_small_blind,
                (int)menu_big_blind);
            snprintf(
                bot_line,
                sizeof(bot_line),
                "OK: Edit bots (%d - %s)",
                (int)(app->configured_player_count - 1),
                holdem_ai_label(app->configured_ai_level_pct));
            holdem_draw_line(canvas, 8, "Game Menu");
            holdem_draw_line_right(canvas, 8, "2/3");
            holdem_draw_line(canvas, 16, "");
            holdem_draw_line(canvas, 24, blind_line);
            holdem_draw_line(canvas, 32, bot_line);
            holdem_draw_line(canvas, 40, "L: Start new game");
        } else {
            holdem_draw_line(canvas, 8, "Hand Ranks");
            holdem_draw_line_right(canvas, 8, "3/3");
            holdem_draw_line(canvas, 16, "");
            holdem_draw_line(canvas, 24, "1 Straight Flush");
            holdem_draw_line(canvas, 32, "2 Quads  3 Full House");
            holdem_draw_line(canvas, 40, "4 Flush  5 Straight");
            holdem_draw_line(canvas, 48, "6 Trips  7 Two Pair");
            holdem_draw_line(canvas, 56, "8 Pair   9 High Card");
        }
        return;
    }

    if(app->mode == UiModeBlindEdit) {
        holdem_draw_line(canvas, 8, "Edit Blinds");
        holdem_draw_line(canvas, 16, "");
        if(!app->blind_edit_progressive_enabled) {
            char blind_line[32];
            snprintf(
                blind_line,
                sizeof(blind_line),
                "SB/BB: %d/%d",
                (int)app->blind_edit_sb,
                (int)(app->blind_edit_sb * 2));
            holdem_draw_line(canvas, 24, "Progressive blinds: Off");
            holdem_draw_line(canvas, 32, blind_line);
            holdem_draw_line(canvas, 40, "");
            holdem_draw_line(canvas, 48, "Up/Down: SB +/-");
            holdem_draw_line(canvas, 56, "R: Turn on progressive blinds");
        } else {
            char period_line[32];
            char increase_line[32];
            snprintf(
                period_line,
                sizeof(period_line),
                "Period: %d hands (0=Off)",
                (int)app->blind_edit_progressive_period_hands);
            snprintf(
                increase_line,
                sizeof(increase_line),
                "Increase SB: $%d",
                (int)app->blind_edit_progressive_step_sb);
            holdem_draw_line(canvas, 24, "Progressive blinds: On");
            holdem_draw_line(canvas, 32, period_line);
            holdem_draw_line(canvas, 40, increase_line);
            holdem_draw_line(canvas, 48, "R/L: Hands +/-");
            holdem_draw_line(canvas, 56, "Up/Down: Increase +/-");
        }
        holdem_draw_line_right(canvas, 64, "Back: Cancel");
        return;
    }

    if(app->mode == UiModeBotCountEdit) {
        char count_line[32];
        char difficulty_line[32];
        snprintf(count_line, sizeof(count_line), "Number of bots: %d", (int)(app->bot_count_edit_value - 1));
        snprintf(
            difficulty_line,
            sizeof(difficulty_line),
            "Bot difficulty: %s",
            holdem_ai_label(app->bot_difficulty_edit_value));
        holdem_draw_line(canvas, 8, "Edit Bots");
        holdem_draw_line(canvas, 16, "");
        holdem_draw_line(canvas, 24, count_line);
        holdem_draw_line(canvas, 32, difficulty_line);
        holdem_draw_line(canvas, 40, "");
        holdem_draw_line(canvas, 46, "Up/Down: Bots +/-");
        holdem_draw_line(canvas, 54, "L/R: Set difficulty");
        canvas_draw_str_aligned(canvas, 127, 64, AlignRight, AlignBottom, "Back: Cancel");
        return;
    }

    if(app->mode == UiModeRestartConfirm) {
        holdem_draw_line(canvas, 8, "Restart Required");
        holdem_draw_line(canvas, 20, "This change will");
        holdem_draw_line(canvas, 28, "require a new game.");
        holdem_draw_line(canvas, 40, "Restart now?");
        holdem_draw_line(canvas, 52, "OK: Yes");
        holdem_draw_line(canvas, 60, "Back: Cancel");
        return;
    }

    if(app->mode == UiModeExitPrompt) {
        holdem_draw_line(canvas, 8, "Exit Hold 'em");
        holdem_draw_line(canvas, 16, "");
        holdem_draw_line(canvas, 24, "OK: Save & Exit");
        holdem_draw_line(canvas, 32, "Hold Back: Exit");
        holdem_draw_line_right(canvas, 64, "Back: Cancel");
        return;
    }

    if(app->mode == UiModePause) {
        holdem_draw_line(canvas, 8, app->pause_title);
        holdem_draw_line(canvas, 20, app->pause_body);
        holdem_draw_line(canvas, 32, app->pause_body2);
        if(app->pause_card_count > 0) {
            holdem_draw_pause_cards(canvas, 44, app->pause_cards_label, app->pause_cards, app->pause_card_count);
        } else {
            holdem_draw_line(canvas, 44, app->pause_body3);
        }
        if(app->pause_footer[0]) {
            holdem_draw_line_right(canvas, 64, app->pause_footer);
        }
        return;
    }

    if(app->mode == UiModeBigWin) {
        holdem_draw_interstitial_screen(canvas, app);
        return;
    }

    holdem_draw_table_header(canvas, &app->game);

    holdem_draw_board_row(canvas, &app->game);

    int active_idx = -1;
    if(app->mode == UiModeActionPrompt) active_idx = app->acting_idx;
    else if(app->bot_turn_active) active_idx = app->bot_turn_idx;

    size_t display_order[HOLDEM_MAX_PLAYERS] = {0};
    holdem_build_display_order(&app->game, display_order);

    for(size_t display_row = 0; display_row < app->game.player_count; display_row++) {
        int player_index = (int)display_order[display_row];
        Player* player = &app->game.players[player_index];
        holdem_draw_player_row(
            canvas,
            &app->game,
            player,
            player_index,
            active_idx,
            app->showdown_waiting,
            app->showdown_winner_mask,
            app->reveal_all,
            24 + (uint8_t)(8 * display_row));
    }
    if(app->mode == UiModeActionPrompt) {
        const char* ok_action = "Call";
        if(app->prompt_bet_total <= app->prompt_to_call) {
            ok_action = (app->prompt_to_call == 0) ? "Check" : "Call";
        } else {
            ok_action = "Raise";
        }

        holdem_draw_line(canvas, 56, "");
        holdem_draw_action_footer(canvas, app->prompt_bet_total, ok_action, 64);
    } else if(app->bot_turn_active && app->bot_turn_idx >= 0 && app->bot_turn_idx < (int)app->game.player_count) {
        const char* bot_status = app->bot_turn_text[0] ? app->bot_turn_text : app->game.players[app->bot_turn_idx].name;
        holdem_draw_line(canvas, 56, "");
        canvas_draw_str_aligned(canvas, 64, 64, AlignCenter, AlignBottom, bot_status);
    } else {
        holdem_draw_line(canvas, 56, "");
        if(app->showdown_waiting) {
            holdem_draw_line_right(canvas, 64, "OK: Result screen");
        } else if(app->game.hand_in_progress && !app->game.players[0].in_hand) {
            // Once the human folds, keep the footer quiet between bot messages so the
            // generic help hint does not flash during the rest of the hand.
            holdem_draw_line(canvas, 64, "");
        } else {
            holdem_draw_line_right(canvas, 64, "Back tap Help hold Exit");
        }
    }
}

static void holdem_input_callback(InputEvent* event, void* ctx) {
    HoldemApp* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

static void show_help(HoldemApp* app) {
    app->return_mode = app->mode;
    app->help_page = 0;
    app->mode = UiModeHelp;
    view_port_update(app->view_port);
}

static void show_exit_prompt(HoldemApp* app) {
    app->exit_return_mode = app->mode;
    app->mode = UiModeExitPrompt;
    view_port_update(app->view_port);
}



static void reset_to_new_game(HoldemApp* app) {
    init_game_with_player_count(&app->game, app->configured_player_count);
    set_ai_level(app, app->configured_ai_level_pct);
    if(app->pending_blinds_dirty) {
        holdem_apply_blind_values_now(app, app->pending_small_blind);
        app->pending_blinds_dirty = false;
    }
    if(app->progressive_blinds_enabled) {
        holdem_reset_progressive_schedule(app);
    } else {
        app->progressive_next_raise_hand_no = 0;
    }
    delete_save_on_storage();
    app->mode = UiModeStartReady;
    app->return_mode = UiModeTable;
    app->reveal_all = false;
    app->showdown_waiting = false;
    app->reset_requested = false;
    app->showdown_winner_mask = 0;
    app->bot_turn_active = false;
    app->bot_turn_idx = -1;
    app->bot_turn_text[0] = '\0';
    app->skip_autoplay_requested = false;
    app->pending_small_blind = app->game.small_blind;
    app->blind_edit_sb = app->game.small_blind;
    app->blind_edit_progressive_enabled = app->progressive_blinds_enabled;
    app->blind_edit_progressive_period_hands = app->progressive_period_hands;
    app->blind_edit_progressive_step_sb = app->progressive_step_sb;
}


static bool prompt_showdown_inspect(HoldemApp* app) {
    if(app->skip_autoplay_requested && !app->game.players[0].in_hand) {
        app->showdown_waiting = false;
        app->showdown_winner_mask = 0;
        app->skip_autoplay_requested = false;
        return true;
    }

    holdem_set_foreground_mode(app, UiModeTable);
    app->reveal_all = true;
    app->showdown_waiting = true;
    view_port_update(app->view_port);
    flush_input_queue(app);

    InputEvent ev;
    while(app->running) {
        if(furi_message_queue_get(app->input_queue, &ev, FuriWaitForever) != FuriStatusOk) continue;
        if(process_global_event(app, &ev)) {
            if(app->exit_requested) {
                app->showdown_waiting = false;
                app->showdown_winner_mask = 0;
                return false;
            }
            continue;
        }
        if(ev.type == InputTypeShort && ev.key == InputKeyOk) {
            app->showdown_waiting = false;
            app->showdown_winner_mask = 0;
            return true;
        }
    }

    app->showdown_waiting = false;
    app->showdown_winner_mask = 0;
    return false;
}
static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

static void set_ai_level(HoldemApp* app, uint8_t level_pct) {
    app->ai_level_pct = (uint8_t)clamp_i32(level_pct, HOLDEM_AI_EASY_LEVEL, HOLDEM_AI_EXTREME_LEVEL);
}

static void set_bot_action_delay(HoldemApp* app, bool enabled, uint16_t delay_ms) {
    app->bot_delay_enabled = enabled;
    app->bot_delay_ms = (uint16_t)clamp_i32((int32_t)delay_ms, 0, 5000);
}

static uint8_t holdem_clamp_progressive_period(uint8_t period_hands) {
    return (uint8_t)clamp_i32(
        period_hands, HOLDEM_PROGRESSIVE_MIN_PERIOD_HANDS, HOLDEM_PROGRESSIVE_MAX_PERIOD_HANDS);
}

static int32_t holdem_clamp_progressive_step(int32_t step_sb) {
    return clamp_i32(step_sb, HOLDEM_PROGRESSIVE_MIN_STEP_SB, HOLDEM_PROGRESSIVE_MAX_STEP_SB);
}

static void holdem_reset_progressive_schedule(HoldemApp* app) {
    app->progressive_next_raise_hand_no = app->game.hand_no + app->progressive_period_hands;
}

static void holdem_apply_blind_values_now(HoldemApp* app, int32_t new_small_blind) {
    app->game.small_blind = new_small_blind;
    app->game.big_blind = new_small_blind * 2;
}

static void holdem_apply_progressive_increase_if_due(HoldemApp* app) {
    if(!app->progressive_blinds_enabled) return;

    while(app->game.hand_no >= app->progressive_next_raise_hand_no) {
        int32_t next_small_blind =
            clamp_i32(app->game.small_blind + app->progressive_step_sb, HOLDEM_BLIND_STEP_SB, 500);
        holdem_apply_blind_values_now(app, next_small_blind);
        app->progressive_next_raise_hand_no += app->progressive_period_hands;
    }
}

// Returns true when the current hand has not progressed past forced blinds.
// In that narrow window, a blind change can be safely applied immediately.
static bool can_apply_blinds_immediately(const HoldemGame* game) {
    if(!game->hand_in_progress) return true;
    if(game->stage != StagePreflop) return false;

    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        int32_t contributed = game->players[player_index].hand_contrib;

        if((int)player_index == game->sb_idx) {
            if(contributed > game->small_blind) return false;
            continue;
        }

        if((int)player_index == game->bb_idx) {
            if(contributed > game->big_blind) return false;
            continue;
        }

        if(contributed > 0) return false;
    }

    return true;
}

// Applies blind values now if safe, otherwise defers to the next hand start.
static void apply_or_defer_blind_edit(HoldemApp* app, int32_t new_small_blind) {
    new_small_blind = clamp_i32(new_small_blind, HOLDEM_BLIND_STEP_SB, 500);

    if(can_apply_blinds_immediately(&app->game)) {
        holdem_apply_blind_values_now(app, new_small_blind);
        app->pending_blinds_dirty = false;
    } else {
        app->pending_small_blind = new_small_blind;
        app->pending_blinds_dirty = true;
    }
}

static void holdem_commit_blind_edit(HoldemApp* app) {
    if(app->blind_edit_progressive_enabled) {
        bool was_disabled = !app->progressive_blinds_enabled;
        app->progressive_blinds_enabled = true;
        app->progressive_period_hands =
            holdem_clamp_progressive_period(app->blind_edit_progressive_period_hands);
        app->progressive_step_sb = holdem_clamp_progressive_step(app->blind_edit_progressive_step_sb);
        if(was_disabled) {
            holdem_reset_progressive_schedule(app);
        }
    } else {
        bool was_enabled = app->progressive_blinds_enabled;
        app->progressive_blinds_enabled = false;
        if(was_enabled) {
            app->progressive_next_raise_hand_no = 0;
        }
        apply_or_defer_blind_edit(app, app->blind_edit_sb);
    }
}

// Handles short-back behavior consistently across all screens.
static void handle_back_short(HoldemApp* app) {
    if(app->mode == UiModeStartChoice) {
        delete_save_on_storage();
        app->mode = UiModeStartReady;
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeBlindEdit) {
        // Cancel blind edit on short back.
        app->mode = UiModeHelp;
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeBotCountEdit) {
        app->mode = UiModeHelp;
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeRestartConfirm) {
        app->mode = UiModeHelp;
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeHelp) {
        app->mode = app->return_mode;
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeExitPrompt) {
        app->mode = app->exit_return_mode;
        view_port_update(app->view_port);
        return;
    }

    if(app->mode == UiModeTable || app->mode == UiModeActionPrompt) {
        show_help(app);
        return;
    }
}

// Handles long-back behavior consistently across all screens.
static void handle_back_long(HoldemApp* app) {
    if(app->mode == UiModeExitPrompt) {
        app->save_on_exit = false;
        app->exit_requested = true;
        app->running = false;
    } else {
        show_exit_prompt(app);
    }
}

// Centralized cross-screen input handling (help, exit flow, blind edit).
static bool process_global_event(HoldemApp* app, const InputEvent* ev) {
    if(!app || !ev) return false;

    if(ev->key == InputKeyBack) {
        if(ev->type == InputTypePress) {
            app->back_down = true;
            app->back_long_handled = false;
            app->back_press_tick = furi_get_tick();
            return true;
        }

        if(ev->type == InputTypeRepeat || ev->type == InputTypeLong) {
            if(app->back_down && !app->back_long_handled) {
                uint32_t held_ms = furi_get_tick() - app->back_press_tick;
                if(held_ms >= HOLDEM_BACK_HOLD_MS) {
                    app->back_long_handled = true;
                    app->back_down = false;
                    handle_back_long(app);
                }
            }
            return true;
        }

        if(ev->type == InputTypeRelease) {
            uint32_t held_ms = app->back_down ? (furi_get_tick() - app->back_press_tick) : 0;
            bool long_handled = app->back_long_handled;
            app->back_down = false;
            app->back_long_handled = false;

            if(!long_handled) {
                if(held_ms >= HOLDEM_BACK_HOLD_MS) {
                    // Fallback path if long event was not emitted while held.
                    handle_back_long(app);
                } else {
                    handle_back_short(app);
                }
            }
            return true;
        }

        if(ev->type == InputTypeShort) {
            // Some firmwares emit short without usable press/release sequencing.
            if(!app->back_down && !app->back_long_handled) {
                handle_back_short(app);
            }
            return true;
        }
    }

    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;

    if(app->mode == UiModeStartChoice) {
        if(ev->key == InputKeyOk) {
            uint8_t loaded_ai_level = HOLDEM_AI_DEFAULT_LEVEL;
            app->progressive_blinds_enabled = false;
            app->progressive_period_hands = HOLDEM_PROGRESSIVE_DEFAULT_PERIOD_HANDS;
            app->progressive_step_sb = HOLDEM_PROGRESSIVE_DEFAULT_STEP_SB;
            app->progressive_next_raise_hand_no = 0;
            (void)load_progress(
                &app->game,
                &loaded_ai_level,
                &app->progressive_blinds_enabled,
                &app->progressive_period_hands,
                &app->progressive_step_sb,
                &app->progressive_next_raise_hand_no);
            set_ai_level(app, loaded_ai_level);
            app->configured_player_count = app->game.player_count;
            app->bot_count_edit_value = app->configured_player_count;
            app->configured_ai_level_pct = app->ai_level_pct;
            app->bot_difficulty_edit_value = app->configured_ai_level_pct;
            app->pending_blinds_dirty = false;
            app->pending_small_blind = app->game.small_blind;
            app->blind_edit_sb = app->game.small_blind;
            app->blind_edit_progressive_enabled = app->progressive_blinds_enabled;
            app->blind_edit_progressive_period_hands = app->progressive_period_hands;
            app->blind_edit_progressive_step_sb = app->progressive_step_sb;
            app->mode = UiModeTable;
            view_port_update(app->view_port);
            return true;
        }
        return false;
    }

    if(app->mode == UiModeStartReady) {
        if(ev->key == InputKeyOk) {
            app->mode = UiModeTable;
            view_port_update(app->view_port);
            return true;
        }
        return true;
    }

    if(app->mode == UiModeExitPrompt) {
        if(ev->key == InputKeyOk) {
            app->save_on_exit = true;
            app->exit_requested = true;
            app->running = false;
            return true;
        }
        return false;
    }

    if(app->mode == UiModeHelp) {
        if(ev->key == InputKeyUp) {
            app->help_page = (uint8_t)((app->help_page + 2) % 3);
            view_port_update(app->view_port);
            return true;
        }

        if(ev->key == InputKeyDown) {
            app->help_page = (uint8_t)((app->help_page + 1) % 3);
            view_port_update(app->view_port);
            return true;
        }

        if(app->help_page == 1 && ev->key == InputKeyOk) {
            app->bot_count_edit_value = app->configured_player_count;
            app->bot_difficulty_edit_value = app->configured_ai_level_pct;
            app->mode = UiModeBotCountEdit;
            view_port_update(app->view_port);
            return true;
        }

        if(ev->key == InputKeyOk) {
            app->mode = app->return_mode;
            view_port_update(app->view_port);
            return true;
        }

        if(app->help_page == 1 && ev->key == InputKeyRight) {
            app->blind_edit_sb = app->pending_blinds_dirty ? app->pending_small_blind : app->game.small_blind;
            app->blind_edit_progressive_enabled = app->progressive_blinds_enabled;
            app->blind_edit_progressive_period_hands = app->progressive_period_hands;
            app->blind_edit_progressive_step_sb = app->progressive_step_sb;
            app->mode = UiModeBlindEdit;
            view_port_update(app->view_port);
            return true;
        }

        if(app->help_page == 1 && ev->key == InputKeyLeft) {
            reset_to_new_game(app);
            app->reset_requested = true;
            view_port_update(app->view_port);
            return true;
        }

        return false;
    }

    if(app->mode == UiModeBlindEdit) {
        if(ev->key == InputKeyUp) {
            if(app->blind_edit_progressive_enabled) {
                app->blind_edit_progressive_step_sb = holdem_clamp_progressive_step(
                    app->blind_edit_progressive_step_sb + HOLDEM_BLIND_STEP_SB);
            } else {
                app->blind_edit_sb =
                    clamp_i32(app->blind_edit_sb + HOLDEM_BLIND_STEP_SB, HOLDEM_BLIND_STEP_SB, 500);
            }
            view_port_update(app->view_port);
            return true;
        }

        if(ev->key == InputKeyDown) {
            if(app->blind_edit_progressive_enabled) {
                app->blind_edit_progressive_step_sb = holdem_clamp_progressive_step(
                    app->blind_edit_progressive_step_sb - HOLDEM_BLIND_STEP_SB);
            } else {
                app->blind_edit_sb =
                    clamp_i32(app->blind_edit_sb - HOLDEM_BLIND_STEP_SB, HOLDEM_BLIND_STEP_SB, 500);
            }
            view_port_update(app->view_port);
            return true;
        }

        if(ev->key == InputKeyRight) {
            if(app->blind_edit_progressive_enabled) {
                app->blind_edit_progressive_period_hands = holdem_clamp_progressive_period(
                    app->blind_edit_progressive_period_hands +
                    HOLDEM_PROGRESSIVE_PERIOD_STEP_HANDS);
            } else {
                app->blind_edit_progressive_enabled = true;
                app->blind_edit_progressive_period_hands = HOLDEM_PROGRESSIVE_DEFAULT_PERIOD_HANDS;
                app->blind_edit_progressive_step_sb = HOLDEM_PROGRESSIVE_DEFAULT_STEP_SB;
            }
            view_port_update(app->view_port);
            return true;
        }

        if(ev->key == InputKeyLeft && app->blind_edit_progressive_enabled) {
            if(app->blind_edit_progressive_period_hands > HOLDEM_PROGRESSIVE_MIN_PERIOD_HANDS) {
                app->blind_edit_progressive_period_hands = holdem_clamp_progressive_period(
                    app->blind_edit_progressive_period_hands -
                    HOLDEM_PROGRESSIVE_PERIOD_STEP_HANDS);
            } else {
                app->blind_edit_progressive_enabled = false;
            }
            view_port_update(app->view_port);
            return true;
        }

        if(ev->key == InputKeyOk) {
            holdem_commit_blind_edit(app);
            app->mode = UiModeHelp;
            view_port_update(app->view_port);
            return true;
        }

        return false;
    }

    if(app->mode == UiModeBotCountEdit) {
        if(ev->key == InputKeyUp) {
            if(app->bot_count_edit_value < HOLDEM_MAX_PLAYERS) app->bot_count_edit_value++;
            view_port_update(app->view_port);
            return true;
        }

        if(ev->key == InputKeyDown) {
            if(app->bot_count_edit_value > HOLDEM_MIN_PLAYERS) app->bot_count_edit_value--;
            view_port_update(app->view_port);
            return true;
        }

        if(ev->key == InputKeyLeft) {
            app->bot_difficulty_edit_value = holdem_prev_ai_level(app->bot_difficulty_edit_value);
            view_port_update(app->view_port);
            return true;
        }

        if(ev->key == InputKeyRight) {
            app->bot_difficulty_edit_value = holdem_next_ai_level(app->bot_difficulty_edit_value);
            view_port_update(app->view_port);
            return true;
        }

        if(ev->key == InputKeyOk) {
            bool bot_count_changed = (app->bot_count_edit_value != app->configured_player_count);
            app->configured_ai_level_pct = app->bot_difficulty_edit_value;
            set_ai_level(app, app->configured_ai_level_pct);
            if(!bot_count_changed) {
                app->mode = UiModeHelp;
            } else {
                app->mode = UiModeRestartConfirm;
            }
            view_port_update(app->view_port);
            return true;
        }

        return false;
    }

    if(app->mode == UiModeRestartConfirm) {
        if(ev->key == InputKeyOk) {
            app->configured_player_count = app->bot_count_edit_value;
            reset_to_new_game(app);
            app->reset_requested = true;
            view_port_update(app->view_port);
            return true;
        }

        return false;
    }

    return false;
}
// Drops queued input events so prior key presses never bleed into the next prompt.
static void flush_input_queue(HoldemApp* app) {
    InputEvent queued_event;
    while(furi_message_queue_get(app->input_queue, &queued_event, 0) == FuriStatusOk) {
    }
}

static bool holdem_can_skip_folded_autoplay(const HoldemApp* app) {
    return app->game.hand_in_progress && !app->game.players[0].in_hand && !app->showdown_waiting;
}
// Render hand result summary and block until player acknowledgement.
static void show_hand_result_screen(HoldemApp* app, const PayoutResult* payout) {
    holdem_set_foreground_mode(app, UiModePause);
    app->reveal_all = true;
    app->skip_autoplay_requested = false;

    int wi = (payout->count > 0) ? payout->idx[0] : -1;
    int32_t amt = (wi >= 0) ? payout->payout[wi] : 0;

    snprintf(app->pause_title, sizeof(app->pause_title), "Hand %d Result", app->game.hand_no);
    snprintf(app->pause_footer, sizeof(app->pause_footer), "OK: Next hand");
    app->pause_cards_label[0] = '\0';
    app->pause_card_count = 0;
    if(wi >= 0) {
        snprintf(app->pause_body, sizeof(app->pause_body), "%s +$%d", app->game.players[wi].name, (int)amt);

        if(app->game.stage == StageShowdown) {
            Card seven[7];
            Card best_five[5];
            seven[0] = app->game.players[wi].hole[0];
            seven[1] = app->game.players[wi].hole[1];
            for(size_t i = 0; i < 5; i++) seven[2 + i] = app->game.board[i];
            Score s = best_five_from_seven_with_cards(seven, best_five);
            sort_five_for_display(best_five);
            snprintf(app->pause_body2, sizeof(app->pause_body2), "Hand: %s", hand_rank_label(s.v[0]));
            snprintf(app->pause_cards_label, sizeof(app->pause_cards_label), "Best5:");
            for(size_t card_index = 0; card_index < 5; card_index++) app->pause_cards[card_index] = best_five[card_index];
            app->pause_card_count = 5;
            app->pause_body3[0] = '\0';
        } else if(!app->game.players[wi].is_bot) {
            snprintf(app->pause_body2, sizeof(app->pause_body2), "Hand: Hole Cards");
            snprintf(app->pause_cards_label, sizeof(app->pause_cards_label), "Cards:");
            app->pause_cards[0] = app->game.players[wi].hole[0];
            app->pause_cards[1] = app->game.players[wi].hole[1];
            app->pause_card_count = 2;
            app->pause_body3[0] = '\0';
        } else {
            snprintf(app->pause_body2, sizeof(app->pause_body2), "Hand: XX");
            snprintf(app->pause_body3, sizeof(app->pause_body3), "Best5: XX");
        }
    } else {
        snprintf(app->pause_body, sizeof(app->pause_body), "No payout");
        snprintf(app->pause_body2, sizeof(app->pause_body2), "Hand: XX");
        snprintf(app->pause_body3, sizeof(app->pause_body3), "Best5: XX");
    }

    view_port_update(app->view_port);
    flush_input_queue(app);

    InputEvent ev;
    while(app->running) {
        if(furi_message_queue_get(app->input_queue, &ev, FuriWaitForever) != FuriStatusOk) continue;
        if(process_global_event(app, &ev)) {
            if(app->exit_requested) break;
            continue;
        }
        if(app->mode == UiModePause && ev.type == InputTypeShort && ev.key == InputKeyOk) break;
    }

    holdem_set_foreground_mode(app, UiModeTable);
    app->reveal_all = false;
}

// Capture human action for current decision point (fold/check/call/raise).
static void wait_human_action(HoldemApp* app, int acting_idx, int32_t to_call, int32_t min_raise) {
    holdem_set_foreground_mode(app, UiModeActionPrompt);
    app->acting_idx = acting_idx;
    app->prompt_to_call = to_call;
    app->prompt_min_raise = min_raise;
    app->prompt_raise_by = 0;
    app->prompt_bet_total = to_call;
    app->action_ready = false;
    app->bot_turn_active = false;
    app->bot_turn_idx = -1;
    app->bot_turn_text[0] = '\0';

    int32_t stack = app->game.players[acting_idx].stack;
    int32_t max_total = stack;
    if(max_total < to_call) max_total = to_call;

    view_port_update(app->view_port);
    flush_input_queue(app);

    InputEvent ev;
    while(app->running && !app->action_ready) {
        if(furi_message_queue_get(app->input_queue, &ev, FuriWaitForever) != FuriStatusOk) continue;
        if(process_global_event(app, &ev)) {
            if(app->exit_requested || app->reset_requested) {
                app->chosen_action = ActFold;
                app->chosen_raise_by = 0;
                app->action_ready = true;
            }
            continue;
        }
        if(app->mode != UiModeActionPrompt) continue;
        if(ev.type != InputTypeShort && ev.type != InputTypeRepeat) continue;

        if(ev.key == InputKeyLeft) {
            app->chosen_action = ActFold;
            app->chosen_raise_by = 0;
            app->action_ready = true;
        } else if(ev.key == InputKeyOk) {
            if(app->prompt_bet_total <= to_call) {
                app->chosen_action = (to_call == 0) ? ActCheck : ActCall;
                app->chosen_raise_by = 0;
            } else {
                int32_t raise_by = app->prompt_bet_total - to_call;
                if(raise_by < min_raise) raise_by = min_raise;
                if(raise_by > (stack - to_call)) raise_by = stack - to_call;
                if(raise_by > 0) {
                    app->chosen_action = ActRaise;
                    app->chosen_raise_by = raise_by;
                } else {
                    app->chosen_action = (to_call == 0) ? ActCheck : ActCall;
                    app->chosen_raise_by = 0;
                }
            }
            app->action_ready = true;
        } else if(ev.key == InputKeyUp) {
            if(app->prompt_bet_total < max_total) {
                int32_t next = app->prompt_bet_total + app->game.big_blind;
                if(next > max_total) next = max_total;
                if(next > to_call && next < (to_call + min_raise)) next = to_call + min_raise;
                if(next > max_total) next = max_total;
                app->prompt_bet_total = next;
                app->prompt_raise_by = (app->prompt_bet_total > to_call) ? (app->prompt_bet_total - to_call) : 0;
            }
        } else if(ev.key == InputKeyDown) {
            int32_t next = app->prompt_bet_total - app->game.big_blind;
            if(next < to_call) next = to_call;
            app->prompt_bet_total = next;
            app->prompt_raise_by = (app->prompt_bet_total > to_call) ? (app->prompt_bet_total - to_call) : 0;
        } else if(ev.key == InputKeyRight) {
            app->prompt_bet_total = to_call;
            app->prompt_raise_by = 0;
        }

        view_port_update(app->view_port);
    }

    holdem_set_foreground_mode(app, UiModeTable);
}

// Hold the most recent bot decision on-screen for the configured pacing interval.
static void wait_for_bot_message(HoldemApp* app) {
    if(!app->bot_delay_enabled || app->bot_delay_ms == 0) return;

    uint32_t wait_start_tick = furi_get_tick();
    while((furi_get_tick() - wait_start_tick) < app->bot_delay_ms) {
        view_port_update(app->view_port);
        InputEvent ev;
        if(furi_message_queue_get(app->input_queue, &ev, 50) == FuriStatusOk) {
            if(process_global_event(app, &ev)) {
                if(app->exit_requested) return;
                continue;
            }

            if(
                holdem_can_skip_folded_autoplay(app) && ev.type == InputTypeShort &&
                ev.key == InputKeyOk) {
                app->skip_autoplay_requested = true;
                return;
            }
        }

        if(app->skip_autoplay_requested) return;
    }
}

// Executes one betting street until all required responses to the current bet are complete.
static void run_betting_round(HoldemApp* app, int start_player_index, int32_t min_raise) {
    HoldemGame* game = &app->game;
    int32_t current_bet = 0;

    // Current bet is the highest street contribution among active players.
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        if(game->players[player_index].in_hand && game->players[player_index].street_bet > current_bet) {
            current_bet = game->players[player_index].street_bet;
        }
    }

    bool needs_action[HOLDEM_MAX_PLAYERS] = {0};
    size_t needs_action_count = 0;
    for(size_t player_index = 0; player_index < game->player_count; player_index++) {
        Player* player = &game->players[player_index];
        if(player->in_hand && !player->all_in) {
            needs_action[player_index] = true;
            needs_action_count++;
        }
    }

    app->bot_turn_active = false;
    app->bot_turn_idx = -1;
    app->bot_turn_text[0] = '\0';

    // If nobody can still bet, the street can advance immediately.
    if(needs_action_count == 0) return;

    int active_player_index = start_player_index;
    while(
        needs_action_count > 0 && active_in_hand_count(game) > 1 && app->running &&
        !app->reset_requested) {
        active_player_index =
            ((active_player_index % (int)game->player_count) + (int)game->player_count) %
            (int)game->player_count;
        Player* active_player = &game->players[active_player_index];

        if(!needs_action[active_player_index] || !active_player->in_hand || active_player->all_in) {
            if(needs_action[active_player_index]) {
                needs_action[active_player_index] = false;
                if(needs_action_count > 0) needs_action_count--;
            }
            active_player_index++;
            continue;
        }

        int32_t to_call = current_bet - active_player->street_bet;
        HoldemAction selected_action = ActCall;
        int32_t raise_amount = 0;

        if(active_player->is_bot) {
            bot_action(
                app,
                game,
                active_player_index,
                to_call,
                min_raise,
                current_bet,
                &selected_action,
                &raise_amount);
        } else {
            app->bot_turn_active = false;
            app->bot_turn_idx = -1;
            app->bot_turn_text[0] = '\0';
            wait_human_action(app, active_player_index, to_call, min_raise);
            selected_action = app->chosen_action;
            raise_amount = app->chosen_raise_by;
        }

        if(app->exit_requested) return;

        if(selected_action == ActFold) {
            active_player->in_hand = false;
            needs_action[active_player_index] = false;
            needs_action_count--;
        } else if(selected_action == ActCheck || selected_action == ActCall) {
            put_chips(game, active_player_index, to_call);
            needs_action[active_player_index] = false;
            needs_action_count--;
        } else if(selected_action == ActRaise) {
            int32_t normalized_raise = (raise_amount < min_raise) ? min_raise : raise_amount;
            int32_t total_required = to_call + normalized_raise;
            if(total_required < 0) total_required = 0;
            put_chips(game, active_player_index, total_required);

            if(active_player->street_bet > current_bet) {
                // A real raise reopens action for everyone else still eligible.
                current_bet = active_player->street_bet;
                memset(needs_action, 0, sizeof(needs_action));
                needs_action_count = 0;
                for(size_t player_index = 0; player_index < game->player_count; player_index++) {
                    if((int)player_index == active_player_index) continue;
                    Player* player = &game->players[player_index];
                    if(player->in_hand && !player->all_in) {
                        needs_action[player_index] = true;
                        needs_action_count++;
                    }
                }
            } else {
                needs_action[active_player_index] = false;
                needs_action_count--;
            }
        }

        if(active_player->is_bot) {
            // Bot turns now use a single visible phase: show the chosen move on the
            // updated board, pause, then continue to the next actor or street.
            app->bot_turn_active = true;
            app->bot_turn_idx = active_player_index;
            if(selected_action == ActFold) {
                snprintf(app->bot_turn_text, sizeof(app->bot_turn_text), "%s Folded", active_player->name);
            } else if(selected_action == ActCheck) {
                snprintf(app->bot_turn_text, sizeof(app->bot_turn_text), "%s Checked", active_player->name);
            } else if(selected_action == ActCall) {
                if(to_call > 0) {
                    snprintf(app->bot_turn_text, sizeof(app->bot_turn_text), "%s Called $%d", active_player->name, (int)to_call);
                } else {
                    snprintf(app->bot_turn_text, sizeof(app->bot_turn_text), "%s Called", active_player->name);
                }
            } else if(selected_action == ActRaise) {
                snprintf(app->bot_turn_text, sizeof(app->bot_turn_text), "%s Raised $%d", active_player->name, (int)active_player->street_bet);
            }
            view_port_update(app->view_port);
            wait_for_bot_message(app);
            app->bot_turn_active = false;
            app->bot_turn_idx = -1;
            app->bot_turn_text[0] = '\0';
        } else {
            view_port_update(app->view_port);
        }

        // End the round only when everyone has responded to the current bet.
        if(needs_action_count == 0) break;
        active_player_index++;
    }
}

static void run_betting_round_or_auto(HoldemApp* app, int start_idx, int32_t min_raise) {
    HoldemGame* game = &app->game;
    if(should_auto_runout(game)) {
        if(app->skip_autoplay_requested) return;
        delay_for_visual_step(app);
        return;
    }
    run_betting_round(app, start_idx, min_raise);
}

// Plays a full hand from preflop through showdown (or fold-win) and returns when a result exists.
static bool play_one_hand(HoldemApp* app, PayoutResult* payout) {
    HoldemGame* g = &app->game;
    if(alive_count(g) < 2) return false;

    // Apply deferred blind edits only at the boundary before a fresh hand starts.
    if(app->pending_blinds_dirty) {
        holdem_apply_blind_values_now(app, app->pending_small_blind);
        app->pending_blinds_dirty = false;
    }
    holdem_apply_progressive_increase_if_due(app);

    reset_hand(g);
    if(!post_blinds(g)) return false;
    deal_hole(g);
    g->hand_in_progress = true;

    app->reveal_all = false;

    g->stage = StagePreflop;
    view_port_update(app->view_port);
    run_betting_round_or_auto(app, next_alive(g, g->bb_idx + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
    if(resolve_fold_win(g, payout)) {
        g->hand_in_progress = false;
        g->hand_no++;
        furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
        return true;
    }

    reset_street_bets(g);
    g->stage = StageFlop;
    deal_community(g, 3);
    view_port_update(app->view_port);
    run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
    if(resolve_fold_win(g, payout)) {
        g->hand_in_progress = false;
        g->hand_no++;
        furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
        return true;
    }

    reset_street_bets(g);
    g->stage = StageTurn;
    deal_community(g, 1);
    view_port_update(app->view_port);
    run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
    if(resolve_fold_win(g, payout)) {
        g->hand_in_progress = false;
        g->hand_no++;
        furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
        return true;
    }

    reset_street_bets(g);
    g->stage = StageRiver;
    deal_community(g, 1);
    view_port_update(app->view_port);
    run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
    if(resolve_fold_win(g, payout)) {
        g->hand_in_progress = false;
        g->hand_no++;
        furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
        return true;
    }

    g->stage = StageShowdown;
    app->reveal_all = true;
    resolve_showdown(g, payout);
    set_showdown_winner_mask(app, payout);
    if(!prompt_showdown_inspect(app)) return false;
    g->hand_in_progress = false;
    g->hand_no++;
    view_port_update(app->view_port);
    return true;
}

// Continues an in-progress saved hand from its saved street and action state.
static bool resume_loaded_hand(HoldemApp* app, PayoutResult* payout) {
    HoldemGame* g = &app->game;
    if(!g->hand_in_progress) return play_one_hand(app, payout);

    app->reveal_all = false;

    if(g->stage == StagePreflop) {
        view_port_update(app->view_port);
        run_betting_round_or_auto(app, next_alive(g, g->bb_idx + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
        if(resolve_fold_win(g, payout)) {
            g->hand_in_progress = false;
            g->hand_no++;
            furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
            return true;
        }
        reset_street_bets(g);
        g->stage = StageFlop;
        deal_community(g, 3);
    }

    if(g->stage == StageFlop) {
        view_port_update(app->view_port);
        run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
        if(resolve_fold_win(g, payout)) {
            g->hand_in_progress = false;
            g->hand_no++;
            furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
            return true;
        }
        reset_street_bets(g);
        g->stage = StageTurn;
        deal_community(g, 1);
    }

    if(g->stage == StageTurn) {
        view_port_update(app->view_port);
        run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
        if(resolve_fold_win(g, payout)) {
            g->hand_in_progress = false;
            g->hand_no++;
            furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
            return true;
        }
        reset_street_bets(g);
        g->stage = StageRiver;
        deal_community(g, 1);
    }

    if(g->stage == StageRiver) {
        view_port_update(app->view_port);
        run_betting_round_or_auto(app, next_alive(g, g->button + 1), g->big_blind);
    if(app->exit_requested) return false;
    if(app->reset_requested) return true;
        if(resolve_fold_win(g, payout)) {
            g->hand_in_progress = false;
            g->hand_no++;
            furi_delay_ms(HOLDEM_ENDTURN_PAUSE_MS);
            return true;
        }
        g->stage = StageShowdown;
    }

    app->reveal_all = true;
    resolve_showdown(g, payout);
    set_showdown_winner_mask(app, payout);
    if(!prompt_showdown_inspect(app)) return false;
    g->hand_in_progress = false;
    g->hand_no++;
    view_port_update(app->view_port);
    return true;
}
// Lightweight startup sequence: splash first, then startup jingle, then clear queued input.
static void startup_splash_and_jingle(HoldemApp* app) {
    static const float notes[] = {523.25f, 659.25f, 783.99f, 659.25f, 523.25f, 659.25f, 880.00f, 783.99f, 659.25f, 587.33f};
    static const uint16_t duration_ms[] = {220, 220, 280, 180, 220, 220, 320, 220, 220, 320};
    static const uint16_t gap_ms[] = {40, 40, 60, 40, 40, 40, 60, 40, 40, 120};

    app->mode = UiModeSplash;
    view_port_update(app->view_port);

    if(furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_set_volume(0.8f);
        for(size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); i++) {
            furi_hal_speaker_start(notes[i], 0.8f);
            furi_delay_ms(duration_ms[i]);
            furi_hal_speaker_stop();
            furi_delay_ms(gap_ms[i]);
        }
        furi_hal_speaker_release();
    } else {
        // If speaker is unavailable, still show splash briefly for consistent UX.
        furi_delay_ms(1200);
    }

    app->mode = UiModeTable;
    view_port_update(app->view_port);
    flush_input_queue(app);
}
static void startup_choose_load_or_new(HoldemApp* app) {
    if(!save_exists_on_storage()) {
        app->mode = UiModeStartReady;
        view_port_update(app->view_port);
        flush_input_queue(app);
        return;
    }

    app->mode = UiModeStartChoice;
    view_port_update(app->view_port);
    flush_input_queue(app);

    InputEvent ev;
    while(app->running && app->mode == UiModeStartChoice) {
        if(furi_message_queue_get(app->input_queue, &ev, FuriWaitForever) != FuriStatusOk) continue;
        (void)process_global_event(app, &ev);
    }
}

// Waits at the explicit start gate used for brand-new games and manual restarts.
static bool wait_for_start_confirmation(HoldemApp* app) {
    if(app->mode != UiModeStartReady) return true;

    view_port_update(app->view_port);
    flush_input_queue(app);

    InputEvent ev;
    while(app->running && app->mode == UiModeStartReady) {
        if(furi_message_queue_get(app->input_queue, &ev, FuriWaitForever) != FuriStatusOk) continue;
        (void)process_global_event(app, &ev);
        if(app->exit_requested) return false;
    }

    return app->running && !app->exit_requested;
}

// App lifecycle entrypoint: init services, run hand loop, persist on exit, release resources.
int32_t holdem_main(void* p) {
    UNUSED(p);

    HoldemApp* app = malloc(sizeof(HoldemApp));
    furi_check(app);
    memset(app, 0, sizeof(HoldemApp));

    init_game(&app->game);
    app->configured_player_count = app->game.player_count;
    app->bot_count_edit_value = app->configured_player_count;
    app->pending_blinds_dirty = false;
    app->pending_small_blind = app->game.small_blind;
    app->blind_edit_sb = app->game.small_blind;
    app->progressive_blinds_enabled = false;
    app->progressive_period_hands = HOLDEM_PROGRESSIVE_DEFAULT_PERIOD_HANDS;
    app->progressive_step_sb = HOLDEM_PROGRESSIVE_DEFAULT_STEP_SB;
    app->progressive_next_raise_hand_no = 0;
    app->blind_edit_progressive_enabled = false;
    app->blind_edit_progressive_period_hands = HOLDEM_PROGRESSIVE_DEFAULT_PERIOD_HANDS;
    app->blind_edit_progressive_step_sb = HOLDEM_PROGRESSIVE_DEFAULT_STEP_SB;

    app->running = true;
    set_ai_level(app, HOLDEM_AI_DEFAULT_LEVEL);
    app->configured_ai_level_pct = app->ai_level_pct;
    app->bot_difficulty_edit_value = app->configured_ai_level_pct;
    set_bot_action_delay(app, true, HOLDEM_BOT_DELAY_MS);
    app->mode = UiModeStartReady;
    app->return_mode = UiModeTable;

    app->input_queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    app->view_port = view_port_alloc();

    view_port_draw_callback_set(app->view_port, holdem_draw_callback, app);
    view_port_input_callback_set(app->view_port, holdem_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);


    startup_splash_and_jingle(app);
    startup_choose_load_or_new(app);
    app->configured_ai_level_pct = app->ai_level_pct;
    app->bot_difficulty_edit_value = app->configured_ai_level_pct;
    app->pending_blinds_dirty = false;
    app->pending_small_blind = app->game.small_blind;
    app->blind_edit_sb = app->game.small_blind;
    app->blind_edit_progressive_enabled = app->progressive_blinds_enabled;
    app->blind_edit_progressive_period_hands = app->progressive_period_hands;
    app->blind_edit_progressive_step_sb = app->progressive_step_sb;

    while(app->running) {
        if(!wait_for_start_confirmation(app)) break;

        PayoutResult payout;
        if(!resume_loaded_hand(app, &payout)) break;
        if(app->exit_requested) break;
        if(app->reset_requested) {
            reset_to_new_game(app);
            continue;
        }

        if(qualifies_for_big_win(app, &payout)) {
            if(!show_big_win_screen(app)) break;
        }

        show_hand_result_screen(app, &payout);

        if(app->exit_requested) break;
        apply_payout(&app->game, &payout);

        int champ = champion_idx(&app->game);
        bool you_busted = (app->game.players[0].stack <= 0);
        if(you_busted) {
            if(!show_interstitial_screen(app, "Game Over", false, 0u)) break;
            if(app->exit_requested) break;
            reset_to_new_game(app);
            continue;
        }

        if(champ >= 0) {
            if(champ == 0) {
                if(!show_interstitial_screen(app, "You Won!", true, 0u)) break;
                if(app->exit_requested) break;
                reset_to_new_game(app);
                continue;
            } else {
                if(!show_interstitial_screen(app, "Game Over", false, 0u)) break;
                if(app->exit_requested) break;
                reset_to_new_game(app);
                continue;
            }
        }
    }

    if(app->exit_requested && app->save_on_exit) {
        (void)save_progress(
            &app->game,
            app->configured_ai_level_pct,
            app->progressive_blinds_enabled,
            app->progressive_period_hands,
            app->progressive_step_sb,
            app->progressive_next_raise_hand_no);
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->input_queue);

    free(app);
    return 0;
}

