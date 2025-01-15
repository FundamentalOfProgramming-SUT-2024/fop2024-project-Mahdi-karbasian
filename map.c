#include <ncurses.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <limits.h>
#include <locale.h>
#include <wchar.h>

#define VERTICAL '|'
#define HORIZ '_'
#define DOOR '+'
#define FLOOR '.'
#define PILLAR 'O'
#define GOLD_SMALL '*'    // 1 gold
#define GOLD_MEDIUM '*'   // 5 gold
#define GOLD_LARGE '*'    // 10 gold
#define STAIR '<'
#define TRAP_HIDDEN '.'    // Hidden trap symbol (will not be displayed until triggered)
#define TRAP_VISIBLE '^'   // Visible trap symbol (shown after triggering)
#define TRAP_DAMAGE 10     // Damage dealt by traps
#define MAX_HEALTH 100     // Maximum player health
#define MACE L"⚒"
#define FOOD '8'

#define MIN_ROOM_SIZE 4
#define MAX_ROOM_SIZE 16
#define MAX_ROOMS 12

#define MAP_WIDTH 160
#define MAP_HEIGHT 40

typedef struct {
    int x;
    int y;
} location;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int room_type; //1=treasure 2=enchant 3=regular
} Room;

typedef struct {
    Room room;
    location stair_pos;
    int original_x;  // Store the original x position of the room
    int original_y;  // Store the original y position of the room
} StairInfo;

typedef struct {
    Room rooms[MAX_ROOMS];
    int room_count;
} Level;

Level current_level;

char map[MAP_HEIGHT][MAP_WIDTH];
location spawn;
int i = 0;
location doors[MAX_ROOMS * 2];
int player_health = MAX_HEALTH;
char revealed_traps[MAP_HEIGHT][MAP_WIDTH] = {0}; // Tracks revealed traps
bool trap_locations[MAP_HEIGHT][MAP_WIDTH] = {{false}};  // Tracks where traps are
int gold_values[MAP_HEIGHT][MAP_WIDTH] = {{0}};
void init_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map[y][x] = ' ';
            gold_values[y][x] = 0;  // Initialize gold values
        }
    }
}

bool check_room_overlap(Room new_room, Room* rooms, int room_count) {
    const int BUFFER = 2;

    for (int i = 0; i < room_count; i++) {
        if (new_room.x - BUFFER < rooms[i].x + rooms[i].width + BUFFER &&
            new_room.x + new_room.width + BUFFER > rooms[i].x - BUFFER &&
            new_room.y - BUFFER < rooms[i].y + rooms[i].height + BUFFER &&
            new_room.y + new_room.height + BUFFER > rooms[i].y - BUFFER) {
            return true;
        }
    }

    if (new_room.x - BUFFER < 2 || new_room.x + new_room.width + BUFFER > MAP_WIDTH - 2 ||
        new_room.y - BUFFER < 2 || new_room.y + new_room.height + BUFFER > MAP_HEIGHT - 2) {
        return true;
    }

    return false;
}

void draw_room(Room room) {
    // Set color based on room type
    int color_pair;
    switch(room.room_type) {
        case 1: // Treasure room
            color_pair = 5;
            break;
        case 2: // Enchant room
            color_pair = 7;
            break;
        default: // Regular room
            color_pair = 6;
            break;
    }
    
    attron(COLOR_PAIR(color_pair));
    
    // Draw floor
    for (int y = room.y + 1; y < room.y + room.height; y++) {
        for (int x = room.x + 1; x < room.x + room.width; x++) {
            map[y][x] = FLOOR;
        }
    }

    // Draw walls
    for (int x = room.x; x <= room.x + room.width; x++) {
        map[room.y][x] = HORIZ;
        map[room.y + room.height][x] = HORIZ;
    }

    for (int y = room.y; y <= room.y + room.height; y++) {
        map[y][room.x] = VERTICAL;
        map[y][room.x + room.width] = VERTICAL;
    }
    
    attroff(COLOR_PAIR(color_pair));
}

Room find_furthest_room(Room* rooms, int room_count, location start_pos) {
    Room furthest_room = rooms[0];
    int max_distance = 0;
    
    for (int i = 0; i < room_count; i++) {
        // Calculate distance from room center to start position
        int room_center_x = rooms[i].x + (rooms[i].width / 2);
        int room_center_y = rooms[i].y + (rooms[i].height / 2);
        
        int distance = abs(room_center_x - start_pos.x) + 
                      abs(room_center_y - start_pos.y);
                      
        if (distance > max_distance) {
            max_distance = distance;
            furthest_room = rooms[i];
        }
    }
    
    return furthest_room;
}

