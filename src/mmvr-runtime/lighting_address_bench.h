#pragma once
namespace mmvr {
// Protected replay experiment only; normal play keeps the measured ordinal
// cache until address-set reuse has passed whole-scene cost/output comparisons.
inline thread_local bool lightingAddressExperiment = false;
inline thread_local bool lightingAddressReference = false;
}
