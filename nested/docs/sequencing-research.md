# Sequencing the "Three Dooms in One Process" explorable

Research basis plus a concrete chapter and scene order.

**How the references were checked.** I confirmed the bibliographic details (title, venue, year, and usually the DOI) with web searches on 2026-10-24. The Crossref API could not be reached from this environment, so I could not resolve every DOI directly. The status column says how each entry was checked:
- **V**: the details and the DOI or URL were confirmed by search results (publisher page, repository record, or several matching reference lists).
- **V-cit**: the citation details were confirmed but the DOI was not seen in the results. The DOI given comes from memory and should be checked at doi.org before publishing.
- **M**: from memory only, not checked in this session. The details are believed correct but must be checked before citing.

---

## Part A. References and what each implies for ordering and chunking

### A1. Programmed instruction and its descendants

| # | Reference | Status |
|---|---|---|
| 1 | Skinner, B. F. (1958). Teaching machines. *Science*, 128(3330), 969–977. https://doi.org/10.1126/science.128.3330.969 | M |
| 2 | Crowder, N. A. (1960). Automatic tutoring by intrinsic programming. In A. A. Lumsdaine & R. Glaser (Eds.), *Teaching Machines and Programmed Learning: A Source Book* (pp. 286–298). Washington, DC: NEA. No DOI. History summary: https://elearn.inf.tu-dresden.de/history/system_tut.htm ; overview: https://pressbooks.pub/lidtfoundations/chapter/programmed-instruction | V-cit (sources disagree on whether branching dates from 1958, 1960 or 1962) |

- **Skinner (1958).** Material is broken into small frames, each needs an active response, prompts are faded as competence grows, and the learner moves at their own pace.
  - *Implication:* the existing Next/Back steps with prediction questions are already Skinnerian frames, so the widget itself is not what is weak. Two pieces are missing. First, fading of prompts: later scenes about the same mechanism should give less scaffolding. Second, an ordering of frames along a deliberate gradient. Each frame should depend only on frames before it.
