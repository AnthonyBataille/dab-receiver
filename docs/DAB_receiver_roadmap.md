DAB+ RECEIVER PROJECT ROADMAP
=============================

Purpose
-------

This document is the operational roadmap for building a DAB+ receiver for
Windows using an RTL-SDR Blog V3 dongle, while learning the digital signal
processing behind DAB+ and practising AI-agent-assisted software development.

The intended final result is a C++ application that:

* acquires IQ samples from the RTL-SDR through librtlsdr;
* tunes, synchronizes with, and demodulates a DAB+ ensemble;
* discovers the services carried by the ensemble;
* decodes a selected DAB+ audio service;
* plays the decoded audio;
* provides a Windows GUI for channel selection and play/stop control; and
* is maintained in a clean Git/GitHub repository using CMake and an agentic
  development workflow.

The roadmap assumes approximately 1 to 2 hours of work per day. Estimated
durations already assume careful use of AI for research assistance,
boilerplate, tests, reviews, and initial implementations. AI output must still
be validated, particularly when it contains standard-derived constants,
indexing, bit ordering, FFT conventions, or error-correction logic.

Revised plan: incremental C++ implementation with theory introduced as needed.
Phase 1 is completed and preserved unchanged. Phase 0 is also preserved.

Estimated remaining effort, Phases 2-12: 122 to 193 hours; use approximately
125 to 195 hours for planning. These estimates include focused theory,
implementation, validation, and necessary documentation within each phase.
At 7 to 10 productive hours per week, allow approximately 13 to 28 weeks;
missed weeks and difficult debugging can extend this range.

The target is roughly 35-40% less remaining effort than the previous plan,
with observable progress every session and a new capability every few sessions.
A 50% reduction would probably require reusing larger existing decoding stages.
See the estimate summary for the baseline and uncertainty.


HOW CHATGPT SHOULD USE THIS ROADMAP
==================================

When the user asks to start or continue a phase, ChatGPT should:

1. Read the relevant phase and summarize the immediate objective in a few
   sentences.
2. Ask only the questions needed to determine the current state. Ask questions
   in small groups, normally one to three at a time, rather than presenting a
   long questionnaire.
3. Use the answers to propose one small next task that can normally be
   completed in a 1- to 2-hour session.
4. Explain only the theory needed for that task before or alongside
   implementation, normally in a 10- to 20-minute introduction. Ask one or two
   focused understanding questions after applying it; do not impose a long
   theory examination before the next implementation step.
5. For coding tasks, define acceptance criteria before generating or modifying
   code. Prefer small, reviewable changes and tests.
6. Request observable evidence after the task: command output, plots, CRC
   results, logs, test results, or a description of behaviour.
7. Diagnose the first failing stage rather than rewriting the whole pipeline.
8. Record decisions, assumptions, commands, and results in the project
   documentation when they may matter later.
9. Use Git commits as checkpoints after coherent, verified increments.
10. Do not declare a phase complete until its completion criteria are met.
11. Target a new visible capability every two to four sessions. During difficult
    decoding work, a reference-vector match or newly valid CRC is a concrete
    result. Split large tasks further rather than promising completion in one
    session.
12. Evolve one C++ command-line receiver from IQ inspection to service discovery
    to audio. Use Python for plots and small algorithm experiments, not a second
    complete receiver. Reuse the completed Phase 1 experiments where useful.
13. Keep short notes on what was learned and what was verified. Let AI assist
    with small implementations, tests and reviews; the user should understand
    the interfaces, state and evidence before accepting a change.

At the beginning of a continuation session, ChatGPT should establish:

* Which phase and subtask is currently active?
* What was the last verified result?
* What files or code changed since then?
* Is there a concrete error, uncertainty, or decision blocking progress?
* How much time is available for the present session?

ChatGPT should distinguish three kinds of claim:

* UNDERSTOOD: the user can explain the concept and apply it in a small example.
* IMPLEMENTED: code exists and builds or runs.
* VERIFIED: objective output demonstrates that it behaves correctly.

Only VERIFIED work satisfies an implementation milestone.


PROJECT-WIDE ENGINEERING RULES
==============================

