# Research grounding: animating program and machine state

For an explorable article on running several Doom (DOS) engine instances in one process, using cooperative stack switching, per-instance memory snapshots, and patched-code isolation.

**How this was checked.** I confirmed each entry's title, authors, venue and year with web searches against publisher, DBLP, author-page or repository records. Direct HTTP fetches (doi.org, dl.acm.org, Crossref) were blocked from this sandbox, so I could not click through the links myself. Each DOI below is one that a bibliographic record or author page reported. Where sources disagree, or a detail is unconfirmed, the entry says so. Most summaries come from abstracts, not full-text reading.

---

## 1. Visualizing program state: stack frames, heap, pointers, memory layout

### 1.1 Online Python Tutor: Embeddable Web-Based Program Visualization for CS Education
- **Authors:** Philip J. Guo
- **Venue:** SIGCSE 2013, pp. 579–584
- **URL:** https://pg.ucsd.edu/publications/Online-Python-Tutor-web-based-program-visualization_SIGCSE-2013.pdf (author PDF). I did not find the DOI in search results, so look it up in the ACM DL.
- **Summary:** A web tool that runs a program and lets learners step forwards and backwards through execution, showing frames, objects and pointer arrows at each step.
- **Design lesson:** Use Python Tutor's layout grammar as your base encoding: frames in a column on the left, heap objects on the right, arrows for references, and a step slider that goes both ways. Readers already know it, so your stack-switching diagram can add one thing to it: several stacks side by side, with only one "active".

### 1.2 Ten Million Users and Ten Years Later: Python Tutor's Design Guidelines for Building Scalable and Sustainable Research Software in Academia
- **Authors:** Philip J. Guo
- **Venue:** UIST 2021, pp. 1235–1251 (Best Paper)
- **URL:** https://doi.org/10.1145/3472749.3474819
- **Summary:** A retrospective on the design choices that let Python Tutor scale and last, including its deliberately narrow, stable feature set.
- **Design lesson:** Keep the visual vocabulary small and do not change it. Fix a few glyphs (frame box, heap block, pointer arrow, "current SP" marker) and reuse them in every figure, rather than inventing a new diagram style for each section.

### 1.3 A Review of Generic Program Visualization Systems for Introductory Programming Education
- **Authors:** Juha Sorva, Ville Karavirta, Lauri Malmi
- **Venue:** ACM Transactions on Computing Education 13(4), 2013, art. 15
- **URL:** https://doi.org/10.1145/2490822
- **Summary:** A survey of many program-visualization systems and their evaluations. It finds that most only let users view animations at their own pace, that more engaging interaction is the trend, and that the evidence is broadly positive but thin.
- **Design lesson:** Self-paced viewing is the baseline, not the goal. Every major animation should also ask the reader to do something, such as choosing which instance runs next or predicting which stack the SP will point to.

### 1.4 Notional Machines and Introductory Programming Education
- **Authors:** Juha Sorva
- **Venue:** ACM TOCE 13(2), 2013, art. 8
- **URL:** https://doi.org/10.1145/2483710.2483713
- **Summary:** Argues that learners need an explicit "notional machine", a simplified model of how the runtime executes programs, and that visualizations should teach that model on purpose.
- **Design lesson:** Name your notional machine early and draw it once, then keep to it: "a CPU with one SP register, N stacks, N memory images, one code segment with patch sites." Every later animation is just this machine changing state.

### 1.5 Notional Machines in Computing Education: The Education of Attention
- **Authors:** Sally Fincher, Johan Jeuring, Craig S. Miller, Peter Donaldson, Benedict du Boulay, Matthias Hauswirth, Arto Hellas, Felienne Hermans, Colleen Lewis, Andreas Mühling, Janice L. Pearce, Andrew Petersen
- **Venue:** ITiCSE Working Group Reports 2020, pp. 21–50
- **URL:** https://doi.org/10.1145/3437800.3439202
- **Summary:** Defines notional machines and their identifying characteristics, reviews the literature, and collects teachers' examples. It frames a notional machine as directing the learner's attention to the relevant part of execution.
- **Design lesson:** Each animation should hide the state that does not matter for its point. In the context-switch animation, grey out the game's heap and show only registers and stacks. In the snapshot animation, collapse the stacks.

