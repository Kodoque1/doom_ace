# 3D scene ideas for the "three Dooms, one CPU" explorable article — grounded research notes

Scope: published work on 3D software/runtime visualization, process and factory visualization, digital twins, and the evidence on when 3D helps or hurts. Every entry below was checked with a web search on 2026-10-24 (title, authors, venue, year). Each URL is a DOI or a publisher, author or repository page the search returned. Where I could not confirm a DOI, I give the repository/project URL instead and say so. Direct Crossref lookups were blocked from this sandbox, so the DOIs come from publisher, IEEE CS, ACM, OpenAlex, institutional or author pages found in search results.

Scene shorthand used below:
- **MAP** = machine map (time strip with gate posts / execution towers + CPU orb + switch box / memory layer with shared Doom, snapshot slots, host state)
- **MEM** = 3D memory map from the real run log (blocks at real addresses, pointer arcs from a dumped stack)
- **FB** = three framebuffers + VGA memory + blit beam

---

## A. 3D visualization of software and running programs

### 1. Visualizing Software Systems as Cities
- **Authors:** Richard Wettel, Michele Lanza
- **Venue/year:** 4th IEEE Int. Workshop on Visualizing Software for Understanding and Analysis (VISSOFT), 2007. Won the VISSOFT 2020 Most Influential Paper award.
- **URL:** https://doi.org/10.1109/VISSOF.2007.4290706 (PDF: https://www.si.usi.ch/assets/publications/conf/vissoft/vissoft2007/WettelL07.pdf)
- **Summary:** Classes become buildings and packages become districts. Metrics map to building height and footprint, so a code base reads as a city.
- **Idea for us:** In MEM, give each block a footprint proportional to its size and a height proportional to a runtime metric, such as bytes copied per switch. "Tall" then means "expensive to snapshot". Keep the city flat (one ground plane, no stacking), so heights are compared against a common baseline.

### 2. Software Systems as Cities: A Controlled Experiment
- **Authors:** Richard Wettel, Michele Lanza, Romain Robbes
- **Venue/year:** ICSE 2011, pp. 551–560
- **URL:** https://www.inf.unibz.it/~rrobbes/p/ICSE2011-codecity.pdf (program page: https://2011.icse-conferences.org/content/software-systems-cities-controlled-experiment.html). DOI not confirmed by search.
- **Summary:** In a 41-participant controlled experiment, CodeCity gave statistically significant gains in correctness and completion time over a baseline of Eclipse plus spreadsheets.
- **Idea for us:** This is evidence that a 3D city works when the task is spotting outliers and structure, not reading exact values. Write MEM's captions as outlier questions ("which region dominates the copy cost?") rather than lookup questions.

### 3. Live Trace Visualization for Comprehending Large Software Landscapes: The ExplorViz Approach
- **Authors:** Florian Fittkau, Jan Waller, Christian Wulf, Wilhelm Hasselbring
- **Venue/year:** IEEE VISSOFT 2013 (NIER). Later named one of the two most influential VISSOFT 2013 papers.
- **URL:** https://oceanrep.geomar.de/id/eprint/21840 (preprint PDF: https://oceanrep.geomar.de/id/eprint/21840/1/ExplorViz_VISSOFT2013_preprint.pdf). Project: https://www.explorviz.net
- **Summary:** Two linked levels: a 2D landscape view (applications and nodes, UML-like) and a 3D city view of one system's internal communication, both fed by live monitoring traces.
- **Idea for us:** Use the same two-level drill-down. MAP is the "landscape" (three instances, one CPU). Clicking a tower opens that instance's "system level", which is its slice of MEM. Communication arcs in ExplorViz are a direct precedent for our pointer arcs and for the CPU's hop path.

### 4. SynchroVis: 3D Visualization of Monitoring Traces in the City Metaphor for Analyzing Concurrency
- **Authors:** Jan Waller, Christian Wulf, Florian Fittkau, Philipp Döhring, Wilhelm Hasselbring
- **Venue/year:** IEEE VISSOFT 2013
- **URL:** https://oceanrep.geomar.de/id/eprint/21839/8/SynchroVis_VISSOFT2013_preprint.pdf (repository: https://oceanrep.geomar.de/id/eprint/21994). DOI not confirmed.
- **Summary:** Puts concurrency traces (threads, lock acquisition, dining philosophers example) on top of a static software city, so dynamic behaviour is shown in place on the static structure.
- **Idea for us:** It is the closest precedent for MAP. The static structure (shared Doom code, per-instance slots) stays fixed, and the dynamic actor (the CPU orb, our "thread") moves across it. Show who "holds" the CPU the way SynchroVis shows lock holders: a lit ring around the current tower, with the others dimmed.

### 5. Memory Cities: Visualizing Heap Memory Evolution Using the Software City Metaphor
- **Authors:** Markus Weninger, Lukas Makor, Hanspeter Mössenböck
- **Venue/year:** IEEE VISSOFT 2020
- **URL:** https://ssw.jku.at/General/Staff/Weninger/Papers/Weninger_VISSOFT_20_Preprint.pdf (proceedings PDF: https://conferences.computer.org/icsme/pdfs/VISSOFT2020-21GoJXGHj7IxcVsXivi2jW/991400a110/991400a110.pdf)
- **Summary:** Heap objects are grouped (by type or allocation site) into buildings sized by count or bytes. Time-travel interaction shows how they grow, strongly growing buildings are coloured, and less suspicious ones are drawn semi-transparent.
- **Idea for us:** This is the best direct model for MEM over time. Scrub through the run and let blocks grow or shrink. Draw unchanged blocks semi-transparent and colour only the bytes that differ between snapshots ("dirty since last switch"). That teaches why the snapshot copy has the size it has.

