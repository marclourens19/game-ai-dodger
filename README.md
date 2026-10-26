# 2D Autonomous AI Dodger (Raylib + C)

A lightweight 2D game AI simulation built in C using Raylib.

The project demonstrates low-level game engineering fundamentals without external physics engines: normalized vector mathematics, Artificial Potential Fields (APF) for real-time obstacle avoidance, kinematic trajectory prediction, spatial memory learning grids, and dynamic boundary constraints.

---

## Version 2: Predictive Trajectory and Spatial Learning

### Dynamic Hazards and Trajectory Prediction
![Version 2 Trajectory Prediction](screenshots/demotwo.gif)

### Spatial Memory Heatmap and Collision Learning
![Version 2 Spatial Memory Learning](screenshots/demothree.gif)

---

## Version 1: Steering and Static Avoidance

![Version 1 Steering Demo](screenshots/demo.gif)

---

## Tech Stack

* Language: C (C17 standard)
* Framework: Raylib
* Toolchain: Visual Studio 2022 (MSVC) / CMake

---

## Core Systems

* Vector Steering: Normalized unit vectors for constant-velocity movement.
* Repulsive Potential Fields: Dynamic evasion forces with linear distance falloff.
* Kinematic Trajectory Prediction: Real-time time-to-impact calculations ($t = d / v$) with floor projection reticles.
* Spatial Memory Grid: 16-column learning array that penalizes past damage locations with linear time decay.
* Dynamic Difficulty: Hazard scaling from 3 to 12 simultaneous falling projectiles.
* Arena Clamping: Strict corridor constraints, 3-life hit tracking, and damage invulnerability frames.
* Visual Debug Telemetry: Active force vectors, intent lines, floor risk heatmaps, and impact reticles.

---

## AI Math Breakdown

### 1. Vector Seeking
To steer toward a target coordinate, the agent calculates a directional delta vector and normalizes it by Euclidean distance:

* dx = target.x - agent.x
* dy = target.y - agent.y
* distance = sqrt(dx * dx + dy * dy)
* seekVector = (dx / distance, dy / distance)

### 2. Obstacle Evasion (Potential Fields)
When inside an obstacle's detection radius, the agent computes a repulsive vector pointing away from the hazard center, scaled by proximity:

* panicFactor = (dangerRadius - distance) / dangerRadius
* avoidVector = (fromObstacle / distance) * panicFactor * weight

### 3. Kinematic Trajectory Prediction
For falling projectiles, the agent evaluates time remaining until the obstacle crosses the agent baseline altitude:

* distanceToFloor = floorY - hazard.y
* timeToImpact = (distanceToFloor / speedY) * (1 / 60)
* urgencyWeight = 1.0 / (timeToImpact + 0.3)

Obstacles with imminent impact times receive immediate evasion priority over hazards higher in the arena.

### 4. Spatial Risk Learning and Decay
The floor corridor is segmented into 16 discrete columns. When hit, a risk penalty is stamped into the active column:

* riskHeatmap[hitColumn] += 55.0
* Decay tick: riskHeatmap[i] -= 2.0 * dt

The agent scans all columns every tick to select the target corridor with the lowest combined threat score.

---

## Visual Telemetry Guide

* Blue Circle: AI Agent (flashes red during invincibility frames)
* Dark Grey Circles: Solid Physical Hazards
* Translucent Red Radii: Hazard Influence Bubbles
* Vertical Grey Lines: Projected Hazard Fall Trajectories
* Red Floor Rings: Real-time Impact Reticles (brighten on near impact)
* Orange Lines: Active Repulsive Force Vectors
* Cyan Vertical Line: Current Target Column Selected by AI
* Floor Heatmap Bar: Spatial Memory Grid (Green = Safe, Red = High Risk)

---

## Building and Running

1. Clone the repository:
   ```bash
   git clone [https://github.com/marclourens19/game-ai-dodger.git](https://github.com/marclourens19/game-ai-dodger.git)