### 1.6 CS1 Assessment Using Memory Diagrams
- **Authors:** Mark A. Holliday, David R. Luginbuhl
- **Venue:** SIGCSE 2004
- **URL:** https://paws.wcu.edu/holliday/papers/MemoryDiagramPublications.pdf (authors' publication list with PDF). I did not find a DOI.
- **Summary:** Uses hand-drawn memory diagrams, with a strict shape convention for variables, references and objects, both to teach object/reference semantics and to assess it.
- **Design lesson:** A strict, documented diagram convention works for testing as well as explaining. Ask readers to draw or complete a small memory diagram (for example, "which snapshot does this pointer belong to?") to check their understanding.

### 1.7 Visualizing Memory Graphs
- **Authors:** Thomas Zimmermann, Andreas Zeller
- **Venue:** Software Visualization (Dagstuhl seminar), LNCS 2269, Springer, 2001/2002, pp. 191–204
- **URL:** https://doi.org/10.1007/3-540-45875-1_15 ; author page: https://www.st.cs.uni-saarland.de/memgraphs/
- **Summary:** Extracts a program's whole state as a graph (values plus dereference, index and member edges) and compares two states with a common-subgraph computation to show what differs.
- **Design lesson:** This is very close to per-instance snapshots. Show two Doom instances' memory images as a diff, with shared and identical regions merged and divergent regions highlighted. Readers then see where isolation actually matters.

### 1.8 Heapviz: Interactive Heap Visualization for Program Understanding and Debugging
- **Authors:** Edward E. Aftandilian, Sean Kelley, Connor Gramazio, Nathan Ricci, Sara L. Su, Samuel Z. Guyer
- **Venue:** ACM SOFTVIS 2010 (extended in Information Visualization 12(2), 2013)
- **URL:** https://doi.org/10.1145/1879211.1879222 ; project: https://www.cs.tufts.edu/r/redline/heapviz/
- **Summary:** Visualizes large Java heap snapshots by grouping nodes into data-structure summaries and laying them out by dominance and ownership.
- **Design lesson:** A real DOS game's memory is too large to draw byte by byte. Group it into labelled regions (code, data, zone heap, stack, patched thunks) and let the reader expand one region at a time.

### 1.9 A Grounded Conceptual Model for Ownership Types in Rust
- **Authors:** Will Crichton, Gavin Gray, Shriram Krishnamurthi
- **Venue:** OOPSLA 2023 (PACMPL 7, OOPSLA2, art. 265, pp. 1224–1252); SIGPLAN Research Highlight
- **URL:** https://doi.org/10.1145/3622841 ; arXiv: https://arxiv.org/abs/2309.04134
- **Summary:** Builds a permissions-on-memory-paths model of Rust's borrow checker, generates compile-time and run-time diagrams from real programs with a compiler plugin, and shows a measured learning gain (about +9%, d = 0.56, N = 342) when the diagrams are added to the Rust Book.
- **Design lesson:** Generate the diagrams from the real system instead of drawing them by hand, as Aquascope does. Instrument your multi-instance runtime to dump actual SP values, stack ranges and snapshot extents, so the figures are evidence and not illustration.

---

## 2. Live programming and projecting runtime values into the code

### 2.1 Example Centric Programming
- **Authors:** Jonathan Edwards
- **Venue:** OOPSLA 2004 Companion (Onward! track), also in SIGPLAN Notices 39(12). Sources give different page numbers.
- **URL:** https://doi.org/10.1145/1028664.1028713
- **Summary:** Proposes an IDE in which concrete examples run all the time and their traces sit next to the code, so programmers reason about specific values rather than abstractions.
- **Design lesson:** Base every explanation on one concrete, named example, such as "instance B is on frame 1023 and has just called `I_GetTime`", rather than on an abstract instance *i*.

### 2.2 Usable Live Programming
- **Authors:** Sean McDirmid
- **Venue:** Onward! 2013 (SPLASH)
- **URL:** https://doi.org/10.1145/2509578.2509585
- **Summary:** Puts live execution feedback into the editor by making points in the execution navigable. It adds probes on expressions and a trace pane whose entries link back to the code.
- **Design lesson:** Link the code view and the execution view in both directions. Clicking the `switch_stack` line should highlight every context switch on the timeline, and clicking a switch on the timeline should highlight the code line.

### 2.3 Projection Boxes: On-the-fly Reconfigurable Visualization for Live Programming
- **Authors:** Sorin Lerner
- **Venue:** CHI 2020
- **URL:** https://doi.org/10.1145/3313831.3376494 ; project: https://cseweb.ucsd.edu/~lerner/pb/
- **Summary:** Starts from the program's full runtime semantics and lets the user choose which projections (variables, loop iterations) to show inline. The study found users valued the configurability and were not distracted by the always-on display.
- **Design lesson:** Readers want different levels of detail, so let them choose which state to show (registers only, plus stack words, plus snapshot pages). Use sensible defaults and progressive disclosure rather than one fixed view.

