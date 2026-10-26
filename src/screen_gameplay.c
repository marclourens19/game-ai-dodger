#include "raylib.h"
#include "screens.h"
#include <math.h>
#include <stdlib.h>

/* Clamp macro constrains value within the inclusive range [min, max] */
#ifndef Clamp
#define Clamp(val, min, max) (((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val)))
#endif

/* Arena Layout Constants in screen pixels */
/* Top HUD bar spans Y coordinates 0 to 48 */
#define ARENA_TOP_BAR_HEIGHT 48.0f

/* Upper bound for agent movement corridor */
#define PLAY_ZONE_TOP        310.0f

/* Lower bound for agent movement corridor */
#define PLAY_ZONE_BOTTOM     375.0f

/* Target baseline altitude for trajectory intersection math and floor reticles */
#define FLOOR_Y              355.0f

/* Vertical position where the spatial memory heatmap bar is drawn */
#define HEATMAP_Y            390.0f

/* Top edge of the dedicated footer bar displaying active control shortcuts */
#define FOOTER_BAR_Y         418.0f

/* Spatial Memory and Difficulty Tuning */
/* The screen width of 800px is partitioned into 16 discrete columns of 50px each */
#define GRID_COLS 16

/* Array storing the lingering threat level of each column based on past impacts */
static float riskHeatmap[GRID_COLS];

/* Hazard bounds: simulation begins with 3 obstacles and dynamically scales up to 14 */
#define MIN_HAZARDS 3
#define MAX_HAZARDS 14

/* Time interval required to introduce an additional falling hazard */
#define SECONDS_PER_NEW_HAZARD 6.5f

static int activeHazardCount = MIN_HAZARDS;

/* Operational simulation modes */
typedef enum {
    MODE_AI_SIMULATION = 0, /* Autonomous navigation via artificial potential fields and spatial memory */
    MODE_PLAYER_MANUAL = 1, /* Direct user control using mouse cursor position */
    MODE_CHAOS_SANDBOX = 2  /* Interactive sandbox where user drops boulders on the dodging agent */
} SimulationMode;

static SimulationMode currentMode = MODE_AI_SIMULATION;

/* Match lifecycle states */
typedef enum {
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAME_OVER
} GameState;

static GameState gameState = STATE_PLAYING;
static float survivalTimer = 0.0f;
static int finishScreen = 0;
static int nearMissCount = 0;
static float chaosBoulderRadius = 32.0f;

/* Toggle for visual AI telemetry including trajectories, force fields, intent line, and memory bar */
static bool showTelemetry = true;

/* Interactive UI button bounding boxes for the pause menu */
static Rectangle btnResume;
static Rectangle btnMenu;
static Rectangle btnExit;

/* Interactive UI button bounding boxes for the game over screen */
static Rectangle btnRetry;
static Rectangle btnGameOverMenu;
static Rectangle btnWipeRestart;

/* Dodger agent definition */
typedef struct {
    Vector2 position;        /* Center coordinates in pixel space */
    float speed;             /* Movement rate in pixels per frame */
    float radius;            /* Physical collision radius in pixels */
    int lives;               /* Hit points remaining before match termination */
    float invulnerableTimer; /* Grace period in seconds following collision damage */
    int targetCol;           /* Spatial column index currently prioritized by global pathfinder */
    Vector2 lastSteerDir;    /* Normalized unit steering direction cached for debug telemetry */
} Agent;

static Agent agent;

/* Falling hazard definition */
typedef struct {
    Vector2 position;        /* Center coordinates in pixel space */
    Vector2 velocity;        /* Displacement vector in pixels per frame */
    float radius;            /* Physical circle radius in pixels */
    float avoidDangerRadius; /* Sensory radius triggering artificial potential field evasion */
    bool active;             /* Slot activity flag */
    float timeToImpact;      /* Estimated seconds until hazard altitude crosses FLOOR_Y */
    bool isPlayerSpawned;    /* Differentiates procedurally spawned rocks from user sandbox clicks */
    bool countedNearMiss;    /* Prevents duplicate counting of near-miss events for a single pass */
} Hazard;

static Hazard hazards[MAX_HAZARDS];

/* Sets the active simulation mode from external callers such as the title screen */
void SetGameplayMode(int mode)
{
    if (mode == 1) currentMode = MODE_PLAYER_MANUAL;
    else if (mode == 2) currentMode = MODE_CHAOS_SANDBOX;
    else currentMode = MODE_AI_SIMULATION;
}

