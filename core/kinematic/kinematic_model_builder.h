#pragma once

#include <string>
#include <vector>

#include "core/model/engine_model.h"
#include "core/kinematic/normalized_kinematic_model.h"

namespace engine::kinematic
{

struct KinematicBuildIssue
{
    std::string message;
};

struct KinematicBuildResult
{
    bool ok = false;
    NormalizedKinematicModel model;
    std::vector<KinematicBuildIssue> issues;
};

class KinematicModelBuilder
{
public:
    static KinematicBuildResult Build(const EngineModel& source);

private:
    static void AddIssue(KinematicBuildResult& result, const std::string& message);
};

} // namespace engine::kinematic