#ifndef MNEMONIC_H
#define MNEMONIC_H

#include <stdbool.h>

#define MNEMONIC_WORDS 24
#define MNEMONIC_WORDS_LETTER 256


// Show a message and wait until user presses the middle button.
void show_message_wait(const char *msg);

// Let the user select the mnemonic length (12, 18, or 24 words) via UI.
int select_length_mnemonic_ui();

// Display a prompt before mnemonic input, e.g. "Enter your Recovery Phrase".
void show_enter_recovery_phrase_ui();

// Generate a new mnemonic phrase (returns true if successful).
// out_mnemonic: buffer for the phrase (should be at least 256 bytes).
bool create_new_mnemonic(char *out_mnemonic, int out_size);

bool create_new_mnemonic_flow(char *out_mnemonic);

void import_mnemonic(const char *mnemonic);

void import_mnemonic_flow(char *out_mnemonic);

// Display the mnemonic, one word at a time, to the user for backup.
void show_mnemonic_ui(const char *mnemonic);

// Check if a mnemonic phrase is valid (BIP39).
bool check_mnemonic(const char *mnemonic);

// Confirm user has written down their mnemonic by asking for random words.
// Returns true if user input is correct.
bool confirm_mnemonic(const char *mnemonic, int num_words, int num_check);

int split_mnemonic(const char *mnemonic, char words[][16], int max_words);

void input_word_ui(int word_index, char *out_word, int out_word_size);

typedef enum {
    MENU_CREATE_NEW = 0,
    MENU_IMPORT_SEED = 1
} MenuOption;

MenuOption select_create_or_import_ui();

void show_message(const char *msg);

void handle_mnemonic_flow(char *out_mnemonic);

#endif // MNEMONIC_H