void place_traps(Room* rooms, int room_count) {
    const float ROOM_TRAP_CHANCE = 0.5f;  // Base chance for a room to have traps
    const int MAX_TRAPS_PER_ROOM = 3;     // Maximum number of traps per room
    
    memset(trap_locations, 0, sizeof(trap_locations));
    
    for (int r = 0; r < room_count; r++) {
        float trap_chance = ROOM_TRAP_CHANCE;
        int max_traps = MAX_TRAPS_PER_ROOM;
        
        // Adjust trap chances based on room type
        if (rooms[r].room_type == 1) { // Treasure room
            trap_chance *= 0.5; // 50% less chance for traps
            max_traps = 1;     // Maximum 1 trap in treasure rooms
        } else if (rooms[r].room_type == 2) { // Enchant room
            trap_chance *= 0.7; // 30% less chance for traps
            max_traps = 2;     // Maximum 2 traps in enchant rooms
        }

        if ((rand() % 100) > (trap_chance * 100)) {
            continue;
        }
        Room room = rooms[r];
        int traps_placed = 0;
        int attempts = 0;
        const int MAX_ATTEMPTS = 20;
        
        while (traps_placed < MAX_TRAPS_PER_ROOM && attempts < MAX_ATTEMPTS) {
            // Generate position inside room (excluding walls)
            int x = room.x + 2 + (rand() % (room.width - 3));
            int y = room.y + 2 + (rand() % (room.height - 3));
            
            // Explicitly check if the position is a floor tile
            if (map[y][x] == FLOOR) {

                bool is_suitable = true;
                
                // Check surrounding area for doors, stairs, gold, and other traps
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        char nearby_tile = map[y + dy][x + dx];
                        if (nearby_tile == DOOR || 
                            nearby_tile == '=' || 
                            nearby_tile == STAIR || 
                            nearby_tile == GOLD_SMALL ||
                            nearby_tile == GOLD_MEDIUM ||
                            nearby_tile == GOLD_LARGE ||
                            nearby_tile == PILLAR ||
                            trap_locations[y + dy][x + dx] ||
                            nearby_tile == VERTICAL ||
                            nearby_tile == HORIZ) {
                            is_suitable = false;
                            break;
                        }
                    }
                    if (!is_suitable) break;
                }
                
                if (is_suitable) {
                    trap_locations[y][x] = true;
                    traps_placed++;
                }
            }
            attempts++;
        }
    }
}

location place_staircase(Room room) {
    location stair_pos;
    
    // Try to place the staircase near the center of the room
    int center_x = room.x + (room.width / 2);
    int center_y = room.y + (room.height / 2);
    
    // Check a few positions around the center for a suitable spot
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int x = center_x + dx;
            int y = center_y + dy;
            
            if (map[y][x] == FLOOR) {
                map[y][x] = STAIR;
                stair_pos.x = x;
                stair_pos.y = y;
                return stair_pos;
            }
        }
    }
    
    // If we couldn't place near center, try anywhere in the room
    for (int y = room.y + 1; y < room.y + room.height; y++) {
        for (int x = room.x + 1; x < room.x + room.width; x++) {
            if (map[y][x] == FLOOR) {
                map[y][x] = STAIR;
                stair_pos.x = x;
                stair_pos.y = y;
                return stair_pos;
            }
        }
    }
    
    return stair_pos;
}

