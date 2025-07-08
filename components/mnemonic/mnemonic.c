#include "mnemonic.h"
#include "u8g2.h"
#include "bip39.h"
#include "ui_state.h"
#include "button_listener.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include "esp_log.h"

extern u8g2_t u8g2; // Assume display is globally initialized
static const char *TAG = "mnemonic";

// ------- 1. Screen: Select mnemonic length --------
int select_length_mnemonic_ui() {
    const int options[3] = {24, 18, 12};
    int selected = 0;
    while (1) {
        ui_wait_until_free();
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);

        // Draw up arrow if not at top
        if (selected > 0) {
            u8g2_SetFont(&u8g2, u8g2_font_open_iconic_arrow_1x_t);
            u8g2_DrawGlyph(&u8g2, 62, 10, 0x0047); // Up arrow
        }
        // Draw current selected option centered
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d words", options[selected]);
        // Center text horizontally (assume 128px width)
        uint16_t w = u8g2_GetStrWidth(&u8g2, buf);
        u8g2_DrawRBox(&u8g2, (128-w)/2-8, 10, w+16, 10, 4); // highlight box
        u8g2_SetDrawColor(&u8g2, 0); // Draw text in black in white box
        u8g2_DrawStr(&u8g2, (128-w)/2, 18, buf);
        u8g2_SetDrawColor(&u8g2, 1); // Reset color

        // Draw down arrow if not at bottom
        if (selected < 2) {
            u8g2_SetFont(&u8g2, u8g2_font_open_iconic_arrow_1x_t);
            u8g2_DrawGlyph(&u8g2, 62, 30, 0x0044); // Down arrow
        }

        u8g2_SendBuffer(&u8g2);

        // Up/Down buttons to change selection (use left as up, right as down)
        if (is_button_left_pressed()) {
            if (selected > 0) selected--;
            vTaskDelay(pdMS_TO_TICKS(400));
        }
        if (is_button_right_pressed()) {
            if (selected < 2) selected++;
            vTaskDelay(pdMS_TO_TICKS(400));
        }
        if (is_button_middle_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(400));
            return options[selected];
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

// ------- 2. Screen: Prompt to enter recovery phrase --------
void show_enter_recovery_phrase_ui() {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    u8g2_DrawStr(&u8g2, 20, 15, "Enter your");
    u8g2_DrawStr(&u8g2, 20, 23, "Recovery phrase");
    u8g2_SendBuffer(&u8g2);
    vTaskDelay(pdMS_TO_TICKS(1500));
}

// ------- 3. Screen: Show message (like "Your new seed phrase") -------
void show_message(const char *msg) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
    u8g2_DrawStr(&u8g2, 20, 20, msg);
    u8g2_SendBuffer(&u8g2);
    vTaskDelay(pdMS_TO_TICKS(1500));
}

