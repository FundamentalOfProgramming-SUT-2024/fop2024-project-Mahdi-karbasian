#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define VERTICAL '|'
#define HORIZ '_'
#define DOOR '+'
#define FLOOR '.'
#define MIN_ROOM_SIZE 4
#define MAX_ROOM_SIZE 12
#define MAX_ROOMS 8

typedef struct {
    int x;
    int y;
} location;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Room;

char map[24][80];
location spawn;             
int i = 0;                    
location doors[MAX_ROOMS * 4];  // Increased size for potentially more doors

void init_map() {
    for(int y = 0; y < 24; y++) {
        for(int x = 0; x < 80; x++) {
            map[y][x] = ' ';
        }
    }
}

bool check_room_overlap(Room new_room, Room* rooms, int room_count) {
    // Add a 2-pixel buffer around each room
    const int BUFFER = 2;
    
    for(int i = 0; i < room_count; i++) {
        // Check if the new room (with buffer) overlaps with any existing room (with buffer)
        if(new_room.x - BUFFER < rooms[i].x + rooms[i].width + BUFFER &&
           new_room.x + new_room.width + BUFFER > rooms[i].x - BUFFER &&
           new_room.y - BUFFER < rooms[i].y + rooms[i].height + BUFFER &&
           new_room.y + new_room.height + BUFFER > rooms[i].y - BUFFER) {
            return true;
        }
    }
    
    // Also check if the room (with buffer) would be too close to the map borders
    if(new_room.x - BUFFER < 2 ||  // Left border
       new_room.x + new_room.width + BUFFER > 77 ||  // Right border
       new_room.y - BUFFER < 2 ||  // Top border
       new_room.y + new_room.height + BUFFER > 21) {  // Bottom border
        return true;
    }
    
    return false;
}

void draw_room(Room room) {
    // Draw floor
    for(int y = room.y + 1; y < room.y + room.height; y++) {
        for(int x = room.x + 1; x < room.x + room.width; x++) {
            map[y][x] = FLOOR;
        }
    }

    // Draw walls
    for(int x = room.x; x <= room.x + room.width; x++) {
        map[room.y][x] = HORIZ;  // Top wall
        map[room.y + room.height][x] = HORIZ;  // Bottom wall
    }

    for(int y = room.y; y <= room.y + room.height; y++) {
        map[y][room.x] = VERTICAL;  // Left wall
        map[y][room.x + room.width] = VERTICAL;  // Right wall
    }
}

void create_corridors(Room* rooms, int room_count) {
    for(int i = 0; i < room_count - 1; i++) {
        // Get center points of rooms
        int start_x = rooms[i].x + rooms[i].width/2;
        int start_y = rooms[i].y + rooms[i].height/2;
        int end_x = rooms[i+1].x + rooms[i+1].width/2;
        int end_y = rooms[i+1].y + rooms[i+1].height/2;

        // Draw L-shaped corridor
        int current_x = start_x;
        while(current_x != end_x) {
            if(map[start_y][current_x] == ' ') {
                map[start_y][current_x] = '=';
            }
            current_x += (current_x < end_x) ? 1 : -1;
        }

        int current_y = start_y;
        while(current_y != end_y) {
            if(map[current_y][end_x] == ' ') {
                map[current_y][end_x] = '=';
            }
            current_y += (current_y < end_y) ? 1 : -1;
        }
    }
}

