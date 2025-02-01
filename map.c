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
#define MAX_HEALTH 1000     // Maximum player health
#define FOOD '8'
#define SWORD_SYMBOL 'I'
#define DAGGER_SYMBOL 'd'
#define MAGIC_WAND_SYMBOL '%'
#define ARROW_SYMBOL 'V'
#define POTION_HEALTH 'H'
#define POTION_SPEED 's'
#define POTION_DAMAGE '7'
#define POTION_DURATION 10  // Duration in moves
#define MIN_ROOM_SIZE 4
#define MAX_ROOM_SIZE 16
#define MAX_ROOMS 12

#define GOLDEN_KEY 'K'  // Symbol for the golden key
#define BROKEN_KEY 'k'  // Symbol for broken key
#define KEY_BREAK_CHANCE 10  // 10% chance of breaking

#define MAP_WIDTH 160
#define MAP_HEIGHT 40
#define MAX_INVENTORY 10
#define FOOD_HEAL 10
#define PASSWORD_SYMBOL '?' // Symbol for password location
#define LOCKED_DOOR '=' // Symbol for locked door
#define PASSWORD_LENGTH 4 // Length of generated password
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX_ENEMIES 50
#define ENEMY_DEMON 'D'
#define ENEMY_FIRE 'F'
#define ENEMY_GIANT 'G'
#define ENEMY_SNAKE 'S'
#define ENEMY_UNDEAD 'U'

// Enemy spawn rate controls
#define SPAWN_RATE_VERY_LOW  0.2f   // 20% chance per room
#define SPAWN_RATE_LOW       0.4f   // 40% chance per room
#define SPAWN_RATE_NORMAL    0.7f   // 70% chance per room (original value)
#define SPAWN_RATE_HIGH      0.85f  // 85% chance per room
#define SPAWN_RATE_VERY_HIGH 1.0f   // 100% chance per room

#define MAX_ENEMIES_VERY_LOW  1     // Max enemies per room
#define MAX_ENEMIES_LOW       2
#define MAX_ENEMIES_NORMAL    3     // Original value
#define MAX_ENEMIES_HIGH      4
#define MAX_ENEMIES_VERY_HIGH 5
#define MIN(a,b) ((a) < (b) ? (a) : (b))

// Global spawn rate controls


typedef struct {
    char type;          // D, F, G, S, or U
    int x, y;          // Position
    int health;        // Current health
    int max_health;    // Maximum health
    int damage;        // Damage dealt to player
    int follow_count;  // How many more moves to follow (for G and U)
    bool is_tracking;  // Whether currently tracking the player
    bool is_active;    // Whether the enemy is alive
    bool is_paralyzed;     // From your original enemy struct
    int paralysis_duration; // From your original enemy struct
} Enemy;

typedef struct {
    Enemy enemies[MAX_ENEMIES];
    int count;
} EnemyList;

EnemyList current_enemies;

typedef struct {
    char symbol;
    char* name;
    int heal_value;    // For food
    int damage;        // For weapons
    bool isEquipped;
    bool isWeapon;
    int count;         // For stackable items
    int max_stack;     // Maximum number that can be carried
    bool can_throw;    // Whether weapon can be thrown
    int throw_range;   // Maximum throwing range
    bool isKey;        // New: whether item is a key
    bool isBroken;     // New: whether key is broken
    bool isPotion;
    int potion_moves_left;  // For tracking duration
    int speed_bonus;        // For speed potion
    int damage_multiplier;  // For damage potion
    int heal_rate;         // For health potion
} Item;

typedef struct {
    Item items[MAX_INVENTORY];
    int count;
} Inventory;

Inventory player_inventory;

typedef struct {
    int x;
    int y;
} location;

location player;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int room_type; //1=treasure 2=enchant 3=regular
    bool visited;
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



/*
typedef struct{
    char symbol;
    int damage;
    char* name;
    bool isEquipped;
    int max_amount;
} weapon;
*/

Item mace;
Item sword;
Item dagger;
Item arrow;
Item Magic_wand;
int health_regen_rate = 0;
int potion_moves_left = 0;
int moves_since_potion = 0;
bool speed_active = false;
bool damage_active = false;
bool health_active = false;
char map[MAP_HEIGHT][MAP_WIDTH];
location spawn;
int i = 0;
location doors[MAX_ROOMS * 2];
int player_health = MAX_HEALTH;
char revealed_traps[MAP_HEIGHT][MAP_WIDTH] = {0}; // Tracks revealed traps
bool trap_locations[MAP_HEIGHT][MAP_WIDTH] = {{false}};  // Tracks where traps are
int gold_values[MAP_HEIGHT][MAP_WIDTH] = {{0}};
bool hidden_door = false;
char current_password[5]; // 4 chars + null terminator
bool password_shown = false;
time_t password_show_time = 0;
location password_location;
location locked_door_location;
float current_spawn_rate = 0.3f;
int enemy_spawn_level = 3;
bool door_unlocked = false;
bool visited_tiles[MAP_HEIGHT][MAP_WIDTH] = {false};
int speed_potion_moves = 0;
const int SPEED_POTION_DURATION = 10; // Duration in moves
void generate_password(void);
void throw_weapon(location start, int dx, int dy, int max_range, int damage, bool drops_after_hit);
void remove_item_from_inventory(Item* item);
void damage_enemy(int x, int y, int damage);
void attack_with_weapon(location player, char weapon_type, int direction_x, int direction_y);
void handle_attack(int key);
void draw_borders(void);


void init_map() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            map[y][x] = ' ';
            gold_values[y][x] = 0;  // Initialize gold values
        }
    }
}

void generate_password() {
    const char charset[] = "0123456789";
    for (int i = 0; i < PASSWORD_LENGTH; i++) {
        current_password[i] = charset[rand() % 10];
    }
    current_password[PASSWORD_LENGTH] = '\0';
}

void place_locked_door_and_password(Room* rooms, int room_count) {
    // First, find rooms that have exactly one door
    typedef struct {
        Room* room;
        location door_loc;
        int door_count;
    } RoomDoorInfo;
    
    RoomDoorInfo room_info[MAX_ROOMS];

    // Initialize room_info
    for (int r = 0; r < room_count; r++) {
        room_info[r].room = &rooms[r];
        room_info[r].door_count = 0;
    }

    // Debug print
    mvprintw(0, 0, "Total doors: %d", i);
    refresh();

    // Count doors for each room
    for (int d = 0; d < i; d++) {  // i is the global door counter
        for (int r = 0; r < room_count; r++) {
            if (rooms[r].room_type == 3) {  // Only consider regular rooms
                // Check if this door belongs to this room
                if ((doors[d].x >= rooms[r].x - 1 && doors[d].x <= rooms[r].x + rooms[r].width + 1) &&
                    (doors[d].y >= rooms[r].y - 1 && doors[d].y <= rooms[r].y + rooms[r].height + 1)) {
                    room_info[r].door_count++;
                    if (room_info[r].door_count == 1) {
                        room_info[r].door_loc = doors[d];
                    }
                }
            }
        }
    }

    // Create array of rooms with exactly one door
    int single_door_count = 0;
    RoomDoorInfo* single_door_rooms[MAX_ROOMS];

    for (int r = 0; r < room_count; r++) {
        if (room_info[r].door_count == 1) {
            single_door_rooms[single_door_count++] = &room_info[r];
        }
    }

    if (single_door_count == 0) return;  // No suitable rooms found

    // Select a random room with one door
    int selected = rand() % single_door_count;
    RoomDoorInfo* selected_info = single_door_rooms[selected];

    // Make this door the locked door
    locked_door_location = selected_info->door_loc;
    map[locked_door_location.y][locked_door_location.x] = LOCKED_DOOR;

    // Place password in starting room (room[0])
    Room* password_room = &rooms[0];
    
    // Find a suitable corner in the starting room for the password
    bool password_placed = false;
    int attempts = 0;
    while (!password_placed && attempts < 20) {
        int corner = rand() % 4;
        switch(corner) {
            case 0: // top-left
                password_location.x = password_room->x + 1;
                password_location.y = password_room->y + 1;
                break;
            case 1: // top-right
                password_location.x = password_room->x + password_room->width - 1;
                password_location.y = password_room->y + 1;
                break;
            case 2: // bottom-left
                password_location.x = password_room->x + 1;
                password_location.y = password_room->y + password_room->height - 1;
                break;
            case 3: // bottom-right
                password_location.x = password_room->x + password_room->width - 1;
                password_location.y = password_room->y + password_room->height - 1;
                break;
        }
        
        if (map[password_location.y][password_location.x] == FLOOR) {
            map[password_location.y][password_location.x] = PASSWORD_SYMBOL;
            password_placed = true;
        }
        attempts++;
    }

    if (password_placed) {
        generate_password();
        door_unlocked = false;
    }
}

