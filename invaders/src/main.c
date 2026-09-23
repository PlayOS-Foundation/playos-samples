/*
 * invaders — a complete single-screen arcade shooter for PlayOS.
 *
 * A reference "full game": it uses only the public libplayos API (input,
 * lifecycle, storage, logging, system) plus raylib for rendering, keeps all
 * state in one process-wide struct, allocates nothing at run time, and builds
 * unchanged for the three SDK profiles (device / desktop / emulator).
 *
 * Controls: left stick or D-pad to move, A (South) to fire, Start to restart.
 * On a desktop build the keyboard also works (arrows/A-D, Space, Enter).
 *
 * SPDX-License-Identifier: MIT
 */
#include <raylib.h>
#include <playos/playos.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Virtual playfield (scaled to whatever the display is) ──────────────── */
#define VW 1280.0f
#define VH 720.0f

/* ── Fleet ──────────────────────────────────────────────────────────────── */
#define INV_ROWS 5
#define INV_COLS 8
#define SPRITE    8              /* 8x8 bit sprites */
#define INV_W     40.0f
#define INV_H     36.0f
#define CELL_W    64.0f
#define CELL_H    46.0f
#define FLEET_TOP 96.0f
#define FLEET_SIDE 56.0f
#define FLEET_DROP 26.0f

#define SHOT_MAX   2
#define BOMB_MAX   8
#define SHIELD_N   4
#define SHIELD_COLS 12
#define SHIELD_ROWS 4
#define SHIELD_CELL 8.0f
#define SHIELD_HP   3

#define PLAYER_W     64.0f
#define PLAYER_H     26.0f
#define PLAYER_Y     636.0f
#define PLAYER_SPEED 470.0f

#define SHOT_SPEED 760.0f
#define BOMB_SPEED 250.0f

#define HIGHSCORE_FILE "highscore.txt"

/* ── Types (one mutable state struct per process) ───────────────────────── */

typedef enum { ST_PLAY, ST_OVER } GameState;

typedef struct {
    float x, y, vy;
    int   active;
} Projectile;

typedef struct {
    int          alive[INV_ROWS][INV_COLS];
    float        x, y;      /* top-left of the grid */
    int          dir;       /* +1 right, -1 left */
    float        speed;     /* px/s */
    int          count;
    float        anim;      /* sprite toggle phase */
} Fleet;

typedef struct {
    float         x, y;
    unsigned char hp[SHIELD_COLS][SHIELD_ROWS];
} Shield;

typedef struct {
    GameState    state;
    int          running;
    int          paused;          /* hidden behind the overlay/shell */
    long         frames;
    float        x;               /* player x (top-left) */
    Fleet        fleet;
    Projectile   shot[SHOT_MAX];
    Projectile   bomb[BOMB_MAX];
    Shield       shield[SHIELD_N];
    int          lives, score, highscore, level;
    float        fire_cd, bomb_cd, hit_cd, flash;
    uint32_t     prev_buttons;    /* for edge detection */
    char         saves_path[512];
    char         last_event[32];
} Game;

static Game g;

/* 8x8 invader bitmaps, top/middle/bottom rows — the classic silhouettes. */
static const unsigned char INV_SPRITE[3][SPRITE] = {
    { 0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0xA5 },
    { 0x3C, 0x7E, 0xDB, 0xFF, 0xFF, 0x5A, 0x81, 0x42 },
    { 0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x7E, 0x24, 0x42 },
};
static const int INV_POINTS[3] = { 30, 20, 10 };

/* ── Tiny RNG (no libc rand, no allocation) ─────────────────────────────── */
static uint32_t s_rng = 0x1a2b3c4du;

