#ifndef SCREENS_H
#define SCREENS_H

// Types and Structures Definition
// Enum representing all possible application screens
typedef enum GameScreen {
    UNKNOWN = -1,   // Uninitialized or invalid screen state
    LOGO = 0,       // Intro raylib logo animation screen
    TITLE,          // Main menu and mode selection screen
    OPTIONS,        // Options/settings screen
    GAMEPLAY,       // Core AI simulation and dodging arena
    ENDING          // Post-game / credits screen
} GameScreen;

// Global Variables Declaration (shared across modules)
extern GameScreen currentScreen;    // Tracks the currently active screen
extern Font font;                   // Global bitmap font loaded in main
extern Music music;                 // Global background music stream
extern Sound fxCoin;                // Global UI/action sound effect

#ifdef __cplusplus
extern "C" {                        // Prevents C++ name mangling if compiled with a C++ compiler
#endif

    // Module Functions Declaration

    // Logo Screen Functions
    void InitLogoScreen(void);          // Initialize logo animation state
    void UpdateLogoScreen(void);        // Step logo animation frame-by-frame
    void DrawLogoScreen(void);          // Render current logo animation state
    void UnloadLogoScreen(void);        // Free logo-specific resources
    int FinishLogoScreen(void);         // Returns non-zero when logo animation completes

    // Title Screen Functions
    void InitTitleScreen(void);         // Initialize menu buttons and background hazards
    void UpdateTitleScreen(void);       // Handle button hover/clicks and background drift
    void DrawTitleScreen(void);         // Render title text, buttons, and ambient hazards
    void UnloadTitleScreen(void);       // Free title screen resources
    int FinishTitleScreen(void);        // Returns 1 for Options, 2 for Gameplay

    // Options Screen Functions
    void InitOptionsScreen(void);       // Initialize options state
    void UpdateOptionsScreen(void);     // Handle options input
    void DrawOptionsScreen(void);       // Render options UI
    void UnloadOptionsScreen(void);     // Free options resources
    int FinishOptionsScreen(void);      // Returns 1 to return to Title

    // Gameplay Screen Functions
    void InitGameplayScreen(void);      // Initialize arena, UI rectangles, and simulation state
    void UpdateGameplayScreen(void);    // Step physics, AI potential fields, and collisions
    void DrawGameplayScreen(void);      // Render arena, telemetry vectors, heatmap, and HUD
    void UnloadGameplayScreen(void);    // Free gameplay resources
    int FinishGameplayScreen(void);     // Returns 1 to transition back to Title screen
    void SetGameplayMode(int mode);     // Sets active SimulationMode (0: AI, 1: Human, 2: Chaos)

    // Ending Screen Functions
    void InitEndingScreen(void);        // Initialize ending screen variables
    void UpdateEndingScreen(void);      // Check for return-to-title input
    void DrawEndingScreen(void);        // Render ending message
    void UnloadEndingScreen(void);      // Free ending resources
    int FinishEndingScreen(void);       // Returns 1 to return to Title

#ifdef __cplusplus
}                                   // Close extern "C" block (Fixed missing brace)
#endif

#endif // SCREENS_H