void place_doors() {
    for(int y = 1; y < 23; y++) {
        for(int x = 1; x < 79; x++) {
            if(map[y][x] == '=') {
                // Check if current position is near a corner by looking for wall intersections
                bool is_corner = false;
                
                // Check for corner patterns
                if((map[y-1][x-1] == HORIZ && map[y][x-1] == VERTICAL) ||
                   (map[y-1][x+1] == HORIZ && map[y][x+1] == VERTICAL) ||
                   (map[y+1][x-1] == HORIZ && map[y][x-1] == VERTICAL) ||
                   (map[y+1][x+1] == HORIZ && map[y][x+1] == VERTICAL)) {
                    is_corner = true;
                }

                if(!is_corner) {
                    // Check for vertical walls
                    if(map[y][x-1] == VERTICAL) {
                        map[y][x-1] = DOOR;  // Place door in the wall
                        doors[i].x = x-1;
                        doors[i].y = y;
                        i++;
                    }
                    else if(map[y][x+1] == VERTICAL) {
                        map[y][x+1] = DOOR;  // Place door in the wall
                        doors[i].x = x+1;
                        doors[i].y = y;
                        i++;
                    }
                    // Check for horizontal walls
                    else if(map[y-1][x] == HORIZ) {
                        map[y-1][x] = DOOR;  // Place door in the wall
                        doors[i].x = x;
                        doors[i].y = y-1;
                        i++;
                    }
                    else if(map[y+1][x] == HORIZ) {
                        map[y+1][x] = DOOR;  // Place door in the wall
                        doors[i].x = x;
                        doors[i].y = y+1;
                        i++;
                    }
                }
            }
        }
    }
}
void generate_rooms() {
    Room rooms[MAX_ROOMS];
    int room_count = 0;
    int max_attempts = 150;
    int attempts = 0;
    i = 0; // Reset door counter

    // Generate rooms
    while (room_count < 6 && attempts < max_attempts) {
        Room new_room;
        new_room.width = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));
        new_room.height = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));
        
        // Adjust the range for room placement to account for buffer space
        new_room.x = 3 + (rand() % (73 - new_room.width));
        new_room.y = 3 + (rand() % (18 - new_room.height));

        if (!check_room_overlap(new_room, rooms, room_count)) {
            rooms[room_count] = new_room;
            draw_room(new_room);
            room_count++;
            attempts = 0;
        } else {
            attempts++;
        }
    }

    // Create corridors between rooms
    create_corridors(rooms, room_count);

    // Place doors where corridors meet rooms
    place_doors();
}
void draw_borders() {
    for(int i = 1; i < 79; i++) {
        if(i % 2 == 1) {
            mvprintw(0, i, "/");
            mvprintw(23, i, "\\");
        } else {
            mvprintw(0, i, "\\");
            mvprintw(23, i, "/");
        }
    }
    for(int i = 1; i < 23; i++) {
        mvprintw(i, 1, "<");
        mvprintw(i, 79, ">");
    }
}

void draw_map() {
    for(int y = 0; y < 24; y++) {
        for(int x = 0; x < 80; x++) {
            if(map[y][x] != ' ') {
                mvprintw(y, x, "%c", map[y][x]);
            }
        }
    }
}

int main() {
    initscr();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    srand(time(NULL));

    init_map();
    generate_rooms();

    draw_borders();
    draw_map();
    location player = {doors[0].x, doors[0].y};
    mvprintw(player.y, player.x, "@");
    refresh();

    int ch;
    while((ch = getch()) != 'q') {
        mvprintw(player.y, player.x, " ");
        
        switch (ch) {
            case KEY_UP:
                if(player.y > 1 && map[player.y-1][player.x] != VERTICAL && 
                   map[player.y-1][player.x] != HORIZ && 
                   (map[player.y-1][player.x] == FLOOR || map[player.y-1][player.x] == DOOR || 
                    map[player.y-1][player.x] == '='))
                    player.y--;
                break;
            case KEY_DOWN:
                if(player.y < 22 && map[player.y+1][player.x] != VERTICAL && 
                   map[player.y+1][player.x] != HORIZ && 
                   (map[player.y+1][player.x] == FLOOR || map[player.y+1][player.x] == DOOR || 
                    map[player.y+1][player.x] == '='))
                    player.y++;
                break;
            case KEY_LEFT:
                if(player.x > 3 && map[player.y][player.x-1] != VERTICAL && 
                   map[player.y][player.x-1] != HORIZ && 
                   (map[player.y][player.x-1] == FLOOR || map[player.y][player.x-1] == DOOR || 
                    map[player.y][player.x-1] == '='))
                    player.x--;
                break;
            case KEY_RIGHT:
                if(player.x < 77 && map[player.y][player.x+1] != VERTICAL && 
                   map[player.y][player.x+1] != HORIZ && 
                   (map[player.y][player.x+1] == FLOOR || map[player.y][player.x+1] == DOOR || 
                    map[player.y][player.x+1] == '='))
                    player.x++;
                break;
        }
        
        draw_borders();
        draw_map();
        mvprintw(player.y, player.x, "@");
        refresh();
    }

    endwin();
    return 0;
}