### 2.4 Omnicode: A Novice-Oriented Live Programming Environment with Always-On Run-Time Value Visualizations
- **Authors:** Hyeonsu Kang, Philip J. Guo
- **Venue:** UIST 2017, pp. 737–745
- **URL:** https://doi.org/10.1145/3126594.3126632 ; PDF: https://pg.ucsd.edu/publications/Omnicode-always-on-runtime-value-visualizations_UIST-2017.pdf
- **Summary:** Continuously shows the full history of every numeric variable as a scatterplot matrix linked to the code by brushing, and lets users zoom into stack and heap views at any step.
- **Design lesson:** Plot whole histories, not only the current state. A small-multiples strip of "which instance was running" over time, or of SP over time per instance, makes scheduling patterns visible at a glance. Brushing a time range should drive the detailed stack view.

### 2.5 Babylonian-style Programming: Design and Implementation of an Integration of Live Examples into General-purpose Source Code
- **Authors:** David Rauch, Patrick Rein, Stefan Ramson, Jens Lincke, Robert Hirschfeld
- **Venue:** *The Art, Science, and Engineering of Programming* 3(3), art. 9, 2019 (‹Programming› 2019)
- **URL:** https://doi.org/10.22152/programming-journal.org/2019/3/9 ; arXiv: https://arxiv.org/abs/1902.00549
- **Summary:** Lets programmers attach named live examples to functions and see probe values for each example inline in a general-purpose editor.
- **Design lesson:** Show several examples side by side. Run the same code snippet (say, the stack-switch routine) against two or three instances, with each one's probe values in its own coloured lane, so the reader sees that one code path is serving many states.

### 2.6 Exploratory and Live, Programming and Coding: A Literature Study Comparing Perspectives on Liveness
- **Authors:** Patrick Rein, Stefan Ramson, Jens Lincke, Robert Hirschfeld, Tobias Pape
- **Venue:** *The Art, Science, and Engineering of Programming* 3(1), art. 1, 2019
- **URL:** https://doi.org/10.22152/programming-journal.org/2019/3/1 ; arXiv: https://arxiv.org/abs/1807.08578
- **Summary:** A thematic analysis of 212 publications. It finds that live-programming research mainly aims at accessibility and understandability and is usually evaluated with user studies.
- **Design lesson:** Use it to position your work. Your article is "liveness for the reader": the simulation responds right away to reader input, which is the property this literature argues supports understanding.

### 2.7 A Perspective on the Evolution of Live Programming
- **Authors:** Steven L. Tanimoto
- **Venue:** LIVE 2013 (1st International Workshop on Live Programming, at ICSE), pp. 31–34
- **URL:** https://homes.cs.washington.edu/~tanimoto/site/wp-content/uploads/2024/10/LiveProgramming2013.pdf (author PDF). IEEE DOI not confirmed.
- **Summary:** Extends Tanimoto's earlier four liveness levels (from informative up to fully live) with predictive levels.
- **Design lesson:** Aim for at least level 3–4 in your figures: when the reader changes something (number of instances, time slice, patch on or off), the animation should update immediately, not wait for a "replay" button.

### 2.8 Live Functional Programming with Typed Holes
- **Authors:** Cyrus Omar, Ian Voysey, Ravi Chugh, Matthew A. Hammer
- **Venue:** POPL 2019 (PACMPL 3, POPL)
- **URL:** https://arxiv.org/abs/1805.00155 (the ACM DOI is in PACMPL; I did not confirm it via search)
- **Summary:** Gives a dynamic semantics to incomplete programs, so live feedback continues around holes instead of disappearing (implemented in Hazel).
- **Design lesson:** An interactive figure should never go blank because the reader is part-way through an action. If they have paused mid-switch, show the half-saved register file explicitly as the "in-between" state.

### 2.9 Small-Step Live Programming by Example
- **Authors:** Kasra Ferdowsifard, Allen Ordookhanians, Hila Peleg, Sorin Lerner, Nadia Polikarpova
- **Venue:** UIST 2020, pp. 614–626
- **URL:** https://doi.org/10.1145/3379337.3415869
- **Summary:** SnipPy combines projection boxes with programming-by-example at the granularity of single statements. In a 13-person study it helped users solve harder problems faster.
- **Design lesson:** Small steps matter. Break the context switch into single-instruction steps (push regs, save SP, load SP, pop regs, ret) that the reader can advance one at a time.