* Develop against repeatable IQ recordings before relying on live reception.
* Keep at least one recording known to decode in an established DAB receiver.
* Preserve intermediate observability: synchronization metrics, constellations,
  CRC counts, frame counters, buffer occupancy, and error statistics.
* Unit-test pure transformations such as CRCs, scramblers, interleavers,
  puncturing maps, and bit packing independently from RF input.
* Cite the specification or another authoritative source beside every table of
  standard-defined constants.
* Prefer soft-decision values through the channel-decoding stages.
* Compare C++ output with independent reference vectors or receiver output.
  Where a focused Python experiment exists, retain its values as a reference.
* Integrate tested Viterbi, Reed-Solomon, FFT and AAC implementations where
  suitable. Check Windows build support, conventions, licensing and test
  evidence before committing to a dependency. Deep internal derivations may
  be deferred; interface understanding and integration validation may not.
* Restrict the first release to Mode I, manual tuning and a documented set of
  supported audio/protection configurations. Reject unsupported configurations
  clearly. Broader profile coverage and optional services follow the release.
* Keep architecture proportional to the current milestone. Define the next
  block's interface and reset rules when needed; avoid speculative frameworks.
* Do not optimize until the correct offline pipeline is established and a
  profiler identifies a real-time bottleneck.
* Keep the DSP/backend independent of the GUI.
* Do not commit large IQ recordings to the main Git repository. Document how to
  obtain them or store small trimmed fixtures through an appropriate mechanism.
* AI should not make large architectural or algorithmic changes without first
  stating the intended change, evidence, risks, and validation plan.


PHASE 0 - SCOPE, BASELINE, AND WORKFLOW
======================================

Estimated effort: 6 to 10 hours (about 1 week)
Difficulty: Low

Goal
----

Turn the general project idea into a bounded minimum viable product (MVP),
establish the development environment, and define how the user and ChatGPT will
collaborate. The initial scope should normally target DAB+ Band III, Mode I,
one RTL-SDR device, manual block selection, ensemble/service discovery, one
selected audio service, and basic Windows playback and GUI controls.

ChatGPT guidance questions
--------------------------

ChatGPT should establish:

* Which Windows and Visual Studio versions will be used?
* Are Git, CMake, Python, and the Codex tooling installed and working?
* Is there already a Git repository, and should it be connected to GitHub now?
* Which features belong to the MVP, and which are explicitly deferred?
* Should the first prototype use Python/NumPy/SciPy?
* What testing and formatting tools are preferred for C++?
* Will development use branches and pull requests, or small direct commits?

Work items
----------

1. Write the MVP and non-goals.
2. Inventory installed hardware and development tools.
3. Create the repository structure and initial README.
4. Create a minimal CMake project that builds on the target Windows machine.
5. Add formatting, warnings, a test framework, and basic continuous
   integration if appropriate.
6. Add concise instructions for AI agents: architecture boundaries, build/test
   commands, coding conventions, and required validation.
7. Create milestones or issues corresponding to this roadmap.

Expected outputs
----------------

* A written MVP and non-goals list.
* A buildable repository connected to GitHub.
* Documented local build and test commands.
* A recorded set of architectural and workflow decisions.

Completion criteria
-------------------

The empty or skeletal project can be cloned and built using documented steps,
and the MVP is sufficiently precise to decide whether a proposed feature is in
scope.

Likely difficulties
-------------------

Scope growth, dependency choices made too early, Windows dependency setup, and
letting AI generate a large structure before the interfaces are understood.


PHASE 1 - SDR AND OFDM FOUNDATIONS
=================================

Estimated effort: 15 to 25 hours (2 to 3 weeks)
Difficulty: Moderate

Goal
----

Understand what the RTL-SDR returns and acquire the minimum DSP knowledge needed
to reason about the receiver: complex IQ sampling, spectra, frequency shifts,
filtering, decimation, FFTs, OFDM, noise, gain, and frequency error.

ChatGPT guidance questions
--------------------------

* How comfortable is the user with complex exponentials, Fourier transforms,
  convolution, probability, and numerical programming?
* Which concepts have already been studied or implemented?
* Is Python with NumPy, SciPy, and Matplotlib available?
* Does the user prefer theory first or a small experiment followed by theory?
* Can generated signals and plots be saved as reproducible exercises?

Work items
----------

