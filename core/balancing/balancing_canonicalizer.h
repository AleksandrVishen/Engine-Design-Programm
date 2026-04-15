#pragma once

#include <string>

#include "core/model/engine_model.h"

namespace engine::balancing
{

std::string BuildCanonicalCandidateSignature(const EngineModel& model);

} // namespace engine::balancing