### 2.10 What You See Is What You Code: A "Live" Algorithm Development and Visualization Environment for Novice Learners
- **Authors:** Christopher D. Hundhausen, Jonathan Lee Brown
- **Venue:** Journal of Visual Languages and Computing 18(1), 2007, pp. 22–47 (earlier version at VL/HCC 2005)
- **URL:** https://doi.org/10.1016/j.jvlc.2006.03.002
- **Summary:** ALVIS Live! updates an algorithm's visualization on every edit. In related studies, students who used it wrote algorithms with fewer semantic errors.
- **Design lesson:** Close the loop between the reader's change and its visual effect. Offer a "write your own scheduler order" sandbox whose animation updates as they type.

---

## 3. Omniscient and time-travel debugging; visualizing execution history

### 3.1 Designing the Whyline: A Debugging Interface for Asking Questions about Program Behavior
- **Authors:** Andrew J. Ko, Brad A. Myers
- **Venue:** CHI 2004
- **URL:** https://doi.org/10.1145/985692.985712 ; PDF mirror: https://hci.rwth-aachen.de/materials/conferences/CHI2004/1p151.pdf
- **Summary:** Lets programmers ask "why did / why didn't" questions about output and answers with the chain of runtime events responsible. The authors report debugging time cut by roughly 8× for novices in Alice. A Java follow-up, "Debugging Reinvented", appeared at ICSE 2008.
- **Design lesson:** Structure the article's interactions as questions readers would actually ask, such as "Why didn't instance B's frame counter advance?" or "Why does instance A see B's global?", and answer each with a highlighted causal chain through the timeline.

### 3.2 Debugging Backwards in Time
- **Authors:** Bil Lewis
- **Venue:** AADEBUG 2003 (5th Int. Workshop on Automated Debugging), pp. 225–235
- **URL:** https://arxiv.org/abs/cs/0310016
- **Summary:** The Omniscient Debugger records every assignment and call, so the programmer can move backwards and forwards through the whole execution.
- **Design lesson:** Memory snapshots are a time machine, so present them that way. A scrubber that rewinds an instance to an earlier snapshot, with the state diagram moving with it, explains snapshots better than prose does.

### 3.3 Scalable Omniscient Debugging
- **Authors:** Guillaume Pothier, Éric Tanter, José Piquer
- **Venue:** OOPSLA 2007
- **URL:** https://doi.org/10.1145/1297027.1297067 ; PDF: https://pleiad.cl/papers/2007/pothierAl-oopsla2007.pdf
- **Summary:** TOD is a trace-oriented debugger for Java that stores and queries large execution traces in a distributed database and supports partial traces to reduce volume.
- **Design lesson:** A full trace is too much. Record only the events that matter (switches, snapshot saves and restores, patch hits) and build the timeline from those, so the history view stays readable.

### 3.4 Interactive Record/Replay for Web Application Debugging
- **Authors:** Brian Burg, Richard Bailey, Andrew J. Ko, Michael D. Ernst
- **Venue:** UIST 2013, pp. 473–484 (Best Paper Honorable Mention)
- **URL:** https://doi.org/10.1145/2501988.2502050 ; PDF: https://homes.cs.washington.edu/~mernst/pubs/record-replay-uist2013.pdf
- **Summary:** Timelapse records and deterministically replays web app sessions with a seekable timeline of input events. In the user study it sped up reproduction, but not overall task completion.
- **Design lesson:** A timeline of discrete input events (keyboard and tick inputs per Doom instance) is a familiar, scrubbable metaphor. Be honest, though: replay helps with reproducing and seeing behaviour, not automatically with deeper understanding, so pair it with explanations.

### 3.5 Engineering Record and Replay for Deployability
- **Authors:** Robert O'Callahan, Chris Jones, Nathan Froyd, Kyle Huey, Albert Noll, Nimrod Partush
- **Venue:** USENIX ATC 2017 (extended tech report on arXiv)
- **URL:** https://www.usenix.org/conference/atc17/technical-sessions/presentation/ocallahan ; arXiv: https://arxiv.org/abs/1705.05937
- **Summary:** rr records and replays real Linux processes in user space, by restricting nondeterminism sources and using hardware performance counters. It is the basis of a widely used reverse-execution debugger.
- **Design lesson:** It is systems-venue rather than PL/HCI, but it gives you the right framing for nondeterminism. Show readers exactly which inputs (timer, keyboard, RNG seed) must be captured per instance for a snapshot to be replayable, as rr does with its sources of nondeterminism.

