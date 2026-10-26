#include "raylib.h"
#include "screens.h"

static int framesCounter = 0;           // Frame counter
static int finishScreen = 0;            // 1 = return to Title screen

void InitEndingScreen(void)
{
    framesCounter = 0;                  // Reset frame counter
    finishScreen = 0;                   // Reset transition flag
}

void UpdateEndingScreen(void)
{
    framesCounter++;                    // Advance frame counter

    // Press Enter or Tap screen to transition back to Title screen
    if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
    {
        finishScreen = 1;
        PlaySound(fxCoin);
    }
}

void DrawEndingScreen(void)
{
    // Fill full screen with solid blue background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), BLUE);

    // Draw header using global custom font scaled by 3.0x
    Vector2 pos = { 20.0f, 10.0f };
    DrawTextEx(font, "ENDING SCREEN", pos, font.baseSize * 3.0f, 4, DARKBLUE);

    // Draw return prompt
    DrawText("PRESS ENTER or TAP to RETURN to TITLE SCREEN", 120, 220, 20, DARKBLUE);
}

void UnloadEndingScreen(void) {}

int FinishEndingScreen(void)
{
    return finishScreen;
}