void init_weapons() {
    mace.name = strdup("MACE");
    mace.damage = 5;
    mace.isEquipped = true;
    mace.symbol = 'M';
    mace.max_stack = 1;

    dagger.name = strdup("DAGGER");
    dagger.damage = 12;
    dagger.symbol = 'D';
    dagger.isEquipped = false;
    dagger.max_stack = 10;  // Can carry up to 10 daggers

    Magic_wand.name = strdup("MAGIC WAND");
    Magic_wand.damage = 15;
    Magic_wand.symbol = '%';
    Magic_wand.isEquipped = false;
    Magic_wand.max_stack = 8;  // Can carry up to 8 wands

    arrow.name = strdup("ARROW");
    arrow.damage = 5;
    arrow.max_stack = 20;  // Can carry up to 20 arrows
    arrow.symbol = 'V';
    arrow.isEquipped = false;

    sword.name = strdup("SWORD");
    sword.damage = 10;
    sword.isEquipped = false;
    sword.max_stack = 1;  // Can only carry 1 sword
    sword.symbol = 'I';
}

void init_starting_weapon() {
    // Add mace to starting inventory
    if (player_inventory.count < MAX_INVENTORY) {
        player_inventory.items[player_inventory.count].symbol = 'M';
        player_inventory.items[player_inventory.count].name = "MACE";
        player_inventory.items[player_inventory.count].damage = mace.damage;
        player_inventory.items[player_inventory.count].isEquipped = true;
        player_inventory.items[player_inventory.count].isWeapon = true;
        player_inventory.items[player_inventory.count].heal_value = 0;
        player_inventory.count++;
    }
}

bool sword_collected = false;
bool sword_placed = false;

bool can_pickup_weapon(char weapon_symbol) {
    if (player_inventory.count >= MAX_INVENTORY) {
        return false;
    }

    int weapon_count = 0;
    for (int i = 0; i < player_inventory.count; i++) {
        if (player_inventory.items[i].symbol == weapon_symbol) {
            weapon_count++;
            // Special case for sword and mace (only one allowed)
            if (weapon_symbol == SWORD_SYMBOL || weapon_symbol == 'M') {
                return false;
            }
            // For other weapons, check their limits
            if ((weapon_symbol == DAGGER_SYMBOL && weapon_count >= 10) ||
                (weapon_symbol == MAGIC_WAND_SYMBOL && weapon_count >= 8) ||
                (weapon_symbol == ARROW_SYMBOL && weapon_count >= 20)) {
                return false;
            }
        }
    }
    return true;
}

bool is_point_in_room(int x, int y, Room room) {
    return (x >= room.x && x <= room.x + room.width &&
            y >= room.y && y <= room.y + room.height);
}

void pickup_weapon(char weapon_symbol, int x, int y) {
    if (!can_pickup_weapon(weapon_symbol)) {
        mvprintw(40, 1, "Cannot pick up more of this weapon type!");
        return;
    }

    // Find if weapon already exists in inventory
    for(int i = 0; i < player_inventory.count; i++) {
        if(player_inventory.items[i].symbol == weapon_symbol) {
            // For stackable weapons
            if(weapon_symbol == DAGGER_SYMBOL || 
               weapon_symbol == MAGIC_WAND_SYMBOL || 
               weapon_symbol == ARROW_SYMBOL) {
                if(player_inventory.items[i].count < player_inventory.items[i].max_stack) {
                    player_inventory.items[i].count++;
                    map[y][x] = FLOOR;
                    mvprintw(40, 1, "Added %s to existing stack!", 
                    player_inventory.items[i].name);
                    return;
                }
            }
        }
    }

    // If not found or not stackable, add new weapon
    Item* new_item = &player_inventory.items[player_inventory.count];
    new_item->symbol = weapon_symbol;
    new_item->isWeapon = true;
    new_item->isEquipped = false;
    new_item->count = 1;

    switch(weapon_symbol) {
        case SWORD_SYMBOL:
            new_item->name = "SWORD";
            new_item->damage = 10;
            new_item->can_throw = false;
            new_item->max_stack = 1;
            sword_collected = true;
            break;
        case DAGGER_SYMBOL:
            new_item->name = "DAGGER";
            new_item->damage = 12;
            new_item->can_throw = true;
            new_item->throw_range = 5;
            new_item->max_stack = 10;
            break;
        case MAGIC_WAND_SYMBOL:
            new_item->name = "MAGIC WAND";
            new_item->damage = 15;
            new_item->can_throw = true;
            new_item->throw_range = 10;
            new_item->max_stack = 8;
            break;
        case ARROW_SYMBOL:
            new_item->name = "ARROW";
            new_item->damage = 5;
            new_item->can_throw = true;
            new_item->throw_range = 5;
            new_item->max_stack = 20;
            break;
        default:
            return;
    }

    player_inventory.count++;
    map[y][x] = FLOOR;
    mvprintw(40, 1, "Picked up %s!", new_item->name);
}

void init_inventory() {
    player_inventory.count = 0;
    for (int i = 0; i < MAX_INVENTORY; i++) {
        player_inventory.items[i].symbol = ' ';
        player_inventory.items[i].name = NULL;
        player_inventory.items[i].heal_value = 0;
        player_inventory.items[i].damage = 0;
        player_inventory.items[i].isEquipped = false;
        player_inventory.items[i].isWeapon = false;
    }

    // Add starting mace
    if (player_inventory.count < MAX_INVENTORY) {
        Item* mace_item = &player_inventory.items[player_inventory.count];
        mace_item->symbol = 'M';
        mace_item->name = "MACE";
        mace_item->damage = 5;
        mace_item->isEquipped = true;
        mace_item->isWeapon = true;
        mace_item->heal_value = 0;
        player_inventory.count++;
    }
}

void display_inventory() {
    clear();
    mvprintw(1, 1, "=== INVENTORY ===");
    mvprintw(2, 1, "Items: %d/%d", player_inventory.count, MAX_INVENTORY);
    mvprintw(3, 1, "Current Health: %d/%d", player_health, MAX_HEALTH);
    
    int row = 5;
    
    // First display weapons
    mvprintw(row++, 1, "WEAPONS:");
    for (int i = 0; i < player_inventory.count; i++) {
        if (player_inventory.items[i].isWeapon) {
            mvprintw(row, 1, "%d. %c %s (Damage: %d)%s", 
                     i + 1,
                     player_inventory.items[i].symbol,
                     player_inventory.items[i].name,
                     player_inventory.items[i].damage,
                     player_inventory.items[i].isEquipped ? " [EQUIPPED]" : "");
            
            if (player_inventory.items[i].count > 1) {
                mvprintw(row, 50, " [%d/%d]", 
                        player_inventory.items[i].count, 
                        player_inventory.items[i].max_stack);
            }
            row++;
        }
    }
    
    row += 2;
    mvprintw(row++, 1, "FOOD:");
    for (int i = 0; i < player_inventory.count; i++) {
        if (!player_inventory.items[i].isWeapon && !player_inventory.items[i].isPotion) {
            mvprintw(row++, 1, "%d. %c %s (Heals: %d)", 
                     i + 1,
                     player_inventory.items[i].symbol,
                     player_inventory.items[i].name,
                     player_inventory.items[i].heal_value);
        }
    }

    row += 2;
    mvprintw(row++, 1, "POTIONS:");
    for (int i = 0; i < player_inventory.count; i++) {
        if (player_inventory.items[i].isPotion) {
            mvprintw(row++, 1, "%d. %c %s", 
                     i + 1,
                     player_inventory.items[i].symbol,
                     player_inventory.items[i].name);
        }
    }

    mvprintw(row + 2, 1, "Press 1-9 to use items or equip weapons");
    mvprintw(row + 3, 1, "Press 't' to throw equipped weapon");
    mvprintw(row + 4, 1, "Press 'i' to close inventory");
    refresh();
}

void init_enemy(Enemy* enemy, char type) {
    enemy->type = type;
    enemy->is_active = true;
    enemy->is_tracking = false;
    enemy->follow_count = 0;
    
    switch(type) {
        case ENEMY_DEMON:
            enemy->max_health = enemy->health = 5;
            enemy->damage = 5;
            break;
        case ENEMY_FIRE:
            enemy->max_health = enemy->health = 10;
            enemy->damage = 8;
            break;
        case ENEMY_GIANT:
            enemy->max_health = enemy->health = 15;
            enemy->damage = 10;
            enemy->follow_count = 5;
            break;
        case ENEMY_SNAKE:
            enemy->max_health = enemy->health = 20;
            enemy->damage = 12;
            break;
        case ENEMY_UNDEAD:
            enemy->max_health = enemy->health = 30;
            enemy->damage = 15;
            enemy->follow_count = 5;
            break;
    }
}