1. Represent a sampled sinusoid as real samples and as complex IQ samples.
2. Plot time-domain signals and FFT spectra with correct frequency axes.
3. Translate a signal in frequency using a complex oscillator.
4. Design and apply a low-pass filter; study transients and group delay.
5. Demonstrate resampling or decimation where appropriate.
6. Build a small synthetic OFDM modulator/demodulator.
7. Add a cyclic prefix and observe robustness to delayed paths.
8. Add noise, frequency offset, and timing offset; observe failure modes.

Expected outputs
----------------

* Small reproducible notebooks or scripts.
* Plots demonstrating frequency translation and filtering.
* A synthetic OFDM round trip whose transmitted symbols are recovered.
* Personal notes relating the mathematical operations to array operations.

Completion criteria
-------------------

The user can explain the role of IQ samples, FFT bins, cyclic prefixes, and
frequency/timing error, and a synthetic OFDM exercise passes with known data.

Likely difficulties
-------------------

FFT normalization and sign conventions, negative frequencies, confusing
sample rate with signal bandwidth, and converting continuous-time formulas into
finite indexed arrays.


PHASE 2 - MINIMUM DAB+ ORIENTATION
==================================

Estimated effort: 3 to 5 hours
Difficulty: Moderate

Goal
----

Understand the minimum structure needed to begin work on a real recording:
Mode I transmission frames, the null and phase-reference symbols, and the
roles of FIC and MSC. Retain a broad IQ-to-audio map without studying every
stage in detail before implementation.

ChatGPT guidance questions
--------------------------

* Which parts of Phase 1 can be reused immediately?
* Which authoritative specifications will supply Mode I constants?
* Can the user explain why service metadata is needed before selecting MSC
  data?

Work items
----------

1. Draw a one-page chain from IQ through synchronization, OFDM, FIC/MSC, DAB+
   framing and AAC decoding to PCM.
2. Create a small, referenced table of the Mode I parameters needed for frame
   and symbol inspection; validate values against the specification.
3. Introduce ensemble, service and subchannel, and distinguish transmission
   frames from the later CIF and superframe concepts without requiring mastery
   yet.
4. Choose the initial split: own synchronization, demodulation and
   integration; reuse suitable error-correction and audio implementations.
5. Define the next capture or IQ-inspection task and its observable acceptance
   criterion.

Expected outputs
----------------

* A one-page receiver map and short glossary that grow only as needed.
* A referenced Mode I parameter table.
* A concrete first recording experiment.

Completion criteria
-------------------

The user can explain the separate roles of FIC and MSC, identify the major
frame regions, and describe the next experiment. Detailed puncturing,
interleaving, FIG parsing and audio-framing knowledge are not prerequisites.

Likely difficulties
-------------------

Dense terminology and unnecessary standard reading. Defer FIC coding to Phase
6, MSC coding and superframes to Phase 7, and AAC configuration to Phase 8. Do
not replace the former theory phase with an equally long design phase.


PHASE 3 - VERIFIED REFERENCE IQ CAPTURE
=======================================

Estimated effort: 4 to 7 hours
Difficulty: Moderate

Goal
----

Establish one strong, repeatable input that is independently known to decode.
Reuse previously verified driver and capture work rather than repeating it.

ChatGPT guidance questions
--------------------------

* Which driver, capture script and recordings already work?
* Which local ensemble can an established receiver decode?
* Can that receiver replay the captured format, or is a documented conversion
  needed?

Work items
----------

1. Verify dongle operation and choose a strong ensemble using an established
   receiver.
2. Adjust antenna position and gain sufficiently for stable reception; avoid
   prolonged weak-signal optimization.
3. Capture a short unsigned 8-bit interleaved IQ recording with documented
   rate, frequency and gain.
4. Replay the actual recording in an established decoder, converting format if
   necessary and preserving the original. Live reception alone is not proof
   that the file is valid.
5. Record independently observed ensemble and service information and retain a
   sufficiently long fixture for later interleaver startup and audio
   verification.
6. Defer difficult captures and a broad reception matrix until the strong
   recording supports a working pipeline.

Expected outputs
----------------

* A reusable capture command or PowerShell script.
* One independently decoded reference recording and its metadata.
* A reference service list and instructions for replaying the recording.

Completion criteria
-------------------