// ------- 4. Screen: Input one mnemonic word (letter picker) -------
void input_word_ui(int word_index, char *out_word, int out_word_size) {
    const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    int char_idx = 0, pos = 0;
    char word[16] = {0};
    bool done = false;

    while (!done) {
        ui_wait_until_free();
        // --- Render UI ---
        u8g2_ClearBuffer(&u8g2);

        // Title: "Enter word #n"
        char buf[32];
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        snprintf(buf, sizeof(buf), "Enter word #%d", word_index + 1);
        u8g2_DrawStr(&u8g2, 33, 8, buf);

        // Draw left arrow
        u8g2_SetFont(&u8g2, u8g2_font_open_iconic_arrow_1x_t);
        u8g2_DrawGlyph(&u8g2, 5, 19, 0x0041); // Left arrow

        // Draw right arrow
        u8g2_DrawGlyph(&u8g2, 110, 19, 0x0042); // Right arrow

        // Draw picker for 3 letters: prev, current (highlight), next
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf); // Font for letter picker (thicker)
        for (int i = 0; i < 3; ++i) {
            int show_idx = (char_idx + i - 1 + 26) % 26;
            int x = 43 + i * 18;
            if (i == 1) {
                // Highlight box for current selection (center letter)
                u8g2_SetDrawColor(&u8g2, 1); // White
                u8g2_DrawRBox(&u8g2, x - 5, 10, 16, 8, 2);
                u8g2_SetDrawColor(&u8g2, 0); // Black
                u8g2_DrawStr(&u8g2, x, 17, (char[]){alphabet[show_idx],0});
                u8g2_SetDrawColor(&u8g2, 1); // Back to white
            } else {
                u8g2_DrawStr(&u8g2, x, 17, (char[]){alphabet[show_idx],0});
            }
        }

        // Draw current input word in a box (centered)
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        uint16_t word_width = u8g2_GetStrWidth(&u8g2, word);
        int box_x = 24, box_y = 20, box_w = 80, box_h = 8;
        int word_x = box_x + (box_w - word_width) / 2;
        u8g2_DrawRBox(&u8g2, box_x, box_y, box_w, box_h, 2); // Box under word
        u8g2_SetDrawColor(&u8g2, 0);
        u8g2_DrawStr(&u8g2, word_x, box_y + 6, word); // Draw word inside box
        u8g2_SetDrawColor(&u8g2, 1);

        u8g2_SendBuffer(&u8g2);

        // --- Button handling

        // Left: move left in alphabet, or hold for backspace
        if (is_button_left_pressed()) {
            char_idx = (char_idx + 25) % 26;
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        // Right: move right in alphabet
        if (is_button_right_pressed()) {
            char_idx = (char_idx + 1) % 26;
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        // Hold both left and right to finish word entry
        if (pos > 0 && is_button_right_pressed() && is_button_left_pressed()) {
            done = true;
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        // Middle: select current letter
        if (is_button_middle_pressed()) {
            if (pos < out_word_size - 2) {
                word[pos++] = alphabet[char_idx];
                word[pos] = 0;
            }
            vTaskDelay(pdMS_TO_TICKS(400));
        }

    }
    strncpy(out_word, word, out_word_size - 1);
    out_word[out_word_size - 1] = '\0';
}


// ------- 5. Screen: Show each mnemonic word for backup --------
int split_mnemonic(const char *mnemonic, char words[][16], int max_words) {
    // Split mnemonic into words array. Returns number of words.
    int count = 0;
    const char *ptr = mnemonic;
    while (*ptr && count < max_words) {
        int len = 0;
        while (*ptr && *ptr != ' ' && len < 15) {
            words[count][len++] = *ptr++;
        }
        words[count][len] = 0;
        if (*ptr == ' ') ptr++;
        count++;
    }
    return count;
}

bool check_mnemonic(const char* mnemonic) {
    return mnemonic_check(mnemonic) == 1;
}

void show_mnemonic_ui(const char *mnemonic) {
    char words[24][16];
    int num_words = split_mnemonic(mnemonic, words, 24);
    int idx = 0;
    while (1) {
        ui_wait_until_free();
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);

        // Show "Write word #N"
        char buf[32];
        snprintf(buf, sizeof(buf), "Write word #%d", idx + 1);
        u8g2_DrawStr(&u8g2, 30, 14, buf);

        // Draw the white rounded box as background for the word
        int box_x = 28, box_y = 20, box_w = 70, box_h = 10;
        u8g2_SetDrawColor(&u8g2, 1); // Set color to white
        u8g2_DrawRBox(&u8g2, box_x, box_y, box_w, box_h, 4);

        // Draw the mnemonic word, centered horizontally inside the white box
        u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr); // Font with thicker lines
        u8g2_SetDrawColor(&u8g2, 0); // Set color to black (for text)
        uint16_t word_width = u8g2_GetStrWidth(&u8g2, words[idx]);
        int text_x = box_x + (box_w - word_width) / 2;
        int text_y = 28;
        u8g2_DrawStr(&u8g2, text_x, text_y, words[idx]);

        // Reset draw color to white for any further UI drawing
        u8g2_SetDrawColor(&u8g2, 1);

        u8g2_SendBuffer(&u8g2);

        // Left/right to navigate, middle to exit at last word
        if (is_button_left_pressed() && idx > 0) { idx--; vTaskDelay(pdMS_TO_TICKS(180)); }
        if (is_button_right_pressed() && idx < num_words - 1) { idx++; vTaskDelay(pdMS_TO_TICKS(180)); }
        if (is_button_middle_pressed() && idx == num_words - 1) { vTaskDelay(pdMS_TO_TICKS(200)); break; }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


// Helper: Pick 'num_check' unique random indices between 0 and num_words-1
void pick_random_indices(int *indices, int num_words, int num_check) {
    bool used[24] = {0}; // Maximum mnemonic words supported: 24
    int picked = 0;
    // Use esp_random or system random for best entropy in production!
    srand((unsigned int)time(NULL) + rand());
    while (picked < num_check) {
        int idx = rand() % num_words;
        if (!used[idx]) {
            indices[picked++] = idx;
            used[idx] = true;
        }
    }
}

/**
 * @brief Confirm the user has written down their mnemonic phrase by requiring them to re-enter 3 random words.
 * @param mnemonic The full mnemonic phrase.
 * @param num_words The number of words in the mnemonic (e.g. 12, 18, 24).
 * @param num_check How many words to check (suggested: 3).
 * @return true if the user correctly enters all required words; false otherwise.
 */
bool confirm_mnemonic(const char *mnemonic, int num_words, int num_check) {
    char words[24][16];
    char user_word[16];
    int indices[24]; // Support up to 24 unique checks if needed

    // Split the mnemonic into words array
    split_mnemonic(mnemonic, words, 24);

    while (1) {
        ui_wait_until_free();
        // Generate num_check unique random indices each round
        pick_random_indices(indices, num_words, num_check);

        bool all_correct = true;
        for (int i = 0; i < num_check; ++i) {
            int word_idx = indices[i];
            // Prompt the user to enter the word at (word_idx + 1)
            char prompt[32];
            snprintf(prompt, sizeof(prompt), "Enter word #%d", word_idx + 1);
            show_message(prompt);

            // Call the word input UI;
            input_word_ui(word_idx, user_word, sizeof(user_word));

            // Compare (case-insensitive)
            if (strcasecmp(user_word, words[word_idx]) != 0) {
                show_message("Incorrect word!\nPlease try again.");
                all_correct = false;
                break;
            }
        }
        if (all_correct) {
            show_message("Mnemonic confirmed!");
            return true;
        }
        // If failed, restart: new 3 indices will be picked next loop
    }
}

bool create_new_mnemonic(char *out_mnemonic, int out_size) {
    const char *mnemonic = mnemonic_generate(MNEMONIC_WORDS_LETTER);
    if (!mnemonic) return false;
    // Copy with safety: at most out_size-1 bytes, always null-terminated
    strncpy(out_mnemonic, mnemonic, out_size - 1);
    out_mnemonic[out_size - 1] = '\0';
    ESP_LOGI(TAG, "Generated mnemonic: %s", out_mnemonic);
    return true;
}


// Main flow: create new mnemonic and ensure user has backed it up.
bool create_new_mnemonic_flow(char *out_mnemonic) {

    // 1. Show "Your new seed phrase" notification
    show_message("Your new seed phrase");

    // 2. Generate a new mnemonic phrase
    bool success = create_new_mnemonic(out_mnemonic, sizeof(out_mnemonic));
    if (!success) {
        show_message("Failed to create mnemonic!");
        return false;
    }

    // 3. Display each mnemonic word for user to write down
    show_mnemonic_ui(out_mnemonic);

    // 4. Confirm the user has written down the mnemonic (ask 3 random words)
    bool confirmed = confirm_mnemonic(out_mnemonic, MNEMONIC_WORDS, 3);
    if (confirmed) {
        show_message("Setup complete!");
        return true;
    }

    return false;
}

void import_mnemonic(const char *mnemonic) {
    // Prepare buffer for the entropy (max 32 bytes = 256 bits)
    uint8_t entropy[32] = {0};

    // Convert mnemonic to entropy
    int entropy_bits = mnemonic_to_entropy(mnemonic, entropy);

    if (entropy_bits == 0) {
        // Invalid mnemonic, cannot extract entropy
        show_message("Mnemonic invalid!\nImport failed.");
        return;
    }


}


void import_mnemonic_flow(char *out_mnemonic) {
    int mnemonicLength = select_length_mnemonic_ui();

    show_enter_recovery_phrase_ui();

    for (int i = 0; i < mnemonicLength; ++i) {
        char word[16] = {0};
        input_word_ui(i, word, sizeof(word));
        if (i > 0) strcat(out_mnemonic, " ");
        strcat(out_mnemonic, word);
    }

    // 4. Optional: Validate the mnemonic phrase before importing
    if (!check_mnemonic(out_mnemonic)) {
        show_message("Invalid mnemonic!\nPlease try again.");
        // Optionally: restart the flow
        import_mnemonic_flow(out_mnemonic);
        return;
    }

    // 5. Import the mnemonic (user function, implement as needed)
    import_mnemonic(out_mnemonic);

    // 6. Show confirmation message
    show_message("Mnemonic imported!");
}

MenuOption select_create_or_import_ui() {
    const char *options[2] = {"Create New", "Import Seed"};
    int selected = 0;

    while (1) {
        ui_wait_until_free();
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);

        // Draw up arrow if not at the top option
        if (selected > 0) {
            u8g2_SetFont(&u8g2, u8g2_font_open_iconic_arrow_1x_t);
            u8g2_DrawGlyph(&u8g2, 60, 10, 0x0047); // Up arrow
        }

        // Draw the currently selected option centered in a highlight box
        u8g2_SetFont(&u8g2, u8g2_font_profont10_tf);
        uint16_t w = u8g2_GetStrWidth(&u8g2, options[selected]);
        int box_x = (128-w)/2 - 8, box_w = w+16;
        u8g2_DrawRBox(&u8g2, box_x, 10, box_w, 10, 4); // highlight box
        u8g2_SetDrawColor(&u8g2, 0); // Draw text in black inside box
        u8g2_DrawStr(&u8g2, (128-w)/2, 17, options[selected]);
        u8g2_SetDrawColor(&u8g2, 1); // Back to white for other UI

        // Draw down arrow if not at the bottom option
        if (selected < 1) {
            u8g2_SetFont(&u8g2, u8g2_font_open_iconic_arrow_1x_t);
            u8g2_DrawGlyph(&u8g2, 60, 28, 0x0044); // Down arrow
        }

        u8g2_SendBuffer(&u8g2);

        // Handle up/down and confirm
        if (is_button_left_pressed()) { // Up
            if (selected > 0) selected--;
            vTaskDelay(pdMS_TO_TICKS(400));
        }
        if (is_button_right_pressed()) { // Down
            if (selected < 1) selected++;
            vTaskDelay(pdMS_TO_TICKS(400));
        }
        if (is_button_middle_pressed()) { // Confirm
            vTaskDelay(pdMS_TO_TICKS(400));
            return (MenuOption)selected;
        }
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

void handle_mnemonic_flow(char *out_mnemonic) {
    MenuOption option = select_create_or_import_ui();


    if (option == MENU_CREATE_NEW) {
        // User chose to create a new mnemonic
        create_new_mnemonic_flow(out_mnemonic);
    } else if (option == MENU_IMPORT_SEED) {
        // User chose to import an existing mnemonic
        import_mnemonic_flow(out_mnemonic);
    } else {
        show_message("Invalid option selected!");
    }
}