void place_potions(Room* rooms, int room_count) {
    const float POTION_CHANCE = 0.4f;    // 40% chance for an enchant room to have potions
    const int MAX_POTIONS = 2;           // Maximum 2 potions per enchant room
    
    for (int r = 0; r < room_count; r++) {
        // Only place potions in enchant rooms (type 2)
        if (rooms[r].room_type != 2) {
            continue;
        }

        // Check if this enchant room should have potions
        if ((rand() % 100) > (POTION_CHANCE * 100)) {
            continue;
        }

        Room room = rooms[r];
        int potions_placed = 0;
        int attempts = 0;
        const int MAX_ATTEMPTS = 20;
        
        while (potions_placed < MAX_POTIONS && attempts < MAX_ATTEMPTS) {
            int x = room.x + 2 + (rand() % (room.width - 3));
            int y = room.y + 2 + (rand() % (room.height - 3));
            
            if (map[y][x] == FLOOR) {
                bool is_suitable = true;
                
                // Check surrounding area for other items
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        char nearby_tile = map[y + dy][x + dx];
                        if (nearby_tile == DOOR || 
                            nearby_tile == STAIR || 
                            nearby_tile == GOLD_SMALL ||
                            nearby_tile == PILLAR ||
                            nearby_tile == FOOD ||
                            nearby_tile == POTION_HEALTH ||
                            nearby_tile == POTION_SPEED ||
                            nearby_tile == POTION_DAMAGE) {
                            is_suitable = false;
                            break;
                        }
                    }
                    if (!is_suitable) break;
                }
                
                if (is_suitable) {
                    // Randomly choose potion type
                    int potion_type = rand() % 3;
                    switch(potion_type) {
                        case 0: map[y][x] = POTION_HEALTH; break;
                        case 1: map[y][x] = POTION_SPEED; break;
                        case 2: map[y][x] = POTION_DAMAGE; break;
                    }
                    potions_placed++;
                }
            }
            attempts++;
        }
    }
}

void pickup_potion(char potion_type, int x, int y) {
    if (player_inventory.count >= MAX_INVENTORY) {
        mvprintw(40, 1, "Inventory full! Cannot pick up potion!");
        return;
    }

    Item* new_potion = &player_inventory.items[player_inventory.count];
    new_potion->symbol = potion_type;
    new_potion->isPotion = true;
    new_potion->potion_moves_left = 0;
    new_potion->isEquipped = false;
    new_potion->isWeapon = false;  // Make sure it's not marked as a weapon

    switch(potion_type) {
        case POTION_HEALTH:
            new_potion->name = "Health Potion";
            new_potion->heal_rate = 2;
            break;
        case POTION_SPEED:
            new_potion->name = "Speed Potion";
            new_potion->speed_bonus = 2;
            break;
        case POTION_DAMAGE:
            new_potion->name = "Damage Potion";
            new_potion->damage_multiplier = 2;
            break;
    }

    player_inventory.count++;
    map[y][x] = FLOOR;
    mvprintw(40, 1, "Picked up %s!", new_potion->name);
}

void use_potion(Item* potion) {
    if (!potion->isPotion) return;

    switch(potion->symbol) {
        case POTION_HEALTH:
            health_active = true;
            health_regen_rate = 10;  // 10 health points per step
            potion_moves_left = 10;  // Last for 10 steps
            mvprintw(40, 1, "Health regeneration activated! +10 HP per step for 10 steps!");
            break;
        case POTION_SPEED:
            speed_active = true;
            speed_potion_moves = SPEED_POTION_DURATION;
            mvprintw(40, 1, "Movement speed doubled for %d moves!", SPEED_POTION_DURATION);
            break;
        case POTION_DAMAGE:
            damage_active = true;
            potion->potion_moves_left = POTION_DURATION;  // 10 moves
            mvprintw(40, 1, "Damage doubled for next %d moves!", POTION_DURATION);
            moves_since_potion = 0;
            break;
    }

    // Remove the used potion
    remove_item_from_inventory(potion);
    refresh();
}

void place_enemies_in_room(Room room) {
    // Basic safety checks
    if (room.width < 5 || room.height < 5 || current_enemies.count >= MAX_ENEMIES) {
        return;
    }

    // Ensure spawn rate is within valid range
    if (current_spawn_rate < 0.0f) current_spawn_rate = 0.0f;
    if (current_spawn_rate > 1.0f) current_spawn_rate = 1.0f;

    // Calculate safe area for enemy placement
    int safe_width = room.width - 4;  // Leave 2 tiles buffer on each side
    int safe_height = room.height - 4; // Leave 2 tiles buffer on each side

    // Safety check for room size
    if (safe_width <= 0 || safe_height <= 0) {
        return;
    }

    // Check spawn chance
    int spawn_chance = (int)(current_spawn_rate * 100.0f);
    if (spawn_chance <= 0 || (rand() % 100) >= spawn_chance) {
        return;
    }

    // Calculate number of enemies based on room size and max setting
    int room_size_max = (safe_width * safe_height) / 16;
    if (room_size_max <= 0) room_size_max = 1;
    if (room_size_max > enemy_spawn_level) room_size_max = enemy_spawn_level;

    int num_enemies = 1 + (rand() % room_size_max);
    if (num_enemies > MAX_ENEMIES - current_enemies.count) {
        num_enemies = MAX_ENEMIES - current_enemies.count;
    }

    for (int i = 0; i < num_enemies; i++) {
        int attempts = 0;
        const int MAX_ATTEMPTS = 20;
        
        while (attempts < MAX_ATTEMPTS) {
            // Ensure we're placing within the safe area
            int x = room.x + 2 + (rand() % safe_width);
            int y = room.y + 2 + (rand() % safe_height);
            
            // Verify coordinates are within map bounds
            if (x <= 1 || x >= MAP_WIDTH - 2 || y <= 1 || y >= MAP_HEIGHT - 2) {
                attempts++;
                continue;
            }

            if (map[y][x] == FLOOR) {
                bool space_is_clear = true;
                
                // Check surrounding area
                for (int dy = -1; dy <= 1 && space_is_clear; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int check_y = y + dy;
                        int check_x = x + dx;
                        
                        // Verify we're not checking outside map bounds
                        if (check_x < 0 || check_x >= MAP_WIDTH || 
                            check_y < 0 || check_y >= MAP_HEIGHT) {
                            space_is_clear = false;
                            break;
                        }
                        
                        char tile = map[check_y][check_x];
                        if (tile != FLOOR && tile != '#') {
                            space_is_clear = false;
                            break;
                        }
                    }
                }
                
                if (space_is_clear && current_enemies.count < MAX_ENEMIES) {
                    Enemy* new_enemy = &current_enemies.enemies[current_enemies.count];
                    
                    // Randomly select enemy type
                    char enemy_types[] = {ENEMY_DEMON, ENEMY_FIRE, ENEMY_GIANT, 
                                        ENEMY_SNAKE, ENEMY_UNDEAD};
                    char type = enemy_types[rand() % 5];
                    
                    init_enemy(new_enemy, type);
                    new_enemy->x = x;
                    new_enemy->y = y;
                    map[y][x] = type;
                    
                    current_enemies.count++;
                    break;
                }
            }
            attempts++;
        }
    }
}

void set_enemy_spawn_rate(char difficulty) {
    if (difficulty >= '1' && difficulty <= '5') {
        enemy_spawn_level = difficulty - '0';
        mvprintw(40, 1, "Enemy spawn rate set to level %d", enemy_spawn_level);
        refresh();
    }
}
// Move enemy towards player
bool move_enemy(Enemy* enemy, int player_x, int player_y) {
    if (!enemy || !enemy->is_active) return false;
    
    // Calculate direction to move
    int dx = (player_x > enemy->x) ? 1 : (player_x < enemy->x) ? -1 : 0;
    int dy = (player_y > enemy->y) ? 1 : (player_y < enemy->y) ? -1 : 0;
    
    // Safety checks for boundaries
    if (enemy->x + dx <= 0 || enemy->x + dx >= MAP_WIDTH - 1 ||
        enemy->y + dy <= 0 || enemy->y + dy >= MAP_HEIGHT - 1) {
        return false;
    }
    
    // Try horizontal movement first
    if (dx != 0 && map[enemy->y][enemy->x + dx] == FLOOR) {
        map[enemy->y][enemy->x] = FLOOR;
        enemy->x += dx;
        map[enemy->y][enemy->x] = enemy->type;
        return true;
    }
    
    // Try vertical movement
    if (dy != 0 && map[enemy->y + dy][enemy->x] == FLOOR) {
        map[enemy->y][enemy->x] = FLOOR;
        enemy->y += dy;
        map[enemy->y][enemy->x] = enemy->type;
        return true;
    }
    
    if (enemy->type == ENEMY_SNAKE){
            if (enemy->x + dx <= 0 || enemy->x + dx >= MAP_WIDTH - 1 ||
        enemy->y + dy <= 0 || enemy->y + dy >= MAP_HEIGHT - 1) {
        return false;
    }
    
    // Try horizontal movement first
    if (dx != 0 && (map[enemy->y][enemy->x + dx] == FLOOR || map[enemy->y][enemy->x + dx] == DOOR || map[enemy->y][enemy->x + dx] == '#')) {
        if(map[enemy->y][enemy->x + dx] == FLOOR){
        map[enemy->y][enemy->x] = FLOOR;
        enemy->x += dx;
        map[enemy->y][enemy->x] = enemy->type;
        return true;
    }
        else if(map[enemy->y][enemy->x + dx] == DOOR){
        map[enemy->y][enemy->x] = DOOR;
        enemy->x += dx;
        map[enemy->y][enemy->x] = enemy->type;
        return true;
    }
        else if(map[enemy->y][enemy->x + dx] == '#'){
        map[enemy->y][enemy->x] = '#';
        enemy->x += dx;
        map[enemy->y][enemy->x] = enemy->type;
        return true;
    }
    }
    // Try vertical movement
    if (dy != 0 && map[enemy->y + dy][enemy->x] == FLOOR) {
        map[enemy->y][enemy->x] = FLOOR;
        enemy->y += dy;
        map[enemy->y][enemy->x] = enemy->type;
        return true;
    }
    }

    return false;
}
// Update enemy tracking status
void update_enemy_tracking(Enemy* enemy, int player_x, int player_y) {
    if (!enemy->is_active) return;
    
    // Check if player is in same room
    bool same_room = false;
    for (int i = 0; i < current_level.room_count; i++) {
        Room room = current_level.rooms[i];
        if (player_x >= room.x && player_x <= room.x + room.width &&
            player_y >= room.y && player_y <= room.y + room.height &&
            enemy->x >= room.x && enemy->x <= room.x + room.width &&
            enemy->y >= room.y && enemy->y <= room.y + room.height) {
            same_room = true;
            break;
        }
    }
    
    if (same_room) {
        enemy->is_tracking = true;
        if (enemy->type == ENEMY_SNAKE) {
            // Snake always tracks
            enemy->follow_count = INT_MAX;
        } else if ((enemy->type == ENEMY_GIANT || enemy->type == ENEMY_UNDEAD) && 
                   enemy->follow_count <= 0) {
            enemy->follow_count = 5;
        }
    }
}