/* Re-randomizes physical properties and initial launch coordinates of an automated hazard */
static void ResetHazard(int index, float speedScale)
{
    /* Physical radius varies between 16px and 36px */
    float r = (float)GetRandomValue(16, 36);
    hazards[index].radius = r;

    /* Danger radius equals rock radius + agent radius (16px) + 35px reaction buffer */
    hazards[index].avoidDangerRadius = r + agent.radius + 35.0f;

    /* Confine horizontal spawning coordinates so the physical circle stays inside screen walls */
    hazards[index].position.x = (float)GetRandomValue((int)(r + 10), (int)(GetScreenWidth() - r - 10));

    /* Spawn off-screen above the visible window between -180px and -20px */
    hazards[index].position.y = (float)GetRandomValue(-180, -20);

    /* Convert randomized speed from pixels per second [130..240] to pixels per frame at 60 FPS */
    float baseSpeed = (float)GetRandomValue(130, 240) / 60.0f;

    /* Vertical velocity scaled by dynamic difficulty multiplier */
    hazards[index].velocity = (Vector2){ 0.0f, baseSpeed * speedScale };
    hazards[index].timeToImpact = 999.0f;
    hazards[index].isPlayerSpawned = false;
    hazards[index].countedNearMiss = false;
}

/* Instantiates a custom boulder at cursor coordinates in Chaos Sandbox mode */
static void SpawnChaosBoulder(Vector2 clickPos)
{
    int slot = -1;
    float highestY = 9999.0f;

    /* Search for an unused slot or overwrite the obstacle highest up in the sky */
    for (int i = 0; i < MAX_HAZARDS; i++)
    {
        if (!hazards[i].active)
        {
            slot = i;
            break;
        }
        if (hazards[i].position.y < highestY)
        {
            highestY = hazards[i].position.y;
            slot = i;
        }
    }

    if (slot != -1)
    {
        hazards[slot].active = true;
        hazards[slot].isPlayerSpawned = true;
        hazards[slot].radius = chaosBoulderRadius;
        hazards[slot].avoidDangerRadius = hazards[slot].radius + agent.radius + 40.0f;

        /* Keep boulder inside screen horizontal bounds */
        float clampedX = Clamp(clickPos.x, chaosBoulderRadius, (float)GetScreenWidth() - chaosBoulderRadius);

        /* Prevent spawning boulders directly on top of the agent by clamping to the sky boundary */
        hazards[slot].position = (Vector2){ clampedX, fminf(clickPos.y, PLAY_ZONE_TOP - 20.0f) };

        /* High vertical speed for player-dropped obstacles: [200..320] pixels/sec converted to frame delta */
        hazards[slot].velocity = (Vector2){ 0.0f, (float)GetRandomValue(200, 320) / 60.0f };
        hazards[slot].timeToImpact = 999.0f;
        hazards[slot].countedNearMiss = false;
        PlaySound(fxCoin);
    }
}

/* Reinitializes agent attributes, obstacle states, and optionally wipes learned spatial risk */
static void ResetGame(bool clearMemory)
{
    gameState = STATE_PLAYING;
    survivalTimer = 0.0f;
    nearMissCount = 0;
    activeHazardCount = (currentMode == MODE_CHAOS_SANDBOX) ? 0 : MIN_HAZARDS;

    /* Center agent horizontally at FLOOR_Y altitude */
    agent.position = (Vector2){ (float)GetScreenWidth() / 2.0f, FLOOR_Y };
    agent.speed = 4.2f;
    agent.radius = 16.0f;
    agent.lives = 3;
    agent.invulnerableTimer = 1.0f;
    agent.targetCol = GRID_COLS / 2;
    agent.lastSteerDir = (Vector2){ 0.0f, 0.0f };

    /* Wipe spatial heatmap when requested */
    if (clearMemory)
    {
        for (int i = 0; i < GRID_COLS; i++) riskHeatmap[i] = 0.0f;
    }

    /* Stagger procedural hazards above the viewport */
    for (int i = 0; i < MAX_HAZARDS; i++)
    {
        hazards[i].active = (currentMode != MODE_CHAOS_SANDBOX && i < MIN_HAZARDS);
        ResetHazard(i, 1.0f);
        hazards[i].position.y = -40.0f - (i * 65.0f);
    }
}

void InitGameplayScreen(void)
{
    finishScreen = 0;
    SetExitKey(KEY_NULL); /* Disables Raylib default window closing on ESC so ESC can toggle pause */

    float btnW = 220.0f;
    float btnH = 42.0f;
    float centerX = ((float)GetScreenWidth() - btnW) / 2.0f;

    /* Pause menu button positions */
    btnResume = (Rectangle){ centerX, 175.0f, btnW, btnH };
    btnMenu = (Rectangle){ centerX, 230.0f, btnW, btnH };
    btnExit = (Rectangle){ centerX, 285.0f, btnW, btnH };

    /* Game over button positions */
    btnRetry = (Rectangle){ centerX, 185.0f, btnW, btnH };
    btnGameOverMenu = (Rectangle){ centerX, 237.0f, btnW, btnH };
    btnWipeRestart = (Rectangle){ centerX, 289.0f, btnW, btnH };

    ResetGame(true);
}