static uint32_t rng_next(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static float rng_f(void)
{
    return (float)(rng_next() >> 8) / (float)0x01000000u;
}

static int rng_range(int n)
{
    return n > 0 ? (int)(rng_next() % (uint32_t)n) : 0;
}

/* ── Geometry helper ────────────────────────────────────────────────────── */
static int overlaps(float ax, float ay, float aw, float ah,
                    float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

/* ── Storage: high score ────────────────────────────────────────────────── */
static void load_highscore(void)
{
    if (g.saves_path[0] == '\0')
        return;

    char path[600];
    snprintf(path, sizeof(path), "%s/%s", g.saves_path, HIGHSCORE_FILE);

    FILE *f = fopen(path, "r");
    if (!f)
        return;
    int v = 0;
    if (fscanf(f, "%d", &v) == 1 && v > 0)
        g.highscore = v;
    fclose(f);
}

static void save_highscore(void)
{
    if (g.saves_path[0] == '\0')
        return;

    char path[600], text[32];
    snprintf(path, sizeof(path), "%s/%s", g.saves_path, HIGHSCORE_FILE);
    int len = snprintf(text, sizeof(text), "%d\n", g.highscore);

    if (playos_storage_atomic_write(path, text, (size_t)len) != 0)
        PLAYOS_LOG_W("save", "could not write %s", path);
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */
static const char *event_name(PlayOSLifecycleEvent e)
{
    switch (e) {
    case PLAYOS_LIFECYCLE_FOREGROUND: return "FOREGROUND";
    case PLAYOS_LIFECYCLE_BACKGROUND: return "BACKGROUND";
    case PLAYOS_LIFECYCLE_SUSPEND:    return "SUSPEND";
    case PLAYOS_LIFECYCLE_RESUME:     return "RESUME";
    case PLAYOS_LIFECYCLE_TERMINATE:  return "TERMINATE";
    }
    return "UNKNOWN";
}

static void handle_lifecycle(void)
{
    PlayOSLifecycleEvent ev;

    while (playos_lifecycle_poll(&ev) == 1) {
        snprintf(g.last_event, sizeof(g.last_event), "%s", event_name(ev));
        PLAYOS_LOG_I("lifecycle", "event: %s", event_name(ev));
        switch (ev) {
        case PLAYOS_LIFECYCLE_FOREGROUND: g.paused = 0; break;
        case PLAYOS_LIFECYCLE_BACKGROUND: g.paused = 1; break;
        case PLAYOS_LIFECYCLE_SUSPEND:    save_highscore(); g.paused = 1; break;
        case PLAYOS_LIFECYCLE_RESUME:     g.paused = 0; break;
        case PLAYOS_LIFECYCLE_TERMINATE:  save_highscore(); g.running = 0; break;
        }
    }
}

/* ── Input: libplayos controller first, keyboard for desktop/dev ────────── */
typedef struct { int left, right, fire, start; } Input;

static Input read_input(void)
{
    Input in = { 0, 0, 0, 0 };
    PlayOSControllerState cs;

    if (playos_input_controller_connected() &&
        playos_input_get_controller_state(&cs) == 0) {
        playos_button_mask_t b = cs.buttons;
        float ax = cs.axes[PLAYOS_AXIS_LEFT_X];

        in.left   = (ax < -0.35f) || (b & PLAYOS_BUTTON_DPAD_LEFT);
        in.right  = (ax >  0.35f) || (b & PLAYOS_BUTTON_DPAD_RIGHT);
        in.fire   = (b & PLAYOS_BUTTON_SOUTH) && !(g.prev_buttons & PLAYOS_BUTTON_SOUTH);
        in.start  = (b & PLAYOS_BUTTON_START) && !(g.prev_buttons & PLAYOS_BUTTON_START);
        g.prev_buttons = b;
    } else {
        g.prev_buttons = 0;
    }

    /* Harmless on device (no keyboard); the way to play a desktop build. */
    in.left   |= IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
    in.right  |= IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    in.fire   |= IsKeyPressed(KEY_SPACE);
    in.start  |= IsKeyPressed(KEY_ENTER);

    return in;
}

/* ── Game setup ─────────────────────────────────────────────────────────── */
static void fleet_reset(void)
{
    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            g.fleet.alive[r][c] = 1;

    g.fleet.count = INV_ROWS * INV_COLS;
    g.fleet.x = (VW - INV_COLS * CELL_W) * 0.5f;
    g.fleet.y = FLEET_TOP;
    g.fleet.dir = 1;
    g.fleet.speed = 44.0f + (float)(g.level - 1) * 14.0f;
    g.fleet.anim = 0.0f;
}

static void shields_reset(void)
{
    float span = VW - 2.0f * 140.0f;
    float step = span / (float)SHIELD_N;

    for (int i = 0; i < SHIELD_N; i++) {
        g.shield[i].x = 140.0f + (float)i * step + (step - SHIELD_COLS * SHIELD_CELL) * 0.5f;
        g.shield[i].y = 556.0f;
        for (int c = 0; c < SHIELD_COLS; c++)
            for (int r = 0; r < SHIELD_ROWS; r++)
                g.shield[i].hp[c][r] = SHIELD_HP;
    }
}

static void new_game(void)
{
    g.state = ST_PLAY;
    g.level = 1;
    g.lives = 3;
    g.score = 0;
    g.x = (VW - PLAYER_W) * 0.5f;
    g.fire_cd = 0.0f;
    g.bomb_cd = 1.2f;
    g.hit_cd = 0.0f;
    g.flash = 0.0f;
    memset(g.shot, 0, sizeof(g.shot));
    memset(g.bomb, 0, sizeof(g.bomb));
    fleet_reset();
    shields_reset();
}

static void next_level(void)
{
    g.level++;
    memset(g.shot, 0, sizeof(g.shot));
    memset(g.bomb, 0, sizeof(g.bomb));
    fleet_reset();
}

/* ── Firing ─────────────────────────────────────────────────────────────── */
static void player_fire(void)
{
    for (int i = 0; i < SHOT_MAX; i++) {
        if (!g.shot[i].active) {
            g.shot[i].active = 1;
            g.shot[i].x = g.x + PLAYER_W * 0.5f - 2.0f;
            g.shot[i].y = PLAYER_Y - 12.0f;
            g.shot[i].vy = -SHOT_SPEED;
            return;
        }
    }
}

static void invader_fire(void)
{
    for (int i = 0; i < BOMB_MAX; i++) {
        if (g.bomb[i].active)
            continue;

        /* Pick a random column, then the lowest living invader in it. */
        int col = rng_range(INV_COLS);
        for (int r = INV_ROWS - 1; r >= 0; r--) {
            if (!g.fleet.alive[r][col])
                continue;
            g.bomb[i].active = 1;
            g.bomb[i].x = g.fleet.x + (float)col * CELL_W + INV_W * 0.5f - 2.0f;
            g.bomb[i].y = g.fleet.y + (float)r * CELL_H + INV_H;
            g.bomb[i].vy = BOMB_SPEED + (float)g.level * 12.0f;
            return;
        }
        return; /* that column is empty; try again next tick */
    }
}

/* ── Damage ─────────────────────────────────────────────────────────────── */
static int shield_hit(float x, float y, float w, float h)
{
    for (int i = 0; i < SHIELD_N; i++) {
        Shield *s = &g.shield[i];
        for (int c = 0; c < SHIELD_COLS; c++) {
            for (int r = 0; r < SHIELD_ROWS; r++) {
                if (s->hp[c][r] == 0)
                    continue;
                float cx = s->x + (float)c * SHIELD_CELL;
                float cy = s->y + (float)r * SHIELD_CELL;
                if (overlaps(x, y, w, h, cx, cy, SHIELD_CELL, SHIELD_CELL)) {
                    s->hp[c][r]--;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int fleet_bottom(void)
{
    for (int r = INV_ROWS - 1; r >= 0; r--)
        for (int c = 0; c < INV_COLS; c++)
            if (g.fleet.alive[r][c])
                return (int)(g.fleet.y + (float)r * CELL_H + INV_H);
    return 0;
}

static void player_hit(void)
{
    if (g.hit_cd > 0.0f)
        return;
    g.lives--;
    g.hit_cd = 1.6f;
    g.flash = 0.25f;
    memset(g.bomb, 0, sizeof(g.bomb));
    PLAYOS_LOG_I("game", "life lost — %d left", g.lives);
    if (g.lives <= 0) {
        g.state = ST_OVER;
        if (g.score > g.highscore) {
            g.highscore = g.score;
            save_highscore();
        }
        PLAYOS_LOG_I("game", "game over — score %d, high %d", g.score, g.highscore);
    }
}

/* ── Update ─────────────────────────────────────────────────────────────── */
static void update_play(float dt, Input in)
{
    /* Player */
    if (in.left)
        g.x -= PLAYER_SPEED * dt;
    if (in.right)
        g.x += PLAYER_SPEED * dt;
    if (g.x < FLEET_SIDE - 16.0f) g.x = FLEET_SIDE - 16.0f;
    if (g.x > VW - PLAYER_W - FLEET_SIDE + 16.0f) g.x = VW - PLAYER_W - FLEET_SIDE + 16.0f;

    g.fire_cd -= dt;
    if (in.fire && g.fire_cd <= 0.0f) {
        player_fire();
        g.fire_cd = 0.22f;
    }

    /* Fleet marches; speeds up as it thins out. */
    float alive_frac = (float)g.fleet.count / (float)(INV_ROWS * INV_COLS);
    float speed = g.fleet.speed + (1.0f - alive_frac) * 150.0f;
    if (g.fleet.count == 0)
        speed = 0.0f;

    g.fleet.anim += dt * (2.0f + speed / 60.0f);
    g.fleet.x += (float)g.fleet.dir * speed * dt;

    float right = g.fleet.x + (float)INV_COLS * CELL_W;
    if (g.fleet.x < FLEET_SIDE) {
        g.fleet.x = FLEET_SIDE;
        g.fleet.dir = 1;
        g.fleet.y += FLEET_DROP;
    } else if (right > VW - FLEET_SIDE) {
        g.fleet.x = VW - FLEET_SIDE - (float)INV_COLS * CELL_W;
        g.fleet.dir = -1;
        g.fleet.y += FLEET_DROP;
    }

    /* Invader bombs */
    g.bomb_cd -= dt;
    if (g.bomb_cd <= 0.0f && g.fleet.count > 0) {
        invader_fire();
        g.bomb_cd = 0.55f + rng_f() * 1.1f - (float)g.level * 0.04f;
        if (g.bomb_cd < 0.18f)
            g.bomb_cd = 0.18f;
    }

    /* Player shots */
    for (int i = 0; i < SHOT_MAX; i++) {
        Projectile *p = &g.shot[i];
        if (!p->active)
            continue;
        p->y += p->vy * dt;
        if (p->y < -16.0f) { p->active = 0; continue; }

        if (shield_hit(p->x, p->y, 4.0f, 12.0f)) { p->active = 0; continue; }

        for (int r = 0; r < INV_ROWS && p->active; r++) {
            for (int c = 0; c < INV_COLS; c++) {
                if (!g.fleet.alive[r][c])
                    continue;
                float ix = g.fleet.x + (float)c * CELL_W + (CELL_W - INV_W) * 0.5f;
                float iy = g.fleet.y + (float)r * CELL_H + (CELL_H - INV_H) * 0.5f;
                if (overlaps(p->x, p->y, 4.0f, 12.0f, ix, iy, INV_W, INV_H)) {
                    int type = (r == 0) ? 0 : (r <= 2 ? 1 : 2);
                    g.fleet.alive[r][c] = 0;
                    g.fleet.count--;
                    g.score += INV_POINTS[type];
                    p->active = 0;
                    break;
                }
            }
        }
    }
    if (g.fleet.count == 0 && g.state == ST_PLAY)
        next_level();

    /* Bombs */
    for (int i = 0; i < BOMB_MAX; i++) {
        Projectile *b = &g.bomb[i];
        if (!b->active)
            continue;
        b->y += b->vy * dt;
        if (b->y > VH + 16.0f) { b->active = 0; continue; }

        if (shield_hit(b->x, b->y, 4.0f, 12.0f)) { b->active = 0; continue; }
        if (overlaps(b->x, b->y, 4.0f, 12.0f, g.x, PLAYER_Y, PLAYER_W, PLAYER_H)) {
            b->active = 0;
            player_hit();
        }
    }

    if (g.hit_cd > 0.0f)
        g.hit_cd -= dt;
    if (g.flash > 0.0f)
        g.flash -= dt;

    /* Reached the ground line? */
    int bottom = fleet_bottom();
    if (bottom > 0 && (float)bottom >= PLAYER_Y - 6.0f) {
        g.lives = 0;
        player_hit();
    }
}

/* ── Draw ───────────────────────────────────────────────────────────────── */
static uint32_t star_hash(int i)
{
    uint32_t v = (uint32_t)i * 2654435761u + 0x9e3779b9u;
    v ^= v >> 13;
    v *= 2246822519u;
    v ^= v >> 15;
    return v;
}

static void draw_sprite(const unsigned char rows[SPRITE], float x, float y,
                        float w, float h, Color c)
{
    float cw = w / (float)SPRITE, ch = h / (float)SPRITE;
    for (int r = 0; r < SPRITE; r++) {
        for (int b = 0; b < SPRITE; b++) {
            if (rows[r] & (unsigned char)(1u << (SPRITE - 1 - b)))
                DrawRectangle((int)(x + (float)b * cw), (int)(y + (float)r * ch),
                              (int)(cw + 0.5f), (int)(ch + 0.5f), c);
        }
    }
}

static void draw_stars(void)
{
    /* Deterministic starfield — the same field every frame, twinkling. */
    float t = (float)GetTime();
    for (int i = 0; i < 70; i++) {
        uint32_t v = star_hash(i);
        float x = (float)(v % 1280u);
        float y = (float)((v >> 11) % 560u);
        unsigned char a = (unsigned char)(80 + (v >> 23 & 0x7f));
        if (((int)(t * 2.0f) + i) % 7 == 0)
            a = (unsigned char)(a / 2);
        DrawRectangle((int)x, (int)y, 2, 2, (Color){ 180, 200, 255, a });
    }
}

static void draw_world(void)
{
    ClearBackground((Color){ 8, 10, 24, 255 });

    draw_stars();

    /* Shields */
    for (int i = 0; i < SHIELD_N; i++) {
        const Shield *s = &g.shield[i];
        for (int c = 0; c < SHIELD_COLS; c++)
            for (int r = 0; r < SHIELD_ROWS; r++) {
                if (s->hp[c][r] == 0)
                    continue;
                Color col = s->hp[c][r] > 1 ? (Color){ 90, 220, 140, 255 }
                                            : (Color){ 200, 120, 60, 255 };
                DrawRectangle((int)(s->x + (float)c * SHIELD_CELL),
                              (int)(s->y + (float)r * SHIELD_CELL),
                              (int)SHIELD_CELL - 1, (int)SHIELD_CELL - 1, col);
            }
    }

    /* Fleet (two animation frames via the sprite x-flip look) */
    int frame = ((int)g.fleet.anim) & 1;
    for (int r = 0; r < INV_ROWS; r++) {
        int type = (r == 0) ? 0 : (r <= 2 ? 1 : 2);
        Color col = type == 0 ? (Color){ 240, 230, 140, 255 }
                  : type == 1 ? (Color){ 150, 230, 255, 255 }
                              : (Color){ 240, 160, 200, 255 };
        for (int c = 0; c < INV_COLS; c++) {
            if (!g.fleet.alive[r][c])
                continue;
            float x = g.fleet.x + (float)c * CELL_W + (CELL_W - INV_W) * 0.5f;
            float y = g.fleet.y + (float)r * CELL_H + (CELL_H - INV_H) * 0.5f;
            draw_sprite(INV_SPRITE[type], x, y, INV_W, INV_H, col);
            if (frame)
                DrawRectangle((int)x, (int)(y + INV_H), (int)INV_W, 2, col);
        }
    }

    /* Projectiles */
    for (int i = 0; i < SHOT_MAX; i++)
        if (g.shot[i].active)
            DrawRectangle((int)g.shot[i].x, (int)g.shot[i].y, 4, 12, (Color){ 255, 255, 200, 255 });
    for (int i = 0; i < BOMB_MAX; i++)
        if (g.bomb[i].active)
            DrawRectangle((int)g.bomb[i].x, (int)g.bomb[i].y, 4, 12, (Color){ 255, 120, 120, 255 });

    /* Player */
    if (g.hit_cd <= 0.0f || ((int)(g.hit_cd * 12.0f) & 1)) {
        float x = g.x, y = PLAYER_Y;
        DrawRectangle((int)(x + PLAYER_W * 0.5f - 4.0f), (int)y, 8, (int)PLAYER_H, (Color){ 120, 240, 255, 255 });
        DrawRectangle((int)x, (int)(y + PLAYER_H - 10.0f), (int)PLAYER_W, 10, (Color){ 120, 240, 255, 255 });
        DrawRectangle((int)(x + 14.0f), (int)(y + 6.0f), 6, (int)PLAYER_H - 10, (Color){ 120, 240, 255, 255 });
        DrawRectangle((int)(x + PLAYER_W - 20.0f), (int)(y + 6.0f), 6, (int)PLAYER_H - 10, (Color){ 120, 240, 255, 255 });
    }

    /* HUD */
    DrawText(TextFormat("SCORE %d", g.score), 24, 20, 28, RAYWHITE);
    DrawText(TextFormat("HIGH %d", g.highscore), 24, 52, 22, (Color){ 150, 200, 255, 255 });
    DrawText(TextFormat("LEVEL %d", g.level), (int)VW - 160, 20, 28, RAYWHITE);
    for (int i = 0; i < g.lives; i++)
        DrawRectangle((int)VW - 170 + i * 40, 56, 28, 12, (Color){ 120, 240, 255, 255 });

    if (g.state == ST_OVER) {
        DrawRectangle(0, 250, (int)VW, 220, (Color){ 0, 0, 0, 170 });
        const char *t = "GAME OVER";
        DrawText(t, (int)(VW - MeasureText(t, 64)) / 2, 290, 64, (Color){ 255, 120, 120, 255 });
        const char *h = "START / ENTER to play again";
        DrawText(h, (int)(VW - MeasureText(h, 28)) / 2, 390, 28, RAYWHITE);
    } else if (g.paused) {
        const char *t = "PAUSED";
        DrawText(t, (int)(VW - MeasureText(t, 56)) / 2, 320, 56, (Color){ 220, 220, 160, 255 });
    }

    if (g.flash > 0.0f)
        DrawRectangle(0, 0, (int)VW, (int)VH, (Color){ 255, 60, 60, 90 });
}

/* ── Entry point ────────────────────────────────────────────────────────── */
int main(void)
{
    memset(&g, 0, sizeof(g));
    g.running = 1;
    snprintf(g.last_event, sizeof(g.last_event), "none");

    PLAYOS_LOG_I("boot", "invaders starting (libplayos %u, os %s)",
                 (unsigned)playos_system_api_version(), playos_system_os_version());
    if (playos_system_api_version() != (uint32_t)PLAYOS_API_VERSION)
        PLAYOS_LOG_W("boot", "API mismatch: runtime %u, headers %u",
                     (unsigned)playos_system_api_version(), (unsigned)PLAYOS_API_VERSION);

    const char *saves = playos_storage_get_saves_path();
    if (saves) {
        snprintf(g.saves_path, sizeof(g.saves_path), "%s", saves);
        load_highscore();
        PLAYOS_LOG_I("save", "saves at %s (high score %d)", g.saves_path, g.highscore);
    } else {
        PLAYOS_LOG_W("save", "no saves path available");
    }

    InitWindow((int)VW, (int)VH, "Invaders");
    SetTargetFPS(60);

    new_game();

    while (g.running && !WindowShouldClose()) {
        handle_lifecycle();

        float dt = GetFrameTime();
        if (dt > 0.05f)
            dt = 0.05f;                     /* clamp after a suspend/stall */

        Input in = read_input();

        /* Backgrounded: skip the simulation but STILL fall through to draw and
         * EndDrawing(). EndDrawing() is what pumps Wayland events
         * (PollInputEvents) and commits a buffer; skipping it leaves the
         * compositor's requests unread, so it kills the client ~1.5 s later
         * ("async: game crashed") and the overlay/exit flow never completes.
         * A backgrounded game must keep servicing its Wayland connection. */
        if (!g.paused) {
            if (g.state == ST_OVER) {
                if (in.start)
                    new_game();
            } else {
                update_play(dt, in);
            }
        }

        /* Scale the virtual playfield to the real display. */
        float scale = fminf(GetScreenWidth() / VW, GetScreenHeight() / VH);
        if (scale <= 0.0f)
            scale = 1.0f;
        Camera2D cam = { 0 };
        cam.offset = (Vector2){ GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
        cam.target = (Vector2){ VW / 2.0f, VH / 2.0f };
        cam.zoom = scale;

        BeginDrawing();
        BeginMode2D(cam);
        draw_world();
        EndMode2D();
        EndDrawing();

        g.frames++;
    }

    if (g.score > g.highscore) {
        g.highscore = g.score;
        save_highscore();
    }
    save_highscore();
    CloseWindow();
    PLAYOS_LOG_I("exit", "invaders exiting after %ld frames", g.frames);
    return 0;
}
