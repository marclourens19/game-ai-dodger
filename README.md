# 2D Autonomous AI Dodger (Raylib + C)

A lightweight 2D game AI simulation built in C using Raylib. The project demonstrates low-level game engineering fundamentals without external physics engines: normalized vector mathematics, Artificial Potential Fields (APF) for real-time obstacle avoidance, kinematic trajectory prediction, spatial memory learning grids, dynamic difficulty scaling, and interactive multi-mode simulation suites.

---

## Version 3: Multi-Mode Simulation & Interactive Sandbox

### Mode 0: Autonomous AI Dodger

Autonomous agent evasive maneuvering powered by kinematic trajectory interception, repulsive potential field vectors, and 16-column spatial memory pathfinding.

![Version 3 AI Mode](screenshots/demofour.gif)

### Mode 1: Human Benchmark (Direct Mouse Control)

Direct cursor positioning enabling human benchmark runs against the procedural falling hazard engine.

![Version 3 Human Mode](screenshots/demofive.gif)

### Mode 2: Chaos Drop (Interactive Sandbox)

Interactive spawner mode allowing the user to drop custom boulders directly onto the AI agent with real-time mouse-wheel radius scaling.

![Version 3 Chaos Drop Mode](screenshots/demosix.gif)

---

## Version 2: Predictive Trajectory and Spatial Learning

### Dynamic Hazards and Trajectory Prediction

Real-time kinematic Time-to-Impact (TTI) projection lines and proximity-fading impact reticles on the arena floor.

![Version 2 Trajectory Prediction](screenshots/demotwo.gif)

### Spatial Memory Heatmap and Collision Learning

Dynamic 16-column spatial risk grid that stamps localized penalties upon damage and linearly decays over time.

![Version 2 Spatial Memory Learning](screenshots/demothree.gif)

---

## Version 1: Steering and Static Avoidance

Basic vector normalization steering and static circle-to-circle obstacle avoidance.

![Version 1 Steering Demo](screenshots/demo.gif)

---

## Tech Stack

- **Language:** C
- **Framework:** Raylib 6.0 (the root CMake configuration fetches this version by default)
- **Audio:** Raylib audio module (miniaudio backend by default)
- **Toolchain:** Visual Studio 2022 (MSVC) / CMake / GCC / Clang

---

## Core Systems

- **Vector Steering:** Normalized Euclidean unit vectors for directional steering.
- **Repulsive Potential Fields (APF):** Imminent threat evaluation exerting proximity-scaled repulsive velocity offsets away from hazard centers.
- **Kinematic Trajectory Prediction:** Time-to-impact estimates and floor-intercept landing reticles.
- **Spatial Memory Grid:** 16-column learning array tracking historical collision damage with linear time decay (`2.0 units/sec`).
- **Interactive Multi-Mode Architecture:** Transitions between Autonomous AI, Human Benchmark, and Chaos Sandbox modes.
- **Live Takeover:** Real-time switching between autonomous AI steering and player cursor control during a simulation.
- **Visual Telemetry Toggle:** Toggle (`[T]`) for debug overlays, force rays, trajectory lines, APF influence bubbles, and the spatial heatmap bar.
- **Dynamic Difficulty:** Procedural hazard density scaling from 3 up to 14 simultaneous falling projectiles (`+1 hazard every 6.5 s`).
- **State Machine UI:** Pause Menu (`[ESC]`) and Game Over overlay with selective memory retention (`[R]`) or complete memory reset (`[C]`).

---

## Controls & Hotkeys

| Key / Input | Action |
| :--- | :--- |
| `[1]` | Switch to **Mode 0: AI Dodger** (Autonomous Simulation) |
| `[2]` | Switch to **Mode 1: Human Benchmark** (Direct Mouse Control) |
| `[3]` | Switch to **Mode 2: Chaos Drop** (Interactive Sandbox) |
| `[P]` | **Live Takeover Toggle** (switch between AI and mouse control) |
| `[T]` | **Toggle Telemetry UI** (show/hide trajectory lines, reticles, APF bubbles, and memory heatmap) |
| `[ESC]` | **Toggle Pause Menu** |
| `[R]` | **Restart Run** (retain learned spatial risk memory) |
| `[C]` | **Restart & Clear Memory** (reset the spatial memory heatmap) |
| `[M]` | **Return to Title Menu** |
| `Mouse Wheel` | *(Chaos Mode)* Adjust spawned boulder radius (`16 px–56 px`) |
| `Left Click` | *(Chaos Mode)* Drop a custom boulder at cursor coordinates |

---

## AI Math Breakdown

### 1. Vector Seeking

To steer toward a target coordinate, the agent calculates a directional delta vector and normalizes it by Euclidean distance:

