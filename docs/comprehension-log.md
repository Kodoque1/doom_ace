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
| 28 | 7 | No reason is shown for switching only at frame ends: no glimpse of a mid-frame switch going wrong (only in Map 07). |
| 29 | 8 | "The clock" is not an object, only a counter label; it is not placed outside the depths. |
| 30 | 8 | Ring positions carry no meaning; the top is both "think" and the frame boundary, so where a depth stops reads as arbitrary. |
| 31 | 8 | The frozen ring seems to remember where to resume, suggesting nothing needs saving. The CPU's state must be seen put away and taken back. |
| 32 | 8 | Who runs next looks random: the request's destination and the pattern are not shown until the keyboard step. |
| 33 | 9 | The scene is not synced with the text: the press-y button is unannounced, and the scene also presses on a timer. The reader's press should drive the animation the text describes. |
| 34 | 9 | The key's line ends on the ring and reads as "key-handling code"; the host word it really goes through is not shown. |
| 35 | 9 | Immediate and deferred effects look alike: the ▼ moves after a fixed delay, so it reads as waiting. |
| 36 | 9 | Nothing links the ▼ to the CPU's next destination; the reader concluded y has no effect on the schedule. |
| 37 | 10 | The snapshot slots are not identifiable without the caption; nothing moves in or out at this step. |
| 38 | 10 | The jobs of snapshots and pins are confusable; the reader took pins to give each depth its own data. |
| 39 | 10 | "Put back" contradicts the scene (the studs stay blue), and whether patches differ per depth is still unsettled (see 16). |
| 40 | 10 | The pin lines land on the snapshot slots, so pins read as "pointers to snapshots"; they need real per-depth targets (see 11). |
| 41 | 11 | Why host state must sit outside the snapshots is not shown at this step (only in Map 07). |
| 42 | 11 | The mtx_hs line is static: no direction, no content. |
| 43 | 11 | Grey means both "not opened yet" and what mtx_hs looks like; host-owned things need their own consistent colour. |
| 44 | 11 | "Host", depth 0 and mtx_hs are tangled; the scene does not say what the host is or where the clock runs. |
| 45 | 12 | The loader never visibly acts, so when it runs is unknowable. |
| 46 | 12 | Where code comes from (file, then memory, then running) is never shown, for the loader or for ACE (see 6). |
| 47 | 12 | What picks the screen's depth is invisible; the screen never shows anything (see 17). |
| 48 | 12 | The input and output layout reads as arbitrary (keyboard on the floor, screen in execution, loader on the left). |
| 49 | 12 | The flow dots look like the CPU ball ("why multiple balls?"). |
| 50 | 12 | Every flow is drawn as a train of several dots, so one key press reads as several requests; the scripted auto-press adds more. One event must be one glyph. |
| 51 | 13 | Spreading the layers hides the cross-layer lines, and nobody noticed: the relationships vanish exactly when the layers are separated. They should stretch. |
| 52 | 13 | The rule for each layer is unclear for input and output (see 48). |
| 53 | 13 | The switch's layer is ambiguous (read as "between execution and time"): it is not clear whether it is code, a moment, or a place. |
| 54 | 15 | The data snapshot is absent from the reader's account of a turn; it is the mechanism the scene shows least (see 37). |
| 55 | 15 | What ends a turn (the clock) is absent from the account (see 29). |
| 56 | 15 | Once versus per depth is unclear: Doom data was listed as "once", and the per-depth heap, stack and state were missing (see 11, 15, 21, 40). |
| 57 | 15 | The chain from the ▼ to the screen is not shown at all (see 17, 36, 47). |
| 58 | 15 | The switch process is the biggest remaining gap: theme G is top priority. |

## Themes for the rework

- A. Running touches memory: 1, 7, 10, 14
- B. A depth has a body: 4, 5, 11, 31
- C. One meaning per colour: 9, 12, 13, 16
- D. Cause, then consequence: 6, 14, 15, 16, 21, 22
- G. The switch does something: 23
- H. The request and the check: 24, 25, 26, 27, 28, 29, 32
- E also takes 30.
- I. Interaction and sync: 33, 35
- L. Layout and glyphs: 48, 49, 50, 51, 52, 53. D also takes 45, 46.
- K. The host and its state: 41, 42, 43, 44
- J. Three keeping-apart mechanisms, each with its own visible action: 37, 38, 39, 40. Step 10 Q5 confirmed gap 23.
- H also takes 34, 36. Step 9 Q3 confirmed gap 17.
- E. Time: 2, 17, 18, 19, 20
- F. Gear: 3

Worked: step 1 Q5 (screen), step 3 Q4 (taking turns), step 4 Q3 (no saved copy visible), step 5 Q1 (reading the strip), step 6 Q2 (no per-depth state visible). Step 6 Q5 confirmed gaps 9 and 11. Step 13 Q3 and Q5 (a), (c) worked; Q5 (b) confirmed 11 and 37. Step 7 Q3 was answered from prior knowledge, not the scene: logged as gap 28.

Rounds 14 and 15 (step 14 and integration): the reader warns that the questioning itself teaches, so Q1 and I2 are not counted as scene successes.

## Proposals from the reader

- P1 (round 10): a tiny procedural game view per depth (corridor plus a health number, no game assets). One on-screen value is linked to a data cell through code on the code plate, and the real screen is composed from the depths' frames as they render. Addresses 1, 14, 17, 21, 22.
- P2 (round 11): replace the moving ball with a fixed schematic CPU: a visible instruction pointer and stack pointer, with separate fetch and read/write ports whose lines light up as it works. The lit, turning ring shows which depth runs. Addresses 4, 23, 31 and theme A.
- P3 (round 12): drop the rings. A depth is its body of saved state on the floor, and a switch visibly moves things (patches back to code, pins reconnecting, CPU state put away and restored). The execution layer belongs to the fixed CPU (P2). Depth identity could come from P1's per-depth views. One detail of the proposal does not match the code (not stated, since it answers a question); the design follows the code. Layout research started: docs/layout-research.md.

## Test method notes

- Step 8 Q2 and Q5: the reader said the question itself taught them. The retest should use open "what do you see happen when..." questions, not ones that point at the answer.

## Retest of the rebuilt Map 00 (v2)

| # | Step | Finding |
|---|------|---------|
| R1 | 1 | Wrong in the scene: the present bead goes straight from the screen buffer to the screen. The CPU does that copy (read the buffer, write the screen). |
| R2 | 1 | The CPU still reads as a black box: the "registers" card is unexplained, and EIP and ESP are text, placed near the screen. They should be visual pointers: a needle into the exact code spot, a marker in the stack. |
| R3 | 1 | Job names: "present" is graphics jargon and "think" is ambiguous. Use input · update · draw · copy to screen. |
| R4 | 1 | Plain Doom reads the keyboard every frame: the keyboard and an input job belong in step 1. |
| R5 | 1 | The health write looks unconnected to the code: nothing ties the fetched instruction to the write it causes. |
| R6 | 1 | Too fast to follow: add pause and step-one-job controls. |
| R7 | 1 | Sizes are not proportional (the stack looks as big as the heap). Use real sizes from the run, or a stated compressed scale. |
| R8 | 1 | Prefer specific moving objects (patch tiles, saved byte blocks) over generic surfaces. |
| R9 | 1 | Use height: stacked layers, and height for size or stack depth. |
| R10 | 1 | Worked: what happens in a frame (Q1), tracing the health from data to screen buffer to screen (Q2), places versus travellers (Q4), what a second game would add (Q5). |
