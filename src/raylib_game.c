/*******************************************************************************************
*   raylib game template
*   Code licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*   Copyright (c) 2021-2026 Ramon Santamaria (@raysan5)
********************************************************************************************/

#include "raylib.h"
#include "screens.h"    // Declares shared extern variables and screen lifecycle functions

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>      // Emscripten header for web browser main loop
#endif

#include <stdio.h>                          // Required for: printf()

// Defines and Macros
#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)
#define LOG(...) printf(__VA_ARGS__)    // Variadic macro forwarding logs to stdout
#else
#define LOG(...)                        // Compiles out log calls when disabled
#endif

// Shared Variables Definition (global)
GameScreen currentScreen = LOGO;            // Active screen state (starts on LOGO)
Font font = { 0 };                          // Global font structure zero-initialized
Music music = { 0 };                        // Global music stream zero-initialized
Sound fxCoin = { 0 };                       // Global sound effect zero-initialized

// Global Variables Definition (local to this module)
static const int screenWidth = 800;         // Fixed horizontal window resolution in pixels
static const int screenHeight = 450;        // Fixed vertical window resolution in pixels

// Variables managing full-screen fade transitions between screens
static float transAlpha = 0.0f;             // Opacity of black overlay (0.0 = transparent, 1.0 = opaque)
static bool onTransition = false;           // Flag indicating if a screen transition is active
static bool transFadeOut = false;           // False = fading to black (in), True = fading from black (out)
static int transFromScreen = -1;            // Stores the screen ID being unloaded
static GameScreen transToScreen = UNKNOWN;  // Stores the target screen ID being loaded

// Module Functions Declaration
static void ChangeToScreen(int screen);     // Immediate screen switch without fade animation
static void TransitionToScreen(int screen); // Starts a smooth fade transition to target screen
static void UpdateTransition(void);         // Steps fade alpha math each frame
static void DrawTransition(void);           // Renders full-screen black overlay with current alpha
static void UpdateDrawFrame(void);          // Runs one complete game tick (Update + Draw)

// Program main entry point
int main(void)
{
    // Initialize OpenGL window context with specified dimensions and title
    InitWindow(screenWidth, screenHeight, "Autonomous Dodger AI - Raylib");

    // Initialize OpenAL audio device context
    InitAudioDevice();

    // Load global assets shared across screens
    font = LoadFont("resources/mecha.png");         // Load custom sprite font
    fxCoin = LoadSound("resources/coin.wav");       // Load feedback sound effect

    // Start on the Logo screen and initialize its state variables
    currentScreen = LOGO;
    InitLogoScreen();

#if defined(PLATFORM_WEB)
    // Web builds delegate loop timing to the browser's requestAnimationFrame at 60 FPS
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    // Lock desktop frame rate to 60 FPS (target frame time ~0.01667 seconds)
    SetTargetFPS(60);

    // Main game loop: runs until user clicks the window X or presses the exit key
    while (!WindowShouldClose())
    {
        UpdateDrawFrame();                          // Process logic and render frame
    }
#endif

    // De-Initialization: Unload whichever screen was active when the loop ended
    switch (currentScreen)
    {
    case LOGO: UnloadLogoScreen(); break;
    case TITLE: UnloadTitleScreen(); break;
    case OPTIONS: UnloadOptionsScreen(); break;
    case GAMEPLAY: UnloadGameplayScreen(); break;
    case ENDING: UnloadEndingScreen(); break;
    default: break;
    }

    // Free global assets from memory/VRAM
    UnloadFont(font);
    if (music.stream.buffer != NULL) UnloadMusicStream(music); // Guarded unload
    UnloadSound(fxCoin);

    CloseAudioDevice();     // Shut down audio subsystem
    CloseWindow();          // Close window and release OpenGL context

    return 0;               // Exit program with success code 0
}

// Module Functions Definition

// Immediately switches screens without a fade effect
static void ChangeToScreen(int screen)
{
    // Unload resources of the outgoing screen
    switch (currentScreen)
    {
    case LOGO: UnloadLogoScreen(); break;
    case TITLE: UnloadTitleScreen(); break;
    case OPTIONS: UnloadOptionsScreen(); break;
    case GAMEPLAY: UnloadGameplayScreen(); break;
    case ENDING: UnloadEndingScreen(); break;
    default: break;
    }

    // Initialize state and resources of the incoming screen
    switch (screen)
    {
    case LOGO: InitLogoScreen(); break;
    case TITLE: InitTitleScreen(); break;
    case OPTIONS: InitOptionsScreen(); break;
    case GAMEPLAY: InitGameplayScreen(); break;
    case ENDING: InitEndingScreen(); break;
    default: break;
    }

    currentScreen = (GameScreen)screen; // Update active screen tracker
}

