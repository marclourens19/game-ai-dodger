#include "raylib.h"
#include "screens.h"
#include <stdlib.h>                     // Required for: exit()

// Module Variables Definition (local)
static int finishScreen = 0;            // 0 = stay, 1 = options, 2 = launch gameplay

// Interactive menu button bounding boxes (x, y, width, height)
static Rectangle btnAI;                 // Mode 0: Autonomous AI Dodger button
static Rectangle btnHuman;              // Mode 1: Human Mouse Benchmark button
static Rectangle btnChaos;              // Mode 2: Chaos Boulder Drop Sandbox button
static Rectangle btnExit;               // Exit application button

// Boolean flags tracking whether mouse cursor is inside each button
static bool hoverAI = false;
static bool hoverHuman = false;
static bool hoverChaos = false;
static bool hoverExit = false;

// Structure for decorative falling circles in the menu background
typedef struct {
    Vector2 position;                   // 2D center coordinates (x, y) in pixels
    float speedY;                       // Vertical fall speed in pixels per frame
    float radius;                       // Circle radius in pixels
} TitleHazard;

#define NUM_TITLE_HAZARDS 6             // Total decorative circles drifting in background
static TitleHazard bgHazards[NUM_TITLE_HAZARDS];

// Title Screen Functions Definition
void InitTitleScreen(void)
{
    finishScreen = 0;                   // Reset screen transition trigger

    float btnWidth = 260.0f;            // Fixed button width in pixels
    float btnHeight = 38.0f;            // Fixed button height in pixels
    // Calculate horizontal center X: (800 - 260) / 2 = 270.0f
    float centerX = ((float)GetScreenWidth() - btnWidth) / 2.0f;

    // Stack buttons vertically with a 48px stride (38px height + 10px padding)
    btnAI = (Rectangle){ centerX, 195.0f, btnWidth, btnHeight };
    btnHuman = (Rectangle){ centerX, 243.0f, btnWidth, btnHeight };
    btnChaos = (Rectangle){ centerX, 291.0f, btnWidth, btnHeight };
    btnExit = (Rectangle){ centerX, 339.0f, btnWidth, btnHeight };

    // Initialize ambient background hazards with random sizes, positions, and speeds
    for (int i = 0; i < NUM_TITLE_HAZARDS; i++)
    {
        bgHazards[i].radius = (float)GetRandomValue(15, 35);                        // Random radius [15..35] px
        bgHazards[i].position.x = (float)GetRandomValue(40, GetScreenWidth() - 40); // Keep 40px away from side edges
        bgHazards[i].position.y = (float)GetRandomValue(-200, GetScreenHeight());   // Scatter vertically across screen
        // Convert random speed from pixels-per-second [40..90] to pixels-per-frame by dividing by 60 FPS
        bgHazards[i].speedY = (float)GetRandomValue(40, 90) / 60.0f;
    }
}

void UpdateTitleScreen(void)
{
    Vector2 mousePos = GetMousePosition();  // Query current (x, y) cursor coordinates

    // Perform AABB point-in-rectangle collision checks for each button
    // Math: mousePos.x >= rec.x && mousePos.x <= (rec.x + rec.width) && mousePos.y >= rec.y && mousePos.y <= (rec.y + rec.height)
    hoverAI = CheckCollisionPointRec(mousePos, btnAI);
    hoverHuman = CheckCollisionPointRec(mousePos, btnHuman);
    hoverChaos = CheckCollisionPointRec(mousePos, btnChaos);
    hoverExit = CheckCollisionPointRec(mousePos, btnExit);

    // Change OS cursor icon to a pointing hand if hovering over any interactive button
    if (hoverAI || hoverHuman || hoverChaos || hoverExit)
    {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    }
    else
    {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    // Handle left-click selection on Mode 0: Autonomous AI
    if (hoverAI && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);   // Restore default cursor before leaving screen
        SetGameplayMode(0);                     // Configure gameplay module for AI Simulation
        PlaySound(fxCoin);                      // Play confirmation audio
        finishScreen = 2;                       // Signal transition to GAMEPLAY
    }
    // Handle left-click selection on Mode 1: Human Benchmark
    if (hoverHuman && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        SetGameplayMode(1);                     // Configure gameplay module for Mouse Control
        PlaySound(fxCoin);
        finishScreen = 2;
    }
    // Handle left-click selection on Mode 2: Chaos Sandbox
    if (hoverChaos && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        SetGameplayMode(2);                     // Configure gameplay module for Chaos Sandbox
        PlaySound(fxCoin);
        finishScreen = 2;
    }
    // Handle left-click on Exit button
    if (hoverExit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        CloseAudioDevice();                     // Cleanly close audio before terminating process
        CloseWindow();                          // Cleanly close window
        exit(0);                                // Terminate application immediately
    }

    // Keyboard shortcut: Press [ENTER] to quick-launch Mode 0 (Autonomous AI)
    if (IsKeyPressed(KEY_ENTER))
    {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        SetGameplayMode(0);
        PlaySound(fxCoin);
        finishScreen = 2;
    }

    // Update kinematics for decorative background hazards
    for (int i = 0; i < NUM_TITLE_HAZARDS; i++)
    {
        // Euler integration step: y_new = y_old + velocity_y
        bgHazards[i].position.y += bgHazards[i].speedY;

        // If the top edge of the circle (y - radius) passes below the bottom of the screen
        if (bgHazards[i].position.y > GetScreenHeight() + bgHazards[i].radius)
        {
            // Wrap hazard back above the top edge of the screen (-radius - 20px buffer)
            bgHazards[i].position.y = -bgHazards[i].radius - 20.0f;
            // Assign a new random horizontal lane
            bgHazards[i].position.x = (float)GetRandomValue(40, GetScreenWidth() - 40);
        }
    }
}