void create_corridors(Room* rooms, int room_count) {
    bool connected[MAX_ROOMS] = { false };
    connected[0] = true;


    for (int count = 1; count < room_count; count++) {
        int best_src = -1;
        int best_dst = -1;
        int min_distance = INT_MAX;

        // Find the closest connection between a connected and unconnected room
        for (int i = 0; i < room_count; i++) {
            if (!connected[i]) continue;
            
            for (int j = 0; j < room_count; j++) {
                if (connected[j]) continue;
                
                int distance = abs(rooms[i].x - rooms[j].x) + abs(rooms[i].y - rooms[j].y);
                if (distance < min_distance) {
                    min_distance = distance;
                    best_src = i;
                    best_dst = j;
                }
            }
        }

        if (best_src != -1 && best_dst != -1) {
            connected[best_dst] = true;

            // Draw corridor between rooms
            int start_x = rooms[best_src].x + rooms[best_src].width / 2;
            int start_y = rooms[best_src].y + rooms[best_src].height / 2;
            int end_x = rooms[best_dst].x + rooms[best_dst].width / 2;
            int end_y = rooms[best_dst].y + rooms[best_dst].height / 2;

            // Draw L-shaped corridor
            if (rand() % 2 == 0) {
                // Horizontal first, then vertical
                int current_x = start_x;
                while (current_x != end_x) {
                    if (map[start_y][current_x] == ' ') {
                        map[start_y][current_x] = '#';
                    }
                    current_x += (current_x < end_x) ? 1 : -1;
                }
                
                int current_y = start_y;
                while (current_y != end_y) {
                    if (map[current_y][end_x] == ' ') {
                        map[current_y][end_x] = '#';
                    }
                    current_y += (current_y < end_y) ? 1 : -1;
                }
            } else {
                // Vertical first, then horizontal
                int current_y = start_y;
                while (current_y != end_y) {
                    if (map[current_y][start_x] == ' ') {
                        map[current_y][start_x] = '#';
                    }
                    current_y += (current_y < end_y) ? 1 : -1;
                }
                
                int current_x = start_x;
                while (current_x != end_x) {
                    if (map[end_y][current_x] == ' ') {
                        map[end_y][current_x] = '#';
                    }
                    current_x += (current_x < end_x) ? 1 : -1;
                }
            }
        }
    }
}

void place_doors() {
    for (int y = 1; y < MAP_HEIGHT - 1; y++) {
        for (int x = 1; x < MAP_WIDTH - 1; x++) {
            if (map[y][x] == '#') {
                bool is_corner = false;

                if ((map[y-1][x-1] == HORIZ && map[y][x-1] == VERTICAL) ||
                    (map[y-1][x+1] == HORIZ && map[y][x+1] == VERTICAL) ||
                    (map[y+1][x-1] == HORIZ && map[y][x-1] == VERTICAL) ||
                    (map[y+1][x+1] == HORIZ && map[y][x+1] == VERTICAL)) {
                    is_corner = true;
                }

                if (!is_corner) {
                    if (map[y][x-1] == VERTICAL) {
                        map[y][x-1] = DOOR;
                        if (map[y][x-2] == HORIZ)
                            map[y][x-2] = DOOR;
                        doors[i].x = x-1;
                        doors[i].y = y;
                        i++;
                    } else if (map[y][x+1] == VERTICAL) {
                        map[y][x+1] = DOOR;
                        if (map[y][x+2] == HORIZ)
                            map[y][x+2] = DOOR;
                        doors[i].x = x+1;
                        doors[i].y = y;
                        i++;
                    } else if (map[y-1][x] == HORIZ || map[y-1][x] == VERTICAL) {
                        map[y-1][x] = DOOR;
                        if (map[y-2][x] == VERTICAL)
                            map[y-2][x] = DOOR;
                        doors[i].x = x;
                        doors[i].y = y-1;
                        i++;
                    } else if (map[y+1][x] == HORIZ || map[y+1][x] == VERTICAL) {
                        map[y+1][x] = DOOR;
                        if (map[y+2][x] == VERTICAL)
                            map[y+2][x] = DOOR;
                        doors[i].x = x;
                        doors[i].y = y+1;
                        i++;
                    }
                }
            }
        }
    }
}

void place_pillars(Room room) {
    int max_pillars = (room.width * room.height) / 20;
    int num_pillars = 1 + (rand() % max_pillars);
    
    for (int i = 0; i < num_pillars; i++) {
        int attempts = 0;
        while (attempts < 10) {
            // Generate random position inside the room (excluding walls and a buffer from edges)
            int x = room.x + 2 + (rand() % (room.width - 3));  // Keep 2 tiles from edges
            int y = room.y + 2 + (rand() % (room.height - 3)); // Keep 2 tiles from edges
            
            // Check if the position is suitable
            if (map[y][x] == FLOOR) {
                // Check a larger surrounding area for doors, corridors, and other pillars
                bool is_suitable = true;
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        char nearby_tile = map[y + dy][x + dx];
                        // Don't place if near doors, corridors, or other pillars
                        if (nearby_tile == DOOR || 
                            nearby_tile == '=' || 
                            nearby_tile == PILLAR) {
                            is_suitable = false;
                            break;
                        }
                    }
                    if (!is_suitable) break;
                }
                
                // If position is suitable, place the pillar
                if (is_suitable) {
                    map[y][x] = PILLAR;
                    break;
                }
            }
            attempts++;
        }
    }
}

