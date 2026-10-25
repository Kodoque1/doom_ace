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
| R5 | 1 | Execution looks decoupled from memory, for every job: input (keyboard → input command), update (input and health → health), draw (health → pixels in the depth's screen buffer), copy to screen (buffer → real screen, done by the CPU). Each fetch must visibly lead to the read or write it causes, at that moment. |
| R6 | 1 | Too fast to follow: add pause and step-one-job controls. |
| R7 | 1 | Sizes are not proportional (the stack looks as big as the heap). Use real sizes from the run, or a stated compressed scale. |
| R8 | 1 | Prefer specific moving objects (patch tiles, saved byte blocks) over generic surfaces. |
| R9 | 1 | Use height: stacked layers, and height for size or stack depth. |
| R11 | 1 | Sync must go both ways: the scene's Next and Back scroll the text, the word for the current job or beat lights up in the text, and terms in the text highlight their part in the scene. |
| R12 | – | Use a pixel font throughout the article, to keep the old-school Doom look. |
| R10 | 1 | Worked: what happens in a frame (Q1), tracing the health from data to screen buffer to screen (Q2), places versus travellers (Q4), what a second game would add (Q5). |

Applied after retest round 1 (R1–R12):
- Four jobs: input, update, draw, copy to screen. Each fetch leads to its reads and writes, and the copy goes through the CPU.
- EIP and ESP are needles from the raised CPU.
- Stack columns grow when registers are pushed.
- Memory blocks are drawn to scale (area grows with √size).
- The keyboard and an input job are there from step 1, and a key press waits for the input job.
- Pause, step-one-job and speed controls.
- Two-way sync: the scene's Next and Back scroll the text, the current job or beat lights up in the text, and terms in the text flash or focus their part.
- Pixel fonts throughout: Jersey 10 for headings, Pixelify Sans for body text, VT323 for mono and labels.

### Retest round 2 (step 2)

| # | Finding |
|---|---------|
| R13 | The time strip depends on a text label. Make it a filmstrip: a finished frame slides in as a thumbnail framed in its depth's colour, and a fresh blank frame comes up to be worked on. |
| R14 | Nothing shows the nesting ("Doom inside Doom"): the three game views are unrelated. |
| R15 | Block sizes are readable, but not their roles (the heap was guessed from CS training). Show each block used for its role, e.g. draw reading textures from the heap, the stack growing and shrinking. |
| R16 | Only EIP came across as "where the CPU is"; the ESP needle, the lit body, the readout and the text highlight did not. |
| R17 | The two legs of the copy (buffer to CPU, CPU to screen) are too weak to register. |
| R18 | Show arrows only while something moves, except real pointers: EIP, ESP, and fainter pins. |
| R19 | Use angular, right-angled paths (circuit trace or automap style), one route per kind of link. |
| R20 | Show the instruction ordering the operation: a plain-words instruction card enters the CPU, becomes its current order, then the read or write leaves. No fetch-decode cycle, no invented x86. |

Worked: sync with the text, stepping through the four jobs.

Applied after retest round 2 (R13–R20, plus layers):
- Three stacked layers: memory on the floor, the machine (CPU, keyboard, screen, WAD disk) on a glass plane above, time on top.
- The time strip is a filmstrip of finished frames, with the frame being made at the right.
- Screen-walls: each depth's draw paints the next live depth's last frame onto a wall (the engine's ring), so the pictures nest.
- Draw reads textures from the heap, and the stack column grows on each job and shrinks after it.
- Instruction cards in plain words enter the CPU and become its order before each read or write.
- Right-angled routes on separate lanes, with short trails; lines appear only while something moves, except the EIP, ESP and (faint) pin pointers.
- The frame count lives in mtx_hs.

### Retest round 3 (step 3)

| # | Finding |
|---|---------|
| R21 | What ACE is, and why even depth 0 (the original DOOM2.EXE) runs it, is never explained. It is a missing piece of the story and must come before any patch. |
| R22 | Where the patches come from is not visible: neither ACE's code arriving from the WAD nor the tiles leaving ACE was seen. The load and the patching run too fast and back to back. |
| R23 | A patch's content is invisible (gap 16 again). Needs a close-up of one patch, bytes before and after. |
| R24 | ACE's code, inside the depth's own memory, reads as data ("it executed code in the data part"). Code needs one look wherever it lives. |
| R25 | Time layer in two lanes: "on screen" (the real screen's picture slides up at each copy) and "who ran" (each depth's finished frame slides out of its buffer). |
| R26 | Reversible execution: record state per op; step back and step forward; a scrubbable timeline of ops with job and beat marks. Changing article step must cancel any animation still playing. |

Worked: EIP jumping into ACE at update.

Applied after retest round 3 (R21–R26):
- Step 3 is now two steps. The first says what ACE is (a DOS Doom 2 mod shipped as a WAD; how the loader gets in is not detailed) and shows the loader bringing ACE's code from the WAD into depth 0's memory. The second shows the patching, tile by tile, with a close-up of one patch (before: Doom's own instruction; after: E8 and an offset, calling that depth's ACE).
- ACE is labelled "ACE code".
- The time layer has two lanes: "on the screen" (the real screen's picture flies up at each copy) and "who ran" (each finished frame flies out of its depth's buffer).
- Reversible execution: every animation, including loading and patching, is an op. Each article step takes a checkpoint, and the timeline under the scene lets you click any op to rewind to it (restore, then replay instantly). There are one-op back and forward buttons, and changing step cancels pending animations.
- Labels size themselves to their text; long labels were being cut off.

### Retest round 3b (new steps 3 and 4)

| # | Finding |
|---|---------|
| R27 | "Depth 0's snapshot" was used for the whole depth area. Name the area "this depth's own memory", distinct from the saved-data slot. |
| R28 | Wrong in the scene: the load goes straight from the WAD into memory. The loader is code the CPU runs; the read and the write must go through the CPU (like R1). |
| R29 | The health write was read as a write into Doom code: the health cell on the data plate sits too close to the code plate. |
| R30 | Where ACE's code lives (its own block, or inside depth 0's heap) is ambiguous. Check against the engine and label what is true. |
| R31 | Show reuse of the original process at instantiation: Doom's code and data stay, the new depth's own blocks appear, and its saved-data slot is seeded from the live data ("starts as a copy of its parent"; dos_state.c load_state, first entry). |
| R32 | Clicking text jumps the scene: a paragraph makes its step current; a job or beat word advances to that job's next occurrence. |
| R33 | Stronger fetch versus data distinction: code on its own raised shelf, labelled instruction and data ports on the CPU, dashed trails for instructions and solid for data, one colour reserved for instruction traffic. |

Worked: patches differ per depth because each calls its own ACE (first time from the scene). Q5 (the patched bytes are Doom's code).

Applied after retest round 3b (R27–R33) and the follow-ups during the build:
- Code sits on its own raised shelf: Doom code at the front, each depth's ACE code in its own block behind it (the stock loader frees Doom's heap and allocates a fresh block for ACE_CODE, see doom_ace/loader/ldr1.S).
- The CPU has labelled ports, "instructions in" at the back and "data" at the front. Instruction cards leave dashed trails, data solid ones, and yellow is reserved for instruction traffic.
- Loading goes through the CPU: depth 0's loader runs from Doom data, and later depths are loaded by depth 0's ACE code. ACE's code comes in as WAD → CPU → its block.
- A new depth's first turn copies the live Doom data into its saved-data slot, block by block through the CPU: an operation, not a label.
- Each depth's area is labelled "its own memory", distinct from its saved-data slot.
- Clicking a paragraph makes it the current step. Clicking a job or beat word advances the scene to that job's next run.
- Screen buffers are see-through when idle and turn opaque while they are written or read.
- "Who ran" is three tracks, one per depth, with one block per frame. "On the screen" stays a strip of pictures.
- Fixed: a step's own camera timers were cancelled as soon as they were set.

### Retest round 4 (step 5)

| # | Finding |
|---|---------|
| R34 | The patch close-up went unnoticed: it was two floating labels. Make it a magnifier panel that compares depth 0's and depth 1's bytes for the same patched place side by side. |
| R35 | Depth 1's load was not seen: the camera stayed on the floor while the load happened at the WAD, CPU and shelf. The camera should follow the action. |
| R36 | Where depth 1's code comes from is not shown (the reader asked how a later depth gets the WAD, and whether there are several). Answer this by showing it. |
| R37 | Bug: depth 0's saved-data slot and patch tray showed from step 1 (a visibility flag set to the number 0, which three.js does not treat as hidden). Fixed. Also, the machine's glass plane hides the code shelf: make it much more transparent. |
| R38 | Show what each depth's load reuses and what it does not (the reader's premise is partly right; details withheld, as they answer Q5). |
| R39 | Show how the deepest depth connects back to depth 0. Every frame, host code writes each depth's screen-wall source into its parameter block in mtx_hs: the next live depth's buffer, wrapping to depth 0 for the deepest (mtx_boot.c, mtx_ring_next / params[].screen_src). Show it as pointers from mtx_hs, re-aimed when a depth goes live, and have draw read the pointer before the buffer. |

Worked: Q1 (what a new depth adds and does not add) and Q4 (screen-wall traced from the next depth's buffer through the CPU). Q3 was inferred rather than seen.
