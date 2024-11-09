#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

#define CODE_LENGTH 4

// Structure for storing guess history
typedef struct {
    int guess[CODE_LENGTH];
    int bulls;
    int cows;
} GuessHistory;

// Declare GTK widgets
GtkWidget *entry;
GtkWidget *labelResult;
GtkWidget *labelTurn;
GtkWidget *submitButton;
GtkWidget *historyView;
GtkWidget *timerLabel;
GtkWidget *attemptsLabel;
GtkWidget *mainBox;
GtkWidget *resultDifferenceLabel;

// Game variables
int secretCode[CODE_LENGTH];
int currentPlayer = 0;
int attempts[3] = {0, 0, 0};
bool gameWon = false;
GuessHistory history[9];  // 3 players, 3 guesses each
int historyCount = 0;
guint timerID = 0;
int gameSeconds = 0;
int gameTimeMono = 0;  // Time taken by the mono process
int gameTimeFork = 0;  // Time taken by the fork process (to be filled after comparison)

// Mutex and condition variable for synchronization
pthread_mutex_t mutex;
pthread_cond_t cond;

// Function to update the timer
gboolean update_timer(gpointer data) {
    gameSeconds++;
    char timeStr[30];
    snprintf(timeStr, sizeof(timeStr), "Time: %02d:%02d", gameSeconds / 60, gameSeconds % 60);
    gtk_label_set_text(GTK_LABEL(timerLabel), timeStr);
    return G_SOURCE_CONTINUE;
}

// Function to generate a secret code
void generateSecretCode(int secretCode[]) {
    bool used[10] = {false};
    for (int i = 0; i < CODE_LENGTH; i++) {
        int num;
        do {
            num = rand() % 10;
        } while (used[num]);
        secretCode[i] = num;
        used[num] = true;
    }
}

// Function to evaluate the guess
void evaluateGuess(int guess[], int secretCode[], int *bulls, int *cows) {
    *bulls = 0;
    *cows = 0;
    bool guessUsed[CODE_LENGTH] = {false}, secretUsed[CODE_LENGTH] = {false};

    for (int i = 0; i < CODE_LENGTH; i++) {
        if (guess[i] == secretCode[i]) {
            (*bulls)++;
            guessUsed[i] = true;
            secretUsed[i] = true;
        }
    }

    for (int i = 0; i < CODE_LENGTH; i++) {
        if (!guessUsed[i]) {
            for (int j = 0; j < CODE_LENGTH; j++) {
                if (!secretUsed[j] && guess[i] == secretCode[j]) {
                    (*cows)++;
                    secretUsed[j] = true;
                    break;
                }
            }
        }
    }
}

// Function to update the guess history
void updateHistory(const int guess[], int bulls, int cows) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(historyView));
    char historyText[100];
    sprintf(historyText, "Player %d: %d%d%d%d → 🎯 %d, 🐮 %d\n",
            currentPlayer + 1, guess[0], guess[1], guess[2], guess[3], bulls, cows);

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, historyText, -1);
}

// Function to update attempts label
void updateAttemptsLabel() {
    char attemptsText[100];
    sprintf(attemptsText, "Attempts left: %d", 3 - attempts[currentPlayer]);
    gtk_label_set_text(GTK_LABEL(attemptsLabel), attemptsText);
}

// Function to handle the submit button click
void on_submit_button_clicked(GtkWidget *widget, gpointer data) {
    int guess[CODE_LENGTH];
    const gchar *entryText = gtk_entry_get_text(GTK_ENTRY(entry));
    int bulls = 0, cows = 0;

    if (strlen(entryText) != CODE_LENGTH) {
        gtk_label_set_text(GTK_LABEL(labelResult), "⚠️ Please enter 4 digits.");
        return;
    }

    for (int i = 0; i < CODE_LENGTH; i++) {
        if (!g_ascii_isdigit(entryText[i])) {
            gtk_label_set_text(GTK_LABEL(labelResult), "⚠️ Only digits are allowed.");
            return;
        }
        guess[i] = entryText[i] - '0';
    }

    evaluateGuess(guess, secretCode, &bulls, &cows);
    updateHistory(guess, bulls, cows);
    attempts[currentPlayer]++;
    updateAttemptsLabel();

    if (bulls == 4) {
        char winMessage[100];
        sprintf(winMessage, "🎉 Congratulations, Player %d! You won in %02d:%02d!",
                currentPlayer + 1, gameSeconds / 60, gameSeconds % 60);
        gtk_label_set_text(GTK_LABEL(labelResult), winMessage);
        gtk_widget_set_sensitive(submitButton, FALSE);
        g_source_remove(timerID);
        gameWon = true;
        gameTimeMono = gameSeconds;
        gtk_label_set_text(GTK_LABEL(resultDifferenceLabel), "Mono time recorded.");
    } else if (attempts[currentPlayer] == 3) {
        currentPlayer = (currentPlayer + 1) % 3;
        char turnMessage[100];
        sprintf(turnMessage, "It's Player %d's turn", currentPlayer + 1);
        gtk_label_set_text(GTK_LABEL(labelTurn), turnMessage);
        updateAttemptsLabel();
    }

    if (attempts[0] == 3 && attempts[1] == 3 && attempts[2] == 3 && !gameWon) {
        char loseMessage[200];
        sprintf(loseMessage, "❌ Game Over! The secret code was: %d%d%d%d",
                secretCode[0], secretCode[1], secretCode[2], secretCode[3]);
        gtk_label_set_text(GTK_LABEL(labelResult), loseMessage);
        gtk_widget_set_sensitive(submitButton, FALSE);
        g_source_remove(timerID);
    }

    gtk_entry_set_text(GTK_ENTRY(entry), "");
}

