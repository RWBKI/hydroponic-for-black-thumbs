// Host-side tests of the dosing logic and conversions. No hardware needed.
//   g++ -std=c++17 -Wall -Wextra -I lib/core lib/core/*.cpp test/host/run_tests.cpp -o run_tests && ./run_tests
#include <cmath>
#include <cstdio>
#include <vector>
#include "convert.h"
#include "dosing.h"

using namespace tower;

static int g_fail = 0, g_checks = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_fail++; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b, eps) (std::fabs((a) - (b)) <= (eps))

static const uint32_t STEP = 30;                     // one reading every 30 s

static Reading good(float ph, float ec) { Reading r; r.ph = ph; r.phValid = true; r.ec = ec; r.ecValid = true; return r; }

struct Run { std::vector<Action> actions; std::vector<uint32_t> times; };

// Feed the same reading for `seconds`, collecting every action.
static Run feed(Controller& c, uint32_t& now, uint32_t seconds, const Reading& r) {
  Run out;
  for (uint32_t end = now + seconds; now < end; now += STEP) {
    Action a;
    if (c.update(now, r, a)) { out.actions.push_back(a); out.times.push_back(now); }
  }
  return out;
}

static Controller fresh(Mode m = Mode::Armed) { Controller c; c.begin(0); c.configure(Settings()); c.setMode(m); return c; }

static void test_no_dose_when_in_range() {
  std::puts("no dose when readings are in range");
  Controller c = fresh(); uint32_t now = 0;
  Run r = feed(c, now, 6 * 3600, good(6.0f, 1.1f));
  CHECK(r.actions.empty());
}

static void test_boot_hold_and_sustain() {
  std::puts("boot hold and 10 min sustain before the first pH dose");
  Controller c = fresh(); uint32_t now = 0;
  Run r = feed(c, now, 3 * 3600, good(6.8f, 1.1f));
  CHECK(!r.actions.empty());
  if (!r.actions.empty()) {
    CHECK(r.times[0] >= Limits::kBootHoldS);
    CHECK(r.times[0] >= Limits::kPhSustainS);
    CHECK(r.actions[0].pump == Pump::PhDown);
    CHECK(NEAR(r.actions[0].ml, 0.5f, 1e-6));
    CHECK(!r.actions[0].shadow);
  }
}

static void test_ph_interval_and_daily_ceiling() {
  std::puts("pH: 20 min between doses, never above the 24 h ceiling");
  Controller c = fresh(); uint32_t now = 0;
  Run r = feed(c, now, 72 * 3600, good(7.5f, 1.1f));   // three days of stubbornly high pH
  for (size_t i = 1; i < r.times.size(); i++) CHECK(r.times[i] - r.times[i - 1] >= Limits::kMinPhIntervalS);
  // sliding 24 h window over every dose
  for (size_t i = 0; i < r.times.size(); i++) {
    int n = 0; float ml = 0;
    for (size_t j = i; j < r.times.size() && r.times[j] - r.times[i] < 24 * 3600; j++) { n++; ml += r.actions[j].ml; }
    CHECK(n <= Limits::kMaxPhDosesPer24h);
    CHECK(ml <= Limits::kMaxPhDownMlPer24h + 1e-3f);
  }
  CHECK(r.actions.size() >= 12);                       // it does keep dosing on later days
  CHECK(r.actions.size() <= 18);
}

static void test_ph_step_is_clamped() {
  std::puts("a pH step above the hard maximum is clamped");
  Controller c; c.begin(0); Settings s; s.phStepMl = 50.0f; c.configure(s); c.setMode(Mode::Armed);
  CHECK(NEAR(c.settings().phStepMl, Limits::kMaxPhStepMl, 1e-6));
  uint32_t now = 0; Run r = feed(c, now, 24 * 3600, good(7.5f, 1.1f));
  float ml = 0; for (auto& a : r.actions) ml += a.ml;
  CHECK(ml <= Limits::kMaxPhDownMlPer24h + 1e-3f);
}

static void test_topup_sequence() {
  std::puts("EC low: Micro, Grow, Bloom in order, 5 min apart, after 30 min sustained");
  Controller c = fresh(); uint32_t now = 0;
  Run r = feed(c, now, 100 * 60, good(6.0f, 0.7f));
  CHECK(r.actions.size() == 3);
  if (r.actions.size() == 3) {
    CHECK(r.times[0] >= Limits::kEcSustainS);
    CHECK(r.actions[0].pump == Pump::Micro && r.actions[1].pump == Pump::Grow && r.actions[2].pump == Pump::Bloom);
    CHECK(r.times[1] - r.times[0] >= Limits::kTopupStepGapS);
    CHECK(r.times[2] - r.times[1] >= Limits::kTopupStepGapS);
    CHECK(NEAR(r.actions[0].ml, 4.8f, 1e-6) && NEAR(r.actions[1].ml, 7.4f, 1e-6) && NEAR(r.actions[2].ml, 2.6f, 1e-6));
  }
}

static void test_topup_ceiling() {
  std::puts("EC low for three days: at most 2 top-ups per 24 h, 2 h apart");
  Controller c = fresh(); uint32_t now = 0;
  Run r = feed(c, now, 72 * 3600, good(6.0f, 0.5f));
  std::vector<uint32_t> starts;
  for (size_t i = 0; i < r.actions.size(); i++) if (r.actions[i].pump == Pump::Micro) starts.push_back(r.times[i]);
  CHECK(r.actions.size() == starts.size() * 3);       // every sequence completed
  for (size_t i = 1; i < starts.size(); i++) CHECK(starts[i] - starts[i - 1] >= Limits::kMinTopupIntervalS);
  for (size_t i = 0; i < starts.size(); i++) {
    int n = 0; for (size_t j = i; j < starts.size() && starts[j] - starts[i] < 24 * 3600; j++) n++;
    CHECK(n <= Limits::kMaxTopupsPer24h);
  }
  CHECK(starts.size() >= 4 && starts.size() <= 6);
}

static void test_no_ph_dose_during_topup() {
  std::puts("pH and nutrients never overlap: rest after any dose");
  Controller c = fresh(); uint32_t now = 0;
  Run r = feed(c, now, 12 * 3600, good(7.0f, 0.6f));   // both wrong at once
  CHECK(!r.actions.empty());
  for (size_t i = 1; i < r.actions.size(); i++) {
    bool prevNut = r.actions[i - 1].pump != Pump::PhDown, curNut = r.actions[i].pump != Pump::PhDown;
    uint32_t gap = r.times[i] - r.times[i - 1];
    if (prevNut && curNut) CHECK(gap >= Limits::kTopupStepGapS);
    else CHECK(gap >= Limits::kRestAfterAnyDoseS);
  }
}

static void test_out_of_range_latches() {
  std::puts("an implausible reading locks dosing until cleared by hand");
  Controller c = fresh(); uint32_t now = 0;
  feed(c, now, 20 * 60, good(6.9f, 1.1f));
  feed(c, now, STEP, good(3.2f, 1.1f));                // probe out of water, broken wire...
  CHECK(c.lockedOut() && c.fault() == Fault::PhOutOfRange);
  Run r = feed(c, now, 24 * 3600, good(7.2f, 0.5f));   // back to "needs dosing" values
  CHECK(r.actions.empty());                            // still nothing: it never re-arms itself
  c.clearFault(now);
  CHECK(!c.lockedOut());
  Run r2 = feed(c, now, 3 * 3600, good(7.2f, 1.1f));
  CHECK(!r2.actions.empty());
  if (!r2.times.empty()) CHECK(r2.times[0] >= now - 3 * 3600 + Limits::kBootHoldS);   // hold-off applies again
}

static void test_ec_out_of_range_latches() {
  std::puts("EC out of range latches too");
  Controller c = fresh(); uint32_t now = 0;
  feed(c, now, STEP, good(6.0f, 0.02f));
  CHECK(c.fault() == Fault::EcOutOfRange);
  Controller d = fresh(); now = 0;
  feed(d, now, STEP, good(6.0f, 3.4f));
  CHECK(d.fault() == Fault::EcOutOfRange);
}

static void test_jump_latches() {
  std::puts("a jump of more than 1.0 within 5 min latches a fault");
  Controller c = fresh(); uint32_t now = 0;
  feed(c, now, 10 * 60, good(6.1f, 1.1f));
  feed(c, now, STEP, good(7.3f, 1.1f));
  CHECK(c.fault() == Fault::PhJump);
  Controller d = fresh(); now = 0;
  feed(d, now, 10 * 60, good(6.1f, 1.1f));
  feed(d, now, STEP, good(6.1f, 2.3f));
  CHECK(d.fault() == Fault::EcJump);
  Controller e = fresh(); now = 0;                      // a slow drift of the same size is fine
  for (float ph = 6.0f; ph < 7.2f; ph += 0.02f) feed(e, now, 60, good(ph, 1.1f));
  CHECK(!e.lockedOut());
}

static void test_tank_empty_blocks_and_aborts() {
  std::puts("tank empty: no dose, and a top-up in progress is abandoned");
  Controller c = fresh(); uint32_t now = 0;
  Reading low = good(6.0f, 0.7f);
  Run r = feed(c, now, 33 * 60, low);                   // sequence has started (Micro delivered)
  CHECK(r.actions.size() >= 1 && c.sequenceActive());
  Reading empty = low; empty.tankEmpty = true;
  Run r2 = feed(c, now, 3600, empty);
  CHECK(r2.actions.empty() && !c.sequenceActive());
  CHECK(!c.lockedOut());                                // blocks without latching
}

static void test_pause_blocks() {
  std::puts("pause blocks dosing");
  Controller c = fresh(); c.setPaused(true); uint32_t now = 0;
  Run r = feed(c, now, 6 * 3600, good(7.5f, 0.5f));
  CHECK(r.actions.empty());
  c.setPaused(false);
  Run r2 = feed(c, now, 3600, good(7.5f, 1.1f));
  CHECK(!r2.actions.empty());
}

static void test_invalid_reading_resets_sustain() {
  std::puts("an invalid reading resets the sustain clock");
  Controller c = fresh(); uint32_t now = 20 * 60;       // past the boot hold
  Reading bad; bad.phValid = false; bad.ecValid = false;
  for (int i = 0; i < 12; i++) {                         // 8 min high, then one invalid sample, repeated
    Run a = feed(c, now, 8 * 60, good(7.0f, 1.1f)); CHECK(a.actions.empty());
    Run b = feed(c, now, STEP, bad); CHECK(b.actions.empty());
  }
}

static void test_shadow_mode() {
  std::puts("shadow mode takes the same decisions but flags them as log-only");
  Controller a = fresh(Mode::Armed), s = fresh(Mode::Shadow); uint32_t na = 0, ns = 0;
  Run ra = feed(a, na, 24 * 3600, good(7.0f, 0.6f)), rs = feed(s, ns, 24 * 3600, good(7.0f, 0.6f));
  CHECK(ra.actions.size() == rs.actions.size() && !ra.actions.empty());
  for (size_t i = 0; i < ra.actions.size() && i < rs.actions.size(); i++) {
    CHECK(ra.times[i] == rs.times[i] && ra.actions[i].pump == rs.actions[i].pump);
    CHECK(!ra.actions[i].shadow && rs.actions[i].shadow);
  }
  Controller d; d.begin(0); CHECK(d.mode() == Mode::Shadow);     // the default is shadow
}

static void test_disabled_channels() {
  std::puts("disabled channels never dose");
  Controller c; c.begin(0); Settings s; s.phDosing = false; s.nutrientDosing = false; c.configure(s); c.setMode(Mode::Armed);
  uint32_t now = 0; Run r = feed(c, now, 24 * 3600, good(7.5f, 0.5f));
  CHECK(r.actions.empty());
}

static void test_reboot_keeps_ceilings() {
  std::puts("ceilings survive a reboot");
  Controller c = fresh(); uint32_t now = 0;
  feed(c, now, 24 * 3600, good(7.5f, 0.5f));
  History h = c.history(now);
  CHECK(h.phDoses == Limits::kMaxPhDosesPer24h && h.topups == Limits::kMaxTopupsPer24h);
  Controller d; d.begin(0); d.configure(Settings()); d.setMode(Mode::Armed);   // power cycle
  d.restoreHistory(0, h);
  uint32_t n2 = 0;
  Run r = feed(d, n2, 23 * 3600, good(7.5f, 0.5f));
  CHECK(r.actions.empty());                              // budget already spent
  Run r2 = feed(d, n2, 6 * 3600, good(7.5f, 0.5f));
  CHECK(!r2.actions.empty());                            // and it comes back a day later
}

static void test_ph_ceiling_does_not_starve_nutrients() {
  std::puts("once the pH ceiling is hit, nutrients can still be topped up");
  Controller c = fresh(); uint32_t now = 0;
  Run r = feed(c, now, 12 * 3600, good(7.5f, 0.5f));
  int ph = 0, nut = 0; for (auto& a : r.actions) (a.pump == Pump::PhDown ? ph : nut)++;
  CHECK(ph == Limits::kMaxPhDosesPer24h);
  CHECK(nut == 3 * Limits::kMaxTopupsPer24h);
}

static void test_fault_survives_reboot() {
  std::puts("a latched fault restored after a reboot still blocks dosing");
  Controller c; c.begin(0); c.configure(Settings()); c.setMode(Mode::Armed);
  c.restoreFault(Fault::PhJump);
  uint32_t now = 0; Run r = feed(c, now, 24 * 3600, good(7.5f, 0.5f));
  CHECK(c.lockedOut() && r.actions.empty());
}

static void test_conversions() {
  std::puts("conversions");
  PhCal cal; cal.v1 = 1.667f; cal.ph1 = 6.86f; cal.v2 = 2.014f; cal.ph2 = 4.00f;
  CHECK(phCalValid(cal));
  CHECK(NEAR(phFromVolts(1.667f, cal), 6.86f, 1e-3));
  CHECK(NEAR(phFromVolts(2.014f, cal), 4.00f, 1e-3));
  CHECK(phFromVolts(1.5f, cal) > 6.86f);
  PhCal bad; bad.v1 = 1.50f; bad.v2 = 1.51f; CHECK(!phCalValid(bad));
  CHECK(NEAR(ecCompensate(1.10f, 25.0f), 1.10f, 1e-6));
  CHECK(ecCompensate(1.10f, 30.0f) < 1.10f && ecCompensate(1.10f, 18.0f) > 1.10f);
  CHECK(NEAR(ecFromTdsVolts(0.0f, 25.0f, 1.0f), 0.0f, 1e-6));
  CHECK(ecFromTdsVolts(1.2f, 25.0f, 1.0f) > ecFromTdsVolts(0.6f, 25.0f, 1.0f));
  CHECK(NEAR(ecFromTdsVolts(1.0f, 25.0f, 2.0f), 2.0f * ecFromTdsVolts(1.0f, 25.0f, 1.0f), 1e-5));
  CHECK(pumpRunMs(0.5f, 30.0f, 60000) == 1000);
  CHECK(pumpRunMs(9.0f, 30.0f, 60000) == 18000);
  CHECK(pumpRunMs(0.5f, 0.0f, 60000) == 0);             // not calibrated
  CHECK(pumpRunMs(100.0f, 30.0f, 60000) == 0);          // would exceed the run cap: refused
  CHECK(pumpRunMs(-1.0f, 30.0f, 60000) == 0);
}

int main() {
  test_no_dose_when_in_range();
  test_boot_hold_and_sustain();
  test_ph_interval_and_daily_ceiling();
  test_ph_step_is_clamped();
  test_topup_sequence();
  test_topup_ceiling();
  test_no_ph_dose_during_topup();
  test_out_of_range_latches();
  test_ec_out_of_range_latches();
  test_jump_latches();
  test_tank_empty_blocks_and_aborts();
  test_pause_blocks();
  test_invalid_reading_resets_sustain();
  test_shadow_mode();
  test_disabled_channels();
  test_reboot_keeps_ceilings();
  test_ph_ceiling_does_not_starve_nutrients();
  test_fault_survives_reboot();
  test_conversions();
  std::printf("\n%d checks, %d failed\n", g_checks, g_fail);
  return g_fail ? 1 : 0;
}