### 6. Visualizing Dynamic Memory Allocations (MemoView)
- **Authors:** Sergio Moreta, Alexandru Telea
- **Venue/year:** IEEE VISSOFT 2007
- **URL:** https://research.tue.nl/en/publications/visualizing-dynamic-memory-allocations/ (project: https://webspace.science.uu.nl/~telea001/SoftVis/MemoVis)
- **Summary:** A 2D view with time on one axis and address space on the other, so each allocation is a rectangle spanning its lifetime.
- **Idea for us:** Use an address-by-time sheet as the floor of MEM, or as a 2D companion to it. Our time strip (MAP) and address axis (MEM) meet there. A switch becomes a vertical "cut" across the sheet showing which address ranges were copied at that moment.

### 7. Heapviz: Interactive Heap Visualization for Program Understanding and Debugging
- **Authors:** Edward E. Aftandilian, Sean Kelley, Connor Gramazio, Nathan Ricci, Sara L. Su, Samuel Z. Guyer
- **Venue/year:** ACM SOFTVIS 2010, pp. 53–62 (extended in Information Visualization 12(2), 2013)
- **URL:** https://doi.org/10.1145/1879211.1879222 (project: https://www.cs.tufts.edu/r/redline/heapviz/)
- **Summary:** Heap snapshots shown as a graph. Nodes are aggregated by data structure, a dominator-based layout makes ownership and containment visible, and users can search, group, colour or hide.
- **Idea for us:** In MEM, aggregate arcs by target region (for example "into instance 2's snapshot" or "into shared Doom data") instead of drawing every word. Add a "group by owner" toggle that colours each block by which snapshot or region owns it. The ownership view is exactly what pinned pointers are about.

### 8. Understanding Complex Multithreaded Software Systems by Using Trace Visualization
- **Authors:** Jonas Trümper, Johannes Bohnet, Jürgen Döllner
- **Venue/year:** ACM SOFTVIS 2010, pp. 133–142
- **URL:** https://doi.org/10.1145/1879211.1879232
- **Summary:** Method-boundary tracing of multithreaded programs, then a post-mortem visualization that aligns threads on a shared time axis to find bottlenecks and interleavings.
- **Idea for us:** In MAP's time strip, give each instance its own lane under the frame strip, aligned on one time axis. The single CPU then shows up as "exactly one lane is lit at any time", which is a visual proof of mutual exclusion.

### 9. Understanding Execution Traces Using Massive Sequence and Circular Bundle Views (EXTRAVIS)
- **Authors:** Bas Cornelissen, Danny Holten, Andy Zaidman, Leon Moonen, Jarke J. van Wijk, Arie van Deursen
- **Venue/year:** ICPC 2007, pp. 49–58 (journal extension: JSS 81, 2008)
- **URL:** https://research.tudelft.nl/en/publications/understanding-execution-traces-using-massive-sequence-and-circula/
- **Summary:** A trace is shown as a compressed "massive sequence" overview linked to a hierarchical edge-bundled circular view of calls between components.
- **Idea for us:** Bundle MEM's pointer arcs by source and destination region (Holten-style edge bundling). Dozens of stack-word arcs become a few thick bundles, and individual arcs fan out only on hover. That reduces clutter in 3D a lot.

### 10. Combing the Communication Hairball: Visualizing Parallel Execution Traces using Logical Time (Ravel)
- **Authors:** Katherine E. Isaacs, Peer-Timo Bremer, Ilir Jusufi, Todd Gamblin, Abhinav Bhatele, Martin Schulz, Bernd Hamann
- **Venue/year:** IEEE TVCG 20(12), 2014 (InfoVis 2014)
- **URL:** https://doi.org/10.1109/TVCG.2014.2346456 (preprint: https://www.cs.ucdavis.edu/~hamann/IsaacsBremerJusufiGamblinBhateleSchulzHamannVisWeek2014PaperFinal08012014.pdf)
- **Summary:** The trace is laid out in logical time (happened-before steps) instead of wall-clock time. Physical-time quantities such as lateness move to colour, which makes structure legible.
- **Idea for us:** This is a strong idea for MAP's time strip. Offer a "logical time" layout where each slot is one frame-boundary yield, so gate posts are evenly spaced and the schedule's structure is obvious. Add a "wall time" layout where slot widths are real durations and the copy cost shows as gaps. Colour encodes the time spent in the switch box.

### 11. State of the Art of Performance Visualization
- **Authors:** Katherine E. Isaacs, Alfredo Giménez, Ilir Jusufi, Todd Gamblin, Abhinav Bhatele, Martin Schulz, Bernd Hamann, Peer-Timo Bremer
- **Venue/year:** EuroVis 2014 STARs, pp. 141–160
- **URL:** https://doi.org/10.2312/eurovisstar.20141177
- **Summary:** A survey that classifies performance visualizations by context (hardware, software, tasks, application) and by goal.
- **Idea for us:** Use it as a checklist. Each of our scenes should be mapped to exactly one context (MAP = tasks/scheduling, MEM = memory/software, FB = hardware/output). A scene that mixes contexts should explicitly link to the scene that owns the other context, not redraw it.

### 12. The Flame Graph
- **Authors:** Brendan Gregg
- **Venue/year:** Communications of the ACM 59(6), 2016 (also ACM Queue)
- **URL:** https://doi.org/10.1145/2909476 (Queue: https://queue.acm.org/detail.cfm?id=2927301)
- **Summary:** Sampled call stacks are drawn as stacked boxes. Width is the share of samples and the y axis is stack depth, which makes hot paths obvious.
- **Idea for us:** Make each execution tower in MAP a literal stack. Its stacked slabs are the real frames of that instance's saved stack at the yield point, with Doom's frames at the bottom and the yield/switch frame on top. Readers who know flame graphs will read "tower = stack" right away. The switch then visibly happens at the top of the tower.

