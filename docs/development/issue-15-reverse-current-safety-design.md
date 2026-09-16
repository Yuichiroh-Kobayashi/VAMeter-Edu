# Issue #15 reverse-current indication and Training-mode relay protection

Status: DESIGN REVIEW ONLY

Implementation authority: NO

Production threshold: NOT ESTABLISHED

Physical safety qualification: NOT RUN

Base commit: 3348bc86d5d911e131315ad35d6cec022ab01a59

## 1. Decision

Issue #15 is **partially scaffolded, not integrated, and not safe to activate**. The current
tree already has a pure reverse-current detector, a signed-current qualification logger,
positive-domain waveform geometry clipping, and a recorder-oriented local-fault model.
It does not connect the detector to the measurement daemon, Training mode, relay control,
or a dedicated warning screen. No production trip threshold is established.

The implementation must keep six concerns separate: signed measurement authority,
classification, Training latch, relay authority, presentation, and reset lifecycle. It
must consume the processed/calibrated signed value before presentation clipping. The
threshold symbol for all production design and code review is:

```text
I_reverse_threshold = PHYSICAL_AUTHORITY_REQUIRED
```

The detector's `3` qualifying cycles remain the Issue's reviewed design assumption, not
physical threshold authority. No new current/voltage rating, AC capability, calibration
redesign, or CSV/D2B schema is authorized.

## 2. Signed-current data flow

| Stage | Exact current source/transformation | Authority effect |
|---|---|---|
| INA226 inputs | Two INA226 instances are configured: HC with `0.01` ohm calibration and LC with `1.0` ohm calibration. Checked reads return `float`; INA226 register signing is retained by those APIs. | Sensor input; signed-capable, but not yet product measurement authority. |
| Range choice | GPIO alert-driven `_is_low_current_mode` selects LC/HC. Startup chooses HC at/above `0.05 A`, LC at/below `0.03 A`; alerts use corresponding shunt limits. A sufficiently negative LC shunt value (`< -0.005`) sets legacy `_is_reverse_measuring`, switches to HC, and repeats acquisition; HC exits that legacy mode above `-0.003`. | **Changes which sensor/range supplies authority.** The constants are legacy range-control seams, not Issue #15 production trip authority. |
| LC processing | LC uses checked shunt-voltage read and assigns volts numerically as amperes because the shunt is `1 ohm`; it subtracts `_pm_data_daemon_current_offset`, then clamps a negative result to `0.0f`. | **Changes measurement authority.** This existing clamp prevents final signed preservation for shallow LC negatives and is a material Issue #15 gap. It must not be confused with graph clipping. |
| HC processing | HC uses `readShuntCurrentChecked()` and presently applies no equivalent offset or zero clamp in this function. The legacy reverse-range transition allows negative HC values to reach `PMData_t::shuntCurrent`. | **Changes/establishes processed authority.** HC can preserve negative current; equivalence across ranges is not proven. |
| Validity | `BuildViValidMask()` marks current valid only when selected checked read succeeds, the processed float is finite, overflow-status read succeeds, and math overflow is not asserted. | Does not numerically transform a valid value; classifies validity. Current reverse polarity is distinct from invalid/no-data. |
| PM authority | `_pm_data_daemon->shuntCurrent` is copied into application `POWER_MONITOR::PMData_t::shuntCurrent`, in amperes. Peak, minimum, averages, capacity, and energy currently consume it. | This processed HAL value is current application authority. Some aggregate semantics would include signed values; Issue #15 must not silently redesign them. |
| Local numeric display | Power Monitor and Waveform format the HAL value using unit adaptation. Formatting/rounding is downstream. | Presentation only. No numeric rewrite is the intended contract. |
| Waveform buffer | Waveform stores `shuntCurrent * 1000`; `_pm_data_a_scale` is chart precision scaling. Auto-scale considers the positive peak and sets a `0 A` lower bound. | Presentation only; the factor is not calibration or a unit change in measurement authority. |
| Waveform geometry | `ClipToPositiveDomain()` rejects two-negative segments, clips a crossing at its calculated zero intersection, and refuses invalid/non-finite endpoints. The renderer uses this helper rather than overwriting buffered samples. | Presentation only. It avoids a fabricated zero plateau and does not bridge an entirely reverse interval. |
| CSV | Recorder borrows the same daemon `PMData_t` and passes `shuntCurrent` directly to `WriteSample()`, which prints current as `%.7f` under schema `voltage,current,elapsed_ms`. | Serializes processed authority without a CSV clamp. Negative HC values survive; LC values already clamped upstream cannot be recovered. CSV has no per-row validity bit. |
| D2B | The daemon calls `D2B_PRODUCER::Tap()` with processed `shuntCurrent` and `validMask`. Acquisition canonicalization retains finite valid negative floats; frame writing emits the float unchanged when current-valid. Invalid current uses positive zero plus an unset validity bit. | Serializes processed authority without a presentation clamp; sign is preserved for valid negative HAL values. |
| Signed-current observation | Optional Kconfig-disabled qualification harness publishes the same processed value, range, validity mask, read outcomes, and overflow state to a bounded SPSC ring and diagnostic task. | Observation only; it does not detect, trip, or change authority. Because publication occurs after LC processing, it cannot observe an LC negative erased by the existing clamp. |
| Reverse detector | Pure `Detector::observe(valid, signedCurrentA)` exists only in host-test code/library inventory; no production call site exists. | Classification only when eventually integrated. It must never rewrite the input. |
| Training safety | `SystemConfig::probeMode` selects Normal/Training UI labeling, but no reverse detector/latch/controller consumes it. | Missing. No Issue #15 safety action currently occurs. |

The required future tap is after range-specific calibration/processing has produced a
signed, validity-qualified sample, but before display cropping. The LC clamp means this
seam is not currently sufficient for all ranges; resolving it is measurement-pipeline
work requiring physical evidence, not deletion of a line based on source inspection.

## 3. Existing #15 code inventory

| Area | Current status and purpose |
|---|---|
| `app/libs/reverse_current_detector/` | Pure C++11 detector and result/event model. Host-tested; not included by any production call site. |
| `tests/reverse_current_detector/` | Covers positive/zero/shallow negative, exact edge, 1/2/3 qualifiers, valid interruption, invalid and non-finite holds, latch persistence, one-shot `enteredLatched`, and invalid configuration. Its `{-0.25F, 3}` is test data only. |
| `app/libs/signed_current_observation/` | Fixed record/formatter and bounded SPSC ring, preserving sign and explicit range/read/overflow metadata. |
| `signed_current_observation_device.*` | Qualification-only static queue/task and logs. Kconfig help explicitly denies detector, relay, fault, and threshold behavior; default is disabled. |
| `tests/signed_current_observation/` | Covers signed formatting, metadata, buffer limits, queue order/full/drop behavior, and counter saturation/wrap-related behavior. |
| `app/libs/current_waveform_clip/` | Header-only geometric clipper for the non-negative visible domain. |
| `tests/current_waveform_clip/` | Covers positive/zero/crossing/reverse, invalid/non-finite endpoints, and no bridge across a fully negative interval. |
| `app/apps/app_waveform/view/waveform.cpp` | Already integrates geometric clipping and a zero lower bound in the current plot. Buffered HAL samples remain signed where HAL supplied a sign. |
| `app/libs/local_fault/` | Recorder fault payload with relay-off software confirmation and cleanup precedence. It permits acknowledgement only in its normal presentation; it is not a reset-only safety latch. |
| `tests/local_fault/` | Covers recorder classification, precedence, retained confirmation, and acknowledgement gates. It does not cover reverse-current reset-only semantics. |
| `tests/educational_interaction_contract/` | Covers Help/QR, recorder/live-share input ownership, not Issue #15 Training protection. It is a future regression seam for blocked UI actions. |

## 4. Acceptance-criteria gap matrix

| Requirement | Classification | Evidence/gap |
|---|---|---|
| Signed negative representability before presentation | **Implemented but not integrated** | HC path, PM type, CSV and D2B are signed-capable; LC processing still clamps negatives. End-to-end all-range authority is not met. |
| No new measurement-stage negative clamp | **Implemented + covered** | No new Issue #15 clamp was added; current standards/tests protect presentation separation. The pre-existing LC clamp remains a gap to resolve. |
| Reverse distinct from invalid/no-data | **Implemented but not integrated** | D2B validity and detector `measurementValid` distinguish them, but UI/Training integration is absent. |
| Graph lower bound exactly `0 A` | **Implemented + covered** | Current positive range and renderer use zero lower bound. |
| Clip geometry, do not replace samples | **Implemented + covered** | Crossing geometry helper and tests; buffer is not rewritten. |
| Do not bridge excluded reverse interval | **Implemented + covered** | two-negative segments are rejected; tests pin the behavior. |
| Visible reverse warning | **Missing** | No warning presentation or threshold-connected screen exists. |
| Detector consumes valid processed signed value before crop | **Host-test scaffold only** | Pure API exists; no firmware integration, and LC sign is lost upstream. |
| Evidence-backed production threshold | **Blocked on physical threshold evidence** | `I_reverse_threshold = PHYSICAL_AUTHORITY_REQUIRED`. `-0.25F` is only host fixture data. |
| Three consecutive qualifying cycles | **Implemented but not integrated** | Generic count supports three and fixture covers it; production configuration/call cadence is absent. |
| Valid non-qualifier resets counter | **Implemented + covered** | Exact detector behavior is tested. |
| Transient/noise does not trip | **Blocked on physical threshold evidence** | Logic exists, but noise margin and cadence are unknown. |
| Training qualifying fault enters latch | **Missing** | No Training controller/latch. |
| Relay OFF and remains OFF | **Missing** | HAL OFF API exists, but no reverse-current owner or re-enable interlock. |
| Dedicated fixed abnormal screen | **Missing** | No view, state, strings, or input ownership. |
| Zero after opening does not clear | **Host-test scaffold only** | Detector latch itself persists, but no integrated Training latch/relay test. |
| UI cannot clear/re-enable | **Missing** | Existing launcher and service/test paths can issue ON; no global latch guard. |
| No automatic retry/reclose | **Missing** | No safety state exists to own and enforce this rule. |
| Physical RESET only recovery | **Missing** | Existing `LOCAL_FAULT` supports acknowledgement and cannot be reused unchanged. |
| Reset follows safe boot | **Implemented but not integrated** | Device relay storage initializes false and `_vabase_init()` commands OFF; integration and physical proof are absent. |
| Positive-current/calibration regression | **Blocked on physical safety validation** | Source design can preserve it, but device behavior requires physical qualification. |
| Existing relay authority reused | **Missing** | The HAL authority exists; there is no Issue #15 controller using it. |
| CSV/D2B signed semantics, no schema change | **Implemented + covered** | Serialization preserves valid processed sign; all-range sign depends on upstream LC resolution. Additional explicit negative regression cases are advisable. |

## 5. Detector semantics audit

- **Configuration:** valid only for finite `negativeThresholdA < 0` and
  `requiredQualifyingCount > 0`; invalid configuration is inert in `Normal`.
- **Qualifier:** inclusive `signedCurrentA <= negativeThresholdA`. Each valid finite
  qualifier increments the count; count `N` changes `Candidate` to `Latched`.
- **Valid non-qualifier:** immediately sets count to zero and state to `Normal`, matching
  Issue #15's definition of consecutive valid qualifying samples.
- **Invalid sample:** returns the current result without incrementing or resetting.
- **NaN/+Inf/-Inf:** treated like invalid and hold the candidate/count.
- **Interrupted sequence:** a valid finite sample above threshold interrupts and resets;
  invalid/non-finite samples create a pause rather than an interruption.
- **Latch:** permanent for the C++ object's lifetime. Later positive, zero, invalid, or
  qualifying inputs do not clear it.
- **`enteredLatched`:** true only in the observe call that performs the transition;
  `result()` and every later observe return false.

The Issue explicitly states that valid samples only feed the detector and only specifies
reset on a non-qualifying **valid** sample. Therefore the current invalid/non-finite hold
does not contradict the Issue text. It does mean “three consecutive measurement cycles”
is more precisely “three qualifying valid observations, with invalid cycles ignored.”
That ambiguity must be resolved in safety review. The recommended fail-closed counting
contract is to make invalid/non-finite observations break the candidate sequence unless
physical cadence evidence and a documented hazard analysis approve the current hold.
Either behavior must be tested explicitly; it must not change accidentally during
integration. Count overflow is unreachable after latching for valid configurations, as
the transition occurs at `N`; no reset API exists, appropriately preventing UI recovery.

## 6. Relay authority audit

The one device relay-control authority is the HAL interface
`HAL::SetBaseRelay(bool)` / `HAL::GetBaseRelayState()`, implemented by
`HAL_VAMeter::setBaseRelay()` in `hal_vabase.cpp`. It updates a software boolean and GPIO.
It is an API/driver authority, **not currently a centralized policy state machine**.

- **OFF command:** `HAL::SetBaseRelay(false)`; “confirmation” is presently only
  `!HAL::GetBaseRelayState()`, a software-state echo, not electrical contact feedback.
- **ON/re-enable paths:** the launcher turns it ON when entering a current/voltage guide;
  Settings Base Test and app base-test paths can also turn it ON. The former Power Monitor
  encoder toggle is commented out. Web/live-share safety paths command OFF only.
- **Boot/safe start:** static state begins false; `_vabase_init()` configures the GPIO and
  commands OFF before later UI entry may command ON.
- **Errors:** setter returns `void`; GPIO result and physical relay/contact state are not
  confirmed. Callers infer success from the cached state. There is no retry/error result.
- **Cleanup:** live-share safety uses an ordered, one-attempt OFF callback with cleanup
  debt and a local recorder fault presentation. That mechanism is useful precedent, but
  ordinary `LOCAL_FAULT` can be acknowledged and is not the Training reset-only owner.
- **Training seam:** `SystemConfig::probeMode` is persisted and changes launcher labels;
  no dedicated Training controller owns relay enable/disable.

Future work must extend policy around this HAL authority, not add a second GPIO path.
On `enteredLatched`, the Training controller records the latch first, then invokes exactly
one OFF command, samples the software state once, stores `OffSoftwareConfirmed` or
`OffUnconfirmed`, and never retries/recloses automatically. Unconfirmed/failure keeps the
same reset-only latch and shows a stronger “output state unconfirmed—disconnect/reset”
presentation; it must not fall back to ordinary measurement. Lack of physical feedback
must be explicit in claims.

Every ON route must consult the same RAM-resident Training safety owner (or a guarded HAL
policy facade) and fail closed while latched. Merely changing the launcher is insufficient
because Settings/test paths exist. Exit/shutdown may command OFF idempotently but must not
clear the latch. Physical reset destroys RAM state, then existing initialization commands
OFF; only normal post-boot policy may later enable it.

## 7. Proposed Training fault state machine

### Orthogonal components

1. **Measurement authority:** `{NoValidSample, ValidSignedSample(range, A)}`. Produces,
   never consumes, a signed processed value and explicit validity/range metadata.
2. **Detector:** `{Normal, Candidate(1..N-1), Latched}` with
   `N = 3` as the current reviewed design assumption and
   `I_reverse_threshold = PHYSICAL_AUTHORITY_REQUIRED`.
3. **Training safety owner:** `{DisarmedNonTraining, ArmedTraining, FaultLatched}`.
4. **Relay authority:** `{Off, On, OffSoftwareConfirmed, OffUnconfirmed}`; software state
   must not be described as contact feedback.
5. **Presentation:** `{NormalMeasurement, NonTrainingReverseWarning,
   TrainingReverseFaultOff, TrainingReverseFaultOutputUnconfirmed}`.
6. **Lifecycle:** only hardware reset reconstructs these objects and returns through
   safe boot; there is no software Clear event.

### Inputs and transitions

| From | Input/guard | Ordered effects | To |
|---|---|---|---|
| `DisarmedNonTraining` | valid finite `I <= -I_reverse_threshold` reaches reviewed warning policy | Update detector/warning only; never issue Training relay action. Preserve current sample for CSV/D2B. | `DisarmedNonTraining` + warning |
| `ArmedTraining/Normal` | qualifying valid signed sample | Increment detector; no relay/UI takeover before count `N`. | `ArmedTraining/Candidate` |
| `ArmedTraining/Candidate` | qualifying valid signed sample and count `< N` | Increment only. Range changes do not inherently reset; the validity and threshold policy decide. Record range for evidence. | same |
| `ArmedTraining/Candidate` | valid finite non-qualifier | Reset count. | `ArmedTraining/Normal` |
| candidate | invalid/non-finite | Proposed safety default: reset candidate; current detector instead holds, so this is a deliberate review decision and code/test change. Invalid must never qualify and must remain distinct from reverse. | normal (proposed) |
| any armed detector state | qualifying sample reaches `N` | (1) commit `FaultLatched`; (2) block every ON/UI navigation route; (3) emit latch-entry event once; (4) command HAL relay OFF once; (5) sample cached state; (6) select confirmed/unconfirmed dedicated screen. | `FaultLatched` |
| `FaultLatched` | zero/positive/negative/invalid sample, range switch, UI input, app exit request | No detector clear, no ON, no normal navigation, no acknowledgement. Rendering may refresh only non-authoritative diagnostics. Exit/shutdown may request OFF but cannot re-arm. | `FaultLatched` |
| `FaultLatched` | OFF unconfirmed | No retry/reclose; preserve latch and suppress UI. Escalate presentation and instruct physical disconnection/reset. | `FaultLatched/OutputUnconfirmed` |
| any | hardware reset | CPU restart; GPIO/relay safe initialization OFF; reconstruct detector/latch; enter ordinary boot. No persisted bypass. | boot, then configured mode |

Commit-before-side-effect ordering ensures that a failed OFF confirmation cannot leave UI
or relay-ON paths enabled. The one-shot event, not repeated `state == Latched` polling,
drives the OFF attempt. The safety owner remains authoritative even if the fault view
cannot render. Range switches must never fabricate a normal sample; a valid range sample
is evaluated normally, while invalid transition samples follow the reviewed invalid rule.

## 8. Presentation semantics

Current firmware already draws current with a `0 A` lower bound, retains signed buffer
values, clips only drawable geometry, and omits fully negative segments. That behavior
should remain. A reverse warning is additional state, not a negative graph domain or a
zero-valued substitute.

- **Training:** on latch, replace the normal screen with a dedicated fixed warning that
  says current direction/ammeter connection is wrong, the circuit was stopped for
  protection, correct wiring, then use physical RESET. The unconfirmed-OFF variant must
  say output state could not be confirmed and direct immediate physical disconnection.
  Encoder, Side, Help, app switching, recorder/live-share start, and relay controls are
  suppressed. Exact Japanese/English copy and glyph availability require UI review and
  later device validation.
- **Non-Training:** show a conspicuous, non-latching reverse-current warning at the
  reviewed threshold. It must not invoke Training relay behavior. Define warning clear
  hysteresis/dwell during implementation review; do not silently equate it to the
  reset-only Training latch.
- **Invalid/no-data:** retain a separate presentation/status; never label it reverse.

## 9. CSV/D2B non-interference

No schema or wire change is needed. CSV remains `voltage,current,elapsed_ms`; D2B remains
the existing timestamped V/I frame with validity mask. Detection and UI read the same
valid signed authority but do not mutate it. A Training trip may naturally stop subsequent
physical current after relay OFF; already acquired negative samples remain negative in
CSV/D2B. The resulting measured zero is a real post-action sample and cannot clear the
latch. Invalid D2B current remains positive-zero placeholder with current-valid unset,
not reverse and not measured zero.

The LC upstream clamp is the sole source-level blocker to claiming all-range signed
preservation. Its eventual resolution must preserve calibration and range behavior and
must be qualified physically. It does not justify a CSV column or D2B protocol change.

## 10. Later host-test plan

Do **not** run these during this design phase. On the later WSL implementation worktree:

```bash
cmake -S . -B build/desktop
cmake --build build/desktop -j 2
ctest --test-dir build/desktop --output-on-failure -R 'reverse_current_detector|signed_current_observation|current_waveform_clip|local_fault|educational_interaction_contract|record_csv|d2b_vi_(producer|frame_writer)'
ctest --test-dir build/desktop --output-on-failure
```

Extend `tests/reverse_current_detector/` for `+`, zero, shallow negative, the eventual
threshold immediately above/equal/below, 1/2/3 cycles, a valid interruption, invalid,
NaN, and both infinities. Freeze the reviewed choice of reset-versus-hold for invalid
cycles. Add explicit same-polarity samples across LC-to-HC and HC-to-LC metadata changes.

Add a new host-testable `training_reverse_current_safety` controller seam with fake
measurement, relay, mode, UI, and reset lifecycle. Cover OFF exactly once, latch before
OFF, confirmed and unconfirmed/failure results, zero after OFF, repeated negative input,
all encoder/Side/navigation/relay-ON actions blocked, exit/shutdown retaining latch, no
automatic retry/reclose, and reset-object reconstruction as the only recovery. Extend
`tests/educational_interaction_contract/` for foreground input suppression, but keep
safety-state unit tests independent from rendering.

Extend `tests/current_waveform_clip/` with sampled reverse intervals bounded by positive
points and invalid points so no cross-gap interpolation occurs. Add renderer-level tests
if practical to prove buffers are unchanged. Extend recorder CSV tests with a valid
negative current row and exact parsed sign. Extend `tests/d2b_vi_producer/` and
`tests/d2b_vi_frame_writer/` with negative finite current, current-valid set, and decoded
IEEE-754 sign/value; retain invalid positive-zero/unset-mask cases. Observation tests
should include both range tokens and valid negative formatting.

Threshold-dependent fixtures must use a test-only symbolic value and state clearly that
they do not establish production authority. Production configuration remains
`I_reverse_threshold = PHYSICAL_AUTHORITY_REQUIRED` until the evidence gate closes.

## 11. Physical threshold evidence plan

All items in this section are **DEFERRED — PHYSICAL AUTHORITY REQUIRED**.

1. Capture timestamped zero-current distributions (mean, extrema, percentiles, drift,
   temperature/supply/setup notes) in both LC and HC ranges, not only formatted display.
2. Repeat for relevant calibration/offset states and document exactly where offset is
   applied; include the current LC clamp and a qualified candidate without lost sign.
3. Capture both directions through the LC/HC transition, including alert hysteresis,
   transient invalid/overflow samples, repeated acquisition via `goto HELL`, and dwell.
4. Select a detection margin from observed worst-case noise/offset plus documented guard
   band and protected classroom reverse-ammeter needs. Do not derive a rating from INA226
   circuit ranges.
5. Measure real valid-sample cycle timing, invalid gaps, and three-qualifier wall-clock
   trip latency; do not infer cadence from the nominal daemon delay.
6. In a low-voltage, current-limited, isolated, protected reverse-ammeter setup, measure
   detector entry, GPIO command, relay de-energization/contact effect, and total hazard
   removal latency with independent instrumentation.
7. Verify the latched warning, no reclose, all UI suppression, reset-only recovery, and
   post-reset relay-OFF safe state before ordinary operation can resume.

The final evidence package must bind firmware SHA/config, device identity, calibration,
fixture, instruments, raw logs, timing captures, threshold derivation, and rollback data.
Every threshold-dependent conclusion is **DEFERRED — PHYSICAL AUTHORITY REQUIRED**.

## 12. Deferred physical safety validation

No hardware was available and no firmware was built or flashed. Relay software-state
readback is not physical contact proof. Display legibility, localized glyphs, reverse
ammeter protection, actual range behavior, threshold/noise margin, trip latency, relay
latency, reset-only behavior, and post-reset safe state are all **DEFERRED — PHYSICAL
AUTHORITY REQUIRED**. Release qualification remains blocked on the repository's frozen
candidate, readback, rollback, and physical-evidence procedures.

## 13. Expected file-impact map

| Likely file/area | Expected future change | Risk |
|---|---|---|
| `platforms/vameter/.../hal_power_monitor.cpp` | Establish signed pre-presentation authority across ranges; publish explicit validity/range to controller. | **Very high:** calibration/range/safety coupling; physical gate. |
| `app/libs/reverse_current_detector/*` | Resolve invalid-sequence policy if review changes current hold behavior; no threshold literal. | Medium. |
| new `app/libs/training_reverse_current_safety/*` | Pure state machine, exactly-once latch event/effects, guarded ON policy. | High. |
| `app/hal/hal.h`, `platforms/vameter/.../hal_vabase.cpp`, relay callers | Reuse/extend HAL authority, return explicit software confirmation, block ON while latched without a second GPIO path. | **Very high.** |
| launcher / Training application/view files | Arm controller, dedicated fixed warning, input suppression, no acknowledgement. | High. |
| waveform/current display files and assets/localization | Non-Training warning while retaining existing clipping. | Medium/high; AssetPool and physical display validation. |
| recorder/CSV and D2B paths | Primarily regression tests; source change only if integration exposes validity/lifecycle need. No schema change. | Low/medium. |
| existing/new tests | Detector, controller, relay fake, interaction, CSV, D2B, clipping coverage. | Medium. |
| docs/standards and validation docs | Update only after implemented/qualified behavior becomes authority. | Medium. |

## 14. Implementation/PR slicing recommendation

**Recommend Strategy A.** PR-A should integrate signed-current authority, explicit
validity/range seams, presentation warning/clipping regression, and detector/controller
host tests while making production trip activation impossible without a compile-time or
construction-time physical-authority token. It must not claim protection. PR-B should
select the evidence-backed threshold and activate Training relay latching only after
physical evidence and safety review.

This separation makes the hazardous activation and threshold derivation independently
reviewable and revertible, keeps measurement/presentation regressions apart from relay
policy, and avoids an “inert” gate accidentally becoming product configuration. The cost
is a temporary integrated-but-disabled seam and careful prevention of dead code drift.

Strategy B (one inert/config-gated PR) reduces short-term cross-PR source churn, but it
couples measurement, UI, relay authority, assets, and activation in one rollback unit and
makes reviewers reason about whether a nominally inert gate can be enabled accidentally.
For classroom safety, that review/rollback disadvantage outweighs convenience.

## 15. Cross-Issue conflict assessment

- **Issue #27 QR UI — medium merge risk.** Likely overlap is
  `app/apps/app_power_monitor/view/live_share_view.cpp`, shared QR/UI helpers under
  `app/apps/utils/qrcode/`, theme/static image assets, localization CSV/generated text
  pools, and possibly launcher/foreground input routing. Issue #15 should prefer a
  dedicated Training fault view and avoid broad QR/theme refactors. AssetPool regeneration
  and visual validation must coordinate if both touch assets.
- **Issue #23 Node/Viewer intake — low source risk, medium artifact/process risk.** Its
  likely files are Viewer bundle web assets, `app/assets/web/`, AssetPool layout/intake
  tooling and manifests, Node/npm configuration, CI, and Viewer acceptance docs. Issue
  #15 should not change the Viewer wire schema or rebuild the Viewer merely to show a
  firmware warning. D2B negative-current regression tests may overlap protocol tests, and
  any AssetPool regeneration can conflict with frozen Viewer hashes/capacity evidence.
  Sequence or rebase UI assets carefully; do not overwrite accepted bundle artifacts.

## 16. Open blockers

1. `I_reverse_threshold = PHYSICAL_AUTHORITY_REQUIRED`.
2. LC range currently erases some processed negative values; safe signed behavior across
   LC/HC, offset, and transition is unqualified.
3. Invalid/non-finite candidate handling must be explicitly approved: current detector
   pauses, while this design recommends breaking the sequence by default.
4. No centralized relay policy owner guards every ON path; setter confirmation is cached
   software state only and cannot prove electrical OFF.
5. No Training safety controller, reset-only fault view, localized copy/assets, or input
   suppression exists.
6. Non-Training warning clear/hysteresis semantics need review after threshold evidence.
7. Actual sample cadence, trip latency, relay latency, reverse-ammeter protection, and
   reset/post-reset behavior lack physical evidence.

## 17. Claim boundary

This report is a source audit and proposed design against base commit
`3348bc86d5d911e131315ad35d6cec022ab01a59`. It does not authorize implementation,
threshold selection, production activation, firmware build equivalence, hardware safety,
new electrical ratings, or AC use. Host fixtures such as `{-0.25F, 3}` prove only generic
logic. Existing HC sign preservation does not prove every range preserves sensor-raw
negative current. Existing software relay state does not prove relay contacts are open.

ISSUE15_DESIGN_READY_FOR_REVIEW

PRODUCTION_REVERSE_THRESHOLD_NOT_ESTABLISHED

IMPLEMENTATION_NOT_STARTED
