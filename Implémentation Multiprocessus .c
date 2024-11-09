#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h> // Include for bool, true, false

int secretCode[4];
int currentPlayer = 0;
int attempts[3] = {0, 0, 0};
int gameSeconds = 0;
bool gameWon = false;
guint timerID = 0;

// Struct to hold the GTK widgets
typedef struct {
    GtkWidget *entry;
    GtkWidget *labelResult;
} GameWidgets;

// Function to generate secret code
void generateSecretCode(int secretCode[]) {
    bool used[10] = { false };
    for (int i = 0; i < 4; i++) {
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
    bool guessUsed[4] = { false }, secretUsed[4] = { false };

    for (int i = 0; i < 4; i++) {
        if (guess[i] == secretCode[i]) {
            (*bulls)++;
            guessUsed[i] = true;
            secretUsed[i] = true;
        }
    }

    for (int i = 0; i < 4; i++) {
        if (!guessUsed[i]) {
            for (int j = 0; j < 4; j++) {
                if (!secretUsed[j] && guess[i] == secretCode[j]) {
                    (*cows)++;
                    secretUsed[j] = true;
                    break;
                }
            }
        }
    }
}

// Forking to handle player guesses
void handlePlayerGuess(GtkWidget *widget, gpointer data) {
    GameWidgets *widgets = (GameWidgets *)data;
    GtkWidget *entry = widgets->entry;
    GtkWidget *labelResult = widgets->labelResult;

    pid_t pid = fork();
    
    if (pid == -1) {
        perror("Fork failed");
        exit(1);
    }
    
    if (pid == 0) {
        // Child process for each player
        int guess[4];
        int bulls = 0, cows = 0;
        
        // Get the guess from the entry widget
        const gchar *guessStr = gtk_entry_get_text(GTK_ENTRY(entry));
        
        if (sscanf(guessStr, "%1d%1d%1d%1d", &guess[0], &guess[1], &guess[2], &guess[3]) != 4) {
            gtk_label_set_text(GTK_LABEL(labelResult), "Invalid input. Enter 4 digits.");
            return;
        }
        
        evaluateGuess(guess, secretCode, &bulls, &cows);
        
        char result[50];
        snprintf(result, sizeof(result), "Bulls: %d, Cows: %d", bulls, cows);
        gtk_label_set_text(GTK_LABEL(labelResult), result);
        
        if (bulls == 4) {
            gtk_label_set_text(GTK_LABEL(labelResult), "Player wins!");
            gameWon = true;
        }
        exit(0); // Exit child process after handling the guess
    } else {
        // Parent process (wait for the child to finish)
        wait(NULL); // Wait for the child process to complete
        if (gameWon) {
            gtk_label_set_text(GTK_LABEL(labelResult), "Game Over!");
        }
    }
}

// Timer function for updating the game time (if needed)
gboolean update_timer(gpointer data) {
    gameSeconds++;
    printf("Time: %d seconds\n", gameSeconds);
    return TRUE; // Return TRUE to continue the timer
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    generateSecretCode(secretCode);
    
    gtk_init(&argc, &argv);
    
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "🎮 Bulls & Cows");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 600);
    gtk_container_set_border_width(GTK_CONTAINER(window), 20);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), box);
    
    GtkWidget *entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    
    GtkWidget *submitButton = gtk_button_new_with_label("Submit Guess");
    gtk_box_pack_start(GTK_BOX(box), submitButton, FALSE, FALSE, 0);
    
    GtkWidget *labelResult = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(box), labelResult, FALSE, FALSE, 0);
    
    // Timer
    timerID = g_timeout_add(1000, update_timer, NULL);

    // Struct with widgets to pass to the callback
    GameWidgets widgets = { entry, labelResult };
    g_signal_connect(submitButton, "clicked", G_CALLBACK(handlePlayerGuess), &widgets);
    
    gtk_widget_show_all(window);
    gtk_main();
    
    return 0;
}