void place_gold(Room* rooms, int room_count) {
    for (int r = 0; r < room_count; r++) {
        Room room = rooms[r];
        int gold_count = 2; // default gold count
        float large_gold_chance = 0.1f; // default 10% chance for large gold
        
        // Adjust based on room type
        if (room.room_type == 1) { // Treasure room
            gold_count = 4; // More gold in treasure rooms
            large_gold_chance = 0.3f; // 30% chance for large gold
        } else if (room.room_type == 2) { // Enchant room
            gold_count = 1; // Less gold in enchant rooms
            large_gold_chance = 0.05f; // 5% chance for large gold
        }

        for (int i = 0; i < gold_count; i++) {
            int attempts = 0;
            while (attempts < 20) {
                int x = room.x + 2 + (rand() % (room.width - 3));
                int y = room.y + 2 + (rand() % (room.height - 3));
                
                if (map[y][x] == FLOOR) {
                    bool is_suitable = true;
                    // [existing suitability check code remains the same]
                    
                    if (is_suitable) {
                        float gold_roll = (float)rand() / RAND_MAX;
                        map[y][x] = GOLD_SMALL;  // Always use '*' symbol
                        
                        // Store the actual value in gold_values array
                        if (gold_roll < large_gold_chance) {
                            gold_values[y][x] = 10;  // Large gold value
                        } else if (gold_roll < large_gold_chance + 0.3f) {
                            gold_values[y][x] = 5;   // Medium gold value
                        } else {
                            gold_values[y][x] = 1;   // Small gold value
                        }
                        break;
                    }
                }
                attempts++;
            }
        }
    }
}

void place_food(Room* rooms, int room_count) {
    const float ROOM_CHANCE = 0.7f;  // 70% chance for a room to have food
    const int GOLD_PER_ROOM = 2;     // Number of food
    
    for (int r = 0; r < room_count; r++) {
        if ((rand() % 100) > (ROOM_CHANCE * 100)) {
            continue;
        }

        Room room = rooms[r];
        int food_placed = 0;
        int attempts = 0;
        
        while (food_placed < GOLD_PER_ROOM && attempts < 20) {
            int x = room.x + 2 + (rand() % (room.width - 3));
            int y = room.y + 2 + (rand() % (room.height - 3));
            
            if (map[y][x] == FLOOR) {
                bool is_suitable = true;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        char nearby_tile = map[y + dy][x + dx];
                        if (nearby_tile == DOOR || 
                            nearby_tile == '=' || 
                            nearby_tile == PILLAR || 
                            nearby_tile == GOLD_SMALL ||
                            nearby_tile == GOLD_MEDIUM ||
                            nearby_tile == GOLD_LARGE) {
                            is_suitable = false;
                            break;
                        }
                    }
                    if (!is_suitable) break;
                }
                
                if (is_suitable) {
                    int gold_type = rand() % 100;
                    if (gold_type < 60) {          
                        map[y][x] = FOOD;
                    }
                    food_placed++;
                }
            }
            attempts++;
        }
    }
}

