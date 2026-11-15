#include "dosing.h"

namespace tower {

static const uint32_t kDay = 24UL * 3600UL;
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

const char* faultName(Fault f) {
  switch (f) {
    case Fault::None:         return "none";
    case Fault::PhOutOfRange: return "pH reading out of range";
    case Fault::EcOutOfRange: return "EC reading out of range";
    case Fault::PhJump:       return "pH jumped more than 1.0 in 5 min";
    case Fault::EcJump:       return "EC jumped more than 1.0 in 5 min";
  }
  return "?";
}

const char* pumpName(Pump p) {
  switch (p) {
    case Pump::Micro:  return "Micro";
    case Pump::Grow:   return "Grow";
    case Pump::Bloom:  return "Bloom";
    case Pump::PhDown: return "pH Down";
  }
  return "?";
}

void Controller::begin(uint32_t nowS) {
  startS_ = nowS;
  phHigh_ = ecLow_ = false;
  seqStep_ = -1;
}

void Controller::configure(const Settings& s) {
  s_ = s;
  s_.phStepMl = clampf(s_.phStepMl, 0.0f, Limits::kMaxPhStepMl);
  for (int i = 0; i < 3; i++) s_.topupMl[i] = clampf(s_.topupMl[i], 0.0f, Limits::kMaxTopupMlPerPart);
}

void Controller::latch(Fault f) {
  if (fault_ == Fault::None) fault_ = f;
  seqStep_ = -1;                       // abandon any top-up in progress
  phHigh_ = ecLow_ = false;
}

void Controller::clearFault(uint32_t nowS) {
  fault_ = Fault::None;
  for (auto& s : samples_) s.used = false;   // old samples would re-trigger the jump check
  phHigh_ = ecLow_ = false;
  startS_ = nowS;                      // and wait the boot hold again before any dose
}

void Controller::remember(uint32_t nowS, const Reading& r) {
  samples_[sampleIdx_] = {nowS, r.ph, r.ec, r.phValid, r.ecValid, true};
  sampleIdx_ = (sampleIdx_ + 1) % kSamples;
}

bool Controller::jumped(uint32_t nowS, const Reading& r, Fault& which) const {
  for (const auto& s : samples_) {
    if (!s.used || !within(nowS, s.t, Limits::kJumpWindowS)) continue;
    if (r.phValid && s.phOk) { float d = r.ph - s.ph; if (d > Limits::kMaxJump || d < -Limits::kMaxJump) { which = Fault::PhJump; return true; } }
    if (r.ecValid && s.ecOk) { float d = r.ec - s.ec; if (d > Limits::kMaxJump || d < -Limits::kMaxJump) { which = Fault::EcJump; return true; } }
  }
  return false;
}

void Controller::recordPh(uint32_t nowS, float ml) {
  int slot = 0;
  for (int i = 0; i < kPhLog; i++) {
    if (!ph_[i].used || !within(nowS, ph_[i].t, kDay)) { slot = i; break; }
    if (ph_[i].t < ph_[slot].t) slot = i;
  }
  ph_[slot] = {nowS, ml, true};
  anyPh_ = anyDose_ = true; lastPhS_ = lastDoseS_ = nowS;
}

void Controller::recordTopup(uint32_t nowS) {
  int slot = 0;
  for (int i = 0; i < kTopupLog; i++) {
    if (!topup_[i].used || !within(nowS, topup_[i].t, kDay)) { slot = i; break; }
    if (topup_[i].t < topup_[slot].t) slot = i;
  }
  topup_[slot] = {nowS, 0, true};
  anyTopup_ = true; lastTopupS_ = nowS;
}

int Controller::phCount(uint32_t nowS) const {
  int n = 0; for (const auto& d : ph_) if (d.used && within(nowS, d.t, kDay)) n++; return n;
}
float Controller::phMl(uint32_t nowS) const {
  float m = 0; for (const auto& d : ph_) if (d.used && within(nowS, d.t, kDay)) m += d.ml; return m;
}
int Controller::topupCount(uint32_t nowS) const {
  int n = 0; for (const auto& d : topup_) if (d.used && within(nowS, d.t, kDay)) n++; return n;
}

History Controller::history(uint32_t nowS) const {
  History h; h.phDoses = (uint8_t)phCount(nowS); h.phMl = phMl(nowS); h.topups = (uint8_t)topupCount(nowS); return h;
}

void Controller::restoreHistory(uint32_t nowS, const History& h) {
  // Elapsed time is unknown after a reboot, so every persisted dose is counted
  // as fresh. This can only make the controller more cautious.
  int n = h.phDoses > kPhLog ? kPhLog : h.phDoses;
  for (int i = 0; i < n; i++) ph_[i] = {nowS, h.phMl / (float)(n ? n : 1), true};
  if (n == 0 && h.phMl > 0) ph_[0] = {nowS, h.phMl, true};
  int t = h.topups > kTopupLog ? kTopupLog : h.topups;
  for (int i = 0; i < t; i++) topup_[i] = {nowS, 0, true};
  if (n || h.phMl > 0) { anyPh_ = anyDose_ = true; lastPhS_ = lastDoseS_ = nowS; }
  if (t) { anyTopup_ = anyDose_ = true; lastTopupS_ = lastDoseS_ = nowS; }
}

uint32_t Controller::nextPhAllowedS() const {
  uint32_t t = startS_ + Limits::kBootHoldS;
  if (anyPh_ && lastPhS_ + Limits::kMinPhIntervalS > t) t = lastPhS_ + Limits::kMinPhIntervalS;
  if (anyDose_ && lastDoseS_ + Limits::kRestAfterAnyDoseS > t) t = lastDoseS_ + Limits::kRestAfterAnyDoseS;
  return t;
}
uint32_t Controller::nextTopupAllowedS() const {
  uint32_t t = startS_ + Limits::kBootHoldS;
  if (anyTopup_ && lastTopupS_ + Limits::kMinTopupIntervalS > t) t = lastTopupS_ + Limits::kMinTopupIntervalS;
  if (anyDose_ && lastDoseS_ + Limits::kRestAfterAnyDoseS > t) t = lastDoseS_ + Limits::kRestAfterAnyDoseS;
  return t;
}

bool Controller::update(uint32_t nowS, const Reading& r, Action& out) {
  // 1. Implausible readings latch a fault. Nothing doses until a person clears it.
  if (!lockedOut()) {
    Fault j = Fault::None;
    if (r.phValid && (r.ph < Limits::kPhMin || r.ph > Limits::kPhMax)) latch(Fault::PhOutOfRange);
    else if (r.ecValid && (r.ec < Limits::kEcMin || r.ec > Limits::kEcMax)) latch(Fault::EcOutOfRange);
    else if (jumped(nowS, r, j)) latch(j);
  }
  remember(nowS, r);
  if (lockedOut()) return false;

  // 2. Conditions that block dosing without latching.
  if (r.tankEmpty || paused_) { seqStep_ = -1; phHigh_ = ecLow_ = false; return false; }

  // 3. A nutrient top-up in progress: Micro, then Grow, then Bloom, spaced apart.
  if (seqStep_ >= 0) {
    if (nowS < seqNextS_) return false;
    static const Pump order[3] = {Pump::Micro, Pump::Grow, Pump::Bloom};
    int step = seqStep_;
    seqStep_ = (step == 2) ? -1 : step + 1;
    seqNextS_ = nowS + Limits::kTopupStepGapS;
    anyDose_ = true; lastDoseS_ = nowS;
    if (s_.topupMl[step] <= 0) return false;
    out = {order[step], s_.topupMl[step], mode_ == Mode::Shadow, "EC low: nutrient top-up"};
    return true;
  }

  // 4. Track how long each condition has held. An invalid reading resets the clock.
  bool phHighNow = r.phValid && r.ph > s_.phHigh;
  if (phHighNow) { if (!phHigh_) { phHigh_ = true; phHighSince_ = nowS; } } else phHigh_ = false;
  bool ecLowNow = r.ecValid && r.ec < s_.ecLow;
  if (ecLowNow) { if (!ecLow_) { ecLow_ = true; ecLowSince_ = nowS; } } else ecLow_ = false;

  // 5. pH first: it is the smallest and most frequent correction.
  if (s_.phDosing && phHigh_ && nowS - phHighSince_ >= Limits::kPhSustainS && nowS >= nextPhAllowedS() && s_.phStepMl > 0) {
    if (phCount(nowS) < Limits::kMaxPhDosesPer24h && phMl(nowS) + s_.phStepMl <= Limits::kMaxPhDownMlPer24h + 1e-4f) {
      recordPh(nowS, s_.phStepMl);
      out = {Pump::PhDown, s_.phStepMl, mode_ == Mode::Shadow, "pH high"};
      return true;
    }
    // 24 h ceiling reached: no more pH Down today. Nutrients may still be due.
  }

  // 6. Nutrients.
  if (s_.nutrientDosing && ecLow_ && nowS - ecLowSince_ >= Limits::kEcSustainS && nowS >= nextTopupAllowedS()) {
    if (topupCount(nowS) < Limits::kMaxTopupsPer24h) {
      recordTopup(nowS);
      seqStep_ = 0; seqNextS_ = nowS;
      return update(nowS, r, out);     // deliver the first bottle now
    }
  }
  return false;
}

}  // namespace tower