The actual saved recording decodes in an independent receiver and its format
and metadata are unambiguous. If local reception blocks progress, use a
legally available known-good Mode I fixture temporarily and keep hardware
verification as an explicit prerequisite for Phase 10.

Likely difficulties
-------------------

Driver conflicts, clipping, poor signal, sample loss and replay-format
incompatibility. These estimates assume ordinary setup issues; unresolved RF
problems should not consume the offline software development schedule.


PHASE 4 - MINIMAL C++ OFFLINE HARNESS
=====================================

Estimated effort: 4 to 6 hours
Difficulty: Moderate

Goal
----

Start the target-language receiver immediately: deterministic file input,
correct IQ conversion, a small command-line runner and exported diagnostics.
Python remains a plotting and focused experimentation tool.

ChatGPT guidance questions
--------------------------

* What CMake project and test setup already exist?
* What is the exact capture format and sample rate?
* What minimal source interface allows file input now and device input later?

Work items
----------

1. Read interleaved unsigned I/Q and convert to complex samples; verify
   centering, scale and I/Q order using a small known input.
2. Report sample count, duration and input configuration.
3. Process bounded, deterministic blocks and preserve samples across block
   boundaries where required.
4. Export a power trace and spectrum for plotting with existing Python tools.
5. Record sample units, array ordering and FFT conventions in a short note.
6. Add only configuration and diagnostic outputs required for the next
   synchronization experiment.

Expected outputs
----------------

* A reproducible C++ command that inspects the recording.
* Correct spectrum and power-versus-time plots.
* A small input-conversion fixture and concise conventions note.

Completion criteria
-------------------

The same input and command produce identical diagnostics. Sample count,
duration and spectrum agree with the capture metadata. File input is bounded
in memory and does not depend on interactive notebook state.

Likely difficulties
-------------------

Incorrect unsigned conversion, I/Q reversal, partial blocks and overbuilding a
generic harness. A spectrogram is optional unless it answers a concrete
diagnostic question.


PHASE 5 - INCREMENTAL SYNCHRONIZATION AND OFDM
==============================================

Estimated effort: 20 to 32 hours
Difficulty: Very high

Goal
----

Recover frame timing and differential carrier values directly in C++, first on
the strong recording. Introduce synchronization theory alongside each
observable step and retain precise carrier and soft-value conventions.

ChatGPT guidance questions
--------------------------

* Can null intervals be located in the power trace?
* Which timing and frequency estimates are currently verified?
* At which intermediate array does output first disagree with a reference?

Work items
----------

1. Detect candidate null intervals and overlay markers on the power trace.
2. Establish repeated tentative frame boundaries and report frame spacing.
3. Construct or use the phase-reference symbol with authoritative definitions;
   verify reference construction separately.
4. Estimate and correct coarse and fine frequency offset; report estimates and
   their stability.
5. Choose symbol windows, remove cyclic prefixes, FFT and select active
   carriers.
6. Differentially demodulate and undo frequency interleaving, preserving
   documented ordering and signs.
7. Produce soft decisions and export a constellation and diagnostic vectors.
8. Add timing/drift tracking to the extent required for the reference capture;
   extend for continuous reception in Phase 10.
9. Use a small Python experiment only when it resolves a specific algorithm
   uncertainty; compare it with C++ results.

Expected outputs
----------------

* Successive checkpoints: null markers, repeated boundaries, frequency
  correction and constellation.
* Frame counters, timing/frequency metrics and soft values in documented
  order.
* Reference-vector checks for carrier mapping and differential demodulation.

Completion criteria
-------------------

Frame lock is stable over the selected reference interval and symbol/carrier
mapping has independent checks. A structured constellation is provisional
evidence only: successful FIB CRCs in Phase 6 are the downstream confirmation
of the complete demodulation path. Revisit this phase when those checks fail.

Likely difficulties
-------------------

Off-by-one timing, FFT conventions, phase-reference construction, frequency
ambiguity and plausible-looking output with wrong signs or ordering. Keep each
diagnostic targeted; do not build a general synchronization dashboard.


PHASE 6 - FIC DECODING AND ENSEMBLE INSPECTOR
=============================================

Estimated effort: 16 to 25 hours
Difficulty: Very high

Goal
----