void generate_rooms(StairInfo* prev_stair, location* player_pos) {
    current_level.room_count = 0;
    i = 0;
    int attempts = 0;
    const int MAX_ATTEMPTS = 1000;
    const int MIN_ROOMS = 6;
    
    // If we have previous stair info, create the first room in exactly the same position
    if (prev_stair != NULL) {
        Room first_room = prev_stair->room;
        // Keep the exact same position
        first_room.x = prev_stair->original_x;
        first_room.y = prev_stair->original_y;
        first_room.width = prev_stair->room.width;
        first_room.height = prev_stair->room.height;
        
        current_level.rooms[current_level.room_count] = first_room;
        draw_room(first_room);
        place_pillars(first_room);
        if(rand()%100 <80){
        first_room.room_type = 3;
        }
        else {
            first_room.room_type = 1;
        }
        current_level.room_count++;
        
        // Set the spawn point to exactly where the stairs were
        doors[0].x = prev_stair->stair_pos.x;
        doors[0].y = prev_stair->stair_pos.y;
    }

    // Generate remaining rooms
    while (current_level.room_count < MIN_ROOMS && attempts < MAX_ATTEMPTS) {
        Room new_room;
        new_room.width = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));
        new_room.height = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));

        new_room.x = 3 + (rand() % (MAP_WIDTH - new_room.width - 6));
        new_room.y = 3 + (rand() % (MAP_HEIGHT - new_room.height - 6));

        if (!check_room_overlap(new_room, current_level.rooms, current_level.room_count)) {
            current_level.rooms[current_level.room_count] = new_room;
            draw_room(new_room);
            place_pillars(new_room);
            if(rand()%100 <50){
        new_room.room_type = 3;
        }
        else if (rand() % 100 <80) {
            new_room.room_type = 1;
        }
        else{
            new_room.room_type = 2;
        }
            current_level.room_count++;
        }
        attempts++;
    }

    // Try to add additional rooms
    attempts = 0;
    while (current_level.room_count < MAX_ROOMS && attempts < MAX_ATTEMPTS/2) {
        if (rand() % 100 < 40) {
            Room new_room;
            new_room.width = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));
            new_room.height = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));

            new_room.x = 3 + (rand() % (MAP_WIDTH - new_room.width - 6));
            new_room.y = 3 + (rand() % (MAP_HEIGHT - new_room.height - 6));

            if (!check_room_overlap(new_room, current_level.rooms, current_level.room_count)) {
                current_level.rooms[current_level.room_count] = new_room;
                draw_room(new_room);
                      if(rand()%100 <50){
                      new_room.room_type = 3;
                      }
                       else if (rand() % 100 <80) {
                       new_room.room_type = 1;
                       }
                       else{
                      new_room.room_type = 2;
                       }
                current_level.room_count++;
            }
        }
        attempts++;
    }

    if (current_level.room_count < MIN_ROOMS) {
        endwin();
        fprintf(stderr, "Error: Could not generate minimum number of rooms\n");
        exit(1);
    }

    create_corridors(current_level.rooms, current_level.room_count);
    place_doors();
    place_gold(current_level.rooms, current_level.room_count);
    place_food(current_level.rooms, current_level.room_count);
    if (prev_stair == NULL) {
        // First level - place stairs in furthest room from start
        Room furthest = find_furthest_room(current_level.rooms, current_level.room_count, 
                                         (location){doors[0].x, doors[0].y});
        location stair_pos = place_staircase(furthest);
    } else {
        // Subsequent levels - place stairs in furthest room from player
        Room furthest = find_furthest_room(current_level.rooms, current_level.room_count, 
                                         (location){player_pos->x, player_pos->y});
        location stair_pos = place_staircase(furthest);
    }
    place_traps(current_level.rooms, current_level.room_count);
}

void draw_borders() {
    for (int i = 1; i < MAP_WIDTH; i++) {
        if (i % 2 == 1) {
            mvprintw(0, i, "/");
            mvprintw(MAP_HEIGHT - 1, i, "\\");
        } else {
            mvprintw(0, i, "\\");
            mvprintw(MAP_HEIGHT - 1, i, "/");
        }
    }
    for (int i = 1; i < MAP_HEIGHT; i++) {
        mvprintw(i, 1, "<");
        mvprintw(i, MAP_WIDTH - 1, ">");
    }
}

void draw_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map[y][x] != ' ') {
                // Determine room type for this position
                int room_type = 0;
                for (int i = 0; i < current_level.room_count; i++) {
                    if (x >= current_level.rooms[i].x && 
                        x <= current_level.rooms[i].x + current_level.rooms[i].width &&
                        y >= current_level.rooms[i].y && 
                        y <= current_level.rooms[i].y + current_level.rooms[i].height) {
                        room_type = current_level.rooms[i].room_type;
                        break;
                    }
                }
                
                // Set color based on room type
                int color_pair;
                switch(room_type) {
                    case 1: // Treasure room
                        color_pair = 5;
                        break;
                    case 2: // Enchant room
                        color_pair = 7;
                        break;
                    default: // Regular room or corridors
                        color_pair = 6;
                        break;
                }
                
                attron(COLOR_PAIR(color_pair));
                mvprintw(y, x, "%c", map[y][x]);
                attroff(COLOR_PAIR(color_pair));
            }
        }
    }
}

int get_current_room_type(location player) {
    for (int i = 0; i < current_level.room_count; i++) {
        Room room = current_level.rooms[i];
        if (player.x >= room.x && player.x <= room.x + room.width &&
            player.y >= room.y && player.y <= room.y + room.height) {
            return room.room_type;
        }
    }
    return 0; // Return 0 if player is in a corridor or outside any room
}

