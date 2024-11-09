#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

// Structure pour stocker l'historique des tentatives
typedef struct {
    int guess[4];
    int bulls;
    int cows;
} GuessHistory;

// Déclaration des widgets GTK
GtkWidget *entry;
GtkWidget *labelResult;
GtkWidget *labelTurn;
GtkWidget *submitButton;
GtkWidget *historyView;
GtkWidget *timerLabel;
GtkWidget *attemptsLabel;
GtkWidget *mainBox;

// Variables de jeu
int secretCode[4];
int currentPlayer = 0;
int attempts[3] = {0, 0, 0};
bool gameWon = false;
GuessHistory history[9];  // 3 tentatives pour 3 joueurs
int historyCount = 0;
guint timerID = 0;
int gameSeconds = 0;

// Fonction pour mettre à jour le timer
gboolean update_timer(gpointer data) {
    gameSeconds++;
    char timeStr[30];  // Increased the size to avoid overflow
    snprintf(timeStr, sizeof(timeStr), "Temps: %02d:%02d", gameSeconds / 60, gameSeconds % 60);
    gtk_label_set_text(GTK_LABEL(timerLabel), timeStr);
    return G_SOURCE_CONTINUE;
}

// Fonction pour générer un code secret
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

// Fonction pour évaluer la tentative
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

// Fonction pour mettre à jour l'historique
void updateHistory(const int guess[], int bulls, int cows) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(historyView));
    char historyText[100];
    sprintf(historyText, "Joueur %d: %d%d%d%d → 🎯 %d, 🐮 %d\n", 
            currentPlayer + 1, guess[0], guess[1], guess[2], guess[3], bulls, cows);
    
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, historyText, -1);
}

// Fonction pour mettre à jour le label des tentatives
void updateAttemptsLabel() {
    char attemptsText[100];
    sprintf(attemptsText, "Tentatives restantes: %d", 3 - attempts[currentPlayer]);
    gtk_label_set_text(GTK_LABEL(attemptsLabel), attemptsText);
}

void on_submit_button_clicked(GtkWidget *widget, gpointer data) {
    int guess[4];
    const gchar *entryText = gtk_entry_get_text(GTK_ENTRY(entry));
    int bulls = 0, cows = 0;

    if (strlen(entryText) != 4) {
        gtk_label_set_text(GTK_LABEL(labelResult), "⚠️ Veuillez entrer 4 chiffres.");
        return;
    }

    for (int i = 0; i < 4; i++) {
        if (!g_ascii_isdigit(entryText[i])) {
            gtk_label_set_text(GTK_LABEL(labelResult), "⚠️ Veuillez entrer uniquement des chiffres.");
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
        sprintf(winMessage, "🎉 Félicitations, Joueur %d! Vous avez gagné en %02d:%02d!", 
                currentPlayer + 1, gameSeconds / 60, gameSeconds % 60);
        gtk_label_set_text(GTK_LABEL(labelResult), winMessage);
        gtk_widget_set_sensitive(submitButton, FALSE);
        g_source_remove(timerID);
        gameWon = true;
    } else if (attempts[currentPlayer] == 3) {
        currentPlayer = (currentPlayer + 1) % 3;
        char turnMessage[100];
        sprintf(turnMessage, "C'est au tour du Joueur %d", currentPlayer + 1);
        gtk_label_set_text(GTK_LABEL(labelTurn), turnMessage);
        updateAttemptsLabel();
    }

    if (attempts[0] == 3 && attempts[1] == 3 && attempts[2] == 3 && !gameWon) {
        char loseMessage[200];
        sprintf(loseMessage, "❌ Game Over! Le code secret était: %d%d%d%d", 
                secretCode[0], secretCode[1], secretCode[2], secretCode[3]);
        gtk_label_set_text(GTK_LABEL(labelResult), loseMessage);
        gtk_widget_set_sensitive(submitButton, FALSE);
        g_source_remove(timerID);
    }

    gtk_entry_set_text(GTK_ENTRY(entry), "");
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    generateSecretCode(secretCode);
    gtk_init(&argc, &argv);

    // Création de la fenêtre principale
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "🎮 Bulls & Cows");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 600);
    gtk_container_set_border_width(GTK_CONTAINER(window), 20);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Création du CSS Provider
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

    // Création du conteneur principal
    mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), mainBox);

    // Titre du jeu
    GtkWidget *titleLabel = gtk_label_new("🎲 Jeu Bulls & Cows");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "title");
    gtk_box_pack_start(GTK_BOX(mainBox), titleLabel, FALSE, FALSE, 10);

    // Timer et tentatives
    GtkWidget *infoBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    timerLabel = gtk_label_new("Temps: 00:00");
    attemptsLabel = gtk_label_new("Tentatives restantes: 3");
    gtk_style_context_add_class(gtk_widget_get_style_context(timerLabel), "timer");
    gtk_box_pack_start(GTK_BOX(infoBox), timerLabel, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(infoBox), attemptsLabel, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), infoBox, FALSE, FALSE, 5);

    // Tour du joueur
    labelTurn = gtk_label_new("👤 C'est au tour du Joueur 1");
    gtk_box_pack_start(GTK_BOX(mainBox), labelTurn, FALSE, FALSE, 5);

    // Zone de saisie
    GtkWidget *inputBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry), 4);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Entrez 4 chiffres");
    gtk_box_pack_start(GTK_BOX(inputBox), entry, TRUE, TRUE, 0);
    submitButton = gtk_button_new_with_label("Valider");
    g_signal_connect(submitButton, "clicked", G_CALLBACK(on_submit_button_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(inputBox), submitButton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), inputBox, FALSE, FALSE, 10);

    // Résultat
    labelResult = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(labelResult), "result");
    gtk_box_pack_start(GTK_BOX(mainBox), labelResult, FALSE, FALSE, 10);

    // Historique des tentatives
    GtkWidget *historyBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    historyView = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(historyView), FALSE);
    gtk_box_pack_start(GTK_BOX(historyBox), historyView, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), historyBox, TRUE, TRUE, 10);

    // Lancer le timer
    timerID = g_timeout_add_seconds(1, update_timer, NULL);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