// Configures state variables to begin a fade-to-black transition
static void TransitionToScreen(int screen)
{
    onTransition = true;                // Lock normal screen updates during fade
    transFadeOut = false;               // Start with Phase 1: Fade In black overlay
    transFromScreen = currentScreen;    // Record current screen to unload at peak darkness
    transToScreen = (GameScreen)screen; // Record destination screen to initialize
    transAlpha = 0.0f;                  // Start overlay completely transparent (0% opacity)
}

// Updates transition alpha value using linear interpolation steps per frame
static void UpdateTransition(void)
{
    if (!transFadeOut)                  // Phase 1: Fading overlay IN (screen darkens to black)
    {
        // Increment alpha by 0.05 per frame -> takes 20 frames (20 / 60 = 0.33 seconds) to reach 1.0
        transAlpha += 0.05f;

        // Check against 1.01f to avoid IEEE-754 floating-point precision rounding issues at 1.0f
        if (transAlpha > 1.01f)
        {
            transAlpha = 1.0f;          // Clamp alpha strictly to 1.0 (fully opaque)

            // Unload outgoing screen while screen is completely black
            switch (transFromScreen)
            {
            case LOGO: UnloadLogoScreen(); break;
            case TITLE: UnloadTitleScreen(); break;
            case OPTIONS: UnloadOptionsScreen(); break;
            case GAMEPLAY: UnloadGameplayScreen(); break;
            case ENDING: UnloadEndingScreen(); break;
            default: break;
            }

            // Initialize incoming screen while screen is still black
            switch (transToScreen)
            {
            case LOGO: InitLogoScreen(); break;
            case TITLE: InitTitleScreen(); break;
            case OPTIONS: InitOptionsScreen(); break;
            case GAMEPLAY: InitGameplayScreen(); break;
            case ENDING: InitEndingScreen(); break;
            default: break;
            }

            currentScreen = transToScreen;  // Switch active screen pointer
            transFadeOut = true;            // Switch to Phase 2: Fade Out black overlay
        }
    }
    else                                // Phase 2: Fading overlay OUT (new screen reveals)
    {
        // Decrement alpha by 0.02 per frame -> takes 50 frames (50 / 60 = 0.83 seconds) to clear
        transAlpha -= 0.02f;

        // Check against -0.01f to ensure alpha reaches 0.0f cleanly despite float inaccuracy
        if (transAlpha < -0.01f)
        {
            transAlpha = 0.0f;          // Clamp alpha to 0.0 (completely transparent)
            transFadeOut = false;       // Reset phase flag
            onTransition = false;       // Unlock normal screen updates
            transFromScreen = -1;       // Clear source screen tracker
            transToScreen = UNKNOWN;    // Clear target screen tracker
        }
    }
}

// Draws a full-window black rectangle with transparency controlled by transAlpha
static void DrawTransition(void)
{
    // Fade(BLACK, transAlpha) multiplies BLACK's alpha (255) by transAlpha [0.0f .. 1.0f]
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, transAlpha));
}

// Master per-frame update and render dispatcher
static void UpdateDrawFrame(void)
{
    // Update Step
    
    if (music.stream.buffer != NULL) UpdateMusicStream(music); // Refill audio buffer if loaded

    if (!onTransition)                  // Only update active screen if not mid-transition
    {
        switch (currentScreen)
        {
        case LOGO:
        {
            UpdateLogoScreen();
            if (FinishLogoScreen()) TransitionToScreen(TITLE);
        } break;
        case TITLE:
        {
            UpdateTitleScreen();
            if (FinishTitleScreen() == 1) TransitionToScreen(OPTIONS);
            else if (FinishTitleScreen() == 2) TransitionToScreen(GAMEPLAY);
        } break;
        case OPTIONS:
        {
            UpdateOptionsScreen();
            if (FinishOptionsScreen()) TransitionToScreen(TITLE);
        } break;
        case GAMEPLAY:
        {
            UpdateGameplayScreen();
            if (FinishGameplayScreen() == 1) TransitionToScreen(TITLE);
        } break;
        case ENDING:
        {
            UpdateEndingScreen();
            if (FinishEndingScreen() == 1) TransitionToScreen(TITLE);
        } break;
        default: break;
        }
    }
    else
    {
        UpdateTransition();             // Step fade alpha when transitioning
    }

    // Draw Step
    BeginDrawing();                     // Prepare framebuffer for rendering

    ClearBackground(RAYWHITE);      // Clear color buffer to off-white

    // Dispatch rendering to the active screen
    switch (currentScreen)
    {
    case LOGO: DrawLogoScreen(); break;
    case TITLE: DrawTitleScreen(); break;
    case OPTIONS: DrawOptionsScreen(); break;
    case GAMEPLAY: DrawGameplayScreen(); break;
    case ENDING: DrawEndingScreen(); break;
    default: break;
    }

    // Render transition veil on top of all screen graphics if active
    if (onTransition) DrawTransition();

    EndDrawing();                       // Swap buffers and present frame to display
}