void update_status_line(int score, int level, int player_health, location player, time_t start_time) {
    int room_type = get_current_room_type(player);
    const char* room_name;
    attron(COLOR_PAIR(1)); 

    switch(room_type) {
        case 1:
            room_name = "Treasure Room";
            break;
        case 2:
            room_name = "Enchant Room";
            break;
        case 3:
            room_name = "Regular Room";
            break;
        default:
            room_name = "Corridor";
    }

    time_t current_time = time(NULL);
    int elapsed_time = (int)difftime(current_time, start_time);
    int minutes = elapsed_time / 60;
    int seconds = elapsed_time % 60;

    mvprintw(41, 1, "Score: %d | Level: %d | Health: %d | Room: %s | Time: %02d:%02d | User: %s", 
             score, level, player_health, room_name, minutes, seconds, "mahdi200584");
    
    attroff(COLOR_PAIR(3));
}

int main() {
    initscr();
    init_map();
    memset(trap_locations, 0, sizeof(trap_locations));
    memset(revealed_traps, 0, sizeof(revealed_traps));
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    
    if (!has_colors()) {
        endwin();
        printf("Your terminal does not support color\n");
        exit(1);
    }
    
    start_color();
    use_default_colors(); 


        init_pair(1, COLOR_BLACK, COLOR_WHITE);    // For messages
        init_pair(2, COLOR_YELLOW, -1);            // Bright player color
        init_pair(3, COLOR_WHITE, -1);             // For regular text
        init_pair(4, COLOR_RED, -1);               // For traps and damage messages

        init_pair(5, COLOR_YELLOW, -1);            // For treasure rooms
        init_pair(6, COLOR_WHITE, -1);             // For regular rooms
        init_pair(7, COLOR_MAGENTA, -1);           // For enchant rooms
        init_pair(8, COLOR_GREEN, -1);

    srand(time(NULL));
    time_t game_start_time = time(NULL);

    // Get current date and time
    time_t now = time(NULL);
    struct tm *tm_struct = gmtime(&now);
    char datetime[26];
    strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", tm_struct);

    StairInfo* prev_stair = NULL;
    init_map();
    memset(revealed_traps, 0, sizeof(revealed_traps));
    
    location player = {0, 0}; // Initialize player position
    player_health = MAX_HEALTH;
    
    generate_rooms(prev_stair, &player);

    draw_borders();
    draw_map();
    player.x = doors[0].x;  // Set initial player position to first door
    player.y = doors[0].y;
    attron(COLOR_PAIR(2));
    mvprintw(player.y, player.x, "@");
    attroff(COLOR_PAIR(2));

    int score = 0;
    int level = 1;  // Track level number

    // Display initial status
    mvprintw(42, 1, "Date/Time (UTC): %s", datetime);
    mvprintw(43, 1, "User: mahdi200584");
    update_status_line(score, level, player_health, player, game_start_time);
    refresh();

    int ch;
    while ((ch = getch()) != 'q') {
        mvprintw(player.y, player.x, " ");
        mvprintw(40, 1, "                                      ");

        switch (ch) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_LEFT:
            case KEY_RIGHT:
                {
                    int new_x = player.x;
                    int new_y = player.y;
                    
                    switch (ch) {
                        case KEY_UP:    new_y--; break;
                        case KEY_DOWN:  new_y++; break;
                        case KEY_LEFT:  new_x--; break;
                        case KEY_RIGHT: new_x++; break;
                    }
                    
                    // Check if move is valid
                    if (new_y > 0 && new_y < MAP_HEIGHT - 1 && new_x > 0 && new_x < MAP_WIDTH - 1 &&
                        map[new_y][new_x] != VERTICAL && map[new_y][new_x] != HORIZ && 
                        map[new_y][new_x] != PILLAR &&
                        (map[new_y][new_x] == FLOOR || map[new_y][new_x] == DOOR ||
                         map[new_y][new_x] == '#' || map[new_y][new_x] == GOLD_SMALL ||
                         map[new_y][new_x] == GOLD_MEDIUM || map[new_y][new_x] == GOLD_LARGE ||
                         map[new_y][new_x] == STAIR || map[new_y][new_x] == TRAP_HIDDEN ||map[new_y][new_x] == FOOD)) {
                        
                        // Clear old position
                        mvprintw(player.y, player.x, " ");
                        
                        // Update player position
                        player.x = new_x;
                        player.y = new_y;
                        
                        // Check for trap
                        if (trap_locations[player.y][player.x] && !revealed_traps[player.y][player.x]) {
                            player_health -= TRAP_DAMAGE;
                            revealed_traps[player.y][player.x] = 1;
                            map[player.y][player.x] = TRAP_VISIBLE;
                            
                            attron(COLOR_PAIR(4) | A_BOLD);
                            mvprintw(40, 1, "You stepped on a trap! -%d HP", TRAP_DAMAGE);
                            attroff(COLOR_PAIR(4) | A_BOLD);
                        }
                        // Check for gold collection
                        else if (map[player.y][player.x] == GOLD_SMALL) {
                        int gold_value = gold_values[player.y][player.x];
                        map[player.y][player.x] = FLOOR;
                        score += gold_value;
                        mvprintw(40, 1, "You found a coin worth %d gold!", gold_value);
}
                        //check for food
                        
                        else if (map[player.y][player.x] == FOOD) {
                            if(player_health < 100){
                            map[player.y][player.x] = FLOOR;
                            player_health += 10;
                            mvprintw(40, 1, "You found food! (+10)");
                            }
                            else
                            mvprintw(40, 1, "Already at full health!");
                        }
                    }
                }
                break;
        }

        // Check for player death
        if (player_health <= 0) {
            clear();
            attron(COLOR_PAIR(4) | A_BOLD);
            mvprintw(20, MAP_WIDTH/2 - 3, "GAME OVER!");
            attroff(COLOR_PAIR(4) | A_BOLD);
            mvprintw(22,MAP_WIDTH/2 - 5, "YOUR SCORE:%d",score);
            refresh();
            napms(2000);  // Wait 2 seconds
            getch();
            break;  // Exit the game loop
        }

        if(map[player.y][player.x] == STAIR) {
            mvprintw(40, 1, "Press ENTER to go to next level");
            refresh();
            int ch;
            while ((ch = getch()) != '\n' && ch != 'q') {
                // Wait for ENTER or q
            }
            if (ch != 'q') {
                if(level == 5){
            clear();
            attron(COLOR_PAIR(2) | A_BOLD);
            int temp = 30;
            mvprintw(0,temp,"                                     :x$$$$$$&&&&&&&&&&&&$$$$x:                                     ");
            mvprintw(1,temp,"                             :&&&&&$X&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&$;                              ");
            mvprintw(2,temp,"                             $&&     ;&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&$                             ");
            mvprintw(3,temp,"                   x&&&&&&&&&&&&     X&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&X                   ");
            mvprintw(4,temp,"                  &&&&&&&&&&&&&&     $&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.                 ");
            mvprintw(5,temp,"                 x&&&        X&&;    $&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&X        &&&X                 ");
            mvprintw(6,temp,"                 +&&&.       ;&&x    $&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&+        &&&+                 ");
            mvprintw(7,temp,"                  &&&&        &&X    $&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&        &&&&                  ");
            mvprintw(8,temp,"                  &&&&        &&X    $&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&        &&&&                  ");
            mvprintw(9,temp,"                   $&&&.      &&&:   x&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&      .&&&&.                  ");
            mvprintw(10,temp,"                    $&&&X     :&&+   .&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&:     x&&&$.                   ");
            mvprintw(11,temp,"                     X&&&$:    &&$.   &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&    .$&&&X                     ");
            mvprintw(12,temp,"                      .&&&&X.   &&+   &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&.   X&&&&:                      ");
            mvprintw(13,temp,"                        ;&&&&X. x&&.   &&&&&&&&&&&&&&&&&&&&&&&&&&&&x .X&&&&;                        ");
            mvprintw(14,temp,"                          +&&&&$+&&$   &&&&&&&&&&&&&&&&&&&&&&&&&&&&+$&&&&x                          ");
            mvprintw(15,temp,"                            x&&&&&&&x   &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&x                            ");
            mvprintw(16,temp,"                              :&&&&&&x  x&&&&&&&&&&&&&&&&&&&&&&&&&&&&:                              ");
            mvprintw(17,temp,"                                 x&&&&X  $&&&&&&&&&&&&&&&&&&&&&&&&x                                 ");
            mvprintw(18,temp,"                                    x&&X .&&&&&&&&&&&&&&&&&&&&&X                                    ");
            mvprintw(19,temp,"                                      &&$.:&&&&&&&&&&&&&&&&&&&                                      ");
            mvprintw(20,temp,"                                        &&::$&&&&&&&&&&&&&&&.                                       ");
            mvprintw(21,temp,"                                         ;&&.$&&&&&&&&&&&&+                                         ");
            mvprintw(22,temp,"                                           +&&&&&&&&&&&$x                                           ");
            mvprintw(23,temp,"                                             +&&&&&&&&+                                             ");
            mvprintw(24,temp,"                                            x&&&&&&&&&&X                                            ");
            mvprintw(25,temp,"                                             X$&&&&&&$X                                             ");
            mvprintw(26,temp,"                                                &&&&                                                ");
            mvprintw(27,temp,"                                                &&&&                                                ");
            mvprintw(28,temp,"                                                &&&&                                                ");
            mvprintw(29,temp,"                                                &&&&                                                ");
            mvprintw(30,temp,"                                                &&&&                                                ");
            mvprintw(31,temp,"                                               +&&&&x                                               ");
            mvprintw(32,temp,"                                              :&&&&&&:                                              ");
            mvprintw(33,temp,"                                             x&&&&&&&&x                                             ");
            mvprintw(34,temp,"                                         :+$&&&&&&&&&&&&$+:                                         ");
            mvprintw(35,temp,"                                    :&&&&&&&&&&&&&&&&&&&&&&&&&&$                                    ");
            mvprintw(36,temp,"                                 .xx&&&&&&&&&&&&&&&&&&&&&&&&&&&&$x+                                 ");
            mvprintw(37,temp,"                                X&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&:                               ");
            mvprintw(38,temp,"                                &&&&&&$++++++++++++++++++++++x&&&&&&+                               ");
            mvprintw(39,temp,"                                &&&&&$ ;                    ;. &&&&&+                               ");
            mvprintw(40,temp,"                                &&&&&$                         &&&&&+                               ");
            mvprintw(41,temp,"                                &&&&&$                         &&&&&+                               ");
            mvprintw(42,temp,"                                &&&&&$                         &&&&&+                               ");
            mvprintw(43,temp,"                                &&&&&&.                       +&&&&&+                               ");
            mvprintw(44,temp,"                                $&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&;                               ");
            mvprintw(45,temp,"                                 X$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$X+                                ");
            attroff(COLOR_PAIR(2) | A_BOLD);
            attron(COLOR_PAIR(8) | A_BOLD);
            mvprintw(39,76, "YOU WON!");
            attroff(COLOR_PAIR(8) | A_BOLD);
            mvprintw(41,74, "YOUR SCORE:%d",score);
            refresh();
            napms(2000);  
            getch();
            break;  
                }
                else{
                // Store current stair information before generating new level
                StairInfo prev_level_stair;
                prev_level_stair.stair_pos.x = player.x;
                prev_level_stair.stair_pos.y = player.y;
                
                // Find the room containing the stairs
                for (int i = 0; i < current_level.room_count; i++) {
                    if (player.x >= current_level.rooms[i].x && 
                        player.x <= current_level.rooms[i].x + current_level.rooms[i].width &&
                        player.y >= current_level.rooms[i].y && 
                        player.y <= current_level.rooms[i].y + current_level.rooms[i].height) {
                        prev_level_stair.room = current_level.rooms[i];
                        prev_level_stair.original_x = current_level.rooms[i].x;
                        prev_level_stair.original_y = current_level.rooms[i].y;
                        break;
                    }
                }

                // Generate new level
                clear();
                init_map();
                memset(trap_locations, 0, sizeof(trap_locations));
                memset(revealed_traps, 0, sizeof(revealed_traps));
                generate_rooms(&prev_level_stair, &player);
                level++;
                
                // Place player at the stairs position
                player.x = prev_level_stair.stair_pos.x;
                player.y = prev_level_stair.stair_pos.y;
                map[player.y][player.x] = FLOOR;  // Remove stair symbol at player position
                
                draw_borders();
                draw_map();
                mvprintw(42, 1, "Date/Time (UTC): %s", datetime);
                update_status_line(score, level, player_health, player, game_start_time);
                attron(COLOR_PAIR(2) | A_BOLD);
                mvprintw(player.y, player.x, "@");
                attroff(COLOR_PAIR(2) | A_BOLD);
                refresh();
            }
        }
        }
        // Update status lines
        update_status_line(score, level, player_health, player, game_start_time);
        draw_borders();
        draw_map();

        // Draw revealed traps
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (trap_locations[y][x] && revealed_traps[y][x]) {
                    attron(COLOR_PAIR(4));
                    mvprintw(y, x, "%c", TRAP_VISIBLE);
                    attroff(COLOR_PAIR(4));
                }
            }
        }

        // Draw player
        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(player.y, player.x, "@");
        attroff(COLOR_PAIR(2) | A_BOLD);

        // Refresh the static info
        mvprintw(42, 1, "Date/Time (UTC): %s", datetime);
        refresh();
    }
    
    endwin();
    return 0;
}