### 3.6 CrossCode: Multi-level Visualization of Program Execution
- **Authors:** Devamardeep Hayatpur, Daniel Wigdor, Haijun Xia
- **Venue:** CHI 2023, art. 593
- **URL:** https://doi.org/10.1145/3544548.3581390 ; arXiv: https://arxiv.org/abs/2304.03445
- **Summary:** Visualizes JavaScript execution at several levels of abstraction (grouping by syntax, control flow and data flow) so users can zoom between overview and line-level detail without losing their place.
- **Design lesson:** Provide semantic zoom over time. At the top level, show "instance A ran, then B, then C"; zoomed in, show one switch; zoomed fully, show the instructions of that switch. Keep a persistent "you are here" marker across levels.

---

## 4. Algorithm and execution animation: does it help?

### 4.1 A Meta-Study of Algorithm Visualization Effectiveness
- **Authors:** Christopher D. Hundhausen, Sarah A. Douglas, John T. Stasko
- **Venue:** Journal of Visual Languages and Computing 13(3), 2002, pp. 259–290
- **URL:** https://doi.org/10.1006/jvlc.2002.0237
- **Summary:** A meta-analysis of 24 experimental studies. It concludes that *how* learners use an algorithm visualization (actively) matters more than *what* it shows.
- **Design lesson:** This is the central finding for your article. Spend effort on interaction (predict, configure, construct) rather than on visual polish. A plain diagram that the reader manipulates beats a beautiful one they only watch.

### 4.2 Exploring the Role of Visualization and Engagement in Computer Science Education
- **Authors:** Thomas L. Naps, Guido Rößling, Vicki Almstrum, Wanda Dann, Rudolf Fleischer, Chris Hundhausen, Ari Korhonen, Lauri Malmi, Myles McNally, Susan Rodger, J. Ángel Velázquez-Iturbide
- **Venue:** ITiCSE Working Group Reports 2002, pp. 131–152 (also SIGCSE Bulletin 35(2), 2003)
- **URL:** https://doi.org/10.1145/960568.782998 (DOI from my own recall plus repository listings; check it in the ACM DL)
- **Summary:** Proposes the engagement taxonomy (No viewing, Viewing, Responding, Changing, Constructing, Presenting) and hypothesizes that learning increases with engagement level.
- **Design lesson:** Use it as a checklist for each section. Reach at least *Responding* (a prediction question) and *Changing* (the reader alters parameters) for the core stack-switch and snapshot mechanics.

### 4.3 Animation: Can It Facilitate?
- **Authors:** Barbara Tversky, Julie Bauer Morrison, Mireille Bétrancourt
- **Venue:** International Journal of Human-Computer Studies 57(4), 2002, pp. 247–262
- **URL:** https://doi.org/10.1006/ijhc.2002.1017 ; PDF: https://hci.stanford.edu/courses/cs448b/papers/Tversky_AnimationFacilitate_IJHCS02.pdf
- **Summary:** Finds that most studies favouring animation compared it unfairly against less informative static graphics. Animations are often too fast or complex to perceive, and people think of continuous processes as discrete steps. Two principles are proposed: congruence and apprehension.
- **Design lesson:** A context switch is a discrete process, so animate it as discrete keyframes (save, swap SP, restore) with pauses, not as continuous motion. Always offer the equivalent static step-sequence too.

### 4.4 Evaluating Animations as Student Aids in Learning Computer Algorithms
- **Authors:** Michael D. Byrne, Richard Catrambone, John T. Stasko
- **Venue:** Computers & Education 33(4), 1999, pp. 253–278
- **URL:** PDF: https://courses.cs.washington.edu/courses/cse591e/02sp/readings/evaluating_animations.pdf (Elsevier DOI not confirmed in search)
- **Summary:** In two experiments, prompting learners to *predict* the algorithm's next step, with or without animation, improved performance on novel problems. The effect of animation itself could not be separated from the effect of prediction.
- **Design lesson:** Before revealing an animation step, ask a prediction question ("After `swapcontext`, which stack does SP point into?"). Prediction may be doing much of the work credited to animation.

