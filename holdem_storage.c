#include "holdem_storage.h"

#include <string.h>

bool save_exists_on_storage(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool exists = storage_file_open(file, HOLDEM_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if(exists) storage_file_close(file);

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return exists;
}

void delete_save_on_storage(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return;

    (void)storage_common_remove(storage, HOLDEM_SAVE_PATH);
    furi_record_close(RECORD_STORAGE);
}

bool save_progress(const HoldemGame* game) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    (void)storage_common_mkdir(storage, HOLDEM_SAVE_DIR);

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool ok = false;
    if(storage_file_open(file, HOLDEM_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        HoldemSave save;
        memset(&save, 0, sizeof(save));
        save.magic = HOLDEM_SAVE_MAGIC;
        save.version = 2;
        save.game = *game;

        ok = (storage_file_write(file, &save, sizeof(save)) == sizeof(save));
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool load_progress(HoldemGame* game) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool ok = false;
    if(storage_file_open(file, HOLDEM_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        HoldemSave save;
        memset(&save, 0, sizeof(save));
        size_t read = storage_file_read(file, &save, sizeof(save));

        if(read == sizeof(save) && save.magic == HOLDEM_SAVE_MAGIC && save.version == 2) {
            // Sanity checks prevent loading corrupted state.
            if(
                save.game.player_count == HOLDEM_MAX_PLAYERS &&
                save.game.deck_pos <= HOLDEM_DECK_SIZE &&
                save.game.board_count <= HOLDEM_BOARD_MAX &&
                save.game.stage <= StageShowdown) {
                *game = save.game;
                ok = true;
            }
        } else {
            // Legacy V1 fallback: stack/button/blinds/hand number only.
            storage_file_seek(file, 0, true);

            HoldemSaveV1 legacy;
            memset(&legacy, 0, sizeof(legacy));
            if(storage_file_read(file, &legacy, sizeof(legacy)) == sizeof(legacy)) {
                if(legacy.magic == HOLDEM_SAVE_MAGIC && legacy.version == 1) {
                    for(size_t player_index = 0; player_index < HOLDEM_MAX_PLAYERS; player_index++) {
                        if(legacy.stack[player_index] > 0) {
                            game->players[player_index].stack = legacy.stack[player_index];
                        }
                    }

                    if(legacy.hand_no >= 0) game->hand_no = legacy.hand_no;
                    game->button = legacy.button;
                    if(legacy.sb > 0) game->small_blind = legacy.sb;
                    if(legacy.bb > 0) game->big_blind = legacy.bb;
                    game->hand_in_progress = false;
                    ok = true;
                }
            }
        }

        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
