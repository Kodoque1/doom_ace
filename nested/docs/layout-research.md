# Layout research: a fixed CPU, per-depth state bodies, in → process → out

Research note for redesigning the 3D "three depths, one CPU" scene after the reader test. The scene being replaced had a moving CPU ball on per-depth rings, flat memory plates, arbitrary placement, static lines, and one colour that meant both owner and last writer.

**Verification key.** "Verified" means the search results confirmed the bibliographic details. Items marked **[unverified]** were not confirmed in this session, so check them before citing. Direct fetches to most hosts (arxiv, wisc.edu, ucsd.edu, visual6502.org) were blocked here, so some claims about content rely on abstracts, search snippets or secondary summaries, and those are flagged. Related notes in this folder: `viz3d-research.md` (3D, digital twins, process mining), `visualization-research.md` (program-state and OS tools) and `sequencing-research.md`. Where an entry overlaps, this note only adds the layout lesson.

---

## A. Laying out process and plant visualisations

**A1. Muther, R. (1961). *Systematic Layout Planning*. Boston: Industrial Education Institute** (later editions published by CBI). Reference via Muther-affiliated training material: https://richardmuther.com/wp-content/uploads/2021/06/LWDC-Training-21-How-to-Develop-Layout-Plans.pdf. Lecture-notes summary: https://ieda.ust.hk/dfaculty/ajay/courses/ieem513/Layout/lecLayout.html. Original book not consulted.
- *Summary:* Factory layout starts from the flow route (product, quantity, routing, support, time) and uses standard flow shapes (I, L, U, S, circular). A relationship chart rates which areas must be close (A/E) and which must be kept apart (X).
- *Lesson:* Use an **I-flow** (straight line) for the main route: keyboard → CPU → screen on one axis. Draw up an A/E/X chart for our areas. CPU↔code and CPU↔live data are rated A. Switch-machinery↔depth bodies is rated E. Host state↔snapshot slots is rated **X**: they must look physically separate, because host state is outside every snapshot.

**A2. Schmidt, M. (2008). "The Sankey Diagram in Energy and Material Flow Management, Part I: History." *Journal of Industrial Ecology* 12(1): 82–94.** DOI 10.1111/j.1530-9290.2008.00004.x **[DOI unverified; venue and pages verified]**. Index entry: https://library.metabolismofcities.org/items/1745
- *Summary:* Traces the Sankey diagram from Sankey's 1898 steam-engine efficiency chart. Flow width shows quantity, and the early charts showed a single flow type.
- *Lesson:* Use a width-coded band **only for the bulk save/restore copy**, the one flow where quantity matters (KB of writable memory, against 405 patch bytes and a few pin words). If every flow were a Sankey band, the scene would be cluttered with bands again.

**A3. Jenny, B., Stephen, D. M., Muehlenhaus, I., Marston, B. E., Sharma, R., Zhang, E., Jenny, H. (2018). "Design principles for origin-destination flow maps." *Cartography and Geographic Information Science* 45(1): 62–75.** DOI 10.1080/15230406.2016.1262280 **[volume/pages/DOI unverified; authors, content and 2018 date verified via a later citing abstract]**. Accepted-manuscript PDF: https://web.engr.oregonstate.edu/~zhange/images/FlowMapPaper.pdf
- *Summary:* A content analysis of 97 flow maps plus a 215-participant study. Curved flows beat straight flows, **arrows beat tapering for showing direction**, and node-to-node flows beat area-to-area flows. Other principles: minimise overlaps, avoid acute crossings, and never run a flow underneath an unrelated node.
- *Lesson:* Every moving flow should carry an **arrowhead or a travelling chevron**, not a taper. Connect lines **port-to-port**: CPU fetch port to a code cell, data port to a data cell, not plate to plate. Route the fetch and data lines so that they **never cross each other or pass under a depth body**.

**A4. Neurath, M., Kinross, R. (2009). *The Transformer: Principles of Making Isotype Charts*. London: Hyphen Press.** https://hyphenpress.co.uk/subjects/Isotype (book verified; year 2009 **[unverified in search]**).
- *Summary:* Marie Neurath's essay on "transforming" information into a picture. Isotype relies on standardised pictograms with a fixed meaning: one sign per kind of thing, with quantity shown by repeating the sign rather than enlarging it.
- *Lesson:* Give each **kind** of thing exactly one glyph and keep it everywhere: a patch tile, a pin plug, a CPU-state card, a data cell. Show quantity by repetition (more patch tiles) and keep each tile the same size.

