#pragma once
#include <algorithm>
#include <cmath>

namespace mmvr {
// Convert an OpenXR predicted display target into the steady-clock domain used
// by SimulationBudget. Sampling both clocks together avoids assuming their
// epochs match, even when the runtime supports KHR clock conversion.
struct InterpolationTimeComparison {
    double predictedSeconds = 0;
    double predictedAlpha = 0;
    double clampedAlpha = 0;
    double errorMs = 0;
    bool valid = false;
};

inline InterpolationTimeComparison CompareInterpolationTime(
    double tickStartSeconds, double tickPeriodSeconds, double selectedAlpha,
    double steadySampleSeconds, long long xrSampleNanoseconds,
    long long predictedDisplayNanoseconds) noexcept {
    InterpolationTimeComparison result;
    if (!std::isfinite(tickStartSeconds) || !std::isfinite(tickPeriodSeconds) ||
        !std::isfinite(selectedAlpha) || !std::isfinite(steadySampleSeconds) ||
        tickPeriodSeconds <= 0 || xrSampleNanoseconds <= 0 || predictedDisplayNanoseconds <= 0)
        return result;
    result.predictedSeconds = steadySampleSeconds +
        double(predictedDisplayNanoseconds - xrSampleNanoseconds) * 1e-9;
    result.predictedAlpha = (result.predictedSeconds - tickStartSeconds) / tickPeriodSeconds;
    result.clampedAlpha = std::clamp(result.predictedAlpha, 0.0, 1.0);
    result.errorMs = (selectedAlpha - result.predictedAlpha) * tickPeriodSeconds * 1000.0;
    result.valid = std::isfinite(result.predictedAlpha) && std::isfinite(result.errorMs);
    return result;
}
} // namespace mmvr
