#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <regex.h>
#include <ctype.h>
#include <time.h>

typedef struct {
    char username[50];
    char password[50];
    char email[100];
    int score;
    char last_signin[50];
    int played;
} User;

typedef struct {
    char username[50];
    int score;
    int rank;
} PlayerRank;

// Global variables
static int menu_y[] = {8, 9, 10, 11};
static const int menu_x = 20;
static char *menu[] = {
    "SIGN IN",
    "LOG IN"
};
static char *m_menu[] = {
    "NEW GAME",
    "LOAD GAME",
    "PROFILE",
    "SCORE"
};
static char *new_game[]={
    "START",
    "SETTINGS"
};
static char *setting[]={
    "DIFFICULTY = ",
    "CHANGE COLOR"
};
static int selected = 0;
static char current_user[50] = "";
static bool is_logged_in = false;
static int dif_lvl = 1;
static int current_color = 4;  // Default to WHITE (4)
static int game_color = 4;     // Game color setting


///////////////////////////////////////////////////////////////

// Function prototypes

void draw_menu(void);

void main_menu(void);

void create_user(void);

bool login_user(void);

void update_signin_time(const char *username);

void get_current_utc_time(char *buffer, size_t size);

int get_player_rank(const char *username);

void display_scoreboard(void);

bool is_valid_password(const char *password);

int is_valid_email_format(const char *email, regex_t *regex);

int is_valid_email_length(const char *email);

int is_common_domain(const char *email);

int compare_players(const void *a, const void *b);

void color_settings_menu(void);
//////////////////////////////////////////////////////////////////////////////////


bool is_valid_password(const char *password) {
    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;
    
    for (int i = 0; password[i] != '\0'; i++) {
        if (isupper(password[i])) has_upper = true;
        if (islower(password[i])) has_lower = true;
        if (isdigit(password[i])) has_digit = true;
    }
    
    return has_upper && has_lower && has_digit;
}

int is_valid_email_format(const char *email, regex_t *regex) {
    return regexec(regex, email, 0, NULL, 0) == 0;
}

int is_valid_email_length(const char *email) {
    return strlen(email) <= 320;
}