**A5. van der Aalst, W. M. P., Adriansyah, A., van Dongen, B. (2012). "Replaying history on process models for conformance checking and performance analysis." *WIREs Data Mining and Knowledge Discovery* 2(2): 182–192.** https://doi.org/10.1002/widm.1045 (verified in `viz3d-research.md`).
- *Summary:* Tokens replayed from an event log move over a fixed process model, so every event attaches to a model element.
- *Lesson:* This is the core reframe. **The model (machine) is fixed and the tokens move.** The old scene moved the CPU itself, which is like moving the factory. Here the CPU, code bank and memory floor stay put, and only data tokens, patch tiles, pins and state cards move.

**A6. Mannhardt, F. *processanimateR: Process Map Token Replay Animation* (R package, bupaverse).** https://github.com/bupaverse/processanimateR (verified).
- *Summary:* Cases rendered as SVG tokens moving along a process map. Token size and colour are mapped to attributes, and there is an optional renderer with fixed coordinates for activities.
- *Lesson:* Map **token shape to flow type** and **colour to depth (owner) only**. processanimateR's separate size/colour channels show that both can be encoded without one channel doing double duty.

**A7. Xu, P., Mei, H., Ren, L., Chen, W. (2017). "ViDX: Visual Diagnostics of Assembly Line Performance in Smart Factories." IEEE TVCG 23(1).** https://doi.org/10.1109/TVCG.2016.2598664 (verified in `viz3d-research.md`).
- *Summary:* Assembly stations are laid out along one axis and products are traced through them over time.
- *Lesson:* A small **Marey/time strip** under the floor (D0 | switch | D1 | switch | D2 …) shows schedule history. The 3D floor then only has to show the current state, not history.