### 13. Ripes (5-stage RISC-V pipeline simulator) and Konata / gem5 O3PipeView (pipeline viewers) — tools, not papers
- **Authors:** Ripes: Morten Borup Petersen (project page). Konata: shioyadan (GitHub). O3PipeView: gem5 project.
- **Venue/year:** Open-source tools. I could not confirm an associated paper (a WCAE paper for Ripes is commonly cited but did not appear in search), so cite them as tools.
- **URL:** https://github.com/mortbopet/Ripes ; https://m5sim.org/documentation/general_docs/cpu_models/visualization ; https://cirosantilli.com/linux-kernel-module-cheat/gem5-konata-o3-pipeline-viewer.html
- **Summary:** Ripes animates a datapath, including stalls and forwarding. Konata and O3PipeView draw one row per instruction and one column per cycle, with stage letters (f, d, n, p, i, c, r).
- **Idea for us:** Draw the switch box's internals as a pipeline chart. Rows are successive switches and columns are the stages: save registers → copy out snapshot → copy in snapshot → re-apply patched bytes → restore SP → resume. It is a 2D panel that pops out of the 3D switch box.

---

## B. Process, factory and flow visualization

### 14. ViDX: Visual Diagnostics of Assembly Line Performance in Smart Factories
- **Authors:** Panpan Xu, Honghui Mei, Liu Ren, Wei Chen
- **Venue/year:** IEEE TVCG 23(1), 2017 (VAST 2016, Best Paper Honorable Mention)
- **URL:** https://doi.org/10.1109/TVCG.2016.2598664
- **Summary:** An extended Marey's graph (stations on one axis, time on the other, one polyline per product) supports both live monitoring and historical analysis of an assembly line. Outlier-preserving aggregation and brushes help find anomalies.
- **Idea for us:** The schedule *is* a Marey graph. The stations are depth 0, the switch and depth 1–3, and the product is "the CPU". Lay a Marey graph flat on the floor of MAP's time layer. Steep segments are fast switches, and a kink is a slow copy. This is the best industry-grounded idea for the time layer.

### 15. Replaying History on Process Models for Conformance Checking and Performance Analysis
- **Authors:** Wil M. P. van der Aalst, Arya Adriansyah, Boudewijn van Dongen
- **Venue/year:** WIREs Data Mining and Knowledge Discovery 2(2):182–192, 2012
- **URL:** https://doi.org/10.1002/widm.1045 (author PDF: https://www.vdaalst.com/publications/p664.pdf)
- **Summary:** Replaying an event log as tokens on a process model links every event to a model element. Combined with timestamps, this gives conformance diagnostics and bottleneck analysis.
- **Idea for us:** Replay our real run log as a token (the CPU orb) on MAP's model, using the logged timestamps. Conformance matters here: the model says "yield only at frame boundaries". If the log ever shows a switch elsewhere, the replay marks that event as non-conforming (see the alarm idea below).

### 16. Disco 1.5.0 animation (Fluxicon) — product documentation
- **Authors:** Fluxicon (Anne Rozinat, Christian W. Günther)
- **Venue/year:** Product blog, 2013
- **URL:** https://fluxicon.com/blog/2013/10/disco-1-5-0/
- **Summary:** Disco's animation projects past cases onto the mined process map as moving dots. Congested arcs merge dots into larger "bubbles", and animations can be exported as video.
- **Idea for us:** Keep play, pause and speed controls, and the "congestion bubble" idea. If several switches pile up in a short window of the log (for example while loading), merge the orb's trail into a thicker blob at the switch box rather than drawing overlapping orbs.

### 17. processanimateR: Process Map Token Replay Animation (R package)
- **Authors:** Felix Mannhardt (bupaverse)
- **Venue/year:** CRAN package, v1.0.5
- **URL:** https://github.com/bupaverse/processanimateR
- **Summary:** An open implementation of token replay. Each case is an SVG token moving along the process map according to its event timestamps, with size and colour driven by attributes.
- **Idea for us:** A concrete reference for implementation. The token's position is interpolated along an edge between two timestamped events. Size and colour come from per-event attributes (for us, bytes copied and instance id).

### 18. Fuzzy Mining – Adaptive Process Simplification Based on Multi-Perspective Metrics
- **Authors:** Christian W. Günther, Wil M. P. van der Aalst
- **Venue/year:** BPM 2007, LNCS 4714, pp. 328–343
- **URL:** https://www.vdaalst.com/publications/p400.pdf
- **Summary:** Avoids "spaghetti" process models with significance and correlation metrics plus a "roadmap" abstraction that clusters and hides less important parts depending on zoom.
- **Idea for us:** Use semantic zoom in MEM. Far away, show only region-level blocks (Doom code, Doom data, three snapshot slots, host state). Close up, show individual allocations and stack words. Match the level of detail to camera distance instead of drawing everything at once.

### 19. Interactive Sankey Diagrams
- **Authors:** Patrick Riehmann, Manfred Hanfler, Bernd Froehlich
- **Venue/year:** IEEE InfoVis 2005, pp. 233–240
- **URL:** https://www.uni-weimar.de/de/medien/professuren/medieninformatik/vr/research/information-visualization/interactive-sankey-diagrams/ (DOI not confirmed by search)
- **Summary:** An interactive Sankey system (city energy flows) with overview and zoom/detail browsing of flow quantities.
- **Idea for us:** Make the bytes moving in a switch a Sankey-style band whose width is proportional to bytes. The band runs from the live region to snapshot slot N (copy out) and from slot M to the live region (copy in). Make the per-switch total visible ("~X KB per switch") and show that host state has no band at all.

---

## C. Digital twins: concepts, interfaces, live state, what-if