int is_common_domain(const char *email) {
    const char *common_domains[] = {
        "gmail.com", "yahoo.com", "hotmail.com", "outlook.com"
    };
    const int domain_count = 4;
    
    char *at_pos = strchr(email, '@');
    if (at_pos == NULL) return 0;
    
    for (int i = 0; i < domain_count; i++) {
        if (strcasestr(at_pos + 1, common_domains[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

void get_current_utc_time(char *buffer, size_t size) {
    time_t now;
    struct tm *tm_utc;
    
    time(&now);
    tm_utc = gmtime(&now);
    
    snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d UTC",
             tm_utc->tm_year + 1900,
             tm_utc->tm_mon + 1,
             tm_utc->tm_mday,
             tm_utc->tm_hour,
             tm_utc->tm_min,
             tm_utc->tm_sec);
}

int compare_players(const void *a, const void *b) {
    return ((PlayerRank *)b)->score - ((PlayerRank *)a)->score;
}

void update_signin_time(const char *username) {
    FILE *users = fopen("user_info.txt", "rb+");
    if (users == NULL) return;

    User temp_user;
    
    while (fread(&temp_user, sizeof(User), 1, users) == 1) {
        if (strcmp(temp_user.username, username) == 0) {
            // Check if this is the first sign in (last_signin is empty)
            if (strlen(temp_user.last_signin) == 0) {
                fseek(users, -sizeof(User), SEEK_CUR);
                char current_time[50];
                get_current_utc_time(current_time, sizeof(current_time));
                strncpy(temp_user.last_signin, current_time, sizeof(temp_user.last_signin) - 1);
                temp_user.last_signin[sizeof(temp_user.last_signin) - 1] = '\0';
                fwrite(&temp_user, sizeof(User), 1, users);
            }
            break;
        }
    }
    fclose(users);
}

int get_player_rank(const char *username) {
    FILE *users = fopen("user_info.txt", "rb");
    if (users == NULL) return 0;

    fseek(users, 0, SEEK_END);
    long fileSize = ftell(users);
    int userCount = fileSize / sizeof(User);
    rewind(users);

    PlayerRank *rankings = malloc(userCount * sizeof(PlayerRank));
    if (rankings == NULL) {
        fclose(users);
        return 0;
    }

    User temp_user;
    int i = 0;
    while (fread(&temp_user, sizeof(User), 1, users) == 1) {
        strncpy(rankings[i].username, temp_user.username, sizeof(rankings[i].username) - 1);
        rankings[i].score = temp_user.score;
        i++;
    }

    qsort(rankings, userCount, sizeof(PlayerRank), compare_players);

    int rank = 0;
    int currentRank = 1;
    int prevScore = -1;
    
    for (i = 0; i < userCount; i++) {
        if (prevScore != rankings[i].score) {
            currentRank = i + 1;
        }
        if (strcmp(rankings[i].username, username) == 0) {
            rank = currentRank;
            break;
        }
        prevScore = rankings[i].score;
    }

    free(rankings);
    fclose(users);
    return rank;
}

void display_scoreboard(void) {
    clear();
    FILE *users = fopen("user_info.txt", "rb");
    if (users == NULL) {
        mvprintw(10, 5, "No users found!");
        refresh();
        getch();
        return;
    }

    fseek(users, 0, SEEK_END);
    long fileSize = ftell(users);
    int userCount = fileSize / sizeof(User);
    rewind(users);

    PlayerRank *rankings = malloc(userCount * sizeof(PlayerRank));
    if (rankings == NULL) {
        fclose(users);
        return;
    }

    User temp_user;
    int i = 0;
    while (fread(&temp_user, sizeof(User), 1, users) == 1) {
        strncpy(rankings[i].username, temp_user.username, sizeof(rankings[i].username) - 1);
        rankings[i].score = temp_user.score;
        i++;
    }

    qsort(rankings, userCount, sizeof(PlayerRank), compare_players);

    mvprintw(4, 5, "SCOREBOARD");
    mvprintw(5, 5, "Rank  Username                Score");
    mvprintw(6, 5, "------------------------------------");

    int currentRank = 1;
    int prevScore = -1;
    for (i = 0; i < userCount && i < 10; i++) {
        if (prevScore != rankings[i].score) {
            currentRank = i + 1;
        }
        mvprintw(7 + i, 5, "%2d.   %-20s %5d", 
                currentRank, 
                rankings[i].username, 
                rankings[i].score);
        prevScore = rankings[i].score;
    }

    mvprintw(18, 5, "Press 'b' to go back");
    refresh();

    free(rankings);
    fclose(users);

    char ch;
    while ((ch = getch()) != 'b');
    clear();
    main_menu();
}

void draw_menu(void) {
    clear();
    mvprintw(4, menu_x - 5, "PLEASE SIGN IN FIRST(IF YOU ALREADY DID IT THEN LOGIN)!");

    for (int i = 0; i < 2; i++) {
        if (i == selected && has_colors()) {
            attron(COLOR_PAIR(2));
        }

        mvhline(menu_y[i], menu_x - 2, ' ', 22);

        if (i == selected) {
            mvprintw(menu_y[i], menu_x - 2, "->");
        }

        mvprintw(menu_y[i], menu_x, "%s", menu[i]);

        if (i == selected && has_colors()) {
            attroff(COLOR_PAIR(2));
        }
    }
    refresh();
}

void main_menu(void) {
    clear();
    mvprintw(4, menu_x - 5, "WELCOME %s!", current_user);

    for (int i = 0; i < 4; i++) {
        if (i == selected && has_colors()) {
            attron(COLOR_PAIR(2));
        }

        mvhline(menu_y[i], menu_x - 2, ' ', 22);

        if (i == selected) {
            mvprintw(menu_y[i], menu_x - 2, "->");
        }

        mvprintw(menu_y[i], menu_x, "%s", m_menu[i]);

        if (i == selected && has_colors()) {
            attroff(COLOR_PAIR(2));
        }
    }
    refresh();
}

void n_game(void) {
    clear();
    mvprintw(4, menu_x - 5, "let's start a new game");

    for (int i = 0; i < 2; i++) {  
        if (i == selected && has_colors()) {
            attron(COLOR_PAIR(2));
        }

        mvhline(menu_y[i], menu_x - 2, ' ', 22);

        if (i == selected) {
            mvprintw(menu_y[i], menu_x - 2, "->");
        }

        mvprintw(menu_y[i], menu_x, "%s", new_game[i]);

        if (i == selected && has_colors()) {
            attroff(COLOR_PAIR(2));
        }
    }
    refresh();
}

void create_user(void) {
    clear();
    echo();
    curs_set(1);
    regex_t regex;
    User new_user;
    int reti;
    
    const char *email_pattern = "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$";
    
    reti = regcomp(&regex, email_pattern, REG_EXTENDED);
    if (reti) {
        mvprintw(10, 2, "Could not compile regex");
        refresh();
        getch();
        return;
    }

    mvprintw(4, 2, "Create New Account");
    mvprintw(6, 2, "Username: ");
    getstr(new_user.username);

    bool valid_password = false;
    do {
        mvprintw(7, 2, "Password (must contain at least 1 uppercase, 1 lowercase, and 1 number): ");
        getstr(new_user.password);
        
        if (!is_valid_password(new_user.password)) {
            mvprintw(10, 2, "Password must contain at least 1 uppercase letter, 1 lowercase letter, and 1 number!");
            refresh();
            getch();
            move(10, 2);
            clrtoeol();
            move(7, 2);
            clrtoeol();
        } else {
            valid_password = true;
        }
    } while (!valid_password);

    int valid_email = 0;
    do {
        mvprintw(8, 2, "Email: ");
        clrtoeol();
        getstr(new_user.email);
        
        if (!is_valid_email_length(new_user.email)) {
            mvprintw(10, 2, "Email is too long! Maximum length is 320 characters.");
            refresh();
            getch();
            move(10, 2);
            clrtoeol();
            continue;
        }
        
        if (!is_valid_email_format(new_user.email, &regex)) {
            mvprintw(10, 2, "Invalid email format! Example: user@domain.com");
            refresh();
            getch();
            move(10, 2);
            clrtoeol();
            continue;
        }
        
        if (!is_common_domain(new_user.email)) {
            mvprintw(10, 2, "Warning: Uncommon email domain. Press 'y' to continue or any other key to retry.");
            refresh();
            if (getch() != 'y') {
                move(10, 2);
                clrtoeol();
                continue;
            }
        }
        
        valid_email = 1;
        
    } while (!valid_email);
    
new_user.score = 0;
    new_user.played = 0;  // Initialize played games count

    // Get current time and set it as first sign-in time
    char current_time[50];
    get_current_utc_time(current_time, sizeof(current_time));
    strncpy(new_user.last_signin, current_time, sizeof(new_user.last_signin) - 1);
    new_user.last_signin[sizeof(new_user.last_signin) - 1] = '\0';

    // Open file only once
    FILE *users = fopen("user_info.txt", "ab+");
    if (users == NULL) {
        mvprintw(10, 2, "Error opening user file!");
        refresh();
        getch();
        regfree(&regex);
        return;
    }

    User old_user;
    int valid = 1;
    while (fread(&old_user, sizeof(User), 1, users) == 1) {
        if (strcmp(old_user.username, new_user.username) == 0) {
            valid = 0;
            mvprintw(10, 2, "Username already exists!");
            break;
        }
    }

    if (valid) {
        fwrite(&new_user, sizeof(User), 1, users);
        mvprintw(10, 2, "Account created successfully! Press any key to continue.");
        is_logged_in = true;
        strncpy(current_user, new_user.username, sizeof(current_user) - 1);
    }
    
    fclose(users);
    regfree(&regex);
    refresh();
    noecho();
    curs_set(0);
    getch();
}

bool login_user(void) {
    clear();
    echo();
    curs_set(1);

    char username[50];
    char password[50];

    mvprintw(4, 2, "Login");
    mvprintw(6, 2, "Username: ");
    getstr(username);

    mvprintw(7, 2, "Password: ");
    getstr(password);

    FILE *users = fopen("user_info.txt", "rb+");
    if (users == NULL) {
        mvprintw(10, 2, "No users found!");
        refresh();
        getch();
        return false;
    }

    User temp_user;
    bool found = false;

    while (fread(&temp_user, sizeof(User), 1, users) == 1) {
        if (strcmp(temp_user.username, username) == 0 &&
            strcmp(temp_user.password, password) == 0) {
            found = true;
            strncpy(current_user, username, sizeof(current_user) - 1);
            update_signin_time(username);
            break;
        }
    }
    fclose(users);

    if (found) {
        mvprintw(10, 2, "Login successful! Press any key to continue.");
    } else {
        mvprintw(10, 2, "Invalid username or password!");
    }

    refresh();
    noecho();
    curs_set(0);
    getch();
    return found;
}

void clear_input_buffer(void) {
    nodelay(stdscr, TRUE);
    while (getch() != ERR);
    nodelay(stdscr, FALSE);
}

void settings_menu(void) {
    clear();
    int settings_selected = 0;
    char *difficulty_levels[] = {"Easy", "Medium", "Hard"};
    
    while (1) {
        clear();
        mvprintw(4, menu_x - 5, "GAME SETTINGS");
        
        // Display current difficulty
        mvprintw(6, menu_x, "Current Difficulty: %s", difficulty_levels[dif_lvl]);
        
        // Display settings options
        for (int i = 0; i < 2; i++) {
            if (i == settings_selected && has_colors()) {
                attron(COLOR_PAIR(2));
            }
            
            mvhline(menu_y[i], menu_x - 2, ' ', 30);
            
            if (i == settings_selected) {
                mvprintw(menu_y[i], menu_x - 2, "->");
            }
            
            if (i == 0) {
                mvprintw(menu_y[i], menu_x, "%s%s", setting[i], difficulty_levels[dif_lvl]);
            } else {
                mvprintw(menu_y[i], menu_x, "%s", setting[i]);
            }
            
            if (i == settings_selected && has_colors()) {
                attroff(COLOR_PAIR(2));
            }
        }
        
        mvprintw(14, menu_x - 5, "Press 'b' to go back");
        refresh();
        
        int ch = getch();
        switch (ch) {
            case KEY_UP:
                settings_selected = (settings_selected > 0) ? settings_selected - 1 : 1;
                break;
            case KEY_DOWN:
                settings_selected = (settings_selected < 1) ? settings_selected + 1 : 0;
                break;
            case '\n':
                if (settings_selected == 0) {
                    dif_lvl = (dif_lvl + 1) % 3;
                } else if (settings_selected == 1) {
                    color_settings_menu();
                }
                break;
            case 'b':
                clear();
                clear_input_buffer(); // Clear any pending input
                refresh();
                return;
        }
    }
}

void color_settings_menu(void) {
    clear();
    int ch;
    
    // Initialize color pairs if not already done
    init_pair(1, COLOR_RED, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_MAGENTA, -1);
    init_pair(4, COLOR_WHITE, -1);
    init_pair(5, COLOR_BLUE, -1);
    init_pair(6, COLOR_CYAN, -1);
    init_pair(7, COLOR_YELLOW, -1);

    while(1) {
        clear();
        mvprintw(4, menu_x - 5, "COLOR SETTINGS");
        mvprintw(5, menu_x - 5, "Use LEFT and RIGHT arrow keys to change color");
        mvprintw(6, menu_x - 5, "Press ENTER to select, 'b' to go back");

        // Display color name
        mvprintw(8, menu_x, "Selected color: ");
        attron(COLOR_PAIR(current_color) | A_BOLD);
        switch (current_color) {
            case 1: printw("RED"); break;
            case 2: printw("GREEN"); break;
            case 3: printw("MAGENTA"); break;
            case 4: printw("WHITE"); break;
            case 5: printw("BLUE"); break;
            case 6: printw("CYAN"); break;
            case 7: printw("YELLOW"); break;
        }

        // Display sample text with selected color
        mvprintw(10, menu_x - 5, "        @@@@@@@@@@        ");
        mvprintw(11, menu_x - 5, "      @@        @@@     ");
        mvprintw(12, menu_x - 5, "     @@   @@@@@@@@@     ");
        mvprintw(13, menu_x - 5, "    @@  @@@     @@@           ");
        mvprintw(14, menu_x - 5, "    @@  @@@     @@@       ");
        mvprintw(15, menu_x - 5, "    @@  @@@     @@@     ");
        mvprintw(16, menu_x - 5, "     @@  @@@    @@@    ");
        mvprintw(17, menu_x - 5, "      @@   @@@@@@@@     ");
        mvprintw(18, menu_x - 5, "       @@             ");        
        mvprintw(19, menu_x - 5, "         @@@@@@@@@@ ");
        
        attroff(COLOR_PAIR(current_color) | A_BOLD);
        refresh();

        ch = getch();
        switch(ch) {
            case KEY_LEFT:
                if(current_color != 1)
                    current_color--;
                else
                    current_color = 7;
                break;
            case KEY_RIGHT:
                if(current_color != 7)
                    current_color++;
                else
                    current_color = 1;
                break;
            case '\n':
                game_color = current_color;
                return;
            case 'b':
                return;
        }
    }
}

int main(void) {
    initscr();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLACK);
        init_pair(2, COLOR_BLACK, COLOR_WHITE);
    }
    if (has_colors()) {
        start_color();
        use_default_colors();
        // Initialize the basic color pair for menu selection
        init_pair(1, COLOR_RED, -1);
        init_pair(2, COLOR_GREEN, -1);
        init_pair(3, COLOR_MAGENTA, -1);
        init_pair(4, COLOR_WHITE, -1);
        init_pair(5, COLOR_BLUE, -1);
        init_pair(6, COLOR_CYAN, -1);
        init_pair(7, COLOR_YELLOW, -1);
    }
    draw_menu();

    int ch;
    while ((ch = getch()) != 'q') {
        if (!is_logged_in) {
            switch (ch) {
                case KEY_UP:
                    selected = (selected > 0) ? selected - 1 : 1;
                    draw_menu();
                    break;
                case KEY_DOWN:
                    selected = (selected < 1) ? selected + 1 : 0;
                    draw_menu();
                    break;
                case '\n':
                    if (selected == 0) {
                        create_user();
                        if (is_logged_in) {
                            selected = 0;
                            main_menu();
                        } else {
                            draw_menu();
                        }
                    } else if (selected == 1) {
                        is_logged_in = login_user();
                        if (is_logged_in) {
                            selected = 0;
                            main_menu();
                        } else {
                            draw_menu();
                        }
                    }
                    break;
            }
        } else {
            switch (ch) {
                case KEY_UP:
                    selected = (selected > 0) ? selected - 1 : 3;
                    main_menu();
                    break;
                case KEY_DOWN:
                    selected = (selected < 3) ? selected + 1 : 0;
                    main_menu();
                    break;
                case '\n':
                    if(selected == 0){ // new game
    clear();
    selected = 0;  // Reset selection for new menu
    n_game();
    bool in_new_game = true;
    
    while(in_new_game) {
        int ch = getch();
        switch (ch) {
            case KEY_UP:
                selected = (selected > 0) ? selected - 1 : 1;
                n_game();
                break;
            case KEY_DOWN:
                selected = (selected < 1) ? selected + 1 : 0;
                n_game();
                break;
            case '\n':
                if(selected == 0) {
                    // Start game code here
                    clear();
                    mvprintw(3, 12, "Starting game...");
                    mvprintw(9, 12, "Difficulty: %s", 
                        dif_lvl == 0 ? "Easy" : 
                        dif_lvl == 1 ? "Medium" : "Hard");
                    refresh();
                    mvprintw(5,5,"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
                    mvprintw(7,5,"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
                    mvprintw(6,4,"|");
                    mvprintw(6,35,"|");
                    refresh();
                    for(int i = 5; i<35; i++){
                    mvprintw(6,i,"#");
                    refresh();
                    int time = (rand()%4)*100;
                    napms(time);
                    switch(dif_lvl){
                        case 0:
                        break;

                        case 1:
                        break;

                        case 2:
                        break;
                    }
    }
                    } else if(selected == 1) {
                    mvprintw(10, 10, "Settings menu...");
                    settings_menu();
                    clear_input_buffer();
                    clear();
                    n_game();
                    refresh();
                }
                break;
            case 'b':  // Add option to go back
                in_new_game = false;
                clear();
                main_menu();
                break;
        }
    }
                    
                    }
                    else if(selected == 1){
                        //load game
                    }
                    else if(selected == 2){ //profile
                        clear();
                        
                        do {
                            FILE *users = fopen("user_info.txt", "rb");
                            User temp_user;
                            char signin_time[50] = "Not available";
                            int user_score = 0;
                            int user_rank = 0;
                            
                            if (users != NULL) {
                                while (fread(&temp_user, sizeof(User), 1, users) == 1) {
                                    if (strcmp(temp_user.username, current_user) == 0) {
                                        strncpy(signin_time, temp_user.last_signin, sizeof(signin_time) - 1);
                                        signin_time[sizeof(signin_time) - 1] = '\0';
                                        user_score = temp_user.score;
                                        break;
                                    }
                                }
                                fclose(users);
                            }
                            use_default_colors();
                            user_rank = get_player_rank(current_user);
                            init_pair(1,COLOR_BLACK,COLOR_WHITE);
                            mvprintw(6, 5, "press ");
                            attron(COLOR_PAIR(1));
                            printw("b");
                            attroff(COLOR_PAIR(1));
                            printw(" to go back");
                            mvprintw(8, 5, "player name: %s", current_user);
                            mvprintw(10, 5, "player score: %d", user_score);
                            mvprintw(12, 5, "last sign in: %s", signin_time);
                            mvprintw(14, 5, "rank: %d", user_rank);
                            mvprintw(16,5,"gemes played:");
                            refresh();
                            
                            char in = getch();
                            if (in == 'b') {
                                clear();
                                main_menu();
                                break;
                            }
                        } while (1);
                    }
                    else if(selected == 3){
                        display_scoreboard();
                    }
                    break;
            }
        }
    }

    endwin();
    return 0;
}