void clrtoline() {
    int y, x;
    getyx(stdscr, y, x);
    move(y, 1);
    clrtoeol();
}

void drop_item() {
    char input;
    int item;
    bool valid_input = false;
    
    // Debug information at start
    mvprintw(38, 1, "Player position: x=%d, y=%d", player.x, player.y);
    mvprintw(39, 1, "Current tile at player position: '%c'", map[player.y][player.x]);
    mvprintw(40, 1, "Inventory count: %d", player_inventory.count);
    
    if (player_inventory.count == 0) {
        mvprintw(41, 1, "No items to drop!");
        refresh();
        return;
    }
    
    mvprintw(42, 1, "Which item do you wish to drop? (1-%d, or ESC to cancel): ", player_inventory.count);
    refresh();
    
    while (!valid_input) {
        input = getch();
        
        if (input == 27) { // ESC key
            mvprintw(43, 1, "Drop cancelled.");
            refresh();
            return;
        }
        
        if (input >= '1' && input <= '9') {
            item = input - '1'; // Convert to 0-based index
            
            // Debug information for selected item
            mvprintw(43, 1, "Selected item index: %d", item);
            refresh();
            
            if (item < player_inventory.count) {
                valid_input = true;
                
                // More debug info
                mvprintw(44, 1, "Selected item: %s (symbol: %c)", 
                    player_inventory.items[item].name,
                    player_inventory.items[item].symbol);
                refresh();
                
                // Check if the floor tile is empty
                if (map[player.y][player.x] != FLOOR) {
                    mvprintw(45, 1, "Cannot drop item here! Current tile: '%c'", map[player.y][player.x]);
                    refresh();
                    return;
                }
                
                // Drop the item
                map[player.y][player.x] = player_inventory.items[item].symbol;
                mvprintw(45, 1, "Dropped item %s!", player_inventory.items[item].name);
                
                // Remove item from inventory
                for (int i = item; i < player_inventory.count - 1; i++) {
                    player_inventory.items[i] = player_inventory.items[i + 1];
                }
                player_inventory.count--;
                
                refresh();
                return;
            }
        }
        
        mvprintw(45, 1, "Invalid input. Please enter a number between 1 and %d: ", player_inventory.count);
        refresh();
    }
}

void use_item(int index) {
    if (index >= player_inventory.count) return;
    
    Item* item = &player_inventory.items[index];
    
    if (item->isWeapon) {
        // Unequip currently equipped weapon
        for (int i = 0; i < player_inventory.count; i++) {
            if (player_inventory.items[i].isWeapon) {
                player_inventory.items[i].isEquipped = false;
            }
        }
        // Equip the selected weapon
        item->isEquipped = true;
        mvprintw(40, 1, "Equipped %s!", item->name);
    } 
    else if (item->isPotion) {
        use_potion(item);
        mvprintw(40, 1, "Used %s!", item->name);
    }
    else if (item->symbol == FOOD) {
        if (player_health < MAX_HEALTH) {
            int heal_amount = item->heal_value;
            if (player_health + heal_amount > MAX_HEALTH) {
                heal_amount = MAX_HEALTH - player_health;
            }
            player_health += heal_amount;
            mvprintw(40, 1, "Used Food Ration: Restored %d health!", heal_amount);
            
            // Remove food item from inventory
            for (int i = index; i < player_inventory.count - 1; i++) {
                player_inventory.items[i] = player_inventory.items[i + 1];
            }
            player_inventory.count--;
        } else {
            mvprintw(40, 1, "Already at full health!");
        }
    }
}

void place_weapons(Room* rooms, int room_count) {
    const float WEAPON_CHANCE = 0.35f;  // 35% chance for a room to have a weapon
    
    for (int r = 0; r < room_count; r++) {
        if ((rand() % 100) > (WEAPON_CHANCE * 100)) {
            continue;
        }

        Room room = rooms[r];
        int attempts = 0;
        const int MAX_ATTEMPTS = 20;
        
        while (attempts < MAX_ATTEMPTS) {
            int x = room.x + 2 + (rand() % (room.width - 3));
            int y = room.y + 2 + (rand() % (room.height - 3));
            
            if (map[y][x] == FLOOR) {
                bool is_suitable = true;
                // Check surrounding area for other items
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        char nearby_tile = map[y + dy][x + dx];
                        if (nearby_tile == DOOR || 
                            nearby_tile == STAIR || 
                            nearby_tile == GOLD_SMALL ||
                            nearby_tile == PILLAR ||
                            nearby_tile == FOOD ||
                            nearby_tile == SWORD_SYMBOL ||
                            nearby_tile == DAGGER_SYMBOL ||
                            nearby_tile == MAGIC_WAND_SYMBOL ||
                            nearby_tile == ARROW_SYMBOL) {
                            is_suitable = false;
                            break;
                        }
                    }
                    if (!is_suitable) break;
                }
                
                if (is_suitable) {
                    // Decide which weapon to place
                    int weapon_type = rand() % 100;
                    
                    // If sword hasn't been placed yet, give it a chance to be placed
                    if (!sword_placed && weapon_type < 25) {
                        map[y][x] = SWORD_SYMBOL;
                        sword_placed = true;
                    }
                    // Otherwise place other weapons
                    else {
                        if (weapon_type < 40) {
                            map[y][x] = DAGGER_SYMBOL;
                        }
                        else if (weapon_type < 60) {
                            map[y][x] = MAGIC_WAND_SYMBOL;
                        }
                        else {
                            map[y][x] = ARROW_SYMBOL;
                        }
                    }
                    break;
                }
            }
            attempts++;
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
    
    // Set the furthest room as treasure room (type 1)
    furthest_room.room_type = 1;
    
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

void place_golden_key(Room* rooms, int room_count) {
    bool key_placed = false;
    
    // Try to place the key in a random room
    while (!key_placed) {
        int room_index = rand() % room_count;
        Room room = rooms[room_index];
        
        // Try to place the key in this room
        int attempts = 0;
        while (attempts < 20 && !key_placed) {
            int x = room.x + 2 + (rand() % (room.width - 3));
            int y = room.y + 2 + (rand() % (room.height - 3));
            
            if (map[y][x] == FLOOR) {
                bool is_suitable = true;
                // Check surrounding area for other items
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        char nearby_tile = map[y + dy][x + dx];
                        if (nearby_tile != FLOOR && nearby_tile != '#') {
                            is_suitable = false;
                            break;
                        }
                    }
                    if (!is_suitable) break;
                }
                
                if (is_suitable) {
                    map[y][x] = GOLDEN_KEY;
                    key_placed = true;
                    break;
                }
            }
            attempts++;
        }
    }
}

bool try_use_golden_key(void) {
    // Check if key breaks
    if (rand() % 100 < KEY_BREAK_CHANCE) {
        // Convert golden key to broken key in inventory
        for (int i = 0; i < player_inventory.count; i++) {
            if (player_inventory.items[i].symbol == GOLDEN_KEY && !player_inventory.items[i].isBroken) {
                player_inventory.items[i].symbol = BROKEN_KEY;
                player_inventory.items[i].name = "Broken Golden Key";
                player_inventory.items[i].isBroken = true;
                mvprintw(40, 1, "The Golden Key broke while using it!");
                refresh();
                return true;
            }
        }
    }
    return true;
}