### 4.5 Rethinking the Evaluation of Algorithm Animations as Learning Aids: An Observational Study
- **Authors:** Colleen Kehoe, John Stasko, Ashley Taylor
- **Venue:** International Journal of Human-Computer Studies 54(2), 2001, pp. 265–284
- **URL:** https://doi.org/10.1006/ijhc.2000.0409 ; PDF: https://courses.cs.washington.edu/courses/cse591e/02sp/readings/rethinking_animations.pdf
- **Summary:** In an open "homework" setting, students combined animations with text in sophisticated ways. Animations made complex algorithms feel more approachable and less intimidating.
- **Design lesson:** Animations and text work as a pair. Place each animation beside the prose it illustrates, and expect readers to go back and forth between them. The motivational payoff ("this is less scary than it sounds") is a real benefit for a systems topic.

### 4.6 Algorithm Visualization in CS Education: Comparing Levels of Student Engagement
- **Authors:** Scott Grissom, Myles F. McNally, Tom Naps
- **Venue:** ACM SOFTVIS 2003, pp. 87–94
- **URL:** https://doi.org/10.1145/774833.774846
- **Summary:** An empirical test of the engagement taxonomy. Learning improved as engagement rose from no visualization, to viewing, to responding.
- **Design lesson:** Direct evidence that adding questions to an animation pays off. Even simple multiple-choice prompts embedded in the animation are worth including.

### 4.7 A Survey of Successful Evaluations of Program Visualization and Algorithm Animation Systems
- **Authors:** Jaime Urquiza-Fuentes, J. Ángel Velázquez-Iturbide
- **Venue:** ACM TOCE 9(2), 2009, art. 9
- **URL:** https://doi.org/10.1145/1538234.1538236
- **Summary:** Reviews 18 systems across 33 evaluations and identifies the features shared by the successful ones. A companion workshop paper names narrative and textual content, feedback on answers, and student-centred construction.
- **Design lesson:** Narrate each animation (captions per step) and give feedback on predictions, saying *why* the answer is right or wrong, not just whether it is.

### 4.8 Animated Transitions in Statistical Data Graphics
- **Authors:** Jeffrey Heer, George Robertson
- **Venue:** IEEE InfoVis 2007 (TVCG 13(6), pp. 1240–1247)
- **URL:** https://doi.org/10.1109/TVCG.2007.70539
- **Summary:** Offers a taxonomy and design principles for animated transitions, such as staging, keeping objects' identity, and simple motion. Controlled experiments show well-designed transitions improve graphical perception.
- **Design lesson:** When the view switches from instance A's state to B's, keep the identity of shared elements (the single code segment and the CPU stay still) and move only what changes (SP arrow, active stack). Stage multi-part changes one at a time.

### 4.9 When Learning Is Just a Click Away: Does Simple User Interaction Foster Deeper Understanding of Multimedia Messages?
- **Authors:** Richard E. Mayer, Paul Chandler
- **Venue:** Journal of Educational Psychology 93(2), 2001, pp. 390–397
- **URL:** https://doi.org/10.1037/0022-0663.93.2.390
- **Summary:** Learners who controlled the pacing of a segmented narrated animation, with a click to continue, learned more deeply than those who watched it continuously. This is the basis of the "segmenting principle". One later health-communication replication did not reproduce it.
- **Design lesson:** Don't autoplay. Split each mechanism into segments advanced by the reader (a "next step" button or scroll steps), each with a short caption.

### 4.10 DeFT: A Conceptual Framework for Considering Learning with Multiple Representations
- **Authors:** Shaaron Ainsworth
- **Venue:** Learning and Instruction 16(3), 2006, pp. 183–198
- **URL:** https://doi.org/10.1016/j.learninstruc.2006.03.001
- **Summary:** A framework for when multiple representations help: complementary information, one constraining interpretation of another, and building deeper understanding by translating between them. It also warns about the cost of translating.
- **Design lesson:** If you show several views (code, register file, memory map, timeline), link them explicitly with shared colours per instance and hover-highlighting across views. Readers should not have to do the translation work themselves.

---

## 5. Visualizing low-level and systems state: CPU, assembly, concurrency, scheduling

### 5.1 MARS: An Education-Oriented MIPS Assembly Language Simulator
- **Authors:** Kenneth Vollmar, Pete Sanderson
- **Venue:** SIGCSE 2006, pp. 239–243
- **URL:** https://doi.org/10.1145/1121341.1121415
- **Summary:** A GUI MIPS simulator with register and memory panes that highlight what changed on each step, and with backstepping. It is widely used in computer-organization courses.
- **Design lesson:** Use the "changed this step" highlight convention for registers and memory words, and support stepping backwards. Readers from systems courses will recognise it immediately for your x86 register and stack views.

