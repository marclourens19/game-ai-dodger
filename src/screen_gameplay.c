#include "raylib.h"
#include <math.h>

// AI Agent physical properties
static Vector2 agentPosition;
static float agentSpeed = 3.5f;   // Pixels moved per frame
static float agentRadius = 18.0f; // Visual size and collision boundary

// Obstacle data structure
typedef struct {
    Vector2 position;
    float radius;            // Physical solid radius (drawn dark grey)
    float avoidDangerRadius; // Reaction bubble (drawn faint red)
} Obstacle;

#define NUM_OBSTACLES 3
static Obstacle obstacles[NUM_OBSTACLES];

// Runs once when the gameplay scene loads into memory
void InitGameplayScreen(void) {
    agentPosition = (Vector2){ 100.0f, 225.0f };

    /*
     * Triangle formation with plenty of clearance from all edges:
     * - Obstacle 0 (Center): Pulsing obstacle
     * - Obstacle 1 (Top Right): Fixed smaller obstacle
     * - Obstacle 2 (Bottom Right): Fixed medium obstacle
     */
    obstacles[0] = (Obstacle){ { 380.0f, 225.0f }, 55.0f, 55.0f + agentRadius + 40.0f };
    obstacles[1] = (Obstacle){ { 600.0f, 130.0f }, 30.0f, 30.0f + agentRadius + 40.0f };
    obstacles[2] = (Obstacle){ { 580.0f, 330.0f }, 38.0f, 38.0f + agentRadius + 40.0f };
}

// Executes 60 times per second to update math, state, and coordinates
void UpdateGameplayScreen(void) {
    /*
     * sinf() returns a value between -1.0 and 1.0.
     * Multiplying by 25.0f oscillates the radius +/- 25px around 65.0f (40px to 90px).
     */
    
     // Oscillates smoothly between 40px and 70px (base 55px +/- 15px)
    float newRadius = 55.0f + sinf((float)GetTime() * 2.0f) * 12.0f;
    obstacles[0].radius = newRadius;
    obstacles[0].avoidDangerRadius = newRadius + agentRadius + 40.0f;

    Vector2 target = GetMousePosition();

    // Target - Agent gives the vector pointing from agent to mouse
    float toTargetX = target.x - agentPosition.x;
    float toTargetY = target.y - agentPosition.y;

    // Euclidean distance formula: sqrt(dx^2 + dy^2)
    float distToTarget = sqrtf(toTargetX * toTargetX + toTargetY * toTargetY);

    float seekX = 0.0f;
    float seekY = 0.0f;

    /*
     * Normalizing the vector (dividing by distance) gives it a length of 1.0.
     * The 5px threshold prevents jittering when reaching the cursor.
     */
    if (distToTarget > 5.0f) {
        seekX = toTargetX / distToTarget;
        seekY = toTargetY / distToTarget;
    }

    float totalAvoidX = 0.0f;
    float totalAvoidY = 0.0f;

    for (int i = 0; i < NUM_OBSTACLES; i++) {
        // Agent - Obstacle points away from the obstacle toward the agent
        float fromObsX = agentPosition.x - obstacles[i].position.x;
        float fromObsY = agentPosition.y - obstacles[i].position.y;
        float distToObs = sqrtf(fromObsX * fromObsX + fromObsY * fromObsY);

        /*
         * Linear falloff: panic is 0.0 at the outer edge, rising to 1.0 at the center.
         * The 2.5f multiplier gives avoidance priority over seeking.
         */
        if (distToObs < obstacles[i].avoidDangerRadius && distToObs > 0.001f) {
            float panic = (obstacles[i].avoidDangerRadius - distToObs) / obstacles[i].avoidDangerRadius;
            totalAvoidX += (fromObsX / distToObs) * (panic * 2.5f);
            totalAvoidY += (fromObsY / distToObs) * (panic * 2.5f);
        }
    }

    // Combine attractive and repulsive forces
    float finalDirX = seekX + totalAvoidX;
    float finalDirY = seekY + totalAvoidY;
    float finalLength = sqrtf(finalDirX * finalDirX + finalDirY * finalDirY);

    // Renormalize combined forces so movement speed remains steady
    if (finalLength > 0.01f && distToTarget > 5.0f) {
        agentPosition.x += (finalDirX / finalLength) * agentSpeed;
        agentPosition.y += (finalDirY / finalLength) * agentSpeed;
    }
}

// Executes immediately after Update to display the calculated states
void DrawGameplayScreen(void) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 245, 245, 245, 255 });

    Vector2 target = GetMousePosition();

    for (int i = 0; i < NUM_OBSTACLES; i++) {
        // Spatial sensor bubble
        DrawCircleV(obstacles[i].position, obstacles[i].avoidDangerRadius, (Color) { 255, 100, 100, 30 });
        DrawCircleLines((int)obstacles[i].position.x, (int)obstacles[i].position.y, obstacles[i].avoidDangerRadius, (Color) { 230, 80, 80, 120 });

        // Physical obstacle geometry
        DrawCircleV(obstacles[i].position, obstacles[i].radius, DARKGRAY);

        // Telemetry line indicating which obstacle is actively repelling the agent
        float dx = agentPosition.x - obstacles[i].position.x;
        float dy = agentPosition.y - obstacles[i].position.y;
        if (sqrtf(dx * dx + dy * dy) < obstacles[i].avoidDangerRadius) {
            DrawLineV(agentPosition, obstacles[i].position, ORANGE);
        }
    }

    // Target and agent geometry
    DrawCircleV(target, 6.0f, RED);
    DrawCircleV(agentPosition, agentRadius, DARKBLUE);

    // Intent line pointing toward target
    DrawLineV(agentPosition, target, LIGHTGRAY);

    // On-screen telemetry metrics
    DrawText("Obstacle Avoidance Active (Array & Loop)", 20, 20, 20, DARKGRAY);
    DrawText("Move the mouse past obstacles to see multi-force steering.", 20, 50, 16, GRAY);
}

void UnloadGameplayScreen(void) {}

int FinishGameplayScreen(void) {
    return 0; // 0 maintains active loop inside the raylib template router
}