void DrawTitleScreen(void)
{
    // Fill background with soft cool-grey tone (R:245, G:245, B:248, A:255)
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 245, 245, 248, 255 });

    // Render drifting ambient background hazards
    for (int i = 0; i < NUM_TITLE_HAZARDS; i++)
    {
        // Outer faint red aura representing potential field danger radius (radius + 20px)
        DrawCircleV(bgHazards[i].position, bgHazards[i].radius + 20.0f, (Color) { 255, 120, 120, 15 });
        // Inner light-grey solid core
        DrawCircleV(bgHazards[i].position, bgHazards[i].radius, (Color) { 225, 225, 230, 255 });
    }

    // Render centered Title and Subtitle strings
    const char* title = "AUTONOMOUS DODGER AI";
    const char* subtitle = "Kinematic Trajectory & Spatial Learning System";
    int titleW = MeasureText(title, 32);        // Calculate pixel width of title at font size 32
    int subW = MeasureText(subtitle, 15);       // Calculate pixel width of subtitle at font size 15

    // Center text horizontally using: (ScreenWidth - TextWidth) / 2
    DrawText(title, (GetScreenWidth() - titleW) / 2, 70, 32, (Color) { 35, 35, 42, 255 });
    DrawText(subtitle, (GetScreenWidth() - subW) / 2, 112, 15, (Color) { 110, 110, 120, 255 });

    // Draw a 340px wide horizontal divider line centered at Y = 142
    // StartX = (800 - 340)/2 = 230, EndX = (800 + 340)/2 = 570
    DrawLine((GetScreenWidth() - 340) / 2, 142, (GetScreenWidth() + 340) / 2, 142, (Color) { 210, 210, 218, 255 });

    // Render Button 1: AI Mode (Highlights blue on hover, dark slate otherwise)
    Color colAI = hoverAI ? (Color) { 30, 110, 210, 255 } : (Color) { 42, 45, 52, 255 };
    DrawRectangleRounded(btnAI, 0.25f, 6, colAI);
    DrawRectangleRoundedLines(btnAI, 0.25f, 6, (Color) { 25, 25, 30, 255 });
    int wAI = MeasureText("AI DODGER (SIMULATION)", 15);
    // Center label inside button: btn.x + (btn.width - textWidth) / 2
    DrawText("AI DODGER (SIMULATION)", (int)(btnAI.x + (btnAI.width - wAI) / 2), (int)(btnAI.y + 11), 15, RAYWHITE);

    // Render Button 2: Human Mode (Highlights green on hover)
    Color colHuman = hoverHuman ? (Color) { 0, 150, 90, 255 } : (Color) { 42, 45, 52, 255 };
    DrawRectangleRounded(btnHuman, 0.25f, 6, colHuman);
    DrawRectangleRoundedLines(btnHuman, 0.25f, 6, (Color) { 25, 25, 30, 255 });
    int wHuman = MeasureText("HUMAN BENCHMARK (MOUSE)", 15);
    DrawText("HUMAN BENCHMARK (MOUSE)", (int)(btnHuman.x + (btnHuman.width - wHuman) / 2), (int)(btnHuman.y + 11), 15, RAYWHITE);

    // Render Button 3: Chaos Mode (Highlights orange-red on hover)
    Color colChaos = hoverChaos ? (Color) { 210, 80, 50, 255 } : (Color) { 42, 45, 52, 255 };
    DrawRectangleRounded(btnChaos, 0.25f, 6, colChaos);
    DrawRectangleRoundedLines(btnChaos, 0.25f, 6, (Color) { 25, 25, 30, 255 });
    int wChaos = MeasureText("CHAOS DROP (SANDBOX)", 15);
    DrawText("CHAOS DROP (SANDBOX)", (int)(btnChaos.x + (btnChaos.width - wChaos) / 2), (int)(btnChaos.y + 11), 15, RAYWHITE);

    // Render Button 4: Exit (Highlights crimson on hover, light grey default)
    Color colExit = hoverExit ? (Color) { 190, 45, 45, 255 } : (Color) { 225, 225, 230, 255 };
    DrawRectangleRounded(btnExit, 0.25f, 6, colExit);
    DrawRectangleRoundedLines(btnExit, 0.25f, 6, (Color) { 180, 180, 185, 255 });
    int wExit = MeasureText("EXIT", 15);
    Color txtExit = hoverExit ? RAYWHITE : (Color) { 60, 60, 65, 255 };
    DrawText("EXIT", (int)(btnExit.x + (btnExit.width - wExit) / 2), (int)(btnExit.y + 11), 15, txtExit);

    // Render centered footer instruction text at Y = 400
    const char* footer = "Select a mode or press [ENTER] to launch AI Dodger";
    int footW = MeasureText(footer, 12);
    DrawText(footer, (GetScreenWidth() - footW) / 2, 400, 12, (Color) { 140, 140, 150, 255 });
}

void UnloadTitleScreen(void)
{
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);       // Ensure cursor is reset when unloading
}

int FinishTitleScreen(void)
{
    return finishScreen;                        // Return transition code (0, 1, or 2)
}