void combine_broken_keys(void) {
    int broken_key_count = 0;
    int first_key_index = -1;
    
    // Count broken keys and find first one
    for (int i = 0; i < player_inventory.count; i++) {
        if (player_inventory.items[i].symbol == BROKEN_KEY && player_inventory.items[i].isBroken) {
            broken_key_count++;
            if (first_key_index == -1) {
                first_key_index = i;
            } else {
                // Found second key, combine them
                // Remove second key
                for (int j = i; j < player_inventory.count - 1; j++) {
                    player_inventory.items[j] = player_inventory.items[j + 1];
                }
                player_inventory.count--;
                
                // Convert first key back to golden key
                player_inventory.items[first_key_index].symbol = GOLDEN_KEY;
                player_inventory.items[first_key_index].name = "Golden Key";
                player_inventory.items[first_key_index].isBroken = false;
                
                mvprintw(40, 1, "Combined 2 broken keys into a new Golden Key!");
                refresh();
                return;
            }
        }
    }
    
    if (broken_key_count < 2) {
        mvprintw(40, 1, "Need 2 broken keys to combine!");
        refresh();
    }
}

void vision(void) {
    // Save any messages from lines 37-42 before clearing
    char messages[6][MAP_WIDTH];
    for(int i = 37; i <= 42; i++) {
        mvinnstr(i, 0, messages[i-37], MAP_WIDTH-1);
    }
    
    clear();
    
    // Define vision radius
    const int VISION_RADIUS = 5;
    
    // Mark current visible tiles as visited
    for(int dy = -VISION_RADIUS; dy <= VISION_RADIUS; dy++) {
        for(int dx = -VISION_RADIUS; dx <= VISION_RADIUS; dx++) {
            int new_y = player.y + dy;
            int new_x = player.x + dx;
            
            if(new_x >= 0 && new_x < MAP_WIDTH && new_y >= 0 && new_y < MAP_HEIGHT) {
                visited_tiles[new_y][new_x] = true;
            }
        }
    }
    
    // First draw all visited areas (rooms and corridors)
    for(int y = 0; y < MAP_HEIGHT; y++) {
        for(int x = 0; x < MAP_WIDTH; x++) {
            if(visited_tiles[y][x]) {
                // Check if this position is in a room
                bool in_room = false;
                int room_type = 0;
                
                for(int i = 0; i < current_level.room_count; i++) {
                    Room* room = &current_level.rooms[i];
                    if(x >= room->x && x <= room->x + room->width &&
                       y >= room->y && y <= room->y + room->height) {
                        in_room = true;
                        room_type = room->room_type;
                        break;
                    }
                }
                
                // Set appropriate color
                int color_pair;
                if(in_room) {
                    switch(room_type) {
                        case 1: color_pair = 5; break; // Treasure room
                        case 2: color_pair = 7; break; // Enchant room
                        default: color_pair = 6; break; // Regular room
                    }
                } else {
                    color_pair = 6; // Use regular room color for corridors
                }
                
                attron(COLOR_PAIR(color_pair));
                mvprintw(y, x, "%c", map[y][x]);
                attroff(COLOR_PAIR(color_pair));
            }
        }
    }
    
    // Draw current vision area (this will overlap visited areas)
    int start_y = player.y - VISION_RADIUS;
    int end_y = player.y + VISION_RADIUS;
    int start_x = player.x - VISION_RADIUS;
    int end_x = player.x + VISION_RADIUS;
    
    // Ensure boundaries are within map limits
    start_y = (start_y < 0) ? 0 : start_y;
    end_y = (end_y >= MAP_HEIGHT) ? MAP_HEIGHT - 1 : end_y;
    start_x = (start_x < 0) ? 0 : start_x;
    end_x = (end_x >= MAP_WIDTH) ? MAP_WIDTH - 1 : end_x;
    
    // Draw current vision area with brighter colors
    for(int y = start_y; y <= end_y; y++) {
        for(int x = start_x; x <= end_x; x++) {
            if (map[y][x] >= ENEMY_DEMON && map[y][x] <= ENEMY_UNDEAD) {
                switch(map[y][x]) {
                    case ENEMY_DEMON:  attron(COLOR_PAIR(10)); break;
                    case ENEMY_FIRE:   attron(COLOR_PAIR(11)); break;
                    case ENEMY_GIANT:  attron(COLOR_PAIR(12)); break;
                    case ENEMY_SNAKE:  attron(COLOR_PAIR(13)); break;
                    case ENEMY_UNDEAD: attron(COLOR_PAIR(14)); break;
                }
                mvprintw(y, x, "%c", map[y][x]);
                attroff(COLOR_PAIR(10) | COLOR_PAIR(11) | COLOR_PAIR(12) | 
                       COLOR_PAIR(13) | COLOR_PAIR(14));
            } else {
                // Check if in room for coloring
                bool in_room = false;
                int room_type = 0;
                for (int i = 0; i < current_level.room_count; i++) {
                    if (x >= current_level.rooms[i].x && 
                        x <= current_level.rooms[i].x + current_level.rooms[i].width &&
                        y >= current_level.rooms[i].y && 
                        y <= current_level.rooms[i].y + current_level.rooms[i].height) {
                        in_room = true;
                        room_type = current_level.rooms[i].room_type;
                        break;
                    }
                }
                
                int color_pair;
                if(in_room) {
                    switch(room_type) {
                        case 1: color_pair = 5; break;
                        case 2: color_pair = 7; break;
                        default: color_pair = 6; break;
                    }
                } else {
                    color_pair = 6; // Use regular color for corridors
                }
                
                attron(COLOR_PAIR(color_pair) | A_BOLD); // Add A_BOLD for current vision
                mvprintw(y, x, "%c", map[y][x]);
                attroff(COLOR_PAIR(color_pair) | A_BOLD);
            }
        }
    }
    
    // Draw player
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(player.y, player.x, "@");
    attroff(COLOR_PAIR(2) | A_BOLD);
    
    // Draw borders
    draw_borders();
    
    // Restore messages
    for(int i = 37; i <= 42; i++) {
        mvprintw(i, 0, "%s", messages[i-37]);
    }
    
    refresh();
}

