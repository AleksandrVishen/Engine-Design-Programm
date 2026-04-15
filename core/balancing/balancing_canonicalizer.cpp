#include "core/balancing/balancing_canonicalizer.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace engine::balancing
{

namespace
{

double NormalizePhaseDeg(double phaseDeg)
{
    double v = std::fmod(phaseDeg, 360.0);
    if (v < 0.0)
        v += 360.0;

    if (std::abs(v - 360.0) < 1e-9)
        v = 0.0;

    return v;
}

double Quantize(double value, double step)
{
    if (step <= 0.0)
        return value;

    return std::round(value / step) * step;
}

double QuantizeMm(double value)
{
    return Quantize(value, 0.001);
}

double QuantizeKg(double value)
{
    return Quantize(value, 0.000001);
}

double QuantizeDeg(double value)
{
    return Quantize(NormalizePhaseDeg(value), 0.001);
}

struct CanonicalCounterweight
{
    double positionAlongShaftMm = 0.0;
    double phaseDeg = 0.0;
};

struct CanonicalShaft
{
    int axis = 0;
    int speedRatio = 0;
    double originAbsXMm = 0.0;
    double originAbsYMm = 0.0;
    double originAbsZMm = 0.0;
    double lengthMm = 0.0;
    double counterweightMassKg = 0.0;
    double counterweightRadiusMm = 0.0;
    std::vector<CanonicalCounterweight> counterweights;
};

CanonicalCounterweight CanonicalizeCounterweight(const BalancerCounterweightSpec& cw)
{
    CanonicalCounterweight out;
    out.positionAlongShaftMm = QuantizeMm(cw.positionAlongShaftMm);
    out.phaseDeg = QuantizeDeg(cw.phaseDeg);
    return out;
}

CanonicalShaft CanonicalizeShaft(const BalancerShaftSpec& shaft)
{
    CanonicalShaft out;

    out.axis = static_cast<int>(shaft.axis);
    out.speedRatio = static_cast<int>(shaft.speedRatio);

    out.originAbsXMm = QuantizeMm(std::abs(shaft.originXMm));
    out.originAbsYMm = QuantizeMm(std::abs(shaft.originYMm));
    out.originAbsZMm = QuantizeMm(std::abs(shaft.originZMm));

    out.lengthMm = QuantizeMm(shaft.lengthMm);
    out.counterweightMassKg = QuantizeKg(shaft.counterweightMassKg);
    out.counterweightRadiusMm = QuantizeMm(shaft.counterweightRadiusMm);

    out.counterweights.reserve(shaft.counterweights.size());
    for (const auto& cw : shaft.counterweights)
        out.counterweights.push_back(CanonicalizeCounterweight(cw));

    std::sort(out.counterweights.begin(),
              out.counterweights.end(),
              [](const CanonicalCounterweight& a, const CanonicalCounterweight& b)
              {
                  return std::tie(a.positionAlongShaftMm, a.phaseDeg) <
                         std::tie(b.positionAlongShaftMm, b.phaseDeg);
              });

    return out;
}

std::string BuildCanonicalCrankSignature(const EngineModel& model)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);

    const auto& cw = model.balancing.crankCounterweights;
    oss << "crank:"
        << static_cast<int>(cw.enabled)
        << ':'
        << QuantizeKg(cw.massKg)
        << ':'
        << QuantizeMm(cw.radiusMm)
        << ':'
        << static_cast<int>(cw.countMode);

    return oss.str();
}

std::string BuildCanonicalShaftSignature(const CanonicalShaft& shaft)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);

    oss << "shaft:"
        << shaft.axis
        << ':'
        << shaft.speedRatio
        << ':'
        << shaft.originAbsXMm
        << ':'
        << shaft.originAbsYMm
        << ':'
        << shaft.originAbsZMm
        << ':'
        << shaft.lengthMm
        << ':'
        << shaft.counterweightMassKg
        << ':'
        << shaft.counterweightRadiusMm;

    for (const auto& cw : shaft.counterweights)
    {
        oss << ":cw("
            << cw.positionAlongShaftMm
            << ','
            << cw.phaseDeg
            << ')';
    }

    return oss.str();
}

} // namespace

std::string BuildCanonicalCandidateSignature(const EngineModel& model)
{
    std::vector<CanonicalShaft> canonicalShafts;
    canonicalShafts.reserve(model.balancing.balancerShafts.size());

    for (const auto& shaft : model.balancing.balancerShafts)
    {
        if (!shaft.enabled)
            continue;

        canonicalShafts.push_back(CanonicalizeShaft(shaft));
    }

    std::sort(canonicalShafts.begin(),
              canonicalShafts.end(),
              [](const CanonicalShaft& a, const CanonicalShaft& b)
              {
                  if (a.axis != b.axis)
                      return a.axis < b.axis;

                  if (a.speedRatio != b.speedRatio)
                      return a.speedRatio < b.speedRatio;

                  if (a.originAbsXMm != b.originAbsXMm)
                      return a.originAbsXMm < b.originAbsXMm;

                  if (a.originAbsYMm != b.originAbsYMm)
                      return a.originAbsYMm < b.originAbsYMm;

                  if (a.originAbsZMm != b.originAbsZMm)
                      return a.originAbsZMm < b.originAbsZMm;

                  if (a.lengthMm != b.lengthMm)
                      return a.lengthMm < b.lengthMm;

                  if (a.counterweightMassKg != b.counterweightMassKg)
                      return a.counterweightMassKg < b.counterweightMassKg;

                  if (a.counterweightRadiusMm != b.counterweightRadiusMm)
                      return a.counterweightRadiusMm < b.counterweightRadiusMm;

                  if (a.counterweights.size() != b.counterweights.size())
                      return a.counterweights.size() < b.counterweights.size();

                  return BuildCanonicalShaftSignature(a) < BuildCanonicalShaftSignature(b);
              });

    std::ostringstream oss;
    oss << BuildCanonicalCrankSignature(model);

    for (const auto& shaft : canonicalShafts)
        oss << "|" << BuildCanonicalShaftSignature(shaft);

    return oss.str();
}

} // namespace engine::balancing