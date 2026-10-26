#include "raylib.h"
#include <stdlib.h>

static int finishScreen = 0;

// Button dimensions and positions
static Rectangle btnPlay;
static Rectangle btnExit;

// Button hover flags
static bool hoverPlay = false;
static bool hoverExit = false;

// Decorative background hazard particles for visual polish
typedef struct {
    Vector2 position;
    float speedY;
    float radius;
} TitleHazard;

#define NUM_TITLE_HAZARDS 6
static TitleHazard bgHazards[NUM_TITLE_HAZARDS];

void InitTitleScreen(void) {
    finishScreen = 0;

    // Center button coordinates on the 800x450 canvas
    float btnWidth = 240.0f;
    float btnHeight = 46.0f;
    float centerX = ((float)GetScreenWidth() - btnWidth) / 2.0f;

    btnPlay = (Rectangle){ centerX, 230.0f, btnWidth, btnHeight };
    btnExit = (Rectangle){ centerX, 290.0f, btnWidth, btnHeight };

    // Initialize faint ambient hazards falling in the background
    for (int i = 0; i < NUM_TITLE_HAZARDS; i++) {
        bgHazards[i].radius = (float)GetRandomValue(15, 35);
        bgHazards[i].position.x = (float)GetRandomValue(40, GetScreenWidth() - 40);
        bgHazards[i].position.y = (float)GetRandomValue(-200, GetScreenHeight());
        bgHazards[i].speedY = (float)GetRandomValue(40, 90) / 60.0f;
    }
}

void UpdateTitleScreen(void) {
    Vector2 mousePos = GetMousePosition();

    // Check hover states
    hoverPlay = CheckCollisionPointRec(mousePos, btnPlay);
    hoverExit = CheckCollisionPointRec(mousePos, btnExit);

    // Update mouse cursor styling
    if (hoverPlay || hoverExit) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    }
    else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    // Handle button click events
    if (hoverPlay && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        finishScreen = 2; // In the raylib template, 2 transitions to GAMEPLAY
    }

    if (hoverExit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        exit(0); // Clean application exit
    }

    // Allow keyboard shortcut: Enter to start
    if (IsKeyPressed(KEY_ENTER)) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        finishScreen = 2;
    }

    // Update faint background hazard drift
    for (int i = 0; i < NUM_TITLE_HAZARDS; i++) {
        bgHazards[i].position.y += bgHazards[i].speedY;
        if (bgHazards[i].position.y > GetScreenHeight() + bgHazards[i].radius) {
            bgHazards[i].position.y = -bgHazards[i].radius - 20.0f;
            bgHazards[i].position.x = (float)GetRandomValue(40, GetScreenWidth() - 40);
        }
    }
}

void DrawTitleScreen(void) {
    // Canvas background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 245, 245, 245, 255 });

    // 1. AMBIENT BACKGROUND HAZARDS
    for (int i = 0; i < NUM_TITLE_HAZARDS; i++) {
        DrawCircleV(bgHazards[i].position, bgHazards[i].radius + 20.0f, (Color) { 255, 120, 120, 15 });
        DrawCircleV(bgHazards[i].position, bgHazards[i].radius, (Color) { 220, 220, 225, 255 });
    }

    // 2. HEADER AND TITLE
    const char* title = "AUTONOMOUS DODGER AI";
    const char* subtitle = "Vector Steering & Spatial Memory Simulation";

    int titleWidth = MeasureText(title, 34);
    int subtitleWidth = MeasureText(subtitle, 16);

    DrawText(title, (GetScreenWidth() - titleWidth) / 2, 100, 34, (Color) { 35, 35, 40, 255 });
    DrawText(subtitle, (GetScreenWidth() - subtitleWidth) / 2, 145, 16, (Color) { 120, 120, 130, 255 });

    // Decorative divider line under title
    DrawLine((GetScreenWidth() - 320) / 2, 175, (GetScreenWidth() + 320) / 2, 175, (Color) { 210, 210, 215, 255 });

    // 3. PLAY BUTTON
    Color playBg = hoverPlay ? (Color) { 30, 110, 210, 255 } : (Color) { 45, 45, 50, 255 };
    DrawRectangleRounded(btnPlay, 0.25f, 6, playBg);
    DrawRectangleRoundedLines(btnPlay, 0.25f, 6, (Color) { 20, 20, 25, 255 });

    const char* playText = "START SIMULATION";
    int playTextWidth = MeasureText(playText, 17);
    DrawText(playText, (int)(btnPlay.x + (btnPlay.width - playTextWidth) / 2), (int)(btnPlay.y + 15), 17, RAYWHITE);

    // 4. EXIT BUTTON
    Color exitBg = hoverExit ? (Color) { 210, 50, 50, 255 } : (Color) { 230, 230, 235, 255 };
    DrawRectangleRounded(btnExit, 0.25f, 6, exitBg);
    DrawRectangleRoundedLines(btnExit, 0.25f, 6, (Color) { 190, 190, 195, 255 });

    const char* exitText = "EXIT";
    int exitTextWidth = MeasureText(exitText, 17);
    Color exitTextColor = hoverExit ? RAYWHITE : (Color) { 60, 60, 65, 255 };
    DrawText(exitText, (int)(btnExit.x + (btnExit.width - exitTextWidth) / 2), (int)(btnExit.y + 15), 17, exitTextColor);

    // 5. FOOTER HINT
    const char* footer = "Press [ENTER] to Quick Start";
    int footerWidth = MeasureText(footer, 13);
    DrawText(footer, (GetScreenWidth() - footerWidth) / 2, 380, 13, (Color) { 160, 160, 170, 255 });
}

void UnloadTitleScreen(void) {}

int FinishTitleScreen(void) {
    return finishScreen;
}