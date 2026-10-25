# Comprehension log: gaps found by grilling a reader

Method: for each step of the article, the reader answers questions using only
that step. Answers are never revealed. Each answer is used only to find what the
scene failed to convey. This file lists those gaps, numbered in the order found.
It does not contain answers.

## Map 00, the whole machine

| # | Step | Gap in the scene |
|---|------|------------------|
| 1 | 1 | Ring nodes (think/render/present) have one-word labels, and nothing happens when the CPU passes one. |
| 2 | 1 | The top of the ring (end of frame) is not marked at step 1. |
| 3 | 1 | The gear says nothing the ball doesn't until freezing matters (step 8). |
| 4 | 1 | The CPU's own state (stack, registers) is not shown, so the switch later has nothing visible to save. |
| 5 | 2 | ACE has no place in memory. |
| 6 | 2 | Where patching comes from is unreadable: a line out of a ring does not say "running code wrote this". |
| 7 | 2 | What a patch does at run time is never shown. |
| 8 | 2 | The code plate's identity after patching is vague. |
| 9 | 3 | Colour means both "owned by" and "last written by". |
| 10 | 3 | The CPU never visibly reaches a patched spot. |
| 11 | 3 | A new depth adds a ring and nothing on the floor. |
| 12 | 3 | Colour shows who wrote, not what was written, so "same bytes" and "different bytes" look alike. |
| 13 | 4 | "Shared" is said but not shown. |
| 14 | 4 | The consequence of running another depth's patch is never shown. |
| 15 | 4 | The code-only focus suggests data is safe. |
| 16 | 4 | Why patches clash is missing: the content of a patch is never shown. |
| 17 | 5 | The screen stays grey while frames are rendered. |
| 18 | 5 | Old strip segments silently change colour: the strip's window on time is invisible. |
| 19 | 5 | The hop's on-screen duration is arbitrary, and the strip does not show it. |
| 20 | 5 | No game time: no clock per depth and no wall clock. |
| 21 | 6 | Shared data is stated flatly; the conflict with "each game has its own state" is never staged as the puzzle (sets up Map 03). |
| 22 | 6 | Nothing links code to the data addresses it uses, so "fixed addresses" has no visible meaning. |
| 23 | 6 | The hop is a flight path: nothing shows what a switch touches. |
| 24 | 7 | The switch request has no visible home; it only pops up as a label at frame 4. |
| 25 | 7 | Nothing shows the CPU checking for a request at a boundary. |
| 26 | 7 | "Post" is undefined; the posts are thin, and tall versus short is barely visible. |
| 27 | 7 | A check that finds nothing looks identical to no check (same as 25, seen from the other side). |

## Themes for the rework

- A. Running touches memory: 1, 7, 10, 14
- B. A depth has a body: 4, 5, 11
- C. One meaning per colour: 9, 12, 13, 16
- D. Cause, then consequence: 6, 14, 15, 16, 21, 22
- G. The switch does something: 23
- H. The request and the check: 24, 25, 26, 27
- E. Time: 2, 17, 18, 19, 20
- F. Gear: 3

Worked: step 1 Q5 (screen), step 3 Q4 (taking turns), step 4 Q3 (no saved copy visible), step 5 Q1 (reading the strip), step 6 Q2 (no per-depth state visible). Step 6 Q5 confirmed gaps 9 and 11. Step 7 Q3 (partial rendering), possibly from prior knowledge.
