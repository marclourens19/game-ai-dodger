#include "raylib.h"
#include "screens.h"

static int framesCounter = 0;           // Frame counter for UI animations
static int finishScreen = 0;            // 1 = return to Title screen

void InitOptionsScreen(void)
{
    framesCounter = 0;                  // Reset frame counter
    finishScreen = 0;                   // Reset transition flag
}

void UpdateOptionsScreen(void)
{
    framesCounter++;                    // Increment frame counter

    // Return to Title screen when user presses Enter or Escape
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || IsGestureDetected(GESTURE_TAP))
    {
        finishScreen = 1;
        PlaySound(fxCoin);
    }
}

void DrawOptionsScreen(void)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color) { 245, 245, 248, 255 });
    const char* msg = "OPTIONS SCREEN - PRESS [ENTER] TO RETURN";
    int w = MeasureText(msg, 20);
    // Center text horizontally: (ScreenWidth - TextWidth) / 2
    DrawText(msg, (GetScreenWidth() - w) / 2, GetScreenHeight() / 2 - 10, 20, DARKGRAY);
}

void UnloadOptionsScreen(void) {}

int FinishOptionsScreen(void)
{
    return finishScreen;
}