### 5.2 Debugging Distributed Systems: Challenges and Options for Validation and Debugging
- **Authors:** Ivan Beschastnikh, Patty Wang, Yuriy Brun, Michael D. Ernst
- **Venue:** Communications of the ACM 59(8), 2016, pp. 32–37 (from ACM Queue). Tool paper: "Visualizing Distributed System Executions", ACM TOSEM 2020.
- **URL:** https://doi.org/10.1145/2909480 ; PDF: https://www.cs.ubc.ca/~bestchai/papers/cacm2016-shiviz.pdf ; TOSEM abstract: https://homes.cs.washington.edu/~mernst/pubs/visualize-distributed-tosem2020-abstract.html
- **Summary:** ShiViz renders logs as interactive time-space diagrams, one vertical line per process with arrows for happens-before, and supports collapsing and searching. The TOSEM study (109 students) found significantly better comprehension.
- **Design lesson:** Use a time-space diagram for your instances: one lane per Doom instance, filled when it runs, with arrows for control transfers (yield → resume). It reads like a Gantt chart of cooperative scheduling.

### 5.3 Understanding Complex Multithreaded Software Systems by Using Trace Visualization
- **Authors:** Jonas Trümper, Johannes Bohnet, Jürgen Döllner
- **Venue:** ACM SOFTVIS 2010, pp. 133–142
- **URL:** https://doi.org/10.1145/1879211.1879232
- **Summary:** Traces method boundaries across threads and visualizes them post-mortem with per-thread call-stack views (icicle plots) aligned on a shared time axis.
- **Design lesson:** Draw each instance's call stack over time as an icicle or flame strip in its own lane, on a shared time axis. Stack switching then appears as one lane's stack freezing while another's grows, which shows directly that each instance keeps its own stack.

### 5.4 Visualizing Classic Synchronization Problems: Dining Philosophers, Producers-Consumers, and Readers-Writers
- **Authors:** Joel C. Adams, Elizabeth R. Koning, Christiaan D. Hazlett
- **Venue:** SIGCSE 2019
- **URL:** NSF PAR copy: https://par.nsf.gov/servlets/purl/10113207. Sources give two different DOIs (10.1145/3287324.3287467 and 10.1145/3287324.3293708), so check the ACM DL before citing.
- **Summary:** Animations of starvation, deadlock and livelock in classic problems. Students found them much more engaging than the textbook, but learning gains did not differ significantly.
- **Design lesson:** A sobering data point: engagement is not the same as learning. Keep animations focused on the specific mental model (who owns the CPU, whose stack is live) and test that model with questions, rather than relying on spectacle.

### 5.5 ThreadMentor (Carr, Mayo, Shene)
- **Authors:** Steve Carr, Jean Mayo, Ching-Kuang Shene
- **Venue:** ITiCSE 2002 / ACM journal article 2003 (I only partly verified the venue details)
- **URL:** https://www.cs.mtu.edu/~shene/NSF-3/index.html (project page)
- **Summary:** A teaching library and visualizer that shows each thread and synchronization primitive live during execution, with event recording for playback.
- **Design lesson:** Show threads/instances and the mechanism coordinating them (here, your scheduler or yield point) as first-class visual objects, not just the instances.

### 5.6 An Interactive, Graphical CPU Scheduling Simulator for Teaching Operating Systems
- **Authors:** Joshua W. Buck, Saverio Perugini
- **Venue:** arXiv 1812.05160 (I did not confirm a peer-reviewed venue)
- **URL:** https://arxiv.org/abs/1812.05160
- **Summary:** A web simulator of preemptive multitasking (scheduling, interrupts, context switches, PCBs) that users step through in units of CPU time.
- **Design lesson:** Show the "PCB" equivalent explicitly. For each instance, keep a small card of saved registers and saved SP that fills during the save and empties during the restore.

---

## 6. Explorable explanations and interactive articles

### 6.1 Explorable Explanations
- **Authors:** Bret Victor
- **Venue:** Essay, worrydream.com, 2011
- **URL:** https://worrydream.com/ExplorableExplanations/
- **Summary:** Calls for "active reading": reactive documents in which the reader changes an author's assumptions and immediately sees the consequences. It deliberately guides attention rather than providing a free-form sandbox.
- **Design lesson:** Put the controls inside the prose. Make "with **3** instances and a **35 Hz** tick…" draggable numbers in the text that drive the adjacent figure.