void UpdateGameplayScreen(void)
{
    /* ESC toggles pause state during active gameplay */
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (gameState == STATE_PLAYING) gameState = STATE_PAUSED;
        else if (gameState == STATE_PAUSED)
        {
            gameState = STATE_PLAYING;
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }
    }

    /* Pause menu event processing */
    if (gameState == STATE_PAUSED)
    {
        Vector2 mousePos = GetMousePosition();
        bool hoverResume = CheckCollisionPointRec(mousePos, btnResume);
        bool hoverMenu = CheckCollisionPointRec(mousePos, btnMenu);
        bool hoverExit = CheckCollisionPointRec(mousePos, btnExit);

        if (hoverResume || hoverMenu || hoverExit) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        else SetMouseCursor(MOUSE_CURSOR_DEFAULT);

        if (hoverResume && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            gameState = STATE_PLAYING;
            PlaySound(fxCoin);
        }
        if (hoverMenu && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            finishScreen = 1;
            PlaySound(fxCoin);
        }
        if (hoverExit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            CloseAudioDevice();
            CloseWindow();
            exit(0);
        }
        return;
    }

    /* Game over menu event processing */
    if (gameState == STATE_GAME_OVER)
    {
        Vector2 mousePos = GetMousePosition();
        bool hoverRetry = CheckCollisionPointRec(mousePos, btnRetry);
        bool hoverMenu = CheckCollisionPointRec(mousePos, btnGameOverMenu);
        bool hoverWipe = CheckCollisionPointRec(mousePos, btnWipeRestart);

        if (hoverRetry || hoverMenu || hoverWipe) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        else SetMouseCursor(MOUSE_CURSOR_DEFAULT);

        /* Restart while preserving learned riskHeatmap array */
        if (hoverRetry && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            ResetGame(false);
            PlaySound(fxCoin);
        }
        if (hoverMenu && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            finishScreen = 1;
            PlaySound(fxCoin);
        }
        /* Restart and zero-initialize spatial memory */
        if (hoverWipe && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            ResetGame(true);
            PlaySound(fxCoin);
        }
        return;
    }

    /* Global hotkeys */
    if (IsKeyPressed(KEY_R)) { ResetGame(false); return; }
    if (IsKeyPressed(KEY_C)) { ResetGame(true); return; }
    if (IsKeyPressed(KEY_M)) { finishScreen = 1; return; }

    /* Mode selection hotkeys */
    if (IsKeyPressed(KEY_ONE)) { currentMode = MODE_AI_SIMULATION; ResetGame(false); return; }
    if (IsKeyPressed(KEY_TWO)) { currentMode = MODE_PLAYER_MANUAL; ResetGame(false); return; }
    if (IsKeyPressed(KEY_THREE)) { currentMode = MODE_CHAOS_SANDBOX; ResetGame(false); return; }

    /* Live takeover toggle: switches control between AI and mouse without restarting */
    if (IsKeyPressed(KEY_P))
    {
        currentMode = (currentMode == MODE_PLAYER_MANUAL) ? MODE_AI_SIMULATION : MODE_PLAYER_MANUAL;
    }

    /* Toggle all AI telemetry, reticles, trajectory rays, bubbles, and heatmap */
    if (IsKeyPressed(KEY_T))
    {
        showTelemetry = !showTelemetry;
    }

    /* Chaos Sandbox controls: scroll wheel adjusts boulder radius, left click drops rock */
    if (currentMode == MODE_CHAOS_SANDBOX)
    {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f)
        {
            chaosBoulderRadius += wheel * 4.0f;
            chaosBoulderRadius = Clamp(chaosBoulderRadius, 16.0f, 56.0f);
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            SpawnChaosBoulder(GetMousePosition());
        }
    }
    else
    {
        /* Dynamic difficulty scaling: increase active hazard count every 6.5 seconds */
        /* Formula: count = min(MAX_HAZARDS, MIN_HAZARDS + floor(t / 6.5)) */
        int calculatedCount = MIN_HAZARDS + (int)(survivalTimer / SECONDS_PER_NEW_HAZARD);
        if (calculatedCount > MAX_HAZARDS) calculatedCount = MAX_HAZARDS;
        if (calculatedCount > activeHazardCount)
        {
            for (int i = activeHazardCount; i < calculatedCount; i++)
            {
                hazards[i].active = true;
                /* Speed increases by 1% for each second survived */
                ResetHazard(i, 1.0f + (survivalTimer * 0.01f));
            }
            activeHazardCount = calculatedCount;
        }
    }

    float dt = GetFrameTime();
    survivalTimer += dt;
    if (agent.invulnerableTimer > 0.0f) agent.invulnerableTimer -= dt;

    /* Base falling velocity increases by 0.7% per second, capped at +35% */
    float currentSpeedScale = 1.0f + fminf(survivalTimer * 0.007f, 0.35f);

    int currentActiveInSandbox = 0;
    for (int i = 0; i < MAX_HAZARDS; i++)
    {
        if (!hazards[i].active) continue;
        currentActiveInSandbox++;

        /* Kinematic position update: position = position + velocity */
        hazards[i].position.x += hazards[i].velocity.x;
        hazards[i].position.y += hazards[i].velocity.y;

        /* Kinematic Time-To-Impact (TTI) formula: */
        /* d = FLOOR_Y - position.y (remaining distance to target altitude) */
        /* t_frames = d / velocity.y (frames until crossing) */
        /* t_seconds = t_frames * (1.0 / 60.0) */
        float distToFloor = FLOOR_Y - hazards[i].position.y;
        if (hazards[i].velocity.y > 0.001f && distToFloor > 0.0f)
        {
            hazards[i].timeToImpact = (distToFloor / hazards[i].velocity.y) * (1.0f / 60.0f);
        }
        else
        {
            hazards[i].timeToImpact = 0.0f;
        }

        /* Euclidean distance between agent center and hazard center: sqrt(dx^2 + dy^2) */
        float dx = agent.position.x - hazards[i].position.x;
        float dy = agent.position.y - hazards[i].position.y;
        float distToAgent = sqrtf(dx * dx + dy * dy);

        /* Near-miss detection: obstacle entered sensory danger radius without physical contact */
        if (distToAgent < hazards[i].avoidDangerRadius && !hazards[i].countedNearMiss)
        {
            hazards[i].countedNearMiss = true;
            nearMissCount++;
        }

        /* Recycle obstacle once its top edge moves past the bottom viewport edge */
        if (hazards[i].position.y > GetScreenHeight() + hazards[i].radius)
        {
            if (currentMode == MODE_CHAOS_SANDBOX) hazards[i].active = false;
            else ResetHazard(i, currentSpeedScale);
        }
    }

    if (currentMode == MODE_CHAOS_SANDBOX) activeHazardCount = currentActiveInSandbox;

    /* Spatial memory linear time decay: */
    /* Risk penalty decays at a rate of 2.0 units per second: R(t + dt) = max(0, R(t) - 2.0 * dt) */
    float colWidth = (float)GetScreenWidth() / (float)GRID_COLS;
    for (int i = 0; i < GRID_COLS; i++)
    {
        if (riskHeatmap[i] > 0.0f)
        {
            riskHeatmap[i] -= 2.0f * dt;
            if (riskHeatmap[i] < 0.0f) riskHeatmap[i] = 0.0f;
        }
    }

    if (currentMode == MODE_PLAYER_MANUAL)
    {
        /* Direct mouse position binding */
        agent.position = GetMousePosition();
    }
    else
    {
        /* Global Pathfinding: evaluate combined threat score across all 16 corridor columns */
        float bestScore = 999999.0f;
        int bestCol = agent.targetCol;

        for (int col = 0; col < GRID_COLS; col++)
        {
            /* Compute center X coordinate of evaluated column */
            float colCenterX = (col * colWidth) + (colWidth / 2.0f);

            /* Threat begins with historical damage memory */
            float candidateRisk = riskHeatmap[col];

            /* Corner penalty: add +16.0 risk to edge columns to avoid corner trapping */
            if (col == 0 || col == GRID_COLS - 1) candidateRisk += 16.0f;

            /* Project kinematic trajectory of all falling hazards onto FLOOR_Y */
            for (int h = 0; h < MAX_HAZARDS; h++)
            {
                if (!hazards[h].active) continue;

                float dy = FLOOR_Y - hazards[h].position.y;
                float projectedX = hazards[h].position.x;

                /* Trajectory projection: x_proj = x + (vx / vy) * dy */
                if (hazards[h].velocity.y > 0.01f && dy > 0.0f)
                {
                    projectedX += (hazards[h].velocity.x / hazards[h].velocity.y) * dy;
                }

                /* Proximity between projected landing point and column center */
                float distToColX = fabsf(projectedX - colCenterX);

                if (distToColX < hazards[h].avoidDangerRadius)
                {
                    /* Inverse Time-To-Impact urgency weight: 1.0 / (TTI + 0.25) */
                    /* Hazards landing sooner contribute vastly higher risk scores */
                    float timeUrgency = 1.0f / (hazards[h].timeToImpact + 0.25f);
                    float proximity = 1.0f - (distToColX / hazards[h].avoidDangerRadius);
                    candidateRisk += proximity * timeUrgency * 15.0f;
                }
            }

            /* Travel dampening penalty: penalize distant columns so the AI prefers nearby safety */
            float travelDist = fabsf(colCenterX - agent.position.x);
            candidateRisk += (travelDist / (float)GetScreenWidth()) * 9.0f;

            if (candidateRisk < bestScore)
            {
                bestScore = candidateRisk;
                bestCol = col;
            }
        }

        agent.targetCol = bestCol;

        /* Attractive seeking force: generate normalized direction vector toward target column center */
        float targetX = (agent.targetCol * colWidth) + (colWidth / 2.0f);
        float toTargetX = targetX - agent.position.x;
        float seekX = 0.0f;

        /* 4px deadzone prevents high-frequency horizontal jitter upon arrival */
        if (fabsf(toTargetX) > 4.0f) seekX = (toTargetX > 0.0f) ? 0.65f : -0.65f;

        /* Repulsive Artificial Potential Field: calculate repulsion forces away from nearby obstacles */
        float totalAvoidX = 0.0f;
        float totalAvoidY = 0.0f;

        for (int i = 0; i < MAX_HAZARDS; i++)
        {
            if (!hazards[i].active) continue;

            /* Vector from hazard center to agent center: V_repel = P_agent - P_hazard */
            float dx = agent.position.x - hazards[i].position.x;
            float dy = agent.position.y - hazards[i].position.y;
            float distToObs = sqrtf(dx * dx + dy * dy);

            if (distToObs < hazards[i].avoidDangerRadius && distToObs > 0.001f)
            {
                /* Linear distance falloff factor: panic = (R_danger - d) / R_danger */
                float panic = (hazards[i].avoidDangerRadius - distToObs) / hazards[i].avoidDangerRadius;

                /* Urgency multiplier based on time remaining to floor impact, capped at 3.0 */
                float urgencyWeight = 1.0f / (hazards[i].timeToImpact + 0.3f);
                if (urgencyWeight > 3.0f) urgencyWeight = 3.0f;

                /* Normalized direction (dx / dist, dy / dist) multiplied by panic and urgency weights */
                /* Horizontal evasion is weighted heavily (3.6) for rapid sidestepping */
                totalAvoidX += (dx / distToObs) * (panic * 3.6f * urgencyWeight);
                totalAvoidY += (dy / distToObs) * (panic * 1.5f);
            }
        }

        /* Superposition: combine attractive seeking force and repulsive potential field vectors */
        float finalDirX = seekX + totalAvoidX;
        float finalDirY = totalAvoidY;

        /* Vector normalization: compute magnitude ||V|| = sqrt(Vx^2 + Vy^2) */
        float finalLen = sqrtf(finalDirX * finalDirX + finalDirY * finalDirY);

        if (finalLen > 0.01f)
        {
            /* Compute unit direction vector: u = V / ||V|| */
            agent.lastSteerDir.x = finalDirX / finalLen;
            agent.lastSteerDir.y = finalDirY / finalLen;

            /* Displace agent along unit vector by constant movement speed */
            agent.position.x += agent.lastSteerDir.x * agent.speed;
            agent.position.y += agent.lastSteerDir.y * agent.speed;
        }
        else
        {
            agent.lastSteerDir = (Vector2){ 0.0f, 0.0f };
        }
    }

    /* Boundary clamping: enforce strict corridor containment */
    if (agent.position.x < agent.radius) agent.position.x = agent.radius;
    if (agent.position.x > GetScreenWidth() - agent.radius) agent.position.x = GetScreenWidth() - agent.radius;
    if (agent.position.y < PLAY_ZONE_TOP) agent.position.y = PLAY_ZONE_TOP;
    if (agent.position.y > PLAY_ZONE_BOTTOM) agent.position.y = PLAY_ZONE_BOTTOM;

    /* Circle-to-circle collision resolution */
    for (int i = 0; i < MAX_HAZARDS; i++)
    {
        if (!hazards[i].active) continue;

        float dx = agent.position.x - hazards[i].position.x;
        float dy = agent.position.y - hazards[i].position.y;
        float distToObs = sqrtf(dx * dx + dy * dy);

        /* Collision occurs when Euclidean distance is less than the sum of radii: d < (r1 + r2) */
        float hitDistance = hazards[i].radius + agent.radius;

        if (distToObs < hitDistance && agent.invulnerableTimer <= 0.0f)
        {
            agent.lives--;
            agent.invulnerableTimer = 1.5f; /* 1.5 seconds of invincibility frames */
            PlaySound(fxCoin);

            /* Spatial learning penalty: stamp hit column with +55.0 risk and neighbors with +22.0 */
            int hitCol = (int)(agent.position.x / colWidth);
            if (hitCol >= 0 && hitCol < GRID_COLS) riskHeatmap[hitCol] += 55.0f;
            if (hitCol > 0) riskHeatmap[hitCol - 1] += 22.0f;
            if (hitCol < GRID_COLS - 1) riskHeatmap[hitCol + 1] += 22.0f;

            if (agent.lives <= 0) gameState = STATE_GAME_OVER;
        }
    }
}