void generate_rooms(StairInfo* prev_stair, location* player_pos) {
    current_level.room_count = 0;
    current_enemies.count = 0;
    i = 0;
    int attempts = 0;
    const int MAX_ATTEMPTS = 1000;
    const int MIN_ROOMS = 6;
    static int current_level_number = 1;  // Add this to track the level number
    memset(visited_tiles, 0, sizeof(visited_tiles));
    // If we have previous stair info, create the first room in exactly the same position
    if (prev_stair != NULL) {
        Room first_room = prev_stair->room;
        // Keep the exact same position
        first_room.x = prev_stair->original_x;
        first_room.y = prev_stair->original_y;
        first_room.width = prev_stair->room.width;
        first_room.height = prev_stair->room.height;
        first_room.visited = false;
        current_level.rooms[current_level.room_count] = first_room;
        draw_room(first_room);
        place_pillars(first_room);
        first_room.room_type = 3; // Always start with regular room
        current_level.room_count++;
        
        // Set the spawn point to exactly where the stairs were
        doors[0].x = prev_stair->stair_pos.x;
        doors[0].y = prev_stair->stair_pos.y;
        
        current_level_number++; // Increment level number when going to next level
    }

    // Generate remaining rooms
    while (current_level.room_count < MIN_ROOMS && attempts < MAX_ATTEMPTS) {
        Room new_room;
        new_room.width = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));
        new_room.height = MIN_ROOM_SIZE + (rand() % (MAX_ROOM_SIZE - MIN_ROOM_SIZE));
        new_room.visited = false;
        new_room.x = 3 + (rand() % (MAP_WIDTH - new_room.width - 6));
        new_room.y = 3 + (rand() % (MAP_HEIGHT - new_room.height - 6));

        if (!check_room_overlap(new_room, current_level.rooms, current_level.room_count)) {
            int num = rand() % 100;
            if (num < 50)
            new_room.room_type = 3; // Set as regular room initially   

            else if(num < 80)
            new_room.room_type = 1;

            else
            new_room.room_type = 2;

            current_level.rooms[current_level.room_count] = new_room;
            draw_room(new_room);
            place_pillars(new_room);
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
                new_room.room_type = 3; // Set as regular room initially
                current_level.rooms[current_level.room_count] = new_room;
                draw_room(new_room);
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
        place_golden_key(current_level.rooms, current_level.room_count);
    create_corridors(current_level.rooms, current_level.room_count);
    place_doors();
    place_locked_door_and_password(current_level.rooms, current_level.room_count);
    place_gold(current_level.rooms, current_level.room_count);
    place_food(current_level.rooms, current_level.room_count);
    place_weapons(current_level.rooms, current_level.room_count);
    for (int i = 0; i < current_level.room_count; i++) {
    place_enemies_in_room(current_level.rooms[i]);
    place_potions(current_level.rooms, current_level.room_count);
}
    // Find the furthest room and set it as treasure room if on level 5
    Room furthest_room;
    if (prev_stair == NULL) {
        // First level - find furthest room from start
        furthest_room = find_furthest_room(current_level.rooms, current_level.room_count, 
                                       (location){doors[0].x, doors[0].y});
    } else {
        // Subsequent levels - find furthest room from player
        furthest_room = find_furthest_room(current_level.rooms, current_level.room_count, 
                                       (location){player_pos->x, player_pos->y});
    }

    // Update the room type in the current_level.rooms array
    for (int i = 0; i < current_level.room_count; i++) {
        if (current_level.rooms[i].x == furthest_room.x && 
            current_level.rooms[i].y == furthest_room.y) {
            // Only make it a treasure room if it's level 5
            if (current_level_number == 5) {
                current_level.rooms[i].room_type = 1; // Set as treasure room
            }
            location stair_pos = place_staircase(current_level.rooms[i]); // Place staircase in furthest room
            break;
        }
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
                
                // Check if the current character is an enemy and set its color
                if (map[y][x] == ENEMY_DEMON) {
                    attron(COLOR_PAIR(10));
                }
                else if (map[y][x] == ENEMY_FIRE) {
                    attron(COLOR_PAIR(11));
                }
                else if (map[y][x] == ENEMY_GIANT) {
                    attron(COLOR_PAIR(12));
                }
                else if (map[y][x] == ENEMY_SNAKE) {
                    attron(COLOR_PAIR(13));
                }
                else if (map[y][x] == ENEMY_UNDEAD) {
                    attron(COLOR_PAIR(14));
                }
                else {
                    attron(COLOR_PAIR(color_pair));
                }
                
                // Draw the character
                mvprintw(y, x, "%c", map[y][x]);
                
                // Turn off enemy colors if it was an enemy
                if (map[y][x] >= ENEMY_DEMON && map[y][x] <= ENEMY_UNDEAD) {
                    attroff(COLOR_PAIR(10) | COLOR_PAIR(11) | COLOR_PAIR(12) | 
                           COLOR_PAIR(13) | COLOR_PAIR(14));
                }
                else {
                    attroff(COLOR_PAIR(color_pair));
                }
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

void attack_with_weapon(location player, char weapon_type, int direction_x, int direction_y) {
    // Find the equipped weapon in inventory
    Item* equipped_weapon = NULL;
    for(int i = 0; i < player_inventory.count; i++) {
        if(player_inventory.items[i].isEquipped && player_inventory.items[i].isWeapon) {
            equipped_weapon = &player_inventory.items[i];
            break;
        }
    }

    if (!equipped_weapon) return;

    switch(weapon_type) {
        case 'M': // Mace
        case 'I': // Sword
            // Attack all 8 adjacent tiles
            for(int dy = -1; dy <= 1; dy++) {
                for(int dx = -1; dx <= 1; dx++) {
                    if(dx == 0 && dy == 0) continue; // Skip player's position
                    
                    int target_x = player.x + dx;
                    int target_y = player.y + dy;
                    
                    // Calculate damage with potion effect
                    int damage = equipped_weapon->damage;
                    if (damage_active) {
                        damage *= 2;  // Double damage when potion is active
                    }
                    
                    // Check if target location contains an enemy
                    if (map[target_y][target_x] == ENEMY_DEMON ||
                        map[target_y][target_x] == ENEMY_FIRE ||
                        map[target_y][target_x] == ENEMY_GIANT ||
                        map[target_y][target_x] == ENEMY_SNAKE ||
                        map[target_y][target_x] == ENEMY_UNDEAD) {
                        damage_enemy(target_x, target_y, damage);
                    }
                }
            }
            break;
            
        case 'D': // Dagger
            {
                int damage = equipped_weapon->damage;
                if (damage_active) {
                    damage *= 2;
                }
                throw_weapon(player, direction_x, direction_y, 5, damage, true);
            }
            break;
            
        case '%': // Magic Wand
            {
                int damage = equipped_weapon->damage;
                if (damage_active) {
                    damage *= 2;
                }
                throw_weapon(player, direction_x, direction_y, 10, damage, false);
            }
            break;
            
        case 'V': // Arrow
            {
                int damage = equipped_weapon->damage;
                if (damage_active) {
                    damage *= 2;
                }
                throw_weapon(player, direction_x, direction_y, 5, damage, false);
            }
            break;
    }
}

void handle_attack(int key) {
    // Get currently equipped weapon
    Item* equipped_weapon = NULL;
    for(int i = 0; i < player_inventory.count; i++) {
        if(player_inventory.items[i].isEquipped && player_inventory.items[i].isWeapon) {
            equipped_weapon = &player_inventory.items[i];
            break;
        }
    }
    
    if(equipped_weapon == NULL) {
        mvprintw(40, 1, "No weapon equipped!");
        return;
    }

    int dx = 0, dy = 0;
    switch(key) {
        case KEY_UP: dy = -1; break;
        case KEY_DOWN: dy = 1; break;
        case KEY_LEFT: dx = -1; break;
        case KEY_RIGHT: dx = 1; break;
    }

    // If weapon can be thrown and player pressed 't'
    if(equipped_weapon->can_throw) {
        mvprintw(40, 1, "Throwing %s...", equipped_weapon->name);
        refresh();
        
        throw_weapon((location){player.x, player.y}, dx, dy, 
                    equipped_weapon->throw_range, 
                    equipped_weapon->damage, 
                    equipped_weapon->symbol == DAGGER_SYMBOL);
        
        // Reduce count for throwable weapons
        equipped_weapon->count--;
        if(equipped_weapon->count <= 0) {
            // Remove weapon if all are used
            remove_item_from_inventory(equipped_weapon);
        }
        
        // Clear the throwing message
        mvprintw(40, 1, "                                        ");
    } else {
        // Melee attack (for sword and mace)
        attack_with_weapon((location){player.x, player.y}, 
                         equipped_weapon->symbol, dx, dy);
    }
    refresh();
}

void remove_item_from_inventory(Item* item) {
    int index = item - player_inventory.items;
    for(int i = index; i < player_inventory.count - 1; i++) {
        player_inventory.items[i] = player_inventory.items[i + 1];
    }
    player_inventory.count--;
}

void throw_weapon(location start, int dx, int dy, int max_range, int damage, bool drops_after_hit) {
    int current_x = start.x;
    int current_y = start.y;
    int distance = 0;
    bool hit_enemy = false;
    
    // Store the original map state
    char original_map[MAP_HEIGHT][MAP_WIDTH];
    memcpy(original_map, map, sizeof(map));
    
    while (distance < max_range && !hit_enemy) {
        int next_x = current_x + dx;
        int next_y = current_y + dy;
        distance++;
        
        // Check if next position would be within map boundaries
        if (next_x <= 1 || next_x >= MAP_WIDTH - 2 || 
            next_y <= 1 || next_y >= MAP_HEIGHT - 2) {
            break;
        }
        
        current_x = next_x;
        current_y = next_y;
        
        // Check for enemy hit
        if (map[current_y][current_x] == ENEMY_DEMON ||
            map[current_y][current_x] == ENEMY_FIRE ||
            map[current_y][current_x] == ENEMY_GIANT ||
            map[current_y][current_x] == ENEMY_SNAKE ||
            map[current_y][current_x] == ENEMY_UNDEAD) {
            
            damage_enemy(current_x, current_y, damage);
            hit_enemy = true;
            break;
        }
        
        // Check for walls or obstacles
        if (map[current_y][current_x] == VERTICAL || 
            map[current_y][current_x] == HORIZ || 
            map[current_y][current_x] == PILLAR ||
            map[current_y][current_x] == DOOR) {
            break;
        }
        
        // Animation
        memcpy(map, original_map, sizeof(map));
        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(current_y, current_x, "*");
        attroff(COLOR_PAIR(2) | A_BOLD);
        refresh();
        napms(50);
    }
    
    // Restore original map state
    memcpy(map, original_map, sizeof(map));
    
    // Drop the weapon if it didn't hit anything and is supposed to drop
    if (!hit_enemy && drops_after_hit) {
        if (map[current_y][current_x] == FLOOR || map[current_y][current_x] == '#') {
            map[current_y][current_x] = DAGGER_SYMBOL;
        }
    }
    
    // Redraw everything
    //draw_map();
    vision();
    draw_borders();
    
    // Ensure player is still visible
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(player.y, player.x, "@");
    attroff(COLOR_PAIR(2) | A_BOLD);
    refresh();
}

void damage_enemy(int x, int y, int damage) {
    for (int i = 0; i < current_enemies.count; i++) {
        Enemy* enemy = &current_enemies.enemies[i];
        if (enemy->is_active && enemy->x == x && enemy->y == y) {
            // Apply damage multiplier if active
            int final_damage = damage;
            if (damage_active) {
                final_damage *= 2;
                mvprintw(38, 1, "Damage boost active! (%dx damage)", 2);
            }
            
            enemy->health -= final_damage;
            
            // Display damage message
            attron(COLOR_PAIR(4) | A_BOLD);
            mvprintw(39, 1, "You hit %c for %d damage! (%d/%d HP)", 
                    enemy->type, final_damage, enemy->health, enemy->max_health);
            attroff(COLOR_PAIR(4) | A_BOLD);
            
            if (enemy->health <= 0) {
                enemy->is_active = false;
                map[enemy->y][enemy->x] = FLOOR;
                attron(COLOR_PAIR(4) | A_BOLD);
                mvprintw(38, 1, "You defeated the %c!", enemy->type);
                attroff(COLOR_PAIR(4) | A_BOLD);
            }
            refresh();
            break;
        }
    }
}

int main() {
    initscr();
    init_map();
    init_inventory();
    init_starting_weapon();
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
        init_pair(9, COLOR_RED, -1);               // For locked door
        init_pair(5, COLOR_YELLOW, -1);            // For treasure rooms
        init_pair(6, COLOR_WHITE, -1);             // For regular rooms
        init_pair(7, COLOR_MAGENTA, -1);           // For enchant rooms
        init_pair(8, COLOR_GREEN, -1);
        init_pair(10, COLOR_RED, -1);     // For Demon
        init_pair(11, COLOR_YELLOW, -1);  // For Fire Monster
        init_pair(12, COLOR_GREEN, -1);   // For Giant
        init_pair(13, COLOR_BLUE, -1);    // For Snake
        init_pair(14, COLOR_MAGENTA, -1); // For Undead
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
    
    //location player = {0, 0}; // Initialize player position
    player_health = MAX_HEALTH;
    current_enemies.count = 0;
    memset(current_enemies.enemies, 0, sizeof(current_enemies.enemies));
    generate_rooms(prev_stair, &player);

    draw_borders();
    vision();
    //draw_map();
    player.x = doors[0].x;  // Set initial player position to first door
    player.y = doors[0].y;
    attron(COLOR_PAIR(2));
    mvprintw(player.y, player.x, "@");
    attroff(COLOR_PAIR(2));

    int score = 0;
    int level = 1;  // Track level number

    // Display initial status
    mvprintw(42, 1, "Date/Time (UTC): %s", datetime);
    update_status_line(score, level, player_health, player, game_start_time);
    refresh();
    password_shown = false;
    door_unlocked = false;
    generate_password();
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
                    mvprintw(42, 40, "player position x=%d, y=%d", player.x, player.y);
                    refresh();
                    int pre_x = player.x;
                    int pre_y = player.y; 
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
                         map[new_y][new_x] == STAIR || map[new_y][new_x] == TRAP_HIDDEN ||map[new_y][new_x] == FOOD||
                         map[new_y][new_x] == SWORD_SYMBOL ||    // Add these lines
                         map[new_y][new_x] == DAGGER_SYMBOL ||
                         map[new_y][new_x] == MAGIC_WAND_SYMBOL ||
                         map[new_y][new_x] == ARROW_SYMBOL ||
                         map[new_y][new_x] == PASSWORD_SYMBOL ||
                         map[new_y][new_x] == LOCKED_DOOR ||
                         map[new_y][new_x] == GOLDEN_KEY||
                         map[new_y][new_x] == BROKEN_KEY ||
                         map[new_y][new_x] == ENEMY_DEMON ||
                         map[new_y][new_x] == ENEMY_FIRE ||
                         map[new_y][new_x] == ENEMY_GIANT ||
                         map[new_y][new_x] == ENEMY_SNAKE ||
                         map[new_y][new_x] == ENEMY_UNDEAD ||
                         map[new_y][new_x] == POTION_HEALTH || 
                         map[new_y][new_x] == POTION_SPEED ||
                         map[new_y][new_x] == POTION_DAMAGE
                         )) {
                        
                        // Clear old position
                        mvprintw(player.y, player.x, " ");
                        
                       if (map[new_y][new_x] == POTION_HEALTH ||
                       map[new_y][new_x] == POTION_SPEED ||
                       map[new_y][new_x] == POTION_DAMAGE) {
                        pickup_potion(map[new_y][new_x], new_x, new_y);
                }   

                

                        // Update player position
                        player.x = new_x;
                        player.y = new_y;
                        
                        // Inside your movement case block, after checking for valid move:
                        if (new_x == password_location.x && new_y == password_location.y && !password_shown) {
                        password_shown = true;
                        password_show_time = time(NULL);
                        attron(COLOR_PAIR(2) | A_BOLD);
                        mvprintw(38, 2, "Password: %s", current_password);
                        attroff(COLOR_PAIR(2) | A_BOLD);
                        }                    
if (new_x == locked_door_location.x && new_y == locked_door_location.y && !door_unlocked) {
    // Check for golden key first
    bool has_key = false;
    for (int i = 0; i < player_inventory.count; i++) {
        if (player_inventory.items[i].symbol == GOLDEN_KEY && !player_inventory.items[i].isBroken) {
            has_key = true;
            if (try_use_golden_key()) {
                door_unlocked = true;
                player_inventory.items[i].isBroken = true;
                map[locked_door_location.y][locked_door_location.x] = DOOR;
                mvprintw(40, 1, "Unlocked door with Golden Key!");
                refresh();
                break;
            }
        }
    }
    if(!has_key){
    // Clear any existing messages
    move(38, 1);
    clrtoeol();
    move(40, 1);
    clrtoeol();
    
    char input_password[5];
    echo(); // Enable input echo
    mvprintw(38, 1, "Enter password: ");
    refresh();
    getnstr(input_password, 4);
    noecho(); // Disable input echo
    input_password[4] = '\0'; // Ensure null termination

    if (strcmp(input_password, current_password) == 0) {
        move(38, 1);
        clrtoeol();
        mvprintw(40, 1, "Door unlocked!");
        refresh();
        door_unlocked = true;
        map[locked_door_location.y][locked_door_location.x] = DOOR;
        // Allow movement to new position
        player.x = new_x;
        player.y = new_y;
    } else {
        move(38, 1);
        clrtoeol();
        mvprintw(40, 1, "Wrong password!");
        player.x = pre_x;
        player.y = pre_y;
        refresh();
        // Do not update player position at all - they stay at their current position
        new_x = player.x;  // Reset the new position to current position
        new_y = player.y;
        
        // Redraw player at current position
        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(player.y, player.x, "@");
        attroff(COLOR_PAIR(2) | A_BOLD);
    }
    
    // Wait a moment to show the message
    refresh();
    napms(1000); // Wait 1 second
    
    // Clear the messages
    move(38, 1);
    clrtoeol();
    move(40, 1);
    clrtoeol();
    
    refresh();
}
}

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
                        if (player_inventory.count < MAX_INVENTORY) {
                        map[player.y][player.x] = FLOOR;
                        player_inventory.items[player_inventory.count].symbol = FOOD;
                        player_inventory.items[player_inventory.count].name = "Food Ration";
                        player_inventory.items[player_inventory.count].heal_value = FOOD_HEAL;
                        player_inventory.items[player_inventory.count].isEquipped = false;
                        player_inventory.count++;
                        mvprintw(40, 1, "Picked up Food!");
    } else {
        mvprintw(40, 1, "Inventory full! Cannot pick up food!");
    }
}

else if (map[player.y][player.x] == GOLDEN_KEY) {
    if (player_inventory.count < MAX_INVENTORY) {
        map[player.y][player.x] = FLOOR;
        Item* new_key = &player_inventory.items[player_inventory.count];
        new_key->symbol = GOLDEN_KEY;
        new_key->name = "Golden Key";
        new_key->isWeapon = false;
        new_key->isKey = true;
        new_key->isBroken = false;
        player_inventory.count++;
        mvprintw(40, 1, "Picked up a Golden Key!");
    } else {
        mvprintw(40, 1, "Inventory full! Cannot pick up Golden Key!");
    }
}

else if (map[player.y][player.x] == SWORD_SYMBOL ||
         map[player.y][player.x] == DAGGER_SYMBOL ||
         map[player.y][player.x] == MAGIC_WAND_SYMBOL ||
         map[player.y][player.x] == ARROW_SYMBOL) {
    // Clear any previous messages
    for(int i = 37; i <= 40; i++) {
        move(i, 1);
        clrtoeol();
    }
    pickup_weapon(map[player.y][player.x], player.x, player.y);
    refresh();
}
if (speed_active) {
    speed_potion_moves--;
    if (speed_potion_moves <= 0) {
        speed_active = false;
        mvprintw(40, 1, "Speed potion effect has worn off!");
        refresh();
    } else {
        mvprintw(40, 1, "Speed boost active! (%d moves left) Press arrow key for second move...", 
                 speed_potion_moves);
        refresh();
        int second_move = getch();
        if (second_move == KEY_UP || second_move == KEY_DOWN ||
            second_move == KEY_LEFT || second_move == KEY_RIGHT) {
            int new_x = player.x;
            int new_y = player.y;
            
            switch (second_move) {
                case KEY_UP:    new_y--; break;
                case KEY_DOWN:  new_y++; break;
                case KEY_LEFT:  new_x--; break;
                case KEY_RIGHT: new_x++; break;
            }
            
            // Check if second move is valid
            if (new_y > 0 && new_y < MAP_HEIGHT - 1 && new_x > 0 && new_x < MAP_WIDTH - 1 &&
                map[new_y][new_x] != VERTICAL && map[new_y][new_x] != HORIZ && 
                map[new_y][new_x] != PILLAR &&
                (map[new_y][new_x] == FLOOR || map[new_y][new_x] == DOOR ||
                 map[new_y][new_x] == '#' || map[new_y][new_x] == GOLD_SMALL ||
                 map[new_y][new_x] == GOLD_MEDIUM || map[new_y][new_x] == GOLD_LARGE ||
                 map[new_y][new_x] == STAIR || map[new_y][new_x] == TRAP_HIDDEN ||
                 map[new_y][new_x] == FOOD ||
                 map[new_y][new_x] == SWORD_SYMBOL ||
                 map[new_y][new_x] == DAGGER_SYMBOL ||
                 map[new_y][new_x] == MAGIC_WAND_SYMBOL ||
                 map[new_y][new_x] == ARROW_SYMBOL ||
                 map[new_y][new_x] == PASSWORD_SYMBOL ||
                 map[new_y][new_x] == LOCKED_DOOR ||
                 map[new_y][new_x] == GOLDEN_KEY ||
                 map[new_y][new_x] == BROKEN_KEY ||
                 map[new_y][new_x] == POTION_HEALTH || 
                 map[new_y][new_x] == POTION_SPEED ||
                 map[new_y][new_x] == POTION_DAMAGE)) {
                
                // Clear old position
                mvprintw(player.y, player.x, " ");
                
                // Process the second move (copy the same item pickup and interaction code from the first move)
                // Handle item pickups and interactions
                if (map[new_y][new_x] == GOLD_SMALL) {
                    int gold_value = gold_values[new_y][new_x];
                    map[new_y][new_x] = FLOOR;
                    score += gold_value;
                    mvprintw(40, 1, "You found a coin worth %d gold!", gold_value);
                }
                // Add other item pickup handling here...
                
                // Update player position
                player.x = new_x;
                player.y = new_y;
                
                // Check for traps at new position
                if (trap_locations[player.y][player.x] && !revealed_traps[player.y][player.x]) {
                    player_health -= TRAP_DAMAGE;
                    revealed_traps[player.y][player.x] = 1;
                    map[player.y][player.x] = TRAP_VISIBLE;
                    mvprintw(40, 1, "You stepped on a trap! -%d HP", TRAP_DAMAGE);
                }
            }
        }
        move(40, 1);
        clrtoeol();  // Clear the speed boost message
    }
    }
    if (damage_active) {
    moves_since_potion++;
    if (moves_since_potion >= POTION_DURATION) {
        damage_active = false;
        mvprintw(40, 1, "Damage potion effect has worn off!");
        refresh();
    } else {
        mvprintw(39, 1, "Damage boost active! (%d moves left)", 
                 POTION_DURATION - moves_since_potion);
    }
}
}
break;
}

                case 'f': // Fight/Attack
    {
        mvprintw(40, 1, "Attack direction? (Use arrow keys)");
        refresh();
        int attack_dir = getch();
        if(attack_dir == KEY_UP || attack_dir == KEY_DOWN || 
           attack_dir == KEY_LEFT || attack_dir == KEY_RIGHT) {
            handle_attack(attack_dir);
        }
    }
    break;