- **Crowder (1960).** "Intrinsic" (branching) programs use the learner's answer to a multiple-choice frame to route them on: forward, or to a remedial frame aimed at that specific misconception.
  - *Implication:* the prediction questions should branch. A wrong prediction (for example, "the scheduler's turn counter gets restored too") should open a short remedial side-scene, then rejoin the main line. Readers who answer correctly skip ahead. This is also the cheapest way to handle the expertise reversal effect (see #12).

### A2. Gagné: hierarchies, prerequisite analysis, events of instruction

| # | Reference | Status |
|---|---|---|
| 3 | Gagné, R. M. (1962). The acquisition of knowledge. *Psychological Review*, 69(4), 355–365. https://doi.org/10.1037/h0042650 | M |
| 4 | Gagné, R. M. (1968). Learning hierarchies. *Educational Psychologist*, 6(1), 1–9. https://doi.org/10.1080/00461526809528968 | V-cit |
| 5 | Gagné, R. M. (1965; 4th ed. 1985). *The Conditions of Learning*. New York: Holt, Rinehart & Winston. Summary: https://en.wikipedia.org/wiki/Conditions_of_Learning | M (book, no DOI) |

- **Gagné (1962, 1968).** A complex capability rests on a hierarchy of subordinate capabilities. You find them by asking "what would the learner need to already be able to do in order to do this?" and recursing. Positive transfer runs upward through the hierarchy, so instruction should be sequenced bottom-up within it.
  - *Implication:* build the prerequisite graph (Part B) by asking that question for each target mechanism. For example: "To understand why the host's turn counter must live outside every snapshot, the reader must already be able to say what a snapshot restores." Never place a scene before its subordinates.
- **Gagné (1965/1985), the nine events of instruction.** The events run in this order: gain attention; inform of objective; stimulate recall of prerequisites; present content; provide guidance; elicit performance; give feedback; assess; enhance retention and transfer.
  - *Implication:* this is a template for a chapter, not for the whole article. Each chapter should open with a one-line objective ("by the end you can trace a switch from A's stack to B's") and a recall prompt for the prerequisite it uses. It should end with an unguided performance (a prediction with no hints) and a transfer check.

### A3. Elaboration Theory, advance organizers, subsumption

| # | Reference | Status |
|---|---|---|
| 6 | Reigeluth, C. M., Merrill, M. D., Wilson, B. G., & Spiller, R. T. (1980). The elaboration theory of instruction: A model for sequencing and synthesizing instruction. *Instructional Science*, 9(3), 195–219. https://doi.org/10.1007/BF00177327 ; author page: https://www.reigeluth.net/pubsinsttheor | V |
| 7 | Ausubel, D. P. (1960). The use of advance organizers in the learning and retention of meaningful verbal material. *Journal of Educational Psychology*, 51(5), 267–272. https://doi.org/10.1037/h0046669 | V-cit |

- **Reigeluth et al. (1980).** Instruction should start with an *epitome*. This is a simple but real and complete version of the whole idea, not an abstract overview. Each later level then *elaborates* one part of it, and frequent *synthesizers* and *summarizers* show how the new detail fits the whole. The theory compares this to a zoom lens: wide shot, zoom in, zoom back out.
  - *Implication:* this is the single most important change. The article needs a first scene that runs the entire system in its simplest true form: three games, one baton passed at frame ends, memory swapped in and out, one screen shown, one keyboard. All of these are black boxes, with no registers and no addresses. Every later chapter then opens one box and closes by zooming back out to the same picture.
- **Ausubel (1960).** An advance organizer is introductory material at a *higher level of abstraction and generality* than the lesson. It gives anchoring ideas under which the new detail can be subsumed. Ausubel distinguished it from an overview or summary at the same level of detail.
  - *Implication:* put one familiar organizer before the epitome. Example: "an OS time-slicing processes, but done by hand, inside one program, by someone who can't change the program." Each later mechanism can then be subsumed under it: context switch, address-space isolation, a process loader, a display owner, input focus, context-switch cost. A good organizer is at a higher level than the article, not a table of contents.

### A4. Intelligent tutoring systems, knowledge components, mastery, prerequisite graphs

| # | Reference | Status |
|---|---|---|
| 8 | Koedinger, K. R., Corbett, A. T., & Perfetti, C. (2012). The Knowledge-Learning-Instruction framework: Bridging the science-practice chasm to enhance robust student learning. *Cognitive Science*, 36(5), 757–798. https://doi.org/10.1111/j.1551-6709.2012.01245.x ; repository: https://kilthub.cmu.edu/articles/journal_contribution/The_knowledge-learning-instruction_framework_bridging_the_science-practice_chasm_to_enhance_robust_student_learning_/6470522 | V-cit (citation and repository confirmed) |
| 9 | Corbett, A. T., & Anderson, J. R. (1994). Knowledge tracing: Modeling the acquisition of procedural knowledge. *User Modeling and User-Adapted Interaction*, 4(4), 253–278. https://doi.org/10.1007/BF01099821 | V |
| 10 | Bloom, B. S. (1968). Learning for mastery. *Evaluation Comment*, 1(2). UCLA Center for the Study of Evaluation of Instructional Programs. No DOI; I found no ERIC record. | V-cit |
| 11 | Bloom, B. S. (1984). The 2 sigma problem: The search for methods of group instruction as effective as one-to-one tutoring. *Educational Researcher*, 13(6), 4–16. https://doi.org/10.3102/0013189X013006004 | M |
| 12 | Doignon, J.-P., & Falmagne, J.-C. (1985). Spaces for the assessment of knowledge. *International Journal of Man-Machine Studies*, 23(2), 175–196. https://doi.org/10.1016/S0020-7373(85)80031-6 | M |

- **KLI framework (2012).** Learning is analysed as the acquisition of *knowledge components* (KCs): units of knowledge that predict performance on particular tasks. KCs differ by kind, from simple facts and constant-to-constant mappings up to variable rules and principles. Different kinds call for different instruction: memory and fluency practice, induction from examples, or understanding and sense-making.
  - *Implication:* write each scene's single target KC down explicitly (Part B). The KC kind should decide the widget:
    - Terms (ESP, callee-saved) are facts: pre-train them, and do not explain them mid-animation.
    - The switch routine is a procedure: use a worked example, then fade it.
    - "Host state must be outside every snapshot" is a principle: use a prediction plus explanation, and contrast a correct case with a broken one.
- **Corbett & Anderson (1994).** Cognitive Tutors estimate, for each KC, the probability that the learner has mastered it, and keep giving practice on a KC until that probability passes a threshold.
  - *Implication:* an article cannot trace knowledge, but it can approximate the idea. A chapter's closing prediction acts as a mastery gate for the KC that the next chapter depends on. If the reader gets it wrong, offer the remedial branch (Crowder) before letting them continue.
- **Bloom (1968, 1984).** With formative checks and corrective loops, most learners reach mastery of a unit before moving on, and the outcomes come close to those of tutoring.
  - *Implication:* the same point. The chapter-end checks are formative and corrective, not scores, and they matter most at the bottleneck KCs (switch, snapshot restore) that everything later depends on.
- **Doignon & Falmagne (1985), knowledge space theory.** The set of feasible knowledge states is constrained by prerequisite relations among items, which forms a surmise relation and a partial order. Any order consistent with that partial order is a valid learning path.
  - *Implication:* the graph in Part B is a partial order. Several total orders are legal. Choose among them by Elaboration Theory (core mechanism first) and by narrative motivation (each chapter's failure motivates the next chapter), not by the order the code runs in.

### A5. Worked examples, fading, expertise reversal, 4C/ID

| # | Reference | Status |
|---|---|---|
| 13 | Atkinson, R. K., Derry, S. J., Renkl, A., & Wortham, D. (2000). Learning from examples: Instructional principles from the worked examples research. *Review of Educational Research*, 70(2), 181–214. https://doi.org/10.3102/00346543070002181 | M |
| 14 | Renkl, A., & Atkinson, R. K. (2003). Structuring the transition from example study to problem solving in cognitive skill acquisition: A cognitive load perspective. *Educational Psychologist*, 38(1), 15–22. https://doi.org/10.1207/S15326985EP3801_3 | V-cit (title confirmed; DOI not seen in results) |
| 15 | Atkinson, R. K., Renkl, A., & Merrill, M. M. (2003). Transitioning from studying examples to solving problems: Effects of self-explanation prompts and fading worked-out steps. *Journal of Educational Psychology*, 95(4), 774–783. https://doi.org/10.1037/0022-0663.95.4.774 | V-cit (existence confirmed; the exact subtitle and DOI are from memory) |
| 16 | Kalyuga, S., Ayres, P., Chandler, P., & Sweller, J. (2003). The expertise reversal effect. *Educational Psychologist*, 38(1), 23–31. https://doi.org/10.1207/S15326985EP3801_4 ; repository: https://ro.uow.edu.au/edupapers/136 | V-cit |
| 17 | van Merriënboer, J. J. G., Clark, R. E., & de Croock, M. B. M. (2002). Blueprints for complex learning: The 4C/ID-model. *Educational Technology Research and Development*, 50(2), 39–64. https://doi.org/10.1007/BF02504993 | V (sources disagree on the page range) |
| 18 | van Merriënboer, J. J. G., Kirschner, P. A., & Kester, L. (2003). Taking the load off a learner's mind: Instructional design for complex learning. *Educational Psychologist*, 38(1), 5–13. https://doi.org/10.1207/S15326985EP3801_2 | M |

- **Atkinson et al. (2000) and Renkl & Atkinson (2003).** Early on, studying worked examples beats solving problems. The examples should be faded step by step, one solution step at a time, so the learner moves from studying to completing to solving. Self-explanation prompts at the faded steps make fading work better (#15). Backward fading, which removes the last step first, works well.
  - *Implication:* the two procedures in the article are the context switch and the snapshot swap. Teach each as a full worked example first. Then show the same procedure again with the last step blank ("you load ESP: which value?"). Then with two steps blank. Finally, the reader orders or completes the whole thing. Most later mechanisms (patched bytes, pinned pointers, blit-source patch) reuse the same save, overwrite, restore pattern, so they can start partly faded.
- **Kalyuga et al. (2003).** Guidance that helps novices (full worked examples, integrated explanations) becomes redundant for experienced learners and can harm them.
  - *Implication:* systems programmers will already know stack switching. Give a visible "I know x86 stack switching, skip to the trick" fork after the epitome (a Crowder branch). Each chapter should also have a fast path, such as one dense diagram, for readers who answer the opening prediction correctly.
- **4C/ID (2002) and van Merriënboer et al. (2003).** Complex learning is organised around *whole learning tasks*, ordered in *task classes* from simple to complex. Within each class, support fades. Supportive information comes before a task class; procedural "just-in-time" information is given at the moment of need; and *part-task practice* is used only for recurrent skills that need automaticity. The 2003 paper adds simple-to-complex sequencing of whole tasks as a way to manage intrinsic load.
  - *Implication:* the whole task is "trace one complete tick of the three-instance system." Present it several times at increasing fidelity: epitome with black boxes, then with real switching, then with snapshots, then with I/O, then with cost. That repetition is the whole-task return. Register names and calling conventions are procedural information, so show them on demand as hover popovers rather than lecturing them. The only part-task drill worth including is "which memory is live right now?"

### A6. Cognitive load: segmenting, pre-training, signaling, isolated elements, productive failure

| # | Reference | Status |
|---|---|---|
| 19 | Mayer, R. E., & Moreno, R. (2003). Nine ways to reduce cognitive load in multimedia learning. *Educational Psychologist*, 38(1), 43–52. https://doi.org/10.1207/S15326985EP3801_6 | M |
| 20 | Mayer, R. E., & Chandler, P. (2001). When learning is just a click away: Does simple user interaction foster deeper understanding of multimedia messages? *Journal of Educational Psychology*, 93(2), 390–397. https://doi.org/10.1037/0022-0663.93.2.390 | V |
| 21 | Mayer, R. E., Mathias, A., & Wetzell, K. (2002). Fostering understanding of multimedia messages through pre-training: Evidence for a two-stage theory of mental model construction. *Journal of Experimental Psychology: Applied*, 8(3), 147–154. https://doi.org/10.1037/1076-898X.8.3.147 | M (the search did not surface it) |
| 22 | Rey, G. D., Beege, M., Nebel, S., Wirzberger, M., Schmitt, T. H., & Schneider, S. (2019). A meta-analysis of the segmenting effect. *Educational Psychology Review*, 31, 389–419. https://doi.org/10.1007/s10648-018-9456-4 | V |
| 23 | Richter, J., Scheiter, K., & Eitel, A. (2016). Signaling text-picture relations in multimedia learning: A comprehensive meta-analysis. *Educational Research Review*, 17, 19–36. https://doi.org/10.1016/j.edurev.2015.12.003 | M |
| 24 | Pollock, E., Chandler, P., & Sweller, J. (2002). Assimilating complex information. *Learning and Instruction*, 12(1), 61–86. https://doi.org/10.1016/S0959-4752(01)00016-0 ; repository: https://ro.uow.edu.au/edupapers/145 | V-cit |
| 25 | Kapur, M. (2008). Productive failure. *Cognition and Instruction*, 26(3), 379–424. https://doi.org/10.1080/07370000802212669 | M |
| 26 | Loibl, K., Roll, I., & Rummel, N. (2017). Towards a theory of when and how problem solving followed by instruction supports learning. *Educational Psychology Review*, 29, 693–715. https://doi.org/10.1007/s10648-016-9379-x | M |
| 27 | Cowan, N. (2001). The magical number 4 in short-term memory: A reconsideration of mental storage capacity. *Behavioral and Brain Sciences*, 24(1), 87–114. https://doi.org/10.1017/S0140525X01003922 | M |

- **Segmenting (Mayer & Chandler 2001; Rey et al. 2019).** Learner-paced segments improve transfer. The meta-analysis finds small to medium effects on retention and transfer, lower overall load, and longer learning time.
  - *Implication:* the step-through format is supported. The research says nothing about the order of segments, which is the weak point. Segment boundaries should fall at meaningful causal joints (for example "registers saved" and "ESP published"), not at arbitrary animation frames.
- **Pre-training (Mayer, Mathias & Wetzell 2002).** Learning the names and behaviour of components *before* a causal explanation helps. Learners build component models first and the causal model second.
  - *Implication:* add a short "parts list" scene before the first mechanism. It should name and show ESP, a stack frame, callee-saved registers, a snapshot region, the framebuffer and blit, and a scancode. Each part is a static, labelled object in the persistent diagram. The causal animations then only have to show *motion*, not introduce nouns.
- **Signaling (Richter et al. 2016; Mayer & Moreno 2003).** Cues that link words to picture parts, and highlight structure, help learning, especially for novices.
  - *Implication:* use one persistent diagram (Part D) throughout. Give each instance a colour (A, B, C) and give the host its own neutral colour. Highlight only the parts that change in the current step. Colour every memory region by owner, so "whose bytes are live?" can be read at a glance.
- **Isolated-then-interacting elements (Pollock, Chandler & Sweller 2002).** High element-interactivity material is learned better when elements are first presented in isolation, with the interactions deliberately left out, and the full interacting version is given second.
  - *Implication:* this is direct licence for a two-pass structure. Teach switching *without* memory isolation, and isolation *without* the cost question. Then reassemble. It also justifies teaching the epitome with deliberate simplifications ("pretend the memory just swaps"), then admitting what was left out.
- **Productive failure (Kapur 2008; Loibl, Roll & Rummel 2017).** Attempting a problem before instruction can improve conceptual understanding and transfer. This depends on the attempt activating prior knowledge, exposing knowledge gaps, and being followed by instruction that contrasts the learners' solutions with the canonical one.
  - *Implication:* use it sparingly, at two points. First, after switching works but before snapshots: "You've switched stacks. Run it. Predict what B sees." The answer is A's globals, and the game corrupts. Second, at the cost chapter: "design a cheaper swap" before revealing the hot-set and compare strategies. In both cases, follow immediately with the canonical answer contrasted with the typical wrong answer.
- **Cowan (2001).** Working memory holds about four chunks.
  - *Implication:* this sets the chunk limits in Part C. At most about 4 *new interacting* elements should be active in any one scene.

### A7. Visualizations and systems or OS teaching specifically

| # | Reference | Status |
|---|---|---|
| 28 | Hundhausen, C. D., Douglas, S. A., & Stasko, J. T. (2002). A meta-study of algorithm visualization effectiveness. *Journal of Visual Languages and Computing*, 13(3), 259–290. https://doi.org/10.1006/jvlc.2002.0237 | V |
| 29 | Naps, T. L., et al. (2002). Exploring the role of visualization and engagement in computer science education. *ACM SIGCSE Bulletin*, 35(2), 131–152 (ITiCSE 2002 Working Group report). https://doi.org/10.1145/960568.782998 | M |
| 30 | Tversky, B., Morrison, J. B., & Bétrancourt, M. (2002). Animation: Can it facilitate? *International Journal of Human-Computer Studies*, 57(4), 247–262. https://doi.org/10.1006/ijhc.2002.1017 | M |
| 31 | Sorva, J., Karavirta, V., & Malmi, L. (2013). A review of generic program visualization systems for introductory programming education. *ACM Transactions on Computing Education*, 13(4), Article 15. https://doi.org/10.1145/2490822 | M |
| 32 | Buck, J., & Perugini, S. (2019). An interactive, graphical CPU scheduling simulator for teaching operating systems. SIGCSE '19 demo. Preprint: https://arxiv.org/abs/1812.05160 | V (a demo paper with no learning evaluation; the author list is from the search summary) |

- **Hundhausen et al. (2002).** Across 24 experiments, *how* learners engage with a visualization (predicting, constructing, answering) matters more than what it shows. Passive viewing often gave no benefit.
  - *Implication:* keep the prediction questions and place them before the reveal at every bottleneck KC. Also add one *construction* task per procedure, such as ordering the switch steps or dragging regions into the snapshot.
- **Naps et al. (2002), the engagement taxonomy.** The levels run: no viewing, viewing, responding, changing, constructing, presenting. The claim is that higher levels produce better learning.
  - *Implication:* raise the engagement level across the article. Early scenes are viewing and responding (predict). Middle scenes are changing (toggle "keep host state inside the snapshot" and watch the system break). The finale is constructing (assemble a correct switch-plus-swap sequence).
- **Tversky et al. (2002).** Many reported animation benefits come from extra information or interactivity, not from the animation itself. Animations help when they are *apprehensible* (slow, segmented, controllable) and *congruent* with the conceptual structure.
  - *Implication:* the animation's units should match the conceptual units, one KC per animated transition. Readers need a static "state" view at each step to compare against, not only motion.
- **Sorva et al. (2013) and Buck & Perugini (2019).** These are the closest computing-education precedents: program and memory-state visualizations, and OS schedulers with step-through context switches.
  - *Implication:* OS education tools show context switches as stepped state transitions with a persistent view of the process table. That is the convention readers from OS courses will expect. I found no controlled study specifically on *sequencing* context-switch or memory-isolation instruction, so the ordering below comes from the general principles above.

---

## Part B. Knowledge-component graph

Each KC is labelled with its KLI kind: F = fact or term, P = procedure, R = principle or rationale. An arrow means "is prerequisite for".

```
PRE-TRAINING TERMS (F)                     ORGANIZER
 T1 stack / stack frame / ret addr          K0 "hand-built time-slicing:
 T2 ESP (stack pointer register)                three games, one CPU, one
 T3 callee-saved regs (EBX ESI EDI EBP)         screen, one keyboard"  (epitome)
 T4 game main loop / frame (tic)                    |
 T5 global variable / fixed address                 |
 T6 framebuffer + blit (copy to screen)             |
 T7 scancode / keyboard handler                     |
                                                    v
T1,T2,T3 ──> K1 execution context = regs + stack (R)
K1 ───────> K2 cooperative switch: save callee-saved, publish ESP,
             load other ESP, ret (P)                         [BOTTLENECK]
K2,T4 ────> K3 yield at frame boundary (where/when switch is safe) (R)
K3,T5 ────> K4 shared-globals collision: one image, three games (R)  [motivating failure]
K4 ───────> K5 snapshot regions: copy out / copy in at switch (P)   [BOTTLENECK]
K5 ───────> K6 private memory: per-instance, never copied (R)
K5 ───────> K7 pinned pointers: overwritten per instance (P)
K5 ───────> K8 patched code bytes saved/restored (P)
K2,K5 ────> K9 host control state outside every snapshot (R)        [KEY INSIGHT]
K4,K6 ────> K10 reentrant loader (load the same binary 3x) (R+P)
K10,K1 ───> K11 per-instance parameter block passed in a register (P)
K8,T6 ────> K12 one screen: patch blit source address (P)
K9,T7,K3 ─> K13 one keyboard: single shared "authority" word (R)
K5..K8,K3 > K14 switch cost: copy-all vs cached hot set vs compare-every-switch (R)
ALL ──────> K15 whole-tick trace + fault diagnosis (integration/transfer)
```

How to read the graph:
- **Two bottlenecks.** K2 (the switch) and K5 (snapshot swap) gate everything else. They get full worked examples, faded repeats, and a mastery check.
- **K9 is the conceptual pivot.** It needs both the switch (who decides) and the snapshot (what gets rolled back). It cannot come earlier than the end of the isolation chapter.
- **K7, K8 and K12 share one schema.** That schema is "save the original, write the per-instance value, restore on the way out." Teach it once, in full, through K5 or K8, then let K7 and K12 reuse it with faded support (a worked-example to completion transfer).
- **K10 and K11 can legally come early,** since loading happens first at runtime. But K10's rationale is K4 (global state) applied to the loader, so it is learned better after K4. Code-execution order is not learning order.
- **K13 is K9 applied to input.** K12 is K8 applied to output. Present them together as a contrast pair: output is chosen by patching code, input by a shared word.

---

## Part C. Proposed sequence

Chunk-size rules, derived from Cowan, Pollock et al., Mayer segmenting, and the KLI framework:
- **Per step** (one Next click): exactly 1 new change, meaning one thing moves or one value changes. Highlight only that change.
- **Per scene** (one widget): 1 target KC, 5–9 steps, and at most 3–4 new *interacting* elements. Terms that were pre-trained do not count. End with one prediction or construction prompt.
- **Per chapter:** 2–4 KCs, 3–6 scenes, about 5–10 minutes. One objective line at the top, one recall prompt, and one zoom-out at the end.
- **Whole article:** 8 chapters (0–7), and about 15 KCs plus the pre-trained terms.

### Chapter 0: The organizer and the epitome (whole task, black boxes)
1. **Organizer (Ausubel).** "An operating system runs many programs on one CPU by taking turns and keeping their memory apart. This project does the same thing by hand, inside one DOS program, to a game that was never written to share." Use one static image. No mechanism yet.
2. **Epitome (Reigeluth), with 4C/ID task class 1.** A 6–8 step animation of three coloured Doom instances. Each runs one frame, hands a baton, and its memory slides out while the next one's slides in. The screen shows only A, and keys go only to A. All mechanisms are labelled boxes: "switch", "swap", "loader", "screen picker", "key owner".
3. **Prediction (gain attention, Gagné event 1).** "If all three share one copy of the game's code and variables, what goes wrong first?" Leave the question open; Chapter 3 answers it.
4. **Map of the article.** The same diagram with its boxes numbered by chapter. This diagram is the persistent anchor used in every later zoom-out.
5. **Expertise fork (Kalyuga, Crowder).** "Comfortable with x86 stack switching? Skip Chapter 2's worked example." The link goes to 2.4.

### Chapter 1: Pre-training the parts (facts only, no causality)
- 1.1 A stack and a stack frame. ESP points at the top (T1, T2).
- 1.2 Callee-saved registers. A tiny static table shows which registers a function must preserve (T3).
- 1.3 A game frame (tic): the main loop as a circle; the frame end marked (T4).
- 1.4 Globals live at fixed addresses, shown as one strip of memory (T5).
- 1.5 Framebuffer and blit, and a scancode reaching a keyboard handler (T6, T7).

Each scene is 3–5 steps with a one-tap recognition check ("which of these is ESP?"). These parts then appear, with the same shapes and labels, in the persistent diagram. Readers on the expertise fork skim this as a single glossary card.

### Chapter 2: Taking turns (K1, K2, K3). Bottleneck 1: worked example, then fading.
- 2.1 **Recall and objective.** "A context = the registers + the stack. Goal: move the CPU from A's context to B's." (K1)
- 2.2 **Fully worked switch (K2).** Push the callee-saved registers, store ESP into A's slot, load B's ESP, pop, `ret` into B. That is 6–8 steps, with ESP and the two stacks always visible.
- 2.3 **Faded once (backward fading, Renkl & Atkinson).** The same switch from B to C, with the last step blank: "which ESP is loaded?" Add a self-explanation prompt: "why does `ret` land in C's code?"
- 2.4 **Faded twice and construction (Naps).** The reader orders shuffled steps for C to A. A wrong order shows the crash (for example, ESP loaded before the save). This is the mastery gate for K2, with a remedial branch back to 2.2.
- 2.5 **Where to yield (K3).** Why a frame boundary? Predict what happens if you switch mid-render (a half-drawn frame or held locks). Reveal the single yield call at the end of the tic.
- 2.6 **Whole-task return, level 2.** The epitome again with the "switch" box opened. Three games now take turns for real, and the memory box is still closed. End with the cliff-hanger: "Run it." (This leads into Chapter 3.)

### Chapter 3: Keeping them apart (K4, K5, K6, K7, K8, K9). Bottleneck 2, then the pivot.
- 3.1 **Productive-failure reveal (K4).** It answers the Chapter 0 prediction. B wakes up reading A's player position and level state, because all three share one set of globals. Contrast the typical guess ("their stacks collide") with the real answer: stacks are already separate, globals are not.
- 3.2 **Fully worked snapshot swap (K5).** At yield, copy A's regions out to A's save area, then copy B's in. Colour bytes by owner. 6–8 steps.
- 3.3 **Faded swap.** For B to C, the reader drags which regions go out and in. This is the mastery gate for K5.
- 3.4 **Private memory (K6).** Some memory never needs copying because each instance gets its own. Contrast it with 3.2: copying costs time, private memory costs address-space planning. Raise the question "then why not make everything private?" (answered by K7 and K8).
- 3.5 **Pinned pointers (K7), shown partly faded.** Some addresses are baked in and cannot move, so overwrite them per instance. Show it as an instance of the save, overwrite, restore schema, with the reader filling the restore step.
- 3.6 **Patched code bytes (K8), more faded.** The same schema applied to code. The reader predicts what must be saved before patching.
- 3.7 **Key insight (K9). Do not shortcut this scene.**
  - Prediction: "The switcher keeps a 'whose turn' counter. Should it be in a snapshot region?" Let the reader toggle it into a snapshot (the "changing" engagement level) and watch the restore roll the counter back, so A runs forever, or the switch returns into the wrong stack.
  - Then reveal the rule: the host's control state lives outside every snapshot.
  - This is a principle-level KC, so explain the rationale explicitly.
- 3.8 **Whole-task return, level 3, and recap.** The persistent diagram now has switch plus swap opened. One step-through of a full tick with memory ownership colours. End with a summary card listing K4 to K9 in a single line each.

### Chapter 4: Starting three (K10, K11)
- 4.1 **Recall.** "Globals collide (Ch. 3). The loader has globals too."
  - K10: loading the same binary three times needs a reentrant loader. It must not keep its own state in globals, and it must place each instance's private memory.
  - Use a prediction: "what happens on the second load if the loader remembers 'already initialised'?"
- 4.2 **Parameter block in a register (K11).** How does instance B know it is B? A pointer is passed in a register at entry. Link back to K1 ("registers are part of the context").
- 4.3 **Zoom-out.** Add the loader to the left of the persistent diagram, as the start of each instance's life.

### Chapter 5: One screen, one keyboard (K12, K13). A contrast pair.
- 5.1 **Screen (K12), faded quickly.** All three render, and one is shown. The blit's source address is patched to point at the chosen instance's framebuffer. The reader should recognise the schema from 3.6 and fill most steps themselves.
- 5.2 **Keyboard (K13).** There is one physical keyboard and one handler. A single shared authority word, stored *outside* the snapshots (callback to K9), says which instance owns input. Use a prediction: "if the authority word were inside a snapshot, who would get the keys?"
- 5.3 **Side-by-side contrast.** Output is chosen by patching code (K8 family). Input is chosen by shared host state (K9 family). One static two-column card.
- 5.4 **Whole-task return, level 4.** A full tick including the player pressing a key and seeing A's screen. Then switch the viewed instance and watch both the patch and the authority word change.

### Chapter 6: What switching costs (K14)
- 6.1 **Productive-failure prompt.** "Copying every region every switch is slow. Propose something cheaper." Use free choice among sketches.
- 6.2 **Three strategies on one timeline.** Copy-everything; a cached hot set (copy only what is usually dirty); compare-every-switch (diff and copy only changed bytes). Show the per-switch cost bars and how often switches happen (K3).
- 6.3 **Trade-off prediction.** "Which wins when instances touch little memory per frame? When they touch a lot?"
- 6.4 **Fast path (expertise reversal).** Readers who answer 6.3 correctly get a single summary chart. Others step through the reasoning.

### Chapter 7: The whole picture (integration and transfer)
- 7.1 **Full trace, highest fidelity.** One complete round A to B to C to A, with every opened box visible. The reader controls speed and can scrub.
- 7.2 **Fault diagnosis (construct and transfer).** Six "what breaks?" cards, each removing one mechanism. Each card names the symptom and asks for the cause:
  - no callee-saved save
  - host counter inside a snapshot
  - pinned pointer not restored
  - blit patch not reverted
  - authority word snapshotted
  - non-reentrant loader
- 7.3 **Return to the organizer.** Map each mechanism back to its OS analogue: context switch, address space, process loader, display owner, input focus, context-switch cost. This closes Ausubel's subsumption loop and gives transfer beyond this project.

---

## Part D. Recaps and "whole picture" moments

- **One persistent master diagram,** introduced in Chapter 0. Every chapter zooms *into* one box of it and ends by zooming *back out* to it, with the newly understood box now open (Reigeluth's synthesizer, plus signaling).
  - Instances always keep their colours.
  - The host is a neutral colour and is drawn *outside* the memory band that snapshots touch. Showing this from Chapter 0 quietly prepares K9.
- **Whole-task returns at increasing fidelity (4C/ID task classes)** come at 0.2, 2.6, 3.8, 5.4 and 7.1. That is five passes over the same tick, each with one more box opened.
- **Recall prompts at the start of each chapter** (Gagné event 3) name the one prerequisite KC from the graph, for example Chapter 4 starts with K4 and Chapter 5.2 with K9.
- **Summary cards** (Reigeluth's summarizer) come at the end of Chapters 2, 3 and 5. Each has one line per KC.
- **Mastery gates with remedial branches** come only at the bottlenecks: 2.4 (K2), 3.3 (K5) and 3.7 (K9). Do not gate everything.
- **Schema-reuse callouts** are explicit "same pattern as 3.6" tags at 3.5, 5.1 and 5.2, so that faded support is visibly justified.

## Part E. Diagnosis of the current "weak sequence" in one paragraph

The individual widgets already have segmenting, active prediction and learner pacing, which are the best-supported micro-features (Mayer & Chandler 2001; Rey et al. 2019; Hundhausen et al. 2002). Four macro-structures are missing:
1. An epitome or organizer, so the reader knows what each widget is a part of (Reigeluth 1980; Ausubel 1960).
2. Prerequisite-ordered dependence, so no widget assumes a later one (Gagné 1968).
3. Fading across widgets that share a schema, so repeated patterns get faster rather than being re-explained (Renkl & Atkinson 2003).
4. Repeated whole-task returns that reassemble the isolated parts (Pollock et al. 2002; van Merriënboer et al. 2002).

The proposal above adds all four without changing the widget format.
