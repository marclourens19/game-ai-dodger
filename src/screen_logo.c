/**********************************************************************************************
*   raylib - Advance Game template
*   Logo Screen Functions Definitions (Init, Update, Draw, Unload)
**********************************************************************************************/

#include "raylib.h"
#include "screens.h"

// Module Variables Definition (local)
static int framesCounter = 0;           // Counts elapsed frames for timing animation stages
static int finishScreen = 0;            // Flag signaling main loop to transition to TITLE

static int logoPositionX = 0;           // Top-left X coordinate of the 256x256 raylib logo box
static int logoPositionY = 0;           // Top-left Y coordinate of the 256x256 raylib logo box

static int lettersCount = 0;            // Number of characters of "raylib" currently visible
static int topSideRecWidth = 0;         // Current animated width of top border bar
static int leftSideRecHeight = 0;       // Current animated height of left border bar
static int bottomSideRecWidth = 0;      // Current animated width of bottom border bar
static int rightSideRecHeight = 0;      // Current animated height of right border bar

static int state = 0;                   // Finite State Machine (FSM) tracker (States 0 to 3)
static float alpha = 1.0f;              // Opacity multiplier for final logo fade-out [0.0f .. 1.0f]

// Logo Screen Functions Definition
void InitLogoScreen(void)
{
    finishScreen = 0;                   // Reset completion flag
    framesCounter = 0;                  // Reset frame timer
    lettersCount = 0;                   // Start with 0 visible letters

    // Center the 256x256 logo square on screen:
    // CenterX = ScreenWidth / 2; Subtract half of logo width (256 / 2 = 128)
    logoPositionX = GetScreenWidth() / 2 - 128;
    // CenterY = ScreenHeight / 2; Subtract half of logo height (256 / 2 = 128)
    logoPositionY = GetScreenHeight() / 2 - 128;

    // Initialize all four border bars to a 16x16 pixel corner square
    topSideRecWidth = 16;
    leftSideRecHeight = 16;
    bottomSideRecWidth = 16;
    rightSideRecHeight = 16;

    state = 0;                          // Begin at FSM State 0 (blinking corner)
    alpha = 1.0f;                       // Start at 100% opacity
}

void UpdateLogoScreen(void)
{
    // Allow user to skip the intro animation by pressing Enter or Space
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
    {
        finishScreen = 1;
        return;
    }

    if (state == 0)                     // State 0: Blink top-left 16x16 square for 80 frames (~1.33s)
    {
        framesCounter++;                // Advance frame counter by 1

        if (framesCounter == 80)        // After 80 frames (80 / 60 = 1.33s)
        {
            state = 1;                  // Advance to State 1
            framesCounter = 0;          // Reset counter for future use in State 3
        }
    }
    else if (state == 1)                // State 1: Expand top bar rightwards and left bar downwards
    {
        topSideRecWidth += 8;           // Grow width by 8 pixels per frame
        leftSideRecHeight += 8;         // Grow height by 8 pixels per frame

        // Starting at 16px, growing +8px/frame reaches 256px in (256 - 16) / 8 = 30 frames (0.5s)
        if (topSideRecWidth == 256) state = 2;
    }
    else if (state == 2)                // State 2: Expand bottom bar rightwards and right bar downwards
    {
        bottomSideRecWidth += 8;        // Grow bottom width by 8 pixels per frame
        rightSideRecHeight += 8;        // Grow right height by 8 pixels per frame

        // Reaches full 256px box dimensions after another 30 frames (0.5s)
        if (bottomSideRecWidth == 256) state = 3;
    }
    else if (state == 3)                // State 3: Type out "raylib" text, then fade out
    {
        framesCounter++;                // Increment frame timer

        if (lettersCount < 10)          // Until letter counter reaches 10
        {
            // Integer division: framesCounter / 12 evaluates to 1 (true) every 12th frame (0.2s)
            if (framesCounter / 12)
            {
                lettersCount++;         // Reveal one more character of "raylib"
                framesCounter = 0;      // Reset timer for next character
            }
        }
        else                            // Once all letters are revealed
        {
            if (framesCounter > 200)    // Hold completed logo before fading
            {
                alpha -= 0.02f;         // Subtract 2% opacity per frame (50 frames = 0.83s fade)

                if (alpha <= 0.0f)      // Once completely invisible
                {
                    alpha = 0.0f;       // Clamp to 0.0f
                    finishScreen = 1;   // Trigger screen transition to TITLE
                }
            }
        }
    }
}

void DrawLogoScreen(void)
{
    if (state == 0)                     // State 0: Render blinking 16x16 square
    {
        // (framesCounter / 10) % 2 alternates between 0 (off) and 1 (on) every 10 frames
        if ((framesCounter / 10) % 2) DrawRectangle(logoPositionX, logoPositionY, 16, 16, BLACK);
    }
    else if (state == 1)                // State 1: Render growing top and left bars
    {
        DrawRectangle(logoPositionX, logoPositionY, topSideRecWidth, 16, BLACK);
        DrawRectangle(logoPositionX, logoPositionY, 16, leftSideRecHeight, BLACK);
    }
    else if (state == 2)                // State 2: Render all four growing border bars
    {
        DrawRectangle(logoPositionX, logoPositionY, topSideRecWidth, 16, BLACK);
        DrawRectangle(logoPositionX, logoPositionY, 16, leftSideRecHeight, BLACK);
        // Offset right bar by +240px (256 total width - 16 bar thickness = 240)
        DrawRectangle(logoPositionX + 240, logoPositionY, 16, rightSideRecHeight, BLACK);
        // Offset bottom bar by +240px (256 total height - 16 bar thickness = 240)
        DrawRectangle(logoPositionX, logoPositionY + 240, bottomSideRecWidth, 16, BLACK);
    }
    else if (state == 3)                // State 3: Render completed box, text, and fade effect
    {
        DrawRectangle(logoPositionX, logoPositionY, topSideRecWidth, 16, Fade(BLACK, alpha));
        // Offset Y by +16 and reduce height by 32 (16 top + 16 bottom) to prevent corner overlap when fading
        DrawRectangle(logoPositionX, logoPositionY + 16, 16, leftSideRecHeight - 32, Fade(BLACK, alpha));
        DrawRectangle(logoPositionX + 240, logoPositionY + 16, 16, rightSideRecHeight - 32, Fade(BLACK, alpha));
        DrawRectangle(logoPositionX, logoPositionY + 240, bottomSideRecWidth, 16, Fade(BLACK, alpha));

        // Draw inner white canvas square: 256 - 32 (border thickness*2) = 224x224, offset by 112 from center
        DrawRectangle(GetScreenWidth() / 2 - 112, GetScreenHeight() / 2 - 112, 224, 224, Fade(RAYWHITE, alpha));

        // Draw substring of "raylib" from index 0 up to lettersCount in bottom-right corner of box
        DrawText(TextSubtext("raylib", 0, lettersCount), GetScreenWidth() / 2 - 44, GetScreenHeight() / 2 + 48, 50, Fade(BLACK, alpha));

        // Draw "powered by" label 27 pixels above the top-left corner of the logo box
        if (framesCounter > 20) DrawText("powered by", logoPositionX, logoPositionY - 27, 20, Fade(DARKGRAY, alpha));
    }
}

void UnloadLogoScreen(void)
{
    // No dynamic heap or GPU allocations to free on Logo screen
}

int FinishLogoScreen(void)
{
    return finishScreen;                // Return completion status to main loop
}