case 't': // Throw weapon
    {
        // Find equipped throwable weapon
        bool has_throwable = false;
        for(int i = 0; i < player_inventory.count; i++) {
            if(player_inventory.items[i].isEquipped && 
               player_inventory.items[i].can_throw) {
                has_throwable = true;
                break;
            }
        }
        
        if(has_throwable) {
            mvprintw(40, 1, "Throw direction? (Use arrow keys)");
            refresh();
            int throw_dir = getch();
            if(throw_dir == KEY_UP || throw_dir == KEY_DOWN || 
               throw_dir == KEY_LEFT || throw_dir == KEY_RIGHT) {
                handle_attack(throw_dir);
            }
        } else {
            mvprintw(40, 1, "No throwable weapon equipped!");
        }
    }
    break;

    

                case 'i':  // Open/close inventory
                display_inventory();
                while (1) {
                int ch = getch();
                if (ch == 'i') {
                break;  // Exit inventory
                } else if (ch >= '1' && ch <= '9') {
                use_item(ch - '1');
                display_inventory();  // Refresh inventory display
                refresh();
        }
                else if(ch == 'd'){
                    drop_item();
                    display_inventory();
                    refresh();
                }
                        else if(ch == 'c'){  // Combine broken keys
            combine_broken_keys();
            display_inventory();
            refresh();
        }

    }
    // Redraw the game screen
            clear();
            draw_borders();
            vision();
            //draw_map();
            update_status_line(score, level, player_health, player, game_start_time);
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
            mvprintw(30,temp,"                                               +&&&&x                                               ");
            mvprintw(31,temp,"                                              :&&&&&&:                                              ");
            mvprintw(32,temp,"                                             x&&&&&&&&x                                             ");
            mvprintw(33,temp,"                                         :+$&&&&&&&&&&&&$+:                                         ");
            mvprintw(34,temp,"                                    :&&&&&&&&&&&&&&&&&&&&&&&&&&$                                    ");
            mvprintw(35,temp,"                                 .xx&&&&&&&&&&&&&&&&&&&&&&&&&&&&$x+                                 ");
            mvprintw(36,temp,"                                X&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&:                               ");
            mvprintw(37,temp,"                                &&&&&&$++++++++++++++++++++++x&&&&&&+                               ");
            mvprintw(38,temp,"                                &&&&&$ ;                    ;. &&&&&+                               ");
            mvprintw(39,temp,"                                &&&&&$                         &&&&&+                               ");
            mvprintw(40,temp,"                                &&&&&$                         &&&&&+                               ");
            mvprintw(41,temp,"                                &&&&&$                         &&&&&+                               ");
            mvprintw(42,temp,"                                &&&&&&.                       +&&&&&+                               ");
            mvprintw(43,temp,"                                $&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&;                               ");
            mvprintw(44,temp,"                                 X$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$X+                                ");
            attroff(COLOR_PAIR(2) | A_BOLD);
            attron(COLOR_PAIR(8) | A_BOLD);
            mvprintw(38,76, "YOU WON!");
            attroff(COLOR_PAIR(8) | A_BOLD);
            mvprintw(40,74, "YOUR SCORE:%d",score);
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
                vision();
                //draw_map();
                mvprintw(42, 1, "Date/Time (UTC): %s", datetime);
                update_status_line(score, level, player_health, player, game_start_time);
                attron(COLOR_PAIR(2) | A_BOLD);
                mvprintw(player.y, player.x, "@");
                attroff(COLOR_PAIR(2) | A_BOLD);
                refresh();
            }
        }
        }