void DrawGameplayScreen(void)
{
    /* Clear arena canvas with neutral light background */
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 245, 245, 248, 255 });

    /* Top HUD header bar */
    Color topBarColor = (currentMode == MODE_CHAOS_SANDBOX) ? (Color) { 55, 30, 35, 255 } : (Color) { 35, 35, 40, 255 };
    DrawRectangle(0, 0, GetScreenWidth(), (int)ARENA_TOP_BAR_HEIGHT, topBarColor);

    const char* modeBadge = "MODE: AI DODGER";
    Color badgeColor = SKYBLUE;
    if (currentMode == MODE_PLAYER_MANUAL) { modeBadge = "MODE: HUMAN (MOUSE)"; badgeColor = GREEN; }
    if (currentMode == MODE_CHAOS_SANDBOX) { modeBadge = "MODE: CHAOS DROP"; badgeColor = (Color){ 255, 120, 100, 255 }; }
    DrawText(modeBadge, 16, 16, 16, badgeColor);

    DrawText(TextFormat("TIME: %.1fs", survivalTimer), 270, 16, 16, RAYWHITE);
    DrawText(TextFormat("MISSES: %i", nearMissCount), 385, 16, 16, LIGHTGRAY);
    DrawText(TextFormat("HAZ: %i/%i", activeHazardCount, MAX_HAZARDS), 495, 16, 16, (activeHazardCount == MAX_HAZARDS) ? RED : GOLD);

    /* Lives counter indicators */
    for (int i = 0; i < 3; i++)
    {
        int heartX = GetScreenWidth() - 30 - (i * 26);
        int heartY = 24;
        if (i < agent.lives) DrawCircle(heartX, heartY, 8, RED);
        else DrawCircleLines(heartX, heartY, 8, (Color) { 120, 120, 120, 255 });
    }

    /* Upper corridor boundary line */
    DrawLine(0, (int)PLAY_ZONE_TOP - 10, GetScreenWidth(), (int)PLAY_ZONE_TOP - 10, (Color) { 220, 220, 225, 255 });

    /* Render hazards and optional visual telemetry */
    for (int i = 0; i < MAX_HAZARDS; i++)
    {
        if (!hazards[i].active) continue;

        float dy = FLOOR_Y - hazards[i].position.y;
        float projX = hazards[i].position.x;
        if (hazards[i].velocity.y > 0.01f && dy > 0.0f)
        {
            projX += (hazards[i].velocity.x / hazards[i].velocity.y) * dy;
        }

        /* Telemetry graphics: trajectory projection lines, floor reticles, APF bubbles, and force lines */
        if (showTelemetry)
        {
            /* Projected trajectory line from hazard center to floor altitude */
            DrawLine((int)hazards[i].position.x, (int)hazards[i].position.y, (int)projX, (int)FLOOR_Y, (Color) { 190, 190, 200, 80 });

            /* Dynamic reticle alpha increases as Time-To-Impact approaches zero */
            float reticleRadius = 4.0f + (hazards[i].radius * 0.35f);
            unsigned char reticleAlpha = (unsigned char)Clamp(255.0f - (hazards[i].timeToImpact * 180.0f), 40.0f, 220.0f);
            Color reticleCol = hazards[i].isPlayerSpawned ? (Color) { 255, 60, 60, reticleAlpha } : (Color) { 230, 80, 80, reticleAlpha };
            DrawCircleLines((int)projX, (int)FLOOR_Y, reticleRadius, reticleCol);

            /* Artificial Potential Field repulsive influence bubble */
            DrawCircleV(hazards[i].position, hazards[i].avoidDangerRadius, (Color) { 255, 100, 100, 20 });
            DrawCircleLines((int)hazards[i].position.x, (int)hazards[i].position.y, hazards[i].avoidDangerRadius, (Color) { 230, 80, 80, 60 });

            /* Active repulsive evasion line connecting agent to threatening obstacle */
            float hdx = agent.position.x - hazards[i].position.x;
            float hdy = agent.position.y - hazards[i].position.y;
            if (sqrtf(hdx * hdx + hdy * hdy) < hazards[i].avoidDangerRadius)
            {
                DrawLineV(agent.position, hazards[i].position, ORANGE);
            }
        }

        /* Physical solid hazard body */
        Color rockColor = hazards[i].isPlayerSpawned ? (Color) { 70, 30, 30, 255 } : DARKGRAY;
        DrawCircleV(hazards[i].position, hazards[i].radius, rockColor);
    }

    float colWidth = (float)GetScreenWidth() / (float)GRID_COLS;

    /* Cyan intent line showing the target column chosen by the global pathfinder */
    if (showTelemetry && currentMode != MODE_PLAYER_MANUAL)
    {
        float targetX = (agent.targetCol * colWidth) + (colWidth / 2.0f);
        DrawLine((int)targetX, (int)PLAY_ZONE_TOP, (int)targetX, (int)PLAY_ZONE_BOTTOM, (Color) { 80, 170, 240, 70 });
    }

    /* Strobe agent rendering during invulnerability frames at 10 Hz */
    bool shouldDrawAgent = true;
    if (agent.invulnerableTimer > 0.0f && ((int)(agent.invulnerableTimer * 10.0f) % 2 == 0)) shouldDrawAgent = false;

    if (shouldDrawAgent)
    {
        Color agentColor = DARKBLUE;
        if (agent.invulnerableTimer > 0.0f) agentColor = RED;
        else if (currentMode == MODE_PLAYER_MANUAL) agentColor = (Color){ 0, 160, 100, 255 };

        DrawCircleV(agent.position, agent.radius, agentColor);

        /* Resultant vector telemetry ray: projects cached normalized direction 32px outward */
        if (showTelemetry && currentMode != MODE_PLAYER_MANUAL && (fabsf(agent.lastSteerDir.x) > 0.01f || fabsf(agent.lastSteerDir.y) > 0.01f))
        {
            Vector2 vectorTip = {
                agent.position.x + agent.lastSteerDir.x * 32.0f,
                agent.position.y + agent.lastSteerDir.y * 32.0f
            };
            DrawLineEx(agent.position, vectorTip, 2.5f, GOLD);
            DrawCircleV(vectorTip, 3.0f, YELLOW);
        }

        if (currentMode == MODE_PLAYER_MANUAL)
        {
            DrawText("YOU", (int)agent.position.x - 12, (int)agent.position.y - 26, 12, (Color) { 0, 120, 70, 255 });
        }
    }

    /* Crosshair preview in Chaos Sandbox mode */
    if (currentMode == MODE_CHAOS_SANDBOX)
    {
        Vector2 m = GetMousePosition();
        if (m.y < PLAY_ZONE_TOP)
        {
            DrawCircleLines((int)m.x, (int)m.y, chaosBoulderRadius, RED);
            DrawLine((int)m.x - 22, (int)m.y, (int)m.x + 22, (int)m.y, RED);
            DrawLine((int)m.x, (int)m.y - 22, (int)m.x, (int)m.y + 22, RED);
        }
    }

    /* Spatial memory heatmap visualization: interpolates green (safe) to red (high risk) */
    if (showTelemetry)
    {
        DrawText("AI MEMORY:", 16, (int)HEATMAP_Y + 2, 11, (Color) { 100, 100, 110, 255 });
        float cellStartX = 110.0f;
        float cellTotalW = (float)GetScreenWidth() - cellStartX - 16.0f;
        float cellW = cellTotalW / (float)GRID_COLS;

        for (int i = 0; i < GRID_COLS; i++)
        {
            /* Normalize risk score [0..50] to [0.0..1.0] for color channels */
            float normalizedRisk = riskHeatmap[i] / 50.0f;
            if (normalizedRisk > 1.0f) normalizedRisk = 1.0f;

            Color cellColor = (Color){
                (unsigned char)(normalizedRisk * 220),          /* Red channel increases with risk */
                (unsigned char)((1.0f - normalizedRisk) * 180), /* Green channel decreases with risk */
                70,
                (unsigned char)(40 + normalizedRisk * 140)      /* Alpha increases with risk */
            };

            DrawRectangle((int)(cellStartX + i * cellW), (int)HEATMAP_Y, (int)cellW - 2, 12, cellColor);
            DrawRectangleLines((int)(cellStartX + i * cellW), (int)HEATMAP_Y, (int)cellW - 2, 12, (Color) { 180, 180, 190, 60 });
        }
    }

    /* Bottom shortcut controls bar */
    DrawRectangle(0, (int)FOOTER_BAR_Y, GetScreenWidth(), GetScreenHeight() - (int)FOOTER_BAR_Y, (Color) { 28, 28, 32, 255 });
    DrawLine(0, (int)FOOTER_BAR_Y, GetScreenWidth(), (int)FOOTER_BAR_Y, (Color) { 45, 45, 52, 255 });
    DrawText("[1] AI Mode   [2] Mouse Play   [3] Chaos Drop   |   [P] Takeover   [T] Telemetry   [ESC] Pause",
        18, (int)FOOTER_BAR_Y + 9, 12, (Color) { 200, 205, 215, 255 });

    /* Pause modal overlay */
    if (gameState == STATE_PAUSED)
    {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 0, 0, 0, 160 });
        const char* pauseTitle = "SIMULATION PAUSED";
        int titleWidth = MeasureText(pauseTitle, 24);
        DrawText(pauseTitle, (GetScreenWidth() - titleWidth) / 2, 120, 24, RAYWHITE);

        Vector2 mPos = GetMousePosition();

        bool hoverResume = CheckCollisionPointRec(mPos, btnResume);
        Color colResume = hoverResume ? (Color) { 30, 110, 210, 255 } : (Color) { 45, 45, 50, 255 };
        DrawRectangleRounded(btnResume, 0.25f, 6, colResume);
        DrawRectangleRoundedLines(btnResume, 0.25f, 6, (Color) { 20, 20, 25, 255 });
        int wResume = MeasureText("RESUME", 16);
        DrawText("RESUME", (int)(btnResume.x + (btnResume.width - wResume) / 2), (int)(btnResume.y + 13), 16, RAYWHITE);

        bool hoverMenu = CheckCollisionPointRec(mPos, btnMenu);
        Color colMenu = hoverMenu ? (Color) { 210, 140, 30, 255 } : (Color) { 45, 45, 50, 255 };
        DrawRectangleRounded(btnMenu, 0.25f, 6, colMenu);
        DrawRectangleRoundedLines(btnMenu, 0.25f, 6, (Color) { 20, 20, 25, 255 });
        int wMenu = MeasureText("RETURN TO MENU", 16);
        DrawText("RETURN TO MENU", (int)(btnMenu.x + (btnMenu.width - wMenu) / 2), (int)(btnMenu.y + 13), 16, RAYWHITE);

        bool hoverExit = CheckCollisionPointRec(mPos, btnExit);
        Color colExit = hoverExit ? (Color) { 210, 50, 50, 255 } : (Color) { 45, 45, 50, 255 };
        DrawRectangleRounded(btnExit, 0.25f, 6, colExit);
        DrawRectangleRoundedLines(btnExit, 0.25f, 6, (Color) { 20, 20, 25, 255 });
        int wExit = MeasureText("EXIT", 16);
        DrawText("EXIT", (int)(btnExit.x + (btnExit.width - wExit) / 2), (int)(btnExit.y + 13), 16, RAYWHITE);
    }

    /* Game Over modal overlay */
    if (gameState == STATE_GAME_OVER)
    {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 0, 0, 0, 180 });

        const char* overTitle = (currentMode == MODE_PLAYER_MANUAL) ? "HUMAN RUN TERMINATED" : "SIMULATION TERMINATED";
        int titleW = MeasureText(overTitle, 22);
        DrawText(overTitle, (GetScreenWidth() - titleW) / 2, 110, 22, RED);

        const char* statsText = TextFormat("Survival Time: %.2fs  |  Near Misses: %i", survivalTimer, nearMissCount);
        int timeW = MeasureText(statsText, 14);
        DrawText(statsText, (GetScreenWidth() - timeW) / 2, 145, 14, RAYWHITE);

        Vector2 mPos = GetMousePosition();

        /* Try again button (retains learned risk memory) */
        bool hoverRetry = CheckCollisionPointRec(mPos, btnRetry);
        Color colRetry = hoverRetry ? (Color) { 30, 140, 90, 255 } : (Color) { 45, 45, 50, 255 };
        DrawRectangleRounded(btnRetry, 0.25f, 6, colRetry);
        DrawRectangleRoundedLines(btnRetry, 0.25f, 6, (Color) { 20, 20, 25, 255 });
        int wRetry = MeasureText("TRY AGAIN", 16);
        DrawText("TRY AGAIN", (int)(btnRetry.x + (btnRetry.width - wRetry) / 2), (int)(btnRetry.y + 13), 16, RAYWHITE);

        /* Return to menu button */
        bool hoverMenu = CheckCollisionPointRec(mPos, btnGameOverMenu);
        Color colMenu = hoverMenu ? (Color) { 210, 140, 30, 255 } : (Color) { 45, 45, 50, 255 };
        DrawRectangleRounded(btnGameOverMenu, 0.25f, 6, colMenu);
        DrawRectangleRoundedLines(btnGameOverMenu, 0.25f, 6, (Color) { 20, 20, 25, 255 });
        int wMenu = MeasureText("GO BACK TO MAIN MENU", 16);
        DrawText("GO BACK TO MAIN MENU", (int)(btnGameOverMenu.x + (btnGameOverMenu.width - wMenu) / 2), (int)(btnGameOverMenu.y + 13), 16, RAYWHITE);

        /* Restart and wipe memory button */
        bool hoverWipe = CheckCollisionPointRec(mPos, btnWipeRestart);
        Color colWipe = hoverWipe ? (Color) { 210, 50, 50, 255 } : (Color) { 45, 45, 50, 255 };
        DrawRectangleRounded(btnWipeRestart, 0.25f, 6, colWipe);
        DrawRectangleRoundedLines(btnWipeRestart, 0.25f, 6, (Color) { 20, 20, 25, 255 });
        int wWipe = MeasureText("RESTART (WIPE MEMORY)", 16);
        DrawText("RESTART (WIPE MEMORY)", (int)(btnWipeRestart.x + (btnWipeRestart.width - wWipe) / 2), (int)(btnWipeRestart.y + 13), 16, RAYWHITE);
    }
}

void UnloadGameplayScreen(void)
{
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
}

int FinishGameplayScreen(void)
{
    return finishScreen;
}