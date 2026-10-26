# 2D Autonomous AI Dodger (Raylib + C)

A lightweight 2D game AI simulation built in C using Raylib. 

The project demonstrates low-level game engineering fundamentals without external physics engines: normalized vector mathematics, Artificial Potential Fields (APF) for real-time obstacle avoidance, dynamic boundary constraints, and visual debug telemetry.

---

## Tech Stack

* Language: C (C17 standard)
* Framework: Raylib
* Environment: Visual Studio 2022 (MSVC) / CMake

---

## Core Systems

* Vector Steering: Normalized unit vectors for constant-velocity movement.
* Repulsive Potential Fields: Dynamic evasion forces with linear distance falloff.
* Struct Array Processing: Batch-updating obstacles in a cache-friendly array structure.
* Debug Telemetry: Real-time visual vectors, spatial danger bubbles, and coordinate tracking.

---

## AI Math Breakdown

### 1. Vector Seeking
To steer toward a target coordinate, the agent calculates a directional vector and divides by its Euclidean distance:

* dx = target.x - agent.x
* dy = target.y - agent.y
* distance = sqrt(dx * dx + dy * dy)
* seekX = dx / distance
* seekY = dy / distance

### 2. Obstacle Evasion
When inside an obstacle's detection radius, the agent calculates a repulsive force pointing directly away from the obstacle center. The force scales with proximity:

* panicFactor = (dangerRadius - distance) / dangerRadius
* avoidX = (fromObsX / distance) * panicFactor * weight
* avoidY = (fromObsY / distance) * panicFactor * weight

### 3. Force Blending
The attractive force and all repulsive forces are summed and renormalized every tick. This ensures the agent slides around obstacles without exceeding its fixed movement speed.

---

## Visual Telemetry Guide

* Blue Circle: AI Agent
* Dark Grey Circles: Solid Obstacles
* Red Translucent Circles: AI Hazard Detection Zones
* Orange Lines: Active Repulsive Force Vectors
* Grey Line: Primary Target Direction

---

## Building and Running

1. Clone the repository:
   ```bash
   git clone [https://github.com/](https://github.com/)marclourens19/game-ai-dodger.git
