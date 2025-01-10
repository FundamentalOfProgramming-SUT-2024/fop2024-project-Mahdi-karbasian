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
} User;

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
static int selected = 0;
static char current_user[50] = "";
static bool is_logged_in = false;

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

void update_signin_time(const char *username) {
    FILE *users = fopen("user_info.txt", "rb+");
    if (users == NULL) return;

    User temp_user;
    char current_time[50];
    get_current_utc_time(current_time, sizeof(current_time));
    
    while (fread(&temp_user, sizeof(User), 1, users) == 1) {
        if (strcmp(temp_user.username, username) == 0) {
            fseek(users, -sizeof(User), SEEK_CUR);
            strncpy(temp_user.last_signin, current_time, sizeof(temp_user.last_signin) - 1);
            temp_user.last_signin[sizeof(temp_user.last_signin) - 1] = '\0';
            fwrite(&temp_user, sizeof(User), 1, users);
            break;
        }
    }
    fclose(users);
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

    // Initialize sign-in time
    char current_time[50];
    get_current_utc_time(current_time, sizeof(current_time));
    strncpy(new_user.last_signin, current_time, sizeof(new_user.last_signin) - 1);
    new_user.last_signin[sizeof(new_user.last_signin) - 1] = '\0';

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
                    if(selected == 0){
                        // new game
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

                            mvprintw(6, 5, "press b to go back");
                            mvprintw(7, 5, "player name: %s", current_user);
                            mvprintw(8, 5, "player score: %d", user_score);
                            mvprintw(9, 5, "last sign in: %s", signin_time);
                            mvprintw(10, 5, "rank:");
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
                        //score board
                    }
                    break;
            }
        }
    }

    endwin();
    return 0;
}