### 20. Digital Twin: Mitigating Unpredictable, Undesirable Emergent Behavior in Complex Systems
- **Authors:** Michael Grieves, John Vickers
- **Venue/year:** In *Transdisciplinary Perspectives on Complex Systems*, Springer, 2017, pp. 85–113
- **URL:** https://doi.org/10.1007/978-3-319-38756-7_4
- **Summary:** The foundational account of the digital twin. A virtual counterpart linked to the physical system across its lifecycle lets you foresee emergent, undesirable behaviour by simulating it first.
- **Idea for us:** Frame the what-if mode with it. "What if we yielded mid-frame?" or "what if this pointer were not pinned?" is the twin's job of showing emergent failure safely before it happens in the real system.

### 21. Digital Twin in Manufacturing: A Categorical Literature Review and Classification
- **Authors:** Werner Kritzinger, Matthias Karner, Georg Traar, Jan Henjes, Wilfried Sihn
- **Venue/year:** IFAC-PapersOnLine 51(11):1016–1022, 2018
- **URL:** https://doi.org/10.1016/j.ifacol.2018.08.474
- **Summary:** Separates the *digital model* (no automatic data link), the *digital shadow* (one-way automatic link from the physical system) and the *digital twin* (two-way link).
- **Idea for us:** It gives honest vocabulary. Our scenes built from a recorded log are a **digital shadow** of a real run, and our what-if toggles are a **digital model**. Label the modes that way ("recorded" vs "hypothetical") so readers never confuse a simulated failure with logged behaviour.

### 22. Digital Twin in Industry: State-of-the-Art
- **Authors:** Fei Tao, He Zhang, Ang Liu, A. Y. C. Nee
- **Venue/year:** IEEE Transactions on Industrial Informatics 15(4):2405–2415, 2019
- **URL:** https://doi.org/10.1109/TII.2018.2873186
- **Summary:** A widely cited review of DT concepts, enabling technologies and industrial applications (prognostics, production, design).
- **Idea for us:** Background for the "data factory" framing. The common five-part DT view (physical entity, virtual entity, data, services, connections) maps onto our layers: CPU/machine, the scene, the log, explanations, and the scrub/replay link.

### 23. Visualizing an Exascale Data Center Digital Twin: Considerations, Challenges and Opportunities
- **Authors:** Matthias Maiterth et al. (ORNL, with Tim Dykes of HPE)
- **Venue/year:** IEEE VIS 2024 Short Papers, pp. 21–25
- **URL:** https://www.ieeevis.org/year/2024/program/paper_v-short-1161.html (ORNL record: https://www.ornl.gov/publication/visualizing-exascale-data-center-digital-twin-considerations-challenges-and). DOI not confirmed.
- **Summary:** A 3D twin of the Frontier supercomputer. It pairs a spatio-temporal 3D view with historical and real-time telemetry and lets users trigger simulations of a system state, while dashboards carry the exact numbers.
- **Idea for us:** It is a VIS-venue precedent for our whole design. 3D carries *where* (which tower, which region) and *when* (scrubbing). A docked 2D panel carries exact numbers (bytes, cycles). What-if runs are launched from the 3D view and their results shown in the same space.

### 24. A Survey of Visualization Techniques for Digital Twins
- **Authors:** Mahsa Paziraei, Ali Motamedi
- **Venue/year:** Proc. ICCCBE 2024, Springer Lecture Notes in Civil Engineering vol. 628, pp. 586–599 (2025)
- **URL:** https://pure.etsmtl.ca/en/publications/a-survey-of-visualization-techniques-for-digital-twins/ (DOI not shown in search)
- **Summary:** Classifies DT visualization techniques by data type, use case, interactivity needs and system complexity, as a practitioner guide.
- **Idea for us:** Use it as a design checklist. For each scene, decide its data type (structure vs. time-series vs. flow) and its interactivity level (watch, scrub, or simulate) before adding features.

### 25. Network Digital Twin: Concepts and Reference Architecture (IRTF NMRG Internet-Draft)
- **Authors:** Cheng Zhou (ed.), Hongwei Yang, Xiaodong Duan, Diego Lopez, Antonio Pastor, Qin Wu, Mohamed Boucadair, Christian Jacquenet
- **Venue/year:** IRTF draft-irtf-nmrg-network-digital-twin-arch-13 (Informational, updated 2026)
- **URL:** https://datatracker.ietf.org/doc/draft-irtf-nmrg-network-digital-twin-arch/
- **Summary:** Defines a network digital twin and a reference architecture in which data, models, and an interface to the real network let operators validate changes in the twin before deploying them.
- **Idea for us:** Borrow "validate in the twin before you change the real thing". The what-if toggle should present a *proposed* change, such as "unpin this pointer", and show the twin's verdict (which arcs dangle after a switch) before the reader "applies" it.

### 26. ISA-101 (HMI for process automation) and High-Performance HMI practice — standards and practitioner guidance
- **Authors:** ISA (ANSI/ISA-101.01-2015). Practitioner summaries such as Control Engineering's "High-performance HMIs".
- **Venue/year:** Standard, 2015. The text itself is paywalled and was not read; the points below come from practitioner summaries.
- **URL:** https://www.controleng.com/high-performance-hmis-designs-to-improve-operator-effectiveness/
- **Summary:** Normal operation is drawn in muted greyscale, and saturated colour is reserved for abnormal states, with redundant coding (shape and text, not only colour). Displays follow a four-level hierarchy from plant overview (L1) to diagnostics (L4).
- **Idea for us:** Draw MAP and MEM in calm greys by default and use colour only for meaning: the current CPU owner, and a violated invariant. Structure drill-down as L1 machine map → L2 one instance → L3 its memory regions → L4 raw words or bytes.

---

## D. When 3D helps or hurts

### 27. Visualization Analysis and Design, Ch. 6 "Rules of Thumb" (No unjustified 3D; Eyes beat memory; Overview first…)
- **Authors:** Tamara Munzner
- **Venue/year:** A K Peters / CRC Press, 2014
- **URL:** https://www.cs.ubc.ca/~tmm/vadbook/ (rules slides: https://cs.ubc.ca/~tmm/talks/vad/VAD-rules.pdf)
- **Summary:** Depth is a weak channel, occlusion hides data, perspective distorts size, and tilted text is hard to read. 3D is justified for genuinely spatial data and needs careful justification for abstract data. Also: "eyes beat memory", which favours side-by-side over animation that forces readers to remember.
- **Idea for us:** Test each 3D encoding against this. Our *justified* 3D is containment and layering: memory under execution under time, and snapshots inside the address space. *Unjustified* would be encoding a quantity in depth. Rule: never encode a number on the z axis. Use height on a common ground plane, or put the number in a label.

### 28. Visualization Task Performance with 2D, 3D, and Combination Displays
- **Authors:** Melanie Tory, Arthur E. Kirkpatrick, M. Stella Atkins, Torsten Möller
- **Venue/year:** IEEE TVCG 12(1), 2006
- **URL:** https://doi.org/10.1109/TVCG.2006.17 (PDF: https://www2.cs.sfu.ca/~stella/papers/2006/TVCG2006.pdf) — DOI from memory and consistent with the paper's TVCG Jan/Feb 2006 listing; PDF verified.
- **Summary:** 3D works for rough navigation and relative position. Precise judgments need 2D views or aids. Combined 2D+3D displays matched or beat either alone.
- **Idea for us:** Pair every 3D scene with a linked 2D view: the Marey/lanes chart for MAP, an address-by-time sheet for MEM, a byte table for FB. Use the 3D for "where is it and what contains what", and the 2D for "how much and exactly when".

### 29. A Taxonomy of 3D Occlusion Management for Visualization
- **Authors:** Niklas Elmqvist, Philippas Tsigas
- **Venue/year:** IEEE TVCG 14(5):1095–1109, 2008
- **URL:** https://doi.org/10.1109/TVCG.2008.59 (PDF: https://cs.au.dk/~elm/pdf/occmgt-journal.pdf)
- **Summary:** Classifies 50 occlusion-management techniques into five design patterns: multiple viewports, virtual X-ray, tour planners, interactive exploders, and projection distorters.
- **Idea for us:** MAP's three stacked layers are an occlusion problem. Add an "explode" slider that pulls time, execution and memory apart (an interactive exploder). Make the execution layer X-ray (transparent) when the reader is focused on memory. Add camera presets as a tour planner, one per article section.

### 30. Evaluating the Effectiveness of Spatial Memory in 2D and 3D Physical and Virtual Environments
- **Authors:** Andy Cockburn, Bruce McKenzie
- **Venue/year:** ACM CHI 2002, pp. 203–210
- **URL:** https://doi.org/10.1145/503376.503413
- **Summary:** Spatial memory for item locations got *worse* as freedom to place items in depth grew, in both physical and virtual setups, and users rated the 3D layouts more cluttered.
- **Idea for us:** Keep each scene's layout stable and mostly planar, with towers on a line and memory on a plane, so readers build spatial memory across sections. Never reshuffle positions between scenes: depth N's tower and depth N's snapshot slot always share the same x.

### 31. Visualizing Graphs in Three Dimensions
- **Authors:** Colin Ware, Peter Mitchell
- **Venue/year:** ACM Transactions on Applied Perception 5(1), Article 2, 2008
- **URL:** https://doi.org/10.1145/1279640.1279642 (record: https://scholars.unh.edu/ccom/467)
- **Summary:** With stereo *and* motion (kinetic depth) cues, observers traced paths in 333-node graphs with under 10% error, which beats 2D for path tracing.
- **Idea for us:** This is the case *for* 3D arcs in MEM, but only with motion cues. Add a slow auto-orbit, or a gentle rocking when the reader hovers, while they trace stack-word → pointee arcs. A frozen 3D arc view loses most of the benefit.

### 32. 3D Visualisations Should Not be Displayed Alone – Encouraging a Need for Multivocality in Visualisation
- **Authors:** Jonathan C. Roberts, Joseph W. Mearman, Peter W. S. Butcher, Hayder M. Al-Maneea, Panagiotis D. Ritsos
- **Venue/year:** EG UK Computer Graphics & Visual Computing (CGVC), 2021
- **URL:** https://arxiv.org/abs/2108.04680 (EG DL: https://diglib.eg.org/items/4be9f590-feb4-405d-a406-2d1c68cdd63e)
- **Summary:** Argues that 3D views should come with other "voices" (2D charts, text, alternative views) because no single 3D view is enough.
- **Idea for us:** Supports the combination-display approach and the article format itself. Each 3D scene sits next to prose, a 2D chart, and a code snippet that say the same thing.

### 33. An Overview of 3D Software Visualization
- **Authors:** Alfredo R. Teyseyre, Marcelo R. Campo
- **Venue/year:** IEEE TVCG 15(1):87–105, 2009
- **URL:** https://doi.org/10.1109/TVCG.2008.86 (DOI as given in citing works; publisher page not opened)
- **Summary:** Surveys 3D software visualization (structure, behaviour, evolution) and discusses the trade-offs: navigation, occlusion, and the gain from extra dimensions.
- **Idea for us:** Background reference. Its classification by structure, behaviour and evolution matches our three scenes well enough to frame the article's visualization section.

### 34. Animation: Can It Facilitate?
- **Authors:** Barbara Tversky, Julie Bauer Morrison, Mireille Bétrancourt
- **Venue/year:** International Journal of Human-Computer Studies 57:247–262, 2002
- **URL:** https://doi.org/10.1006/ijhc.2002.1017 (PDF: https://hci.stanford.edu/courses/cs448b/papers/Tversky_AnimationFacilitate_IJHCS02.pdf)
- **Summary:** Animations are often too fast or complex to perceive, and people think of continuous processes as discrete steps. Animation helps when it is interactive and follows those steps.
- **Idea for us:** Make the switch animation *steppable*, one stage per click (save regs → copy out → copy in → patch → resume), with continuous play as an option. Snap the scrubber to meaningful events (yields, switches), not to arbitrary milliseconds.

### 35. Animated Transitions in Statistical Data Graphics
- **Authors:** Jeffrey Heer, George Robertson
- **Venue/year:** IEEE TVCG 13(6):1240–1247, 2007 (InfoVis 2007)
- **URL:** https://doi.org/10.1109/TVCG.2007.70539
- **Summary:** Staged, well-designed animated transitions between views improve graphical perception, and the paper gives design principles (stage changes, keep it simple, preserve object constancy).
- **Idea for us:** When moving between MAP → MEM → FB, keep shared objects constant. Snapshot slot N in MAP should fly to its real address range in MEM, and framebuffer N in FB should be visibly the same object as the block in MEM. Stage the transition: first move the camera, then morph the layout.

### 36. Effectiveness of Animation in Trend Visualization
- **Authors:** George Robertson, Roland Fernandez, Danyel Fisher, Bongshin Lee, John Stasko
- **Venue/year:** IEEE TVCG 14(6):1325–1332, 2008 (InfoVis 2008)
- **URL:** https://doi.org/10.1109/TVCG.2008.125 (PDF: https://faculty.cc.gatech.edu/~stasko/papers/infovis08-anim.pdf)
- **Summary:** Animation was fastest and most enjoyed for *presentation*, but error-prone and least effective for *analysis*. Small multiples were more accurate.
- **Idea for us:** Use the token replay as the *presentation* hook. For the analysis claim ("every switch happens at a frame boundary", "copy cost is constant"), also show a static small-multiples or Marey summary of the whole run.

### 37. A Descriptive Framework for Temporal Data Visualizations Based on Generalized Space-Time Cubes
- **Authors:** Benjamin Bach, Pierre Dragicevic, Daniel Archambault, Christophe Hurter, Sheelagh Carpendale
- **Venue/year:** Computer Graphics Forum 36(6), 2017 (online 2016)
- **URL:** https://doi.org/10.1111/cgf.12804 (open copy: https://hal-enac.archives-ouvertes.fr/hal-01303506)
- **Summary:** Treats temporal visualizations as operations on a space-time cube: time slicing, flattening, cutting, extracting, and so on.
- **Idea for us:** MAP's time strip is a space-time cube (x = instance, y = memory/execution, z = time). Offer two canonical operations as buttons: **time-slice** (freeze at a scrubbed instant and see the full machine state) and **flatten** (collapse time to get the Marey or lanes chart).

### 38. The Eyes Have It: A Task by Data Type Taxonomy for Information Visualizations
- **Authors:** Ben Shneiderman
- **Venue/year:** IEEE Symposium on Visual Languages, 1996, pp. 336–343
- **URL:** https://doi.org/10.1109/VL.1996.545307
- **Summary:** "Overview first, zoom and filter, then details-on-demand."
- **Idea for us:** Every scene opens on an overview camera. Filters cover instance (show only depth 2) and kind (code/data/snapshot/host). Hovering or clicking gives details: real address, size, owning instance, last switch that copied it.

---

## E. Ranked, implementable ideas for our scenes (main deliverable)

Ranking weighs teaching value for this article's core claims, grounding in the literature, legibility risk, and implementation cost in three.js.

### 1. Token replay of the real logged schedule, conformance-checked (MAP)
- **What:** Drive the CPU orb from the actual log. Each yield and switch event is a timestamped step, and the orb interpolates tower → switch box → tower. Add play, pause, speed and step controls. Check each event against the model ("switch only at a frame-boundary gate post"). A conforming event passes through the gate post, which flashes; a non-conforming one would trigger the alarm state (idea 5).
- **Teaches:** One CPU, cooperative turns, and the yield point at the frame boundary, proven by real data rather than staged animation.
- **Drawn from:** van der Aalst, Adriansyah & van Dongen 2012 (replay + conformance); Disco 1.5.0 animation; processanimateR; SynchroVis/ExplorViz (traces on static structure).
- **Legibility risk:** Fast animation is hard to perceive (Tversky 2002) and bad for analysis (Robertson 2008). Mitigate with step mode, a default speed of about one switch per second, and a static summary next to it (idea 3).

### 2. Event-snapped time scrubber with logical vs. wall-time layouts (MAP time layer, drives MEM and FB too)
- **What:** One global scrubber over the recorded run that snaps to yields and switches. Moving it time-slices all scenes: MAP orb position, MEM dirty bytes, FB contents. A toggle switches the strip between **logical time** (one equal slot per turn, gate posts evenly spaced) and **wall time** (slot width = measured duration, so switch overhead shows as gaps).
- **Teaches:** The schedule's structure (round-robin at frame boundaries) and its cost (copy time between slices), as two separate readings.
- **Drawn from:** Isaacs et al. 2014 Ravel (logical time, physical metrics in colour); Bach et al. 2017 (space-time cube time-slice); Weninger et al. 2020 Memory Cities (time travel); Maiterth et al. 2024 (historical telemetry in a 3D twin).
- **Legibility risk:** Two time bases can confuse readers. Label the active mode clearly in the strip, and animate between layouts with object constancy (Heer & Robertson 2007) so each slot visibly stretches rather than jumping.

### 3. Marey-graph floor under the time strip (MAP, 2D-in-3D companion)
- **What:** On the time layer's ground plane, draw a Marey graph. Horizontal lanes are depth 0..3 plus a "switch" lane, time runs along the strip, and one polyline traces the CPU's path. It is also available flattened as a pure 2D panel (Bach "flatten").
- **Teaches:** Mutual exclusion (only one lane occupied at a time), regularity of turns, and outlier switches, all in one static picture of the whole run.
- **Drawn from:** Xu et al. 2017 ViDX (Marey graph for assembly lines, live + historical); Trümper et al. 2010 (aligned thread lanes); Tory et al. 2006 (2D+3D combination); Robertson et al. 2008 (static for analysis).
- **Legibility risk:** It sits under the towers and can be hidden by them. Use the explode slider (idea 7) or a top-down camera preset. Keep its lines thin and grey, with only the current position highlighted.

### 4. Snapshot conveyor / Sankey bands on each switch (MAP memory layer, linked to MEM)
- **What:** During a switch, two bands run in sequence. **Copy out** runs from the live working region to snapshot slot N, then **copy in** runs from slot M to the live region. Band width is proportional to bytes copied, and "parcels" move along it at a rate proportional to measured throughput. Host state has no belt at all, drawn as a visibly unconnected island, and the shared Doom code has no belt either, because it is never copied.
- **Teaches:** What a switch actually costs, that snapshots are full copies, and the key negative space: host state and shared code are *outside* every snapshot.
- **Drawn from:** Riehmann et al. 2005 (Sankey widths as quantity); ViDX (production line); Fluxicon Disco (moving tokens on arcs); the factory framing generally.
- **Legibility risk:** A conveyor suggests parallel hardware. Run the two belts strictly one after the other, and show the CPU orb *riding* the belt (the CPU does the memcpy), so it is clear the copy uses the same single CPU.

### 5. Greyscale-by-default with invariant alarms and four-level drill-down (MAP + MEM)
- **What:** Render the steady state in muted greys (ISA-101 style). Colour only (a) the current CPU owner and (b) an alarm: a pointer arc whose target is not in the current owner's memory or the pinned shared region, a patched code byte not in the expected state for the current instance, or host state written from inside a snapshot range. Alarms use colour plus an icon and a text badge. Drill-down: L1 machine map → L2 one instance → L3 its regions in MEM → L4 the raw stack words or bytes with addresses.
- **Teaches:** *Why* pinning, patching and host-state separation exist: they are the invariants, and the alarm shows exactly what breaks without them.
- **Drawn from:** ISA-101 / High-Performance HMI practice; Shneiderman 1996 (overview → details); ExplorViz (two-level drill-down); van der Aalst 2012 (conformance diagnostics).
- **Legibility risk:** The recorded run should have *no* alarms, which is the point. Alarms appear only in what-if mode (idea 6), so they must look clearly hypothetical. Do not rely on red/green alone; add shape and text.

### 6. What-if "simulation mode" toggles (MEM primarily, echoed in MAP)
- **What:** A small set of toggles that each remove one mechanism, then re-run the switch on the recorded state: **unpin pointer P**, **skip restoring patched bytes**, **put host state inside the snapshot**, **yield mid-frame**. The scene shows the consequence: arcs dangle into another instance's bytes and turn into alarms, a patched instruction shows the wrong instance's value, the keyboard queue gets rolled back. Hypothetical geometry is drawn hatched or ghosted, with a persistent banner reading "simulated — not from the log".
- **Teaches:** Each mechanism justified by counterfactual. This is the strongest way to make the reader *feel* why a design choice was needed.
- **Drawn from:** Grieves & Vickers 2017 (twin for foreseeing emergent failures); NMRG Network Digital Twin draft (validate a change in the twin first); Maiterth et al. 2024 (trigger simulations from the 3D view); Kritzinger et al. 2018 (model vs. shadow vs. twin vocabulary for honest labelling).
- **Legibility risk:** Readers may confuse simulated failure with real logged behaviour. Use the "recorded / hypothetical" label (Kritzinger's shadow vs. model), a distinct visual style, and a one-click reset. Precompute outcomes from real data where possible instead of inventing them.

### 7. Explode slider, X-ray and section camera presets (MAP; also MEM)
- **What:** One slider that pulls MAP's three layers apart vertically, from tight to fully separated. The layers not in focus become semi-transparent (X-ray). Each article section has a camera preset ("tour stop") with mostly front-on views so labels stay in the image plane.
- **Teaches:** Not a concept itself. It is what lets the layered containment (time over execution over memory) be read without occlusion.
- **Drawn from:** Elmqvist & Tsigas 2008 (interactive exploders, virtual X-ray, tour planners); Munzner 2014 (occlusion, tilted text); Cockburn & McKenzie 2002 (stable layout for spatial memory).
- **Legibility risk:** Low. Keep x-alignment fixed while exploding (depth N's tower, slot and lane always share an x) so the layout stays stable in memory.

### 8. Bundled, owner-coloured pointer arcs with gentle motion (MEM)
- **What:** Bundle stack-word arcs by (source region, target region), so you see thick bundles such as "instance 2 stack → shared Doom data" and "→ own snapshot". A bundle fans out into individual words on hover. Colour blocks by owner: shared, instance N, host. Add a slow auto-rock or orbit while the reader traces arcs.
- **Teaches:** Where a saved stack's pointers point, and so why pointers into shared or pinned regions survive a switch while pointers into per-instance data only work because that data is restored to the same addresses.
- **Drawn from:** Cornelissen et al. 2007 / Holten bundling (EXTRAVIS); Heapviz 2010 (aggregation and ownership layout); Ware & Mitchell 2008 (motion cues make 3D path tracing work).
- **Legibility risk:** Arcs in 3D can still tangle. Bundles must default to collapsed, motion must be slow and pausable, and labels should come in a 2D overlay rather than as 3D text.

### 9. Memory city with dirty-byte heights and time travel (MEM)
- **What:** Blocks sit at their real addresses on a single ground plane. Footprint is block size and height is bytes changed since the previous switch, linked to the scrubber. Unchanged blocks are drawn semi-transparent and low. The address-by-time sheet (MemoView) is available as a 2D side view.
- **Teaches:** Which parts of Doom's state actually change per turn, and how that relates to snapshot size (full copy vs. what actually changed). It also motivates any optimization the article discusses.
- **Drawn from:** Weninger et al. 2020 (Memory Cities); Wettel & Lanza 2007/2011 (CodeCity); Moreta & Telea 2007 (address-by-time).
- **Legibility risk:** Height comparisons in perspective are imprecise (Munzner, Tory). Put exact byte counts in hover labels and the 2D panel. Never encode a quantity in z-depth, only as height above a common plane.

### 10. Stack towers as literal flame-graph stacks (MAP execution layer)
- **What:** Each tower is the real stack of that instance at its last yield, stacked as frames with names from the log (Doom main loop … frame-end → yield). The top slab is the yield/switch frame, and the CPU orb leaves and enters at the top. Width can encode frame size in bytes.
- **Teaches:** Cooperative stack switching: each instance keeps its own stack, frozen mid-call, and the switch swaps the stack pointer between towers.
- **Drawn from:** Gregg 2016 (flame graph); Trümper et al. 2010 (per-thread call stacks over time).
- **Legibility risk:** Frame-name text on vertical towers gets tilted. Show names in a 2D tooltip, or rotate them to face the camera (billboards), and keep tower count at three or four.

### 11. Switch-box "pipeline chart" pop-out (MAP switch box)
- **What:** Clicking the switch box opens a 2D Konata-style chart. Rows are successive switches and columns are stages: save registers / copy out / copy in / re-patch code bytes / restore SP / resume. Cell width is measured duration. A selected row syncs with the scrubber.
- **Teaches:** The exact order of operations in a switch, and that it is identical every time, a fixed pipeline of steps.
- **Drawn from:** gem5 O3PipeView / Konata (stage-by-cycle charts); Ripes (stepped datapath animation); Tversky et al. 2002 (show discrete steps).
- **Legibility risk:** Low. It is 2D. If per-stage timings are not in the log, show equal-width stages and say so instead of inventing durations.

### 12. One screen, one keyboard as single-ownership "docks" (FB)
- **What:** Model VGA memory and the keyboard as single-occupancy docks, like one loading bay shared by three lines. Only the current owner's framebuffer is lit and connected by the blit beam, while the other two framebuffers sit greyed as "work in progress". Keyboard input is a small parcel stream routed by a valve that points at one instance (or wherever the article says input goes). Tie it to the scrubber so the dock visibly changes hands at each switch.
- **Teaches:** Shared physical resources outside every snapshot and how ownership of them moves at switch time.
- **Drawn from:** Factory/flow metaphors (ViDX, Sankey); SynchroVis (showing the lock holder); ISA-101 (colour only for the active state).
- **Legibility risk:** The metaphor must match the real policy (who blits when, where keys go). Verify against the article's actual mechanism before drawing the valve.

---

## Six most relevant references

1. **Xu, Mei, Ren, Chen — ViDX (TVCG 2017)**, https://doi.org/10.1109/TVCG.2016.2598664. The Marey graph of a production line maps directly onto our schedule.
2. **van der Aalst, Adriansyah, van Dongen — Replaying History on Process Models (WIREs DMKD 2012)**, https://doi.org/10.1002/widm.1045. Token replay plus conformance checking of the real log.
3. **Isaacs et al. — Ravel / Combing the Communication Hairball (TVCG 2014)**, https://doi.org/10.1109/TVCG.2014.2346456. Logical vs. physical time for the time strip.
4. **Weninger, Makor, Mössenböck — Memory Cities (VISSOFT 2020)**, https://ssw.jku.at/General/Staff/Weninger/Papers/Weninger_VISSOFT_20_Preprint.pdf. 3D memory over time with time travel.
5. **Elmqvist & Tsigas — A Taxonomy of 3D Occlusion Management (TVCG 2008)**, https://doi.org/10.1109/TVCG.2008.59. Explode, X-ray and tour presets for the layered map.
6. **Tory, Kirkpatrick, Atkins, Möller — 2D, 3D, and Combination Displays (TVCG 2006)**, https://doi.org/10.1109/TVCG.2006.17, read together with **Munzner, VAD Ch. 6**. When 3D is justified and why every 3D scene needs a linked 2D view.

Close runners-up: SynchroVis / ExplorViz (traces on 3D structure, two-level drill-down); Maiterth et al. VIS 2024 (3D data-centre twin with history and what-if); Kritzinger et al. 2018 (model/shadow/twin vocabulary for labelling recorded vs. hypothetical).

## Verification notes
- DOIs confirmed through search results (publisher, IEEE CS, ACM, OpenAlex, institutional or author pages): 1, 3 (repository), 7, 8, 10, 11, 12, 14, 15, 20, 21, 22, 29, 30, 31, 34, 35, 36, 37, 38.
- DOI given as reported in citing works or from memory, consistent with the paper's listing: 28 (Tory 2006), 33 (Teyseyre & Campo).
- No DOI confirmed; repository/project URL given instead: 2, 4, 5, 6, 19, 23, 24.
- Tools/standards (not peer-reviewed papers): 13, 16, 17, 25, 26. The ISA-101 points come from practitioner summaries, not the standard's text.
- Left out because I could not verify them: Brath "3D InfoVis is here to stay: Deal with it" (2014). It is listed on SciSpace, but its venue and DOI were not confirmed.