$$
\Delta x = x_{\text{target}} - x_{\text{agent}}, \quad \Delta y = y_{\text{target}} - y_{\text{agent}}
$$

$$
\text{distance} = \sqrt{\Delta x^2 + \Delta y^2}
$$

$$
\vec{v}_{\text{seek}} = \left( \frac{\Delta x}{\text{distance}}, \frac{\Delta y}{\text{distance}} \right)
$$

### 2. Obstacle Evasion (Artificial Potential Fields)

When inside an obstacle's detection radius (`R_danger`), the agent computes a repulsive vector pointing away from the hazard center, scaled by proximity:

$$
\text{panic} = \frac{R_{\text{danger}} - \text{distance}}{R_{\text{danger}}}
$$

$$
W_{\text{urgency}} = \min\left(3.0, \frac{1.0}{t_{\text{impact}} + 0.3}\right)
$$

$$
\vec{v}_{\text{avoid}} = \left( \frac{\Delta x}{\text{distance}} \cdot \text{panic} \cdot W_{\text{urgency}} \cdot 3.6,\; \frac{\Delta y}{\text{distance}} \cdot \text{panic} \cdot 1.5 \right)
$$

### 3. Kinematic Trajectory Prediction

For falling projectiles, the agent estimates the time remaining until the obstacle crosses the agent baseline altitude, $d_{\text{floor}}$:

```math
d_{\text{floor}} = \text{FLOOR\_Y} - y_{\text{hazard}}
```

```math
t_{\text{impact}} = \left(\frac{d_{\text{floor}}}{v_y}\right) \cdot \left(\frac{1}{60}\right)
```

```math
x_{\text{projected}} = x_{\text{hazard}} + \left(\frac{v_x}{v_y}\right) \cdot d_{\text{floor}}
```

Obstacles with imminent impact times receive immediate evasion priority over hazards higher in the arena.

### 4. Spatial Risk Learning and Decay

The floor corridor is segmented into 16 discrete columns (`50 px` each). When hit, a localized penalty is stamped into the active and neighboring columns:

$$
\text{riskHeatmap}[\text{col}_{\text{hit}}] \mathrel{+}= 55.0, \quad \text{riskHeatmap}[\text{col}_{\text{hit}} \pm 1] \mathrel{+}= 22.0
$$

During every frame tick, historical risk decays linearly:

$$
\text{riskHeatmap}[i] = \max\left(0.0,\; \text{riskHeatmap}[i] - 2.0 \cdot \Delta t\right)
$$

The agent evaluates candidate column safety by minimizing:

$$
\text{Score} = \text{riskHeatmap}[\text{col}] + \sum \left( \text{proximity} \cdot \frac{15.0}{t_{\text{impact}} + 0.25} \right) + \left( \frac{\text{travelDist}}{\text{ScreenWidth}} \right) \cdot 9.0 + \text{Penalty}_{\text{corner}}
$$

---

## Visual Telemetry Guide

- **Blue Circle:** Autonomous AI Agent (flashes red during invincibility frames).
- **Green Circle ("YOU"):** Player-controlled agent in Human Benchmark mode.
- **Dark Grey Circles:** Procedurally generated physical hazards.
- **Dark Red Circles:** User-spawned boulders (Chaos Sandbox mode).
- **Translucent Red Bubbles:** APF sensory influence radii.
- **Vertical Grey Lines:** Kinematic floor trajectory projection rays.
- **Red Floor Rings:** Real-time landing reticles (brighten as TTI approaches zero).
- **Orange Lines:** Active repulsive force vectors exerted on the agent.
- **Cyan Vertical Line:** Global pathfinding target column selected by the AI.
- **Gold Ray & Yellow Tip:** Normalized resultant steering vector calculated via superposition.
- **Floor Heatmap Bar:** 16-cell spatial memory grid (`Green = Safe`, `Red = High Risk`).

---

## Building and Running

### Prerequisites

- CMake 3.24 or higher
- A C compiler
- Git
- Internet access during initial configuration if CMake needs to download Raylib

The root `CMakeLists.txt` uses CMake FetchContent to obtain Raylib 6.0 by default, while allowing an installed Raylib package to be found first.

### Build Instructions

1. Clone the repository:

   ```bash
   git clone https://github.com/marclourens19/game-ai-dodger.git
   cd game-ai-dodger
   ```

2. Configure a Release build:

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   ```

3. Build the project:

   ```bash
   cmake --build build --config Release
   ```

4. Run the executable from the output directory. The current CMake project/target name is `raylib-game-template`, and CMake sets the runtime output directory to `build/raylib-game-template`. With a multi-configuration generator such as Visual Studio, a configuration directory such as `Release` may be added beneath that folder. The executable filename is `raylib-game-template` (or `raylib-game-template.exe` on Windows).

---

## License

This project is distributed under the [MIT License](LICENSE).