Turn soft demodulator output into CRC-valid FIBs and enough metadata to list
audio services and configure one supported subchannel. Learn FIC coding and
metadata structure while obtaining these successive results.

ChatGPT guidance questions
--------------------------

* Which tested soft-decision Viterbi implementation fits the Windows build?
* Are puncturing, bit order and decoder metric conventions independently
  checked?
* Which FIGs are necessary for the reference service and its configuration?

Work items
----------

1. Integrate and test the Viterbi decoder with independent vectors, including
   soft-value signs, state conventions and termination.
2. Implement the FIC depuncturing map using cited definitions and verify its
   dimensions and ordering.
3. Decode the FIC channel data, remove energy dispersal in the specified
   receiver order, assemble FIBs and verify CRCs.
4. Make the first valid FIB a checkpoint, then measure sustained CRC success.
5. Parse only the FIGs needed for ensemble identity, labels, service/component
   mapping and supported subchannel configuration.
6. Keep parsing separate from a small service database; handle incomplete
   metadata without inventing values.
7. Compare the ensemble, service list and selected configuration with the
   independent receiver.

Expected outputs
----------------

* A first valid FIB, followed by a CRC success report.
* An ensemble name and service list from the C++ command.
* Selected subchannel start, size, bit rate and protection configuration.
* Tests for dispersal, depuncturing, decoder integration and CRC.

Completion criteria
-------------------

Many consecutive FIBs pass CRC on the strong recording. The service list and
selected subchannel configuration agree with independent evidence. Unsupported
configurations are identified clearly. Exhaustive optional FIG support is
deferred.

Likely difficulties
-------------------

Bit ordering, metric signs, puncturing tables and incomplete or changing
metadata. Understand what convolutional coding and soft decoding accomplish;
defer a full Viterbi derivation and a custom decoder implementation.


PHASE 7 - ONE MSC SERVICE TO VALID DAB+ AUDIO UNITS
===================================================

Estimated effort: 28 to 43 hours
Difficulty: Very high; largest remaining risk

Goal
----

Extract and decode one selected reference service through to validated audio
access units. Initially implement the protection configuration that this
service actually uses, with explicit supported-scope checks.

ChatGPT guidance questions
--------------------------

* What exact protection configuration does the selected service use?
* What history and reset behaviour does time deinterleaving require?
* Which earliest check fails: extraction, channel decoding, alignment or
  audio-unit CRC?

Work items
----------

1. Extract selected capacity units using FIC metadata; verify ranges against a
   reference.
2. Implement time deinterleaving with explicit startup, history and reset
   behaviour; test across input chunk boundaries.
3. Implement the selected protection profile, depuncture and reuse the
   verified soft-decision Viterbi decoder.
4. Remove energy dispersal and compare channel-decoded bytes where reference
   data are available.
5. Accumulate candidate DAB+ superframes and implement alignment/fire-code
   checks according to authoritative definitions.
6. Integrate tested Reed-Solomon correction with verified field, layout and
   correction conventions.
7. Parse superframe configuration and access-unit boundaries, and verify
   access-unit CRCs.
8. Retain useful failure fixtures and corrected/uncorrectable statistics.
9. Reject unsupported profiles clearly; defer additional profile coverage and
   advanced recovery until the core path works.

Expected outputs
----------------

* Checkpoints for extracted subchannel data, decoded bytes, aligned
  superframes and valid audio units.
* A stream or file of validated audio access units and required codec
  metadata.
* Tests for interleaver state, protection mapping, bit packing and error-
  correction integration.

Completion criteria
-------------------

The reference recording yields a sustained sequence of correctly aligned,
Reed-Solomon-processed superframes and CRC-valid access units after documented
startup buffering. The selected protection configuration is recorded as
supported.

Likely difficulties
-------------------

Stateful interleaving, capacity-unit indexing, puncturing, packing and false
alignment. A byte dump is an observable checkpoint but is not proof of correct
decoding. Defer Reed-Solomon algebraic derivations, not integration checks.


PHASE 8 - FIRST OFFLINE AUDIO
=============================

Estimated effort: 8 to 13 hours
Difficulty: High

Goal
----

Convert verified DAB+ audio units to PCM using an existing AAC decoder and
write a WAV file. Obtain the first audible result before implementing
continuous playback and live buffering.

ChatGPT guidance questions
--------------------------

* Which available decoder supports the required DAB+ audio configuration?
* What framing and configuration does its API expect?
* Are access units CRC-valid before entering the decoder?

Work items
----------

1. Choose and build a suitable decoder dependency; record its licence and
   distribution requirements.
2. Adapt validated DAB+ access units and configuration to the decoder input
   interface.
3. Decode a short interval to PCM and write a WAV with correct sample rate,
   channel count and sample format.
4. Listen to the short clip, then decode the full reference recording.
5. Check duration with documented startup loss accounted for, and report
   rejected frames or decoder failures.
6. Keep decoder resets and invalid-input handling explicit; defer continuous
   playback queues to Phase 10.

Expected outputs
----------------

* A short intelligible clip, then a reproducible full-recording WAV.
* A documented decoder dependency and configuration adapter.
* Basic invalid-input handling and audio-format checks.

Completion criteria
-------------------

The reference recording reproducibly produces intelligible audio with correct
rate, channel layout and expected duration after startup losses. There are no
unexplained decoder errors. Live playback is not required at this gate.

Likely difficulties
-------------------

DAB+ framing versus ordinary AAC containers, codec configuration and upstream
errors masquerading as codec failures. Internal HE-AAC mathematics is
deferred; the framing and configuration contract must be understood.


PHASE 9 - C++ CONSOLIDATION AND OFFLINE REGRESSION
==================================================

Estimated effort: 5 to 8 hours
Difficulty: Moderate

Goal
----

Consolidate the C++ pipeline already built in Phases 4-8. This replaces the
former full Python-to-C++ port and introduces no second receiver
implementation.

ChatGPT guidance questions
--------------------------

* Which interfaces or ownership rules have caused concrete integration
  problems?
* Are reset and startup behaviours explicit for every stateful block?
* Can a single documented command reproduce service discovery and audio
  output?

Work items
----------

1. Review source, DSP, metadata, audio and output boundaries without
   redesigning working modules speculatively.
2. Resolve concrete state ownership, lifetime and reset issues.
3. Connect the existing module checks into an offline regression command using
   the reference fixture.
4. Verify service discovery, CRC statistics and audio format/duration; compare
   exact intermediate bytes where deterministic.
5. Keep short architecture, dependency and command documentation.
6. Refactor only while preserving the verified behaviour.

Expected outputs
----------------

* One documented C++ offline receiver command.
* A compact automated regression path and explicit reset rules.
* Updated architecture and dependency notes.

Completion criteria
-------------------

The C++ command lists services and decodes the selected supported service to
audio. Relevant module tests and the offline regression pass. Diagnostics
remain available without depending on Python for receiver execution.

Likely difficulties
-------------------

Turning consolidation into a rewrite, dropping diagnostics, or adding
premature concurrency. Python remains useful for independent experiments and
plots but is not a runtime prerequisite for the receiver.


PHASE 10 - LIVE ACQUISITION AND PLAYBACK
========================================

Estimated effort: 16 to 25 hours
Difficulty: Very high

Goal
----

Replace file input with librtlsdr, add bounded audio playback and keep the
verified backend running continuously. Handle ordinary signal loss and orderly
shutdown within the supported scope.

ChatGPT guidance questions
--------------------------

* Does independent device acquisition work without sample loss?
* Where are bounded queues and thread boundaries actually necessary?
* Can the pipeline process input faster than real time with headroom?

Work items
----------

1. Implement the RTL-SDR source behind the existing input interface and verify
   live sample statistics.
2. Feed live input through service discovery before enabling audio playback.
3. Add audio output with documented PCM format and bounded buffering.
4. Define start, stop, manual retune and reset transitions; propagate input
   discontinuities explicitly.
5. Measure processing throughput, queue depth, sample loss, underruns and CPU
   use.
6. Profile and optimize only measured bottlenecks; extend synchronization
   tracking when continuous input requires it.
7. Recover frame lock after ordinary signal disturbances; handle device
   removal with clear status and a controlled restart path.
8. Progress from a short live clip to a longer run and then the one-hour
   stability check.

Expected outputs
----------------

* Live IQ diagnostics, then a live service list, then audible live radio.
* Bounded queue and performance metrics.
* Verified stop, retune, signal recovery and shutdown behaviour.

Completion criteria
-------------------

A supported local station plays for at least one hour without unbounded memory
growth, persistent underruns or deadlock. Ordinary signal interruptions permit
automatic lock recovery without a manual decoder restart. Device removal and
shutdown are controlled; automatic device hot-plug recovery is optional.

Likely difficulties
-------------------

Back-pressure, clock drift, USB discontinuities, processing spikes and thread
shutdown. Basic recovery remains required; sophisticated weak-signal behaviour
and broad hardware support are deferred.


PHASE 11 - MINIMAL WINDOWS GUI
==============================

Estimated effort: 8 to 13 hours
Difficulty: Moderate

Goal
----

Expose the working receiver through a small responsive Windows interface. Keep
the first GUI focused on manual tuning, service selection and playback.

ChatGPT guidance questions
--------------------------

* Which one toolkit best fits the existing Windows build and dependency
  constraints?
* What small command/status interface already exists in the backend?
* How should no-device, no-signal and unsupported-service states appear?

Work items
----------

1. Choose one toolkit using brief explicit criteria; avoid a prolonged
   framework comparison.
2. Expose receiver commands and status independently of widgets.
3. Add manual block selection, supported service selection, play/stop and
   volume.
4. Display ensemble/service identity and basic reception/error status.
5. Keep device and DSP operations off the GUI thread and discard stale service
   data after retuning.
6. Verify loading, no-device, no-signal and unsupported-configuration states.
7. Defer scanning, presets, visual polish, advanced plots and nonessential
   settings persistence.

Expected outputs
----------------

* A usable radio window connected to the verified backend.
* Clear basic status and common failure states.
* Responsive tuning, stopping and application shutdown.

Completion criteria
-------------------

The user can launch, manually tune, select a supported service, play and stop
audio without the command line. The GUI remains responsive and the backend has
no dependency on GUI widgets.

Likely difficulties
-------------------

Toolkit deployment, stale asynchronous updates, blocking shutdown and scope
growth. Automatic scanning is outside the first release.


PHASE 12 - FOCUSED PACKAGING AND FIRST RELEASE
==============================================

Estimated effort: 10 to 16 hours
Difficulty: High

Goal
----

Produce a reproducible Windows release for the configurations actually
implemented and verified. Package dependencies and document limits without
expanding into a comprehensive DAB conformance project.

ChatGPT guidance questions
--------------------------

* What exact modes, profiles and audio configurations are verified?
* Which runtime DLLs, notices and driver instructions are required?
* Can a clean compatible Windows setup reproduce installation and operation?

Work items
----------

1. Run the existing unit, reference-vector and offline regression checks; add
   coverage for concrete remaining gaps.
2. Check the supported reference service, corrupt/truncated file input,
   stop/retune, signal loss and device disconnection.
3. Finalize a reproducible release build and appropriate CI checks; keep
   hardware checks separately documented.
4. Package executables, dependencies and required licence notices.
5. Document installation, RTL-SDR driver setup, antenna basics, normal
   operation and diagnostic collection.
6. Test the package on a clean compatible Windows setup.
7. List supported configurations and known limits explicitly, tag the source
   and produce a versioned package.
8. Place broad ensemble/profile testing, difficult captures and optional
   features in a post-release backlog.

Expected outputs
----------------

* A packaged, versioned Windows application.
* Reproducible build and regression instructions.
* Concise user documentation and an explicit supported-scope statement.

Completion criteria
-------------------

Following the instructions on a clean compatible Windows setup produces a
working receiver for the documented supported scope. The release is
rebuildable from its tagged source and required dependencies/notices are
included.

Likely difficulties
-------------------

Missing DLLs, local-path assumptions, driver setup and redistribution
requirements. Do not cut packaging reproducibility to meet the estimate;
reduce optional feature scope instead.


MILESTONES AND DECISION GATES
=============================

Milestone A - Verified RF recording
After Phase 3, an actual saved recording decodes independently, with metadata
and a reference service list. A temporary external fixture can unblock offline
work, but does not satisfy local hardware readiness for Phase 10.

Milestone B - C++ OFDM demonstrator
After Phase 5, the receiver reports stable frame boundaries, frequency estimates
and documented differential carrier values. Phase 6 CRCs confirm the complete
demodulation path; a constellation alone is not proof.

Milestone C - Ensemble inspector
After Phase 6, the same executable reports CRC-valid FIBs, ensemble identity,
audio services and a supported selected subchannel. This is a useful standalone
intermediate application.

Milestone D - Offline C++ radio
After Phase 8, the selected recording yields intelligible WAV audio directly
through the C++ pipeline. There is no later full-language port.

Milestone E - Consolidated offline receiver
After Phase 9, the working C++ chain has verified reset rules and a reproducible
regression command.

Milestone F - Live command-line radio
After Phase 10, live reception and playback pass the one-hour stability gate.

Milestone G - First complete release
After Phase 12, the minimal GUI application is packaged and documented for its
explicitly supported configurations.

At each gate, briefly review evidence, remaining risks and scope. Continue the
agreed plan unless evidence calls for a change; do not add a lengthy approval
or examination step. Re-estimate after the first valid FIB and first valid audio
units, when the largest implementation uncertainties become clearer.

Keep optional features after the first release: automatic scanning, presets,
recording controls, slideshows, dynamic labels, EPG, other transmission modes,
non-audio services, broader protection-profile coverage and advanced diagnostics.
Additional profiles may be brought forward only if needed for the selected
reference service or explicitly requested.


SESSION TEMPLATE
================

Current phase:
Current subtask:
Time available today:
Last verified result:
Evidence or test result:
Current failure or uncertainty:
Files changed:
Immediate concept to understand:
Decision needed:
Next acceptance criterion:

Start with one small task and a visible acceptance criterion. Explain the needed
theory, implement, run, inspect, then ask one or two focused understanding
questions. End with a coherent verified checkpoint where possible and record
the exact next action. A failed check can still narrow the problem usefully;
do not label it verified success.


ESTIMATE SUMMARY
================

Phase 0:   Scope, baseline, workflow                 6-10 hours (unchanged)
Phase 1:   SDR and OFDM foundations                 15-25 hours (completed;
                                                               unchanged)
Phase 2:   Minimum DAB+ orientation                   3-5 hours
Phase 3:   Verified reference IQ capture              4-7 hours
Phase 4:   Minimal C++ offline harness                4-6 hours
Phase 5:   Incremental synchronization and OFDM     20-32 hours
Phase 6:   FIC and ensemble inspector               16-25 hours
Phase 7:   One MSC service to valid audio units     28-43 hours
Phase 8:   First offline audio                       8-13 hours
Phase 9:   C++ consolidation                         5-8 hours
Phase 10:  Live acquisition and playback            16-25 hours
Phase 11:  Minimal Windows GUI                       8-13 hours
Phase 12:  Focused packaging and first release      10-16 hours

Remaining phase budgets total: 122-193 hours.
Rounded planning range: approximately 125-195 hours remaining.

These revised budgets assign theory, tests, documentation and integration to
the phase doing that work; do not add a separate porting or theory allowance.
Previously verified work should be credited, not repeated. Any unfinished
Phase 0 work is additional to the remaining Phase 2-12 estimate.

The original roadmap stated 220-330 effective total hours and explicitly treated
its phase budgets as overlapping. Using its low-end total minus the low-end
Phase 0/1 budgets, and its high-end total minus the high-end Phase 0/1 budgets,
gives a rough 199-295-hour remaining comparison baseline. This assumes Phase 0
is complete; it is a scenario comparison, not measured historical effort or
a formal uncertainty interval.

Against that baseline, the rounded revised range saves approximately 74-100
hours, or 34-37% when comparing corresponding endpoints. The practical target
is roughly 35-40%. A 50% saving is a stretch and would likely require reusing
larger existing FIC/MSC stages, reducing the amount implemented personally.

At 7-10 productive hours per week, the rounded remaining range corresponds to
about 13-28 weeks. Use actual weekly hours and milestone evidence to revise the
calendar estimate. Synchronization, MSC debugging and Windows dependency
integration remain the main uncertainties; these are not guaranteed deadlines.

Savings come primarily from avoiding a complete Python implementation and C++
port, reducing up-front theory, integrating tested error-correction/codec
components, and narrowing first-release scope. Independent validation,
observability, bounded memory, clean shutdown and reproducible packaging remain
required.
