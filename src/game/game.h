/*
 * game.h - Redneck Rampage game definitions
 * Redneck Rampage PSP Port
 */

#ifndef GAME_H
#define GAME_H

#include "engine.h"
#include "psp_input.h"

/* ============================================================
 * Game constants
 * ============================================================ */
#define GAME_TICRATE     26       /* Build engine standard: 26 Hz */
#define GAME_TARGET_FPS  30       /* PSP rendering target */
#define MAX_EPISODES     2
#define MAX_LEVELS       7
#define MAX_SOUNDS       256

/* Player starting stats */
#define PLAYER_MAX_HEALTH  100
#define PLAYER_MAX_ARMOR   100
#define PLAYER_START_HEALTH 100
#define PLAYER_EYE_HEIGHT  (32 << 8)  /* Z units (height in Build units) */
#define PLAYER_MOVE_SPEED  2048
#define PLAYER_RUN_SPEED   4096
#define PLAYER_TURN_SPEED  32
#define PLAYER_JUMP_VEL    (-768)
#define PLAYER_GRAVITY     48

/* ============================================================
 * Game states
 * ============================================================ */
typedef enum {
    GAMESTATE_TITLE,
    GAMESTATE_MENU,
    GAMESTATE_PLAYING,
    GAMESTATE_PAUSED,
    GAMESTATE_LOADING,
    GAMESTATE_VICTORY,
    GAMESTATE_GAMEOVER,
    GAMESTATE_QUIT
} gamestate_t;

/* ============================================================
 * Weapon IDs (Redneck Rampage weapons)
 * ============================================================ */
typedef enum {
    WPN_CROWBAR = 0,       /* Melee weapon */
    WPN_REVOLVER,          /* Pistol / Revolver */
    WPN_SHOTGUN,           /* Sawed-off shotgun */
    WPN_RIFLE,             /* Hunting rifle */
    WPN_DYNAMITE,          /* Throw dynamite */
    WPN_CROSSBOW,          /* Explosive crossbow */
    WPN_POWDERKEG,         /* Powder keg */
    WPN_TITGUN,            /* Alien weapon */
    WPN_BOWLINGBALL,       /* Rolling projectile */
    WPN_MOONSHINE,         /* Health item used as weapon */
    WPN_COUNT
} weapon_id_t;

/* ============================================================
 * Actor types (Redneck Rampage enemies + objects)
 * ============================================================ */
typedef enum {
    ACTOR_NONE = 0,
    ACTOR_PLAYER,
    ACTOR_BILLYRAY,        /* Standard redneck */
    ACTOR_HOUND,           /* Hound dog */
    ACTOR_VIXEN,           /* Female enemy */
    ACTOR_PIG,             /* Pig / Sherriff */
    ACTOR_ALIEN,           /* Alien enemy */
    ACTOR_TURD,            /* Turd minion */
    ACTOR_HULKGUARD,       /* Big guard */
    ACTOR_MOSQUITO,        /* Flying mosquito */
    ACTOR_EXPLOSION,       /* Explosion effect */
    ACTOR_PROJECTILE,      /* Generic projectile */
    ACTOR_PICKUP_HEALTH,
    ACTOR_PICKUP_ARMOR,
    ACTOR_PICKUP_AMMO,
    ACTOR_PICKUP_WEAPON,
    ACTOR_PICKUP_KEY,
    ACTOR_BREAKABLE,       /* Breakable object */
    ACTOR_COUNT
} actor_type_t;

/* ============================================================
 * Sound IDs
 * ============================================================ */
typedef enum {
    SND_SHOTGUN_FIRE = 0,
    SND_RIFLE_FIRE,
    SND_REVOLVER_FIRE,
    SND_DYNAMITE_THROW,
    SND_EXPLOSION,
    SND_CROWBAR_SWING,
    SND_PLAYER_PAIN,
    SND_PLAYER_DEATH,
    SND_ENEMY_PAIN,
    SND_ENEMY_DEATH,
    SND_DOOR_OPEN,
    SND_DOOR_CLOSE,
    SND_PICKUP_WEAPON,
    SND_PICKUP_HEALTH,
    SND_PICKUP_AMMO,
    SND_SWITCH,
    SND_SPLASH,
    SND_COUNT
} sound_id_t;

/* ============================================================
 * Ammo definitions
 * ============================================================ */
typedef struct {
    int32_t current;
    int32_t max;
} ammo_t;

/* ============================================================
 * Game state structure
 * ============================================================ */
typedef struct {
    gamestate_t state;

    /* Current level */
    int episode;
    int level;
    char map_name[16];

    /* Player position (from engine) */
    int32_t player_x, player_y, player_z;
    int16_t player_ang;
    int16_t player_horiz;        /* Look up/down pitch (100=center) */
    int16_t player_sectnum;

    /* Player stats */
    int32_t health;
    int32_t armor;
    int32_t cur_weapon;
    int32_t weapon_owned[WPN_COUNT];
    ammo_t  ammo[WPN_COUNT];

    /* Player physics */
    int32_t vel_x, vel_y, vel_z;
    int     on_ground;
    int     crouching;
    int     jumping;
    int     running;

    /* Weapon state */
    int32_t weapon_timer;        /* Weapon fire cooldown */
    int32_t weapon_frame;        /* Animation frame */
    int     firing;

    /* Keys */
    int keys[4];                 /* Red, Blue, Yellow, Green */

    /* Game clock */
    int32_t ticcount;
    int32_t frametime_ms;

    /* Score */
    int32_t kills;
    int32_t total_kills;
    int32_t secrets;
    int32_t total_secrets;

    /* Input */
    psp_input_t input;

} game_t;

/* Global game state */
extern game_t g_game;

/* ============================================================
 * Game API
 * ============================================================ */

/* Initialize game systems */
int  game_init(const char *grp_path);

/* Shut down game */
void game_shutdown(void);

/* Run main game loop (doesn't return until quit) */
void game_run(void);

/* Load a level */
int  game_load_level(int episode, int level);

/* Process one game tick */
void game_tick(void);

/* Render one frame */
void game_render(void);

#endif /* GAME_H */
