#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <gtk/gtk.h>

#define NUM_PLAYERS 3
#define MAX_ATTEMPTS 3
#define BUFFER_SIZE 1  // Only one player can guess at a time

// Variables globales
int secretCode[4];
int attempts[NUM_PLAYERS] = {0};  // Tentatives pour chaque joueur
bool gameWon = false;
sem_t semaphores[NUM_PLAYERS];
pthread_mutex_t mutex;
int buffer[BUFFER_SIZE]; // Shared buffer to hold the player's guess
int bufferIndex = 0; // Index to keep track of the buffer position

GtkWidget *entryFields[NUM_PLAYERS];  // Array for player guess inputs
GtkWidget *resultLabels[NUM_PLAYERS]; // Result labels for bulls and cows
GtkWidget *turnLabel;                 // Label for current turn
GtkWidget *submitButton;              // Button to submit guesses

// Fonction pour générer un code secret sans répétition
void generateSecretCode(int secretCode[]) {
    bool used[10] = {false};
    for (int i = 0; i < 4; i++) {
        int num;
        do {
            num = rand() % 10;
        } while (used[num]);
        secretCode[i] = num;
        used[num] = true;
    }
}

// Fonction pour évaluer le nombre de taureaux et de vaches
void evaluateGuess(int guess[], int *bulls, int *cows) {
    *bulls = 0;
    *cows = 0;
    for (int i = 0; i < 4; i++) {
        if (guess[i] == secretCode[i]) {
            (*bulls)++;
        } else {
            for (int j = 0; j < 4; j++) {
                if (i != j && guess[i] == secretCode[j]) {
                    (*cows)++;
                    break;
                }
            }
        }
    }
}

// Fonction du thread pour chaque joueur (producteur)
void* playerThread(void* arg) {
    int playerId = *(int*)arg;
    int guess[4];
    int bulls = 0, cows = 0;

    while (attempts[playerId] < MAX_ATTEMPTS && !gameWon) {
        // Attendre le tour du joueur
        sem_wait(&semaphores[playerId]);

        if (gameWon) {
            sem_post(&semaphores[(playerId + 1) % NUM_PLAYERS]); // Pass the turn to the next player
            break;
        }

        pthread_mutex_lock(&mutex);
        
        // Get the guess from the GUI
        const gchar *input = gtk_entry_get_text(GTK_ENTRY(entryFields[playerId]));
        for (int i = 0; i < 4; i++) {
            guess[i] = input[i] - '0';  // Convert string to digits
        }

        attempts[playerId]++;
        // Place the guess into the shared buffer (Producer step)
        buffer[bufferIndex] = playerId;
        bufferIndex = (bufferIndex + 1) % BUFFER_SIZE; // Circular buffer

        pthread_mutex_unlock(&mutex);

        // Pass the turn to the next player
        sem_post(&semaphores[(playerId + 1) % NUM_PLAYERS]);

        // Wait for the consumer (game logic) to process the guess
        sem_wait(&semaphores[NUM_PLAYERS]); // Game logic will consume the guess

        // Evaluate the guess (Consumer step)
        evaluateGuess(guess, &bulls, &cows);
        printf("Taureaux : %d, Vaches : %d\n", bulls, cows);

        // Update the GUI with the result
        gchar result[50];
        snprintf(result, sizeof(result), "Taureaux: %d, Vaches: %d", bulls, cows);
        gtk_label_set_text(GTK_LABEL(resultLabels[playerId]), result);

        if (bulls == 4) {
            printf("\nFélicitations Joueur %d ! Vous avez trouvé le nombre secret en %d tentatives.\n", playerId + 1, attempts[playerId]);
            gameWon = true;
        } else if (attempts[playerId] == MAX_ATTEMPTS) {
            printf("Joueur %d a épuisé ses %d tentatives.\n", playerId + 1, MAX_ATTEMPTS);
        }

        // Signal the game logic to process the next guess
        sem_post(&semaphores[NUM_PLAYERS]);
    }

    return NULL;
}

// Fonction pour le jeu (le consommateur)
void* gameLogic(void* arg) {
    while (!gameWon) {
        // Wait for the next guess from a player (Producer)
        sem_wait(&semaphores[NUM_PLAYERS]);

        // Consume the guess from the buffer
        if (bufferIndex > 0) {
            int playerId = buffer[0]; // Get the player who made the guess
            printf("Game Logic: Processing guess from Joueur %d...\n", playerId + 1);
        }

        // Signal the next player to continue
        sem_post(&semaphores[NUM_PLAYERS]);
    }
    return NULL;
}

// Callback function to submit the guess (triggered by the GUI button)
void on_submit_button_clicked(GtkWidget *widget, gpointer data) {
    int playerId = *(int*)data;
    // Simulate a player's guess submission through the GUI
    sem_post(&semaphores[playerId]);
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    generateSecretCode(secretCode);

    // Initialiser les sémaphores pour chaque joueur et le jeu
    for (int i = 0; i < NUM_PLAYERS; i++) {
        sem_init(&semaphores[i], 0, (i == 0) ? 1 : 0);  // Only the first player starts
    }
    sem_init(&semaphores[NUM_PLAYERS], 0, 0); // Game logic semaphore

    pthread_mutex_init(&mutex, NULL);

    // Initialize GTK
    gtk_init(&argc, &argv);

    // Create the main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Taureau-Vache Game");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create a vertical box layout
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Create a label for the game title
    GtkWidget *titleLabel = gtk_label_new("Taureau-Vache Game");
    gtk_box_pack_start(GTK_BOX(vbox), titleLabel, FALSE, FALSE, 5);

    // Create input fields and result labels for each player
    for (int i = 0; i < NUM_PLAYERS; i++) {
        GtkWidget *entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 5);
        entryFields[i] = entry;

        GtkWidget *resultLabel = gtk_label_new("Result: ");
        gtk_box_pack_start(GTK_BOX(vbox), resultLabel, FALSE, FALSE, 5);
        resultLabels[i] = resultLabel;
    }

    // Create the submit button
    submitButton = gtk_button_new_with_label("Submit Guess");
    g_signal_connect(submitButton, "clicked", G_CALLBACK(on_submit_button_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), submitButton, FALSE, FALSE, 5);

    // Create the turn label
    turnLabel = gtk_label_new("Turn: Player 1");
    gtk_box_pack_start(GTK_BOX(vbox), turnLabel, FALSE, FALSE, 5);

    // Show all widgets
    gtk_widget_show_all(window);

    // Start the game logic and player threads
    pthread_t threads[NUM_PLAYERS];
    pthread_t gameThread;
    int playerIds[NUM_PLAYERS];

    for (int i = 0; i < NUM_PLAYERS; i++) {
        playerIds[i] = i;
        pthread_create(&threads[i], NULL, playerThread, (void*)&playerIds[i]);
    }
    pthread_create(&gameThread, NULL, gameLogic, NULL);

    // Run the GTK main loop
    gtk_main();

    // Clean up
    pthread_mutex_destroy(&mutex);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        sem_destroy(&semaphores[i]);
    }
    sem_destroy(&semaphores[NUM_PLAYERS]);

    return 0;
}
