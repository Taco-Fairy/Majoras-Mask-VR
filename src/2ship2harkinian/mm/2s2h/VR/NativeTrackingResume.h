#pragma once
// Exact-state resume adapters preserve issued game actions while replacing
// process-local controller histories and timestamps with a fresh XR sample.
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "runtime.h"
namespace mmvrgame {
void BeginStateTrackingResume();
bool StateTrackingResumePending();
bool VerifyStateTrackingResume();
void RebasePresentationClock(double now);
void RebaseItemTracking(const mmvr::TrackingFrame&);
void RebaseInteractionTracking(const mmvr::TrackingFrame&);
void RebaseBowTracking(const mmvr::TrackingFrame&);
void RebaseBottleTracking(const mmvr::TrackingFrame&);
void RebaseCombatTracking(const mmvr::TrackingFrame&);
void RebaseFinTracking(const mmvr::TrackingFrame&);
void RebaseGoronTracking(const mmvr::TrackingFrame&);
void RebaseClimbTracking(const mmvr::TrackingFrame&);
}
#endif