// Function to simulate player's turn in a separate thread
void *playerThread(void *data) {
    pthread_mutex_lock(&mutex);
    while (!gameWon) {
        // Wait for the turn
        pthread_cond_wait(&cond, &mutex);
        on_submit_button_clicked(NULL, NULL);
        pthread_mutex_unlock(&mutex);
        // Sleep to simulate thinking time
        sleep(1);
    }
    return NULL;
}

// Main function
int main(int argc, char *argv[]) {
    srand(time(NULL));
    generateSecretCode(secretCode);
    gtk_init(&argc, &argv);

    // Create main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "🎮 Bulls & Cows");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 600);
    gtk_container_set_border_width(GTK_CONTAINER(window), 20);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create CSS Provider
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window { background: linear-gradient(135deg, #f5f5f5, #e0e0e0); }"
        "label { font-size: 16px; margin: 5px; }"
        "button { background: linear-gradient(to bottom right, #3498db, #2980b9);"
        "         color: white; padding: 10px; border-radius: 5px; }"
        "entry { font-size: 20px; padding: 8px; border-radius: 5px; }"
        "textview { font-size: 14px; background: white; border-radius: 5px; }"
        ".title { font-size: 24px; font-weight: bold; color: #2c3e50; }"
        ".result { font-size: 18px; padding: 10px; background: #f0f8ff; border-radius: 5px; }"
        ".timer { font-size: 20px; color: #e74c3c; }", -1, NULL);

    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Create main container
    mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), mainBox);

    // Title label
    GtkWidget *titleLabel = gtk_label_new("🎲 Bulls & Cows");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "title");
    gtk_box_pack_start(GTK_BOX(mainBox), titleLabel, FALSE, FALSE, 10);

    // Timer and attempts labels
    GtkWidget *infoBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    timerLabel = gtk_label_new("Time: 00:00");
    attemptsLabel = gtk_label_new("Attempts left: 3");
    gtk_style_context_add_class(gtk_widget_get_style_context(timerLabel), "timer");
    gtk_box_pack_start(GTK_BOX(infoBox), timerLabel, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(infoBox), attemptsLabel, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(mainBox), infoBox, FALSE, FALSE, 10);

    // Entry for guesses
    entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), CODE_LENGTH);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter your guess");
    gtk_box_pack_start(GTK_BOX(mainBox), entry, FALSE, FALSE, 10);

    // Submit button
    submitButton = gtk_button_new_with_label("Submit Guess");
    g_signal_connect(submitButton, "clicked", G_CALLBACK(on_submit_button_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(mainBox), submitButton, FALSE, FALSE, 10);

    // Result label
    labelResult = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(labelResult), "result");
    gtk_box_pack_start(GTK_BOX(mainBox), labelResult, FALSE, FALSE, 10);

    // Guess history
    historyView = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(historyView), FALSE);
    gtk_box_pack_start(GTK_BOX(mainBox), historyView, TRUE, TRUE, 10);

    // Turn label
    labelTurn = gtk_label_new("It's Player 1's turn");
    gtk_box_pack_start(GTK_BOX(mainBox), labelTurn, FALSE, FALSE, 10);

    // Result difference label
    resultDifferenceLabel = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(mainBox), resultDifferenceLabel, FALSE, FALSE, 10);

    // Start game timer
    timerID = g_timeout_add_seconds(1, update_timer, NULL);

    // Start the GTK main loop
    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
