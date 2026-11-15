// Dosing decisions and safeguards. Pure C++: no Arduino dependency, so the
// logic is tested on a computer (see test/host). Hardware code only executes
// the actions this controller returns.
#pragma once
#include <stdint.h>

namespace tower {

enum class Pump : uint8_t { Micro = 0, Grow = 1, Bloom = 2, PhDown = 3 };
static const int kPumpCount = 4;

// Hard ceilings. These are constants in the code on purpose: no setting,
// reading or web request can raise them.
struct Limits {
  static constexpr float    kMaxPhDownMlPer24h  = 6.0f;
  static constexpr int      kMaxPhDosesPer24h   = 6;
  static constexpr float    kMaxPhStepMl        = 1.0f;
  static constexpr int      kMaxTopupsPer24h    = 2;
  static constexpr float    kMaxTopupMlPerPart  = 30.0f;
  static constexpr uint32_t kMinPhIntervalS     = 20 * 60;    // between two pH Down doses
  static constexpr uint32_t kMinTopupIntervalS  = 2 * 3600;   // between two nutrient top-ups
  static constexpr uint32_t kRestAfterAnyDoseS  = 20 * 60;    // dead band after any dose
  static constexpr uint32_t kPhSustainS         = 10 * 60;    // pH must stay high this long
  static constexpr uint32_t kEcSustainS         = 30 * 60;    // EC must stay low this long
  static constexpr uint32_t kTopupStepGapS      = 5 * 60;     // Micro -> Grow -> Bloom spacing
  static constexpr uint32_t kBootHoldS          = 10 * 60;    // no dosing right after power-up
  static constexpr uint32_t kJumpWindowS        = 5 * 60;
  static constexpr float    kMaxJump            = 1.0f;       // pH units or mS/cm within the window
  static constexpr float    kPhMin = 4.0f, kPhMax = 9.0f;     // outside: probe is suspect
  static constexpr float    kEcMin = 0.1f, kEcMax = 3.0f;
};

struct Reading {
  float ph = 0;    bool phValid = false;
  float ec = 0;    bool ecValid = false;    // mS/cm, compensated to 25 C
  bool  tankEmpty = false;                  // low float switch open: probes may be dry
};

struct Settings {
  float phHigh   = 6.4f;                    // dose pH Down above this
  float ecLow    = 0.9f;                    // top up nutrients below this (alert low of the regime)
  float phStepMl = 0.5f;                    // one pH Down dose
  float topupMl[3] = {4.8f, 7.4f, 2.6f};    // Micro, Grow, Bloom for one top-up (37 L tank)
  bool  phDosing = true;
  bool  nutrientDosing = true;
};

enum class Mode  : uint8_t { Shadow = 0, Armed = 1 };
enum class Fault : uint8_t { None = 0, PhOutOfRange, EcOutOfRange, PhJump, EcJump };

struct Action {
  Pump  pump;
  float ml;
  bool  shadow;          // true: log only, do not run the pump
  const char* reason;
};

struct History {         // what must survive a reboot so ceilings still hold
  uint8_t phDoses = 0;
  float   phMl = 0;
  uint8_t topups = 0;
};

class Controller {
 public:
  void begin(uint32_t nowS);
  void configure(const Settings& s);
  const Settings& settings() const { return s_; }

  void setMode(Mode m) { mode_ = m; }
  Mode mode() const { return mode_; }
  void setPaused(bool p) { paused_ = p; }
  bool paused() const { return paused_; }

  Fault fault() const { return fault_; }
  bool  lockedOut() const { return fault_ != Fault::None; }
  void  clearFault(uint32_t nowS);          // manual only: the controller never re-arms itself
  void  restoreFault(Fault f) { if (f != Fault::None) latch(f); }   // a fault survives a reboot

  // Feed every new reading. Returns true when a dose is due now and fills `out`.
  // nowS is seconds from a monotonic clock that does not wrap (on the ESP32:
  // esp_timer_get_time() / 1000000, not millis() / 1000, which wraps after 49 days).
  // The caller runs the pump (or just logs it when out.shadow is true).
  bool update(uint32_t nowS, const Reading& r, Action& out);

  // Doses counted over the last 24 h.
  History history(uint32_t nowS) const;
  // After a reboot: count the persisted doses as if they had just happened.
  void restoreHistory(uint32_t nowS, const History& h);

  bool     sequenceActive() const { return seqStep_ >= 0; }
  uint32_t nextPhAllowedS() const;
  uint32_t nextTopupAllowedS() const;

 private:
  static const int kPhLog = 8, kTopupLog = 4, kSamples = 16;
  struct Dose { uint32_t t; float ml; bool used; };
  struct Sample { uint32_t t; float ph; float ec; bool phOk; bool ecOk; bool used; };

  void    latch(Fault f);
  void    recordPh(uint32_t nowS, float ml);
  void    recordTopup(uint32_t nowS);
  int     phCount(uint32_t nowS) const;
  float   phMl(uint32_t nowS) const;
  int     topupCount(uint32_t nowS) const;
  bool    jumped(uint32_t nowS, const Reading& r, Fault& which) const;
  void    remember(uint32_t nowS, const Reading& r);
  static bool within(uint32_t nowS, uint32_t t, uint32_t windowS) { return nowS - t < windowS; }

  Settings s_;
  Mode     mode_ = Mode::Shadow;
  bool     paused_ = false;
  Fault    fault_ = Fault::None;
  uint32_t startS_ = 0;
  bool     phHigh_ = false;   uint32_t phHighSince_ = 0;
  bool     ecLow_  = false;   uint32_t ecLowSince_  = 0;
  bool     anyDose_ = false;  uint32_t lastDoseS_ = 0;
  bool     anyPh_ = false;    uint32_t lastPhS_ = 0;
  bool     anyTopup_ = false; uint32_t lastTopupS_ = 0;
  int      seqStep_ = -1;     uint32_t seqNextS_ = 0;
  Dose     ph_[kPhLog] = {};
  Dose     topup_[kTopupLog] = {};
  Sample   samples_[kSamples] = {};
  int      sampleIdx_ = 0;
};

const char* faultName(Fault f);
const char* pumpName(Pump p);

}  // namespace tower
