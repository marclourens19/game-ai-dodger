#include "raylib.h"
#include <math.h>

// Top spawner rectangle height (Y: 0 to 60)
#define ARENA_TOP_BAR_HEIGHT 60.0f

// Upper Y limit for agent movement (prevents climbing into hazard spawn zone)
#define PLAY_ZONE_TOP 340.0f

// Lower Y limit for agent movement (keeps agent above floor padding)
#define PLAY_ZONE_BOTTOM 420.0f

// Target baseline Y coordinate where the agent prefers to stay anchored
#define FLOOR_Y 380.0f

// Number of vertical memory columns spanning the 800px screen (800 / 16 = 50px per column)
#define GRID_COLS 16

// Array tracking learned danger levels per column; higher values indicate dangerous areas
static float riskHeatmap[GRID_COLS];

// State machine values for active match vs game over screen
typedef enum {
    STATE_PLAYING,
    STATE_GAME_OVER
} GameState;

// Tracks active match status
static GameState gameState = STATE_PLAYING;

// Counts total seconds survived in the current match
static float survivalTimer = 0.0f;

// Starting number of falling balls
#define MIN_HAZARDS 3

// Maximum simultaneous falling balls allowed at peak difficulty
#define MAX_HAZARDS 12

// Interval in seconds before activating another falling ball
#define SECONDS_PER_NEW_HAZARD 7.0f

// Current count of hazards updated and drawn
static int activeHazardCount = MIN_HAZARDS;

// Represents the autonomous dodging circle
typedef struct {
    Vector2 position;         // Current 2D coordinates on screen (X, Y)
    float speed;              // Movement rate in pixels per frame
    float radius;             // Physical size for drawing and collision checks
    int lives;                // Hit points remaining (starts at 3)
    float invulnerableTimer;  // Seconds remaining during which incoming damage is ignored
    int targetCol;            // Column index (0 to 15) evaluated as having lowest threat
} Agent;

static Agent agent;

// Represents a falling circular obstacle
typedef struct {
    Vector2 position;         // Current coordinates (X, Y)
    float speedY;             // Downward speed in pixels per frame
    float radius;             // Solid collider radius
    float avoidDangerRadius;  // Expanded sensor bubble where the AI starts steering away
    bool active;              // True if active in the current wave; false if dormant in pool
    float timeToImpact;       // Calculated seconds remaining before reaching FLOOR_Y
} Hazard;

// Pre-allocated object pool to prevent memory reallocations during gameplay
static Hazard hazards[MAX_HAZARDS];

// Resets a single hazard back above the top spawner with fresh random properties
static void ResetHazard(int index, float speedScale) {
    // Generate a random solid radius between 16px and 36px
    float r = (float)GetRandomValue(16, 36);
    hazards[index].radius = r;

    // Reaction boundary: obstacle radius + agent radius + 35px safety buffer
    // Adding both radii accounts for distance between edges rather than centers
    hazards[index].avoidDangerRadius = r + agent.radius + 35.0f;

    // Pick random X coordinate within visible window margins
    hazards[index].position.x = (float)GetRandomValue((int)(r + 10), (int)(GetScreenWidth() - r - 10));

    // Stagger spawn height above screen top (negative Y) so balls arrive at different times
    hazards[index].position.y = (float)GetRandomValue(-180, -20);

    // Pick random base speed (130 to 240 px/sec) and convert to pixels per frame at 60 FPS
    float baseSpeed = (float)GetRandomValue(130, 240) / 60.0f;

    // Apply difficulty scaling multiplier to increase speed as match progresses
    hazards[index].speedY = baseSpeed * speedScale;

    // High placeholder value until kinematics run in the update step
    hazards[index].timeToImpact = 999.0f;
}

// Resets game state for a fresh match
static void ResetGame(bool clearMemory) {
    gameState = STATE_PLAYING;
    survivalTimer = 0.0f;
    activeHazardCount = MIN_HAZARDS;

    // Place agent at horizontal center and baseline floor height
    agent.position = (Vector2){ (float)GetScreenWidth() / 2.0f, FLOOR_Y };
    agent.speed = 4.2f;
    agent.radius = 16.0f;
    agent.lives = 3;
    agent.invulnerableTimer = 0.0f;
    agent.targetCol = GRID_COLS / 2;

    // Wipe memory array only if requested (pressing C wipes, pressing R retains)
    if (clearMemory) {
        for (int i = 0; i < GRID_COLS; i++) {
            riskHeatmap[i] = 0.0f;
        }
    }

    // Initialize all pool hazards; activate starting baseline count
    for (int i = 0; i < MAX_HAZARDS; i++) {
        hazards[i].active = (i < MIN_HAZARDS);
        ResetHazard(i, 1.0f);
        // Stagger initial Y heights across ceiling to prevent an initial clustered wall
        hazards[i].position.y = -40.0f - (i * 65.0f);
    }
}