// In your main game loop, where you update enemies:
for (int i = 0; i < current_enemies.count && i < MAX_ENEMIES; i++) {
    Enemy* enemy = &current_enemies.enemies[i];
    if (!enemy || !enemy->is_active) continue;
    
    // Update tracking status
    update_enemy_tracking(enemy, player.x, player.y);
    
    // Move enemy if tracking
    if (enemy->is_tracking) {
        // Check if enemy is adjacent to player
        if (abs(enemy->x - player.x) <= 1 && abs(enemy->y - player.y) <= 1) {
            // Attack player
            player_health -= enemy->damage;
            attron(COLOR_PAIR(4) | A_BOLD);
            mvprintw(40, 1, "%c attacks you for %d damage!", enemy->type, enemy->damage);
            attroff(COLOR_PAIR(4) | A_BOLD);
        } else {
            // Move enemy
            if (move_enemy(enemy, player.x, player.y)) {
                if (enemy->type != ENEMY_SNAKE) {
                    enemy->follow_count--;
                    if (enemy->follow_count <= 0) {
                        enemy->is_tracking = false;
                    }
                }
            }
        }
    }
    // Add this after your movement switch statement and before enemy updates
if (health_active && potion_moves_left > 0) {
    if (player_health < MAX_HEALTH) {
        int heal_amount = health_regen_rate;
        if (player_health + heal_amount > MAX_HEALTH) {
            heal_amount = MAX_HEALTH - player_health;
        }
        player_health += heal_amount;
        mvprintw(39, 1, "Health regenerated! +%d HP (%d steps left)", 
                 heal_amount, potion_moves_left);
    }
    
    potion_moves_left--;
    if (potion_moves_left <= 0) {
        health_active = false;
        health_regen_rate = 0;
        mvprintw(40, 1, "Health regeneration effect has worn off!");
    }
}
}
        // Update status lines
        update_status_line(score, level, player_health, player, game_start_time);
        draw_borders();
        vision();
        //draw_map();

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

        if (password_shown && time(NULL) - password_show_time >= 5) {
        password_shown = false;
        mvprintw(38, 1, "                                     ");
}

        // Refresh the static info
        mvprintw(42, 1, "Date/Time (UTC): %s", datetime);
        refresh();
    }
    
    endwin();
    return 0;
}