### 6.2 Learnable Programming
- **Authors:** Bret Victor
- **Venue:** Essay, worrydream.com, September 2012
- **URL:** https://worrydream.com/LearnableProgramming/
- **Summary:** Argues that programming environments must "show the data", make flow visible over time, and support moving from concrete to abstract. Live coding alone is not enough.
- **Design lesson:** Always show actual values (real addresses, real SP values, real byte counts for snapshots) rather than placeholders. Let readers move from one concrete switch to the general pattern.

### 6.3 Communicating with Interactive Articles
- **Authors:** Fred Hohman, Matthew Conlen, Jeffrey Heer, Duen Horng (Polo) Chau
- **Venue:** Distill, 2020
- **URL:** https://doi.org/10.23915/distill.00028
- **Summary:** Reviews research on interactive articles and identifies their affordances: connecting people with data, making systems playful, prompting self-reflection, personalising reading, and reducing cognitive load. It also covers the practical costs of authoring.
- **Design lesson:** Use "making systems playful" carefully. A playable mini-Doom grid where readers trigger switches is engaging, but pair it with self-reflection prompts so play turns into understanding.

### 6.4 Idyll: A Markup Language for Authoring and Publishing Interactive Articles on the Web
- **Authors:** Matthew Conlen, Jeffrey Heer
- **Venue:** UIST 2018
- **URL:** https://doi.org/10.1145/3242587.3242600 ; project: https://idyll-lang.org/
- **Summary:** A compile-to-web markup language that binds reader-driven variables (scroll triggers, sliders, buttons) to JavaScript components.
- **Design lesson:** Use scroll-driven steppers ("scrollytelling") for the narrative sections and explicit controls for the sandbox sections. Use one shared state store so every figure reads the same simulated machine.

---

## Cross-cutting findings

1. **Active engagement matters more than how good the animation looks.** The meta-study (Hundhausen et al. 2002), the engagement taxonomy (Naps et al. 2002) and its empirical test (Grissom et al. 2003) all point the same way: learning tracks what learners *do* with a visualization. Prediction prompts are the cheapest effective form (Byrne et al. 1999). Engagement alone is not enough, though: Adams et al. 2019 found more engagement with no significant learning gain. Interactions must target the specific mental model.
2. **Let the reader control pacing, and animate discrete processes as discrete steps.** Tversky et al. 2002 show that many "animation wins" disappear against fair static sequences, and that people understand processes as discrete steps. Mayer & Chandler 2001 show that learner-paced segments beat continuous playback. SnipPy (2020) and MARS (2006) show the value of single-step and backstep control. For a context switch: keyframes, a step button, a back button, no autoplay.
3. **Use one consistent, minimal visual grammar based on a named notional machine.** Python Tutor's frames, heap and arrows; Sorva's notional machines; Holliday & Luginbuhl's diagram conventions; and Heer & Robertson's object-identity principle all argue for a small, stable visual vocabulary. Shared elements stay still and only changed state moves. Assign each instance a fixed colour and keep it everywhere.
4. **Show progressively more detail, and let readers choose the level.** Projection Boxes (configurability), Heapviz (grouping before detail), CrossCode (multi-level zoom) and the Fincher et al. "education of attention" framing all support starting coarse (instances on a timeline), letting readers drill down (one switch, then instructions, then bytes), and hiding what does not matter at each level.
5. **Use linked views and histories, not single snapshots.** Omnicode (histories plus brushing), McDirmid (navigable execution points), ShiViz and Trümper et al. (lane-per-thread time-space views), Zimmermann & Zeller (state diffs) and Ainsworth's DeFT (linked representations) all support pairing a timeline with a detail view, synchronised by colour and hover. For snapshots specifically, show the *difference* between memory images rather than two full dumps.

## Verification caveats
- Elsevier DOI not confirmed: Byrne et al. 1999. ACM DOI not confirmed: Guo 2013 (SIGCSE), Holliday & Luginbuhl 2004, Hazel (POPL 2019). The Naps et al. 2002 DOI comes from my own recall rather than a search result.
- Adams et al. 2019 has two conflicting DOIs in sources.
- Buck & Perugini (§5.6) has no confirmed peer-reviewed venue. ThreadMentor (§5.5) venue details are only partly verified.
- Victor's essays are not peer-reviewed. Cite them as essays.
- I searched for, but could not verify and left out: Dragon & Dickson "Memory diagrams" (SIGCSE 2016) and "Seymour" (LIVE 2017).