// Runs once when the screen loads into memory
void InitGameplayScreen(void) {
    ResetGame(true);
}

// Main logic loop executed 60 times per second
void UpdateGameplayScreen(void) {
    // If lives hit 0, suspend updates and listen for restart keys
    if (gameState == STATE_GAME_OVER) {
        if (IsKeyPressed(KEY_R)) {
            // Restart match while retaining learned risk heatmap
            ResetGame(false);
        }
        if (IsKeyPressed(KEY_C)) {
            // Restart match and wipe learned memory
            ResetGame(true);
        }
        return;
    }

    // Get time elapsed since previous frame in seconds (approximately 0.0166s at 60 FPS)
    float dt = GetFrameTime();

    // Accumulate total match survival time
    survivalTimer += dt;

    // Count down invulnerability timer toward zero
    if (agent.invulnerableTimer > 0.0f) {
        agent.invulnerableTimer -= dt;
    }

    // 1. DYNAMIC DIFFICULTY SCALING
    // Calculate how many balls should be active based on time elapsed
    int calculatedCount = MIN_HAZARDS + (int)(survivalTimer / SECONDS_PER_NEW_HAZARD);
    if (calculatedCount > MAX_HAZARDS) {
        calculatedCount = MAX_HAZARDS;
    }

    // If a new hazard threshold is reached, activate it from above the screen
    if (calculatedCount > activeHazardCount) {
        for (int i = activeHazardCount; i < calculatedCount; i++) {
            hazards[i].active = true;
            // Introduce new hazard with current speed multiplier
            ResetHazard(i, 1.0f + (survivalTimer * 0.01f));
        }
        activeHazardCount = calculatedCount;
    }

    // Increase fall speed over time (capped at +35% after ~50 seconds)
    float currentSpeedScale = 1.0f + fminf(survivalTimer * 0.007f, 0.35f);

    // 2. KINEMATIC UPDATE & TRAJECTORY PREDICTION
    for (int i = 0; i < activeHazardCount; i++) {
        if (!hazards[i].active) continue;

        // Move obstacle downward: Position = Position + Velocity
        hazards[i].position.y += hazards[i].speedY;

        // Vertical distance remaining before hazard center reaches agent floor level: dy = FLOOR_Y - y
        float distToFloor = FLOOR_Y - hazards[i].position.y;

        // Kinematic time calculation: time = distance / velocity
        // hazards[i].speedY is in pixels/frame; multiply by (1/60) to convert to seconds
        if (hazards[i].speedY > 0.001f && distToFloor > 0.0f) {
            hazards[i].timeToImpact = (distToFloor / hazards[i].speedY) * (1.0f / 60.0f);
        }
        else {
            // Zero indicates the hazard has already reached or passed floor level
            hazards[i].timeToImpact = 0.0f;
        }

        // Object recycling: once hazard clears bottom window edge, wrap it back to top
        if (hazards[i].position.y > GetScreenHeight() + hazards[i].radius) {
            ResetHazard(i, currentSpeedScale);
        }
    }

    // 3. SPATIAL MEMORY DECAY
    // Pixel width of each column: 800 / 16 = 50.0 pixels
    float colWidth = (float)GetScreenWidth() / (float)GRID_COLS;
    for (int i = 0; i < GRID_COLS; i++) {
        if (riskHeatmap[i] > 0.0f) {
            // Decay risk by 2.0 units per second so older damage spots fade
            riskHeatmap[i] -= 2.0f * dt;
            if (riskHeatmap[i] < 0.0f) riskHeatmap[i] = 0.0f;
        }
    }

    // 4. COLUMN EVALUATION & TARGET SELECTION
    // High initial sentinel value to find the minimum risk score
    float bestScore = 999999.0f;
    int bestCol = agent.targetCol;

    for (int col = 0; col < GRID_COLS; col++) {
        // Find horizontal center coordinate of the current column
        float colCenterX = (col * colWidth) + (colWidth / 2.0f);

        // Start score with learned historical risk from previous hits
        float candidateRisk = riskHeatmap[col];

        // Add edge penalty so the agent naturally avoids getting trapped in screen corners
        if (col == 0 || col == GRID_COLS - 1) {
            candidateRisk += 16.0f;
        }

        // Add incoming projectile threats based on kinematic time-to-impact
        for (int h = 0; h < activeHazardCount; h++) {
            if (!hazards[h].active) continue;

            // Horizontal distance between falling hazard and column center: |hazard.x - col.x|
            float distToColX = fabsf(hazards[h].position.x - colCenterX);

            // Check if ball's danger radius covers this column
            if (distToColX < hazards[h].avoidDangerRadius) {
                // Inverse time urgency: 1 / (t + 0.25). Smaller time = higher urgency score
                float timeUrgency = 1.0f / (hazards[h].timeToImpact + 0.25f);

                // Linear proximity factor: 1.0 at center, dropping to 0.0 at outer edge
                float proximity = 1.0f - (distToColX / hazards[h].avoidDangerRadius);

                // Add combined threat score to this column
                candidateRisk += proximity * timeUrgency * 14.0f;
            }
        }

        // Travel penalty: penalize distant columns so agent does not dash across the screen unnecessarily
        float travelDist = fabsf(colCenterX - agent.position.x);
        candidateRisk += (travelDist / (float)GetScreenWidth()) * 9.0f;

        // Track column with lowest calculated threat
        if (candidateRisk < bestScore) {
            bestScore = candidateRisk;
            bestCol = col;
        }
    }

    // Set destination column
    agent.targetCol = bestCol;

    // 5. SEEK SAFEST COLUMN (Attractive Steering Force)
    float targetX = (agent.targetCol * colWidth) + (colWidth / 2.0f);
    float toTargetX = targetX - agent.position.x;
    float seekX = 0.0f;

    // Deadzone check: steer left or right only if further than 4px from column center
    if (fabsf(toTargetX) > 4.0f) {
        seekX = (toTargetX > 0) ? 0.65f : -0.65f;
    }

    // 6. ARTIFICIAL POTENTIAL FIELD (Repulsive Avoidance Force)
    float totalAvoidX = 0.0f;
    float totalAvoidY = 0.0f;

    for (int i = 0; i < activeHazardCount; i++) {
        if (!hazards[i].active) continue;

        // Vector pointing from obstacle to agent: Agent - Obstacle
        float dx = agent.position.x - hazards[i].position.x;
        float dy = agent.position.y - hazards[i].position.y;

        // Euclidean distance formula: sqrt(dx^2 + dy^2)
        float distToObs = sqrtf(dx * dx + dy * dy);

        // Check if agent is inside the repulsive zone
        if (distToObs < hazards[i].avoidDangerRadius && distToObs > 0.001f) {
            // Panic factor: 0.0 at outer perimeter, scaling to 1.0 at physical center
            float panic = (hazards[i].avoidDangerRadius - distToObs) / hazards[i].avoidDangerRadius;

            // Urgency scaling: prioritize obstacles with lower time-to-impact
            float urgencyWeight = 1.0f / (hazards[i].timeToImpact + 0.3f);
            if (urgencyWeight > 3.0f) urgencyWeight = 3.0f;

            // Unit repulsion vector (dx/dist, dy/dist) scaled by panic and urgency weights
            totalAvoidX += (dx / distToObs) * (panic * 3.5f * urgencyWeight);
            totalAvoidY += (dy / distToObs) * (panic * 1.5f);
        }

        // 7. COLLISION CHECK & LEARNING REINFORCEMENT
        // Circle-circle overlap formula: Collision occurs if distance < (r1 + r2)
        float hitDistance = hazards[i].radius + agent.radius;

        if (distToObs < hitDistance && agent.invulnerableTimer <= 0.0f) {
            agent.lives--;
            agent.invulnerableTimer = 1.5f; // Grant 1.5s grace period to prevent instant death

            // Determine which column index agent was in when hit
            int hitCol = (int)(agent.position.x / colWidth);
            if (hitCol >= 0 && hitCol < GRID_COLS) {
                // Add penalty to heatmap so AI remembers this spot led to damage
                riskHeatmap[hitCol] += 55.0f;
            }

            // Also penalize adjacent columns to encourage moving further away
            if (hitCol > 0) riskHeatmap[hitCol - 1] += 22.0f;
            if (hitCol < GRID_COLS - 1) riskHeatmap[hitCol + 1] += 22.0f;

            if (agent.lives <= 0) {
                gameState = STATE_GAME_OVER;
            }
        }
    }

    // 8. FORCE BLENDING & VECTOR NORMALIZATION
    // Sum attractive seeking vector with all repulsive vectors
    float finalDirX = seekX + totalAvoidX;
    float finalDirY = totalAvoidY; // Vertical movement is driven purely by obstacle repulsion

    // Calculate length of the combined direction vector: sqrt(x^2 + y^2)
    float finalLength = sqrtf(finalDirX * finalDirX + finalDirY * finalDirY);

    // Normalize (divide by length) to preserve constant movement speed regardless of force counts
    if (finalLength > 0.01f) {
        agent.position.x += (finalDirX / finalLength) * agent.speed;
        agent.position.y += (finalDirY / finalLength) * agent.speed;
    }

    // 9. ARENA BOUNDARY CLAMPING
    // Clamp X within screen bounds minus agent radius to avoid leaving view
    if (agent.position.x < agent.radius) agent.position.x = agent.radius;
    if (agent.position.x > GetScreenWidth() - agent.radius) agent.position.x = GetScreenWidth() - agent.radius;

    // Clamp Y strictly within the bottom play zone corridor
    if (agent.position.y < PLAY_ZONE_TOP) agent.position.y = PLAY_ZONE_TOP;
    if (agent.position.y > PLAY_ZONE_BOTTOM) agent.position.y = PLAY_ZONE_BOTTOM;
}