**A8. ISA-101 / High-Performance HMI practice.** The standard (ANSI/ISA-101.01-2015) was not read. Secondary sources: Emerson/IPD white paper https://resources.ipd.com.au/documents/white-papers/Best-practices-for-high-performance-HMI-design_White%20Paper_ENUS_2023.pdf and Control.com "Going Gray" https://control.com/technical-articles/going-gray/ (verified as summaries).
- *Summary:* Plant displays are greyscale in normal operation, with saturated colour **reserved for meaning** (abnormal state). Important distinctions are never carried by colour alone.
- *Lesson:* Fixes the colour bug directly. Draw the machine in neutral greys. Give **colour one meaning: depth identity (owner)**. Show "last writer" with a **transient outline** that fades, never a hue (the "changed this step" convention of MARS: Vollmar & Sanderson, SIGCSE 2006, https://doi.org/10.1145/1121341.1121415). Back up depth hue with a label and position, for colour-blind readers.

---

## B. CPU and memory visualisers for teaching

**B1. James, G., Silverman, B., Silverman, B. (2010). "Visualizing a classic CPU in action: the 6502." SIGGRAPH 2010 talk.** https://history.siggraph.org/learning/visualizing-a-classic-cpu-in-action-the-6502-by-james-silverman-and-silverman ; project: http://www.visual6502.org (verified; ACM DOI not found).
- *Summary:* A transistor-level simulation built from roughly 20,000 polygons traced from die photos. Active nodes light up on the **unmoving chip layout** as real software runs.
- *Lesson:* This is the strongest precedent for a **fixed CPU whose activity is shown by lighting, not motion**. Our CPU should stay still. Its IP and SP readouts change, and its fetch and data ports light up for each access.

**B2. Madnick, S. (1965), MIT: the Little Man Computer.** Overview: https://en.wikipedia.org/wiki/Little_Man_Computer. Madnick's authorship is per his bio and secondary sources; no 1965 primary document seen **[original publication unverified]**.
- *Summary:* A mailroom metaphor: 100 numbered mailboxes holding instruction *or* data, an inbox and outbox for I/O, a calculator (accumulator) and a program counter.
- *Lesson:* LMC places **input on one side and output on the other** of a fixed worker, with the PC as a visible counter. That is our keyboard → CPU → screen line. It also shows a counter-example: LMC mixes code and data in one bank. Our point (shared code, private data) needs the **two kinds of memory physically separate**.

**B3. Dauscher, P. (2009–2014), Laubersheimer, D. (2019–2021). *JOHNNY: a simulator of a simple von Neumann computer*.** https://johnnysimulator.sourceforge.io/ ; ILTIS 2019 talk: https://fg-ibmv.gi.de/fileadmin/FG/IBMV/ILTiS_2019/JOHNNY_fuer_ILTIS19.pdf (verified as software; GPLv3).
- *Summary:* Memory, ALU and control unit are shown as fixed blocks joined by a simplified bus. Programs are stepped through, and the microcode is visible and editable. An experimental build adds a framebuffer and keyboard.
- *Lesson:* The fixed-block "chip diagram" with labelled **buses whose lines are active during a transfer** is what readers expect from school. The framebuffer variant shows that a tiny screen next to the machine reads naturally as output. Details of how its bus animates were not confirmed.

**B4. Harris, S. L., Harris, D. M. *Digital Design and Computer Architecture*, Ch. 7 "Microarchitecture" (2nd ed. 2012 MIPS; RISC-V ed. 2021).** Slides: https://pages.hmc.edu/harris/class/e85/old/fall24/DDCArv_Ch7.pdf (verified via slides; edition years from memory **[unverified]**).
- *Summary:* The single-cycle datapath has a separate **Instruction Memory** feeding the decoder and a **Data Memory** on the far side of the ALU. The PC sits at the left and data flows left to right.
- *Lesson:* Give the CPU **two ports on opposite faces**: a fetch port facing the code bank and a data port facing data memory. Fetch lines and data lines are then separated by geometry, so they cannot be confused even before colour or glyph. The PC/IP sits at the fetch side.

**B5. Null, L., Lobur, J. *MarieSim: The MARIE Computer Simulator*, with the MarieDPath datapath animator (v1.2).** https://pure.psu.edu/en/publications/mariesim-the-marie-computer-simulator/ (verified as record; venue believed to be ACM JERIC 2003 **[unverified]**).
- *Summary:* A teaching machine whose register set (PC, IR, MAR, MBR, AC) is drawn as fixed boxes on a bus. The datapath animator steps each instruction's register transfers.
- *Lesson:* Show **named registers as fixed labelled boxes**, and in particular IP and SP as readouts on the CPU. At a switch, the reader sees the IP/SP values themselves get saved and replaced, rather than a symbolic ball leaving.

**B6. Guo, P. J. (2013). "Online Python Tutor: Embeddable Web-Based Program Visualization for CS Education." SIGCSE 2013, pp. 579–584.** https://pg.ucsd.edu/publications/Online-Python-Tutor-web-based-program-visualization_SIGCSE-2013.pdf (verified).
- *Summary:* Code is on the left with a current-line arrow. Frames and objects are on the right, with pointer arrows. Readers can step forward and back.
- *Lesson:* There are **two distinct pointer glyphs**: the "current line" arrow into code, and reference arrows into data. Our IP→code marker and SP→stack marker should look different from data-access lines, and both should be anchored at the CPU.

**B7. Sorva, J., Karavirta, V., Malmi, L. (2013). "A Review of Generic Program Visualization Systems for Introductory Programming Education." ACM TOCE 13(4), Art. 15.** https://doi.org/10.1145/2490822 (verified). Also: Sorva, J. (2013), "Notional Machines and Introductory Programming Education," ACM TOCE 13(2): 8:1–8:31 (verified via DBLP).
- *Summary:* A survey of dozens of program visualisers. A visualisation should make a *notional machine* visible: an idealised model of the computer at the right level of abstraction.
- *Lesson:* Decide our notional machine explicitly: **one CPU, one shared code bank, one live data area, three saved bodies, one host block**. Then draw only those. Anything below that level (bus cycles, caches) should be left out.

---

## C. Context switching, isolation and address spaces in OS teaching

**C1. Arpaci-Dusseau, R. H., Arpaci-Dusseau, A. C. (2018, v1.00+). *Operating Systems: Three Easy Pieces*, Ch. 6 "Mechanism: Limited Direct Execution."** https://pages.cs.wisc.edu/~remzi/OSTEP/cpu-mechanisms.pdf (book verified; Figure 6.3 not read directly, and its content is from secondary course slides).
- *Summary:* A timer interrupt returns control to the OS. The context switch saves the running process's registers (to its kernel stack and process structure) and restores the next process's. The book presents this as a timeline with **OS / hardware / program columns**.
- *Lesson:* Two direct lessons. (1) A **periodic timer drives the switch**: our clock is an input with its own fixed position, raising a visible request flag at the CPU. (2) Saved registers go **into a per-process structure**, so the CPU-state card should slide **into a pocket on the outgoing depth's body**, and the incoming card should slide out of its body into the CPU.

**C2. Arpaci-Dusseau & Arpaci-Dusseau, *OSTEP*, Ch. 13 "The Abstraction: Address Spaces," Fig. 13.2 "Three Processes: Sharing Memory."** https://pages.cs.wisc.edu/~remzi/OSFEP/vm-intro.pdf (verified via search).
- *Summary:* Physical memory is drawn as one column. OS code/data sits at the bottom and processes A, B and C each own a region, with one running on the CPU and the others ready. The chapter notes that saving all of memory at every switch is too slow, which motivates address spaces.
- *Lesson:* It is a near-perfect static template: **three owned regions plus one OS/host region, side by side, with the owner marked by region**. It also gives the article a useful contrast. Our system *does* copy memory on every switch, the expensive option OSTEP rejects, so the copy band (A2) should look visibly heavy.

**C3. Buck, J. W., Perugini, S. "An Interactive, Graphical CPU Scheduling Simulator for Teaching Operating Systems." arXiv:1812.05160** (verified as preprint; possibly also SIGCSE 2019 **[unverified]**). https://arxiv.org/abs/1812.05160
- *Summary:* A web simulator of preemptive multitasking, with interrupts, context switches and PCBs, stepped through in units of CPU time.
- *Lesson:* Show the PCB-equivalent as a **first-class object per depth** that fills on save and empties on restore. Never imply that the state simply vanishes.

**C4. UW CSE351 Virtual Memory Simulator, SIGCSE TS 2024 demo** (https://sigcse2024.sigcse.org/details/sigcse-ts-2024-demos/12/Demo-3B-UW-CSE351-Virtual-Memory-Simulator), and **VMV "Work in Progress: A Visualization Aid for Learning Virtual Memory Concepts," ASEE 2022** (https://peer.asee.org/40560). Both verified as listings; authors not confirmed **[unverified]**.
- *Summary:* Step-by-step animated address translation, covering page tables, faults, writes and TLB.
- *Lesson:* A **step-by-step** treatment of one access is how these tools teach a redirect. Our "pin" re-pointing is the same idea in miniature: show one pinned pointer unplug from depth A's heap and plug into depth B's, one step at a time.

**C5. Tversky, B., Morrison, J. B., Bétrancourt, M. (2002). "Animation: can it facilitate?" *IJHCS* 57: 247–262.** https://doi.org/10.1006/ijhc.2002.1017 (verified). **Heer, J., Robertson, G. (2007). "Animated Transitions in Statistical Data Graphics." IEEE TVCG 13(6).** https://doi.org/10.1109/TVCG.2007.70539 (verified in `viz3d-research.md`).
- *Summary:* Animations are often too fast or complex, and people understand continuous processes as discrete steps. Staged transitions that keep object constancy work better.
- *Lesson:* The switch should be **four staged, steppable beats**, never simultaneous: (1) CPU state card out, (2) copy band out/in, (3) patch tiles fly to code, (4) pins replug, then resume. Each object keeps its identity throughout.

**C6. Richter, J., Scheiter, K., Eitel, A. (2016). "Signaling text–picture relations in multimedia learning: A comprehensive meta-analysis." *Educational Research Review* 17: 19–36** (verified; r = 0.17, larger for low-prior-knowledge learners). Data: https://psycharchives.org/handle/20.500.12034/2019
- *Summary:* Cues that link text to picture elements help learning, especially for novices.
- *Lesson:* Every prose term (pin, patch, snapshot, host state) should highlight its glyph when hovered or scrolled to, and vice versa. The **game-view value ↔ data cell link** is a signal of the same kind and should light up on both ends together.

---

## Recommended layout

**Principle.** Use a fixed factory in which only tokens move (A5, B1). The main axis is an I-flow (A1, B2): input on the left, process in the centre, output on the right. Code and data sit on opposite faces of the CPU (B4). Saved state lies on the floor in front, one body per depth (C2). Host state is set apart (A1 "X" rating).

```
  BACK ─────────────────────────────────────────────────────────────
                 [ CODE BANK  (shared, grey; 405 patch sites marked) ]
                                │  fetch port (top/back face)
   [CLOCK]──req─►               ▼
   [KEYBOARD]──────────►  [   CPU   ]  ─────────────►  [ SCREEN ]
                         IP ▸  SP ▸  │  data port              ▲
   ......................................│.................... │ (viewed depth)
   [HOST STATE]          [ LIVE DATA AREA (heap, stack, fb) ]   │
   (fenced; own            pin sockets ◉ ◉        │             │
    material)                       ▲ copy band ▼ │             │
  FRONT/FLOOR   [ DEPTH 0 body ]  [ DEPTH 1 body ]  [ DEPTH 2 body ]
                 slot · patch tray · pins · CPU-state pocket · mini game view
```

**Positions**
- **CPU** sits centre, at the waist of the scene, and never moves. It has an IP readout on the code-facing side and an SP readout on the data-facing side (B5, B6). The fetch port is on the back/top face and the data port on the front/bottom face (B4).
- **Code bank** sits directly behind the CPU, raised. It is one shared object. Its 405 patch sites are shown as notches that a patch tile can sit in (A4).
- **Live data area** sits directly in front of and below the CPU. It holds the heap, stack and screen buffer, plus **pin sockets** where the shared pointers plug in.
- **Depth bodies (×3)** lie on the floor in front, in a row, each in one depth hue. Each body contains: its **snapshot slot** (a slab whose size reflects its byte count), a **patch tray** of tiles, its **pin plugs**, a **CPU-state pocket** (C1, C3), and a **mini game view** standing on it, linked by a leader line to the data cell that drives its on-screen value (C6). Inactive bodies are dimmed but never hidden.
- **Host state** sits to the left of the floor, fenced and in a different material, with no copy band touching it. Its separation *is* the explanation (A1, C2).
- **Keyboard and clock** are both on the far left as inputs. The clock posts a request flag at the CPU every 4 frames (C1). The keyboard's line runs to the **screen selector**, not to the CPU's work.
- **Screen** is on the far right. One physical screen is joined by a single lit cable to the viewed depth's frame buffer. Pressing a key visibly re-routes that cable; nothing else changes.

**Flow types, glyphs and motion (one glyph per type, A4; arrows, A3)**

| Flow | Glyph | Motion / when | Colour |
|---|---|---|---|
| Instruction fetch | short dash pulses on a fixed back conduit, chevrons pointing *into* the CPU | continuous while a depth runs | neutral grey-white |
| Data read / write | round bead on the front conduit; read = bead toward CPU, write = bead away, with arrowhead | continuous; the written cell gets a decaying outline (A8) | neutral; target cell flashes |
| Save / restore copy | Sankey band, width ∝ bytes (A2) | switch beat 2 only, out then in | outgoing hue, then incoming hue |
| Code patches | small square tiles | switch beat 3: tiles lift from code notches back into the outgoing tray, then the incoming depth's tiles drop in | depth hue |
| Pins | plug-and-cable | switch beat 4: unplug from socket, replug to the incoming body | depth hue on the cable end |
| CPU state (IP, SP…) | a card | switch beat 1: slides from CPU into the outgoing pocket; incoming card slides into the CPU | depth hue |
| Switch request | a flag on the CPU | raised by a clock tick, lowered when beat 1 starts | amber (the one alert colour, A10) |
| View selection | a single screen cable | re-routes when a key is pressed | white |

**Colour rule (A8).** Hue means **owner (depth)** and nothing else. Last-writer is shown by a fading outline, and alerts use amber. The machine itself (CPU, code, live area, host) is grey. The current owner is shown by which body is lit and by the depth hue on the CPU's state card. The CPU is never repainted.

**Pacing (C5).** During normal running, show only the fetch and data flows, which keeps the view calm. A switch plays as four labelled beats with a step control, and the Marey strip below the floor records the schedule over time (A7).
