#pragma once
#include "arm_run.h"
#include <cstdio>
#include <limits>
#include <stdexcept>
inline void ArmRunChecks() {
  auto require = [](bool ok, const char *what) {
    if (!ok)
      throw std::runtime_error(what);
  };
  using Hands = std::array<std::array<float, 3>, 2>;
  for (float amplitude : {.025f, .04f, .08f})
  for (int hz : {72, 80, 90, 120}) {
    mmvr::ArmRunGesture run;
    int active = 0, sampled = 0;
    for (int i = 0; i < hz * 3; ++i) {
      double t = double(i) / hz;
      float swing = amplitude * std::sin(float(t) * 6.2831853f * 1.8f);
      float other = -amplitude * .85f * std::sin(float(t) * 6.2831853f * 1.8f + .25f);
      Hands hands{{{-.3f, 1.f, swing}, {.3f, 1.f, other}}};
      run.Update(t, 1, 1, true, hands);
      if (t > 1) {
        ++sampled;
        active += run.Multiplier(t) == 1.15f;
      }
    }
    require(active > sampled * .8,
            "Running gesture must reliably sustain across supported cadences");
    Hands stopped{{{-.3f, 1.f, 0}, {.3f, 1.f, 0}}};
    for (int i = 0; i < hz; ++i)
      run.Update(3. + double(i) / hz, 1, 1, true, stopped);
    require(run.Multiplier(3.9) == 1, "Stopping must remove the run boost");
    run.Update(4., 2, 1, true, stopped);
    require(run.Multiplier(4.) == 1,
            "Recenter/new session must clear gesture history");
  }
  for (int mode = 0; mode < 4; ++mode) {
    mmvr::ArmRunGesture run;
    for (int i = 0; i < 360; ++i) {
      double t = i / 90.;
      float s = .08f * std::sin(float(t) * 11.3f);
      Hands hands{{{-.3f, 1.f, s}, {.3f, 1.f, s}}};
      if (mode == 1) {
        hands[0][2] = s * .03f;
        hands[1][2] = -s * .03f;
      }
      if (mode == 2)
        hands[1][2] = 0;
      if (mode == 3) {
        hands[0][2] = float(std::min(t, .4)) * .3f;
        hands[1][2] = -hands[0][2];
      }
      run.Update(t, 1, 1, true, hands);
      require(
          run.Multiplier(t) == 1,
          "Common motion, noise, one arm and single reposition must not run");
    }
  }
  mmvr::ArmRunGesture run;
  Hands hands{};
  for (int i = 0; i < 180; ++i) {
    float s = .08f * std::sin(i / 90.f * 11.3f);
    hands = {{{-.3f, 1, s}, {.3f, 1, -s}}};
    run.Update(i / 90., 1, 1, true, hands);
  }
  require(run.Multiplier(179 / 90.) == 1.15f,
          "Fixture must be active before invalidation");
  run.Update(2, 1, 1, false, hands);
  require(run.Multiplier(2) == 1, "Invalid tracking/pause must reset");
  hands[0][0] = std::numeric_limits<float>::quiet_NaN();
  run.Update(2.01, 1, 1, true, hands);
  require(run.Multiplier(2.01) == 1,
          "Non-finite tracking must not enable boost");
  std::puts("Arm-run cadence, gesture rejection, stop and reset checks passed");
}