// Executes once per frame to draw all visuals and telemetry
void DrawGameplayScreen(void) {
    // Clear canvas with light grey background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 245, 245, 245, 255 });

    // 1. TOP SPAWN ZONE CEILING
    DrawRectangle(0, 0, GetScreenWidth(), (int)ARENA_TOP_BAR_HEIGHT, (Color) { 45, 45, 48, 255 });
    DrawText("HAZARD SPAWN ZONE", 20, 20, 18, LIGHTGRAY);

    // 2. PLAY ZONE BOUNDARY LINE
    DrawLine(0, (int)PLAY_ZONE_TOP - 15, GetScreenWidth(), (int)PLAY_ZONE_TOP - 15, (Color) { 215, 215, 215, 255 });

    // 3. DRAW SPATIAL MEMORY HEATMAP ALONG THE FLOOR
    float colWidth = (float)GetScreenWidth() / (float)GRID_COLS;
    for (int i = 0; i < GRID_COLS; i++) {
        // Normalize risk score to 0.0 - 1.0 range (50.0 is base hit penalty)
        float normalizedRisk = riskHeatmap[i] / 50.0f;
        if (normalizedRisk > 1.0f) normalizedRisk = 1.0f;

        // Interpolate color: Green when safe (low risk), turning Red as risk increases
        Color zoneColor = (Color){
            (unsigned char)(normalizedRisk * 220),
            (unsigned char)((1.0f - normalizedRisk) * 180),
            70,
            (unsigned char)(40 + normalizedRisk * 120) // Alpha increases with risk
        };

        // Draw individual column rectangle along the floor
        DrawRectangle((int)(i * colWidth), (int)PLAY_ZONE_BOTTOM + 10, (int)colWidth - 2, 14, zoneColor);
    }
    DrawText("AI MEMORY HEATMAP", 20, (int)PLAY_ZONE_BOTTOM + 12, 10, DARKGRAY);

    // 4. DRAW TRAJECTORY RAYS, IMPACT RETICLES & HAZARDS
    for (int i = 0; i < activeHazardCount; i++) {
        if (!hazards[i].active) continue;

        // A. Draw projected vertical trajectory line from ball straight down to FLOOR_Y
        DrawLine((int)hazards[i].position.x, (int)hazards[i].position.y,
            (int)hazards[i].position.x, (int)FLOOR_Y, (Color) { 180, 180, 190, 80 });

        // B. Draw impact reticle ring on floor; radius scales with obstacle size
        float reticleRadius = 4.0f + (hazards[i].radius * 0.4f);

        // Alpha brightens as timeToImpact approaches zero
        unsigned char reticleAlpha = (unsigned char)Clamp(255.0f - (hazards[i].timeToImpact * 180.0f), 40.0f, 220.0f);
        DrawCircleLines((int)hazards[i].position.x, (int)FLOOR_Y, reticleRadius, (Color) { 230, 80, 80, reticleAlpha });

        // C. Draw translucent repulsive danger circle around hazard
        DrawCircleV(hazards[i].position, hazards[i].avoidDangerRadius, (Color) { 255, 100, 100, 20 });
        DrawCircleLines((int)hazards[i].position.x, (int)hazards[i].position.y, hazards[i].avoidDangerRadius, (Color) { 230, 80, 80, 60 });

        // D. Draw solid physical obstacle
        DrawCircleV(hazards[i].position, hazards[i].radius, DARKGRAY);

        // E. Draw orange telemetry line when hazard is actively repelling the agent
        float dx = agent.position.x - hazards[i].position.x;
        float dy = agent.position.y - hazards[i].position.y;
        if (sqrtf(dx * dx + dy * dy) < hazards[i].avoidDangerRadius) {
            DrawLineV(agent.position, hazards[i].position, ORANGE);
        }
    }

    // 5. DRAW TARGET COLUMN INTENT LINE
    // Vertical line indicating the chosen column the AI is currently steering toward
    float targetX = (agent.targetCol * colWidth) + (colWidth / 2.0f);
    DrawLine((int)targetX, (int)PLAY_ZONE_TOP, (int)targetX, (int)PLAY_ZONE_BOTTOM, (Color) { 100, 200, 255, 70 });

    // 6. DRAW AGENT WITH INVULNERABILITY FLICKER
    bool shouldDrawAgent = true;

    // Alternate visibility every 0.1s during i-frames to create visual blink effect
    if (agent.invulnerableTimer > 0.0f) {
        if ((int)(agent.invulnerableTimer * 10.0f) % 2 == 0) {
            shouldDrawAgent = false;
        }
    }

    if (shouldDrawAgent) {
        // Red color while invulnerable; dark blue during normal state
        Color agentColor = (agent.invulnerableTimer > 0.0f) ? RED : DARKBLUE;
        DrawCircleV(agent.position, agent.radius, agentColor);
    }

    // 7. HUD METRICS
    // Draw 3 life indicators in top-right corner
    for (int i = 0; i < 3; i++) {
        if (i < agent.lives) {
            DrawCircle(GetScreenWidth() - 40 - (i * 30), 30, 10, RED);        // Filled circle = life remaining
        }
        else {
            DrawCircleLines(GetScreenWidth() - 40 - (i * 30), 30, 10, GRAY);  // Outline = life lost
        }
    }

    // Survival timer in seconds
    DrawText(TextFormat("SURVIVED: %.1fs", survivalTimer), 240, 22, 18, RAYWHITE);

    // Active hazard density metric
    DrawText(TextFormat("HAZARDS: %i/%i", activeHazardCount, MAX_HAZARDS), 430, 22, 18, (activeHazardCount == MAX_HAZARDS) ? RED : GOLD);

    // 8. GAME OVER OVERLAY
    if (gameState == STATE_GAME_OVER) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 0, 0, 0, 160 });
        DrawText("SIMULATION TERMINATED", GetScreenWidth() / 2 - 180, 170, 28, RED);
        DrawText(TextFormat("Final Survival Time: %.2f seconds", survivalTimer), GetScreenWidth() / 2 - 150, 215, 20, RAYWHITE);
        DrawText(TextFormat("Peak Hazard Density: %i Simultaneous Obstacles", activeHazardCount), GetScreenWidth() / 2 - 200, 245, 18, GOLD);
        DrawText("Press [R] to Retry (Retain Learned Memory)", GetScreenWidth() / 2 - 200, 285, 18, GREEN);
        DrawText("Press [C] to Reset Memory", GetScreenWidth() / 2 - 120, 315, 16, LIGHTGRAY);
    }
}

// Unloads assets from memory (not needed for procedural primitives)
void UnloadGameplayScreen(void) {}

// Controls screen routing inside the raylib template
int FinishGameplayScreen(void) {
    return 0;
}