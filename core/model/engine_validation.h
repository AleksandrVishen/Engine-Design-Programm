#pragma once

#include <string>
#include <vector>
#include "engine_model.h"

struct ValidationMessage
{
    std::string text;
};

struct ValidationResult
{
    bool ok = true;
    std::vector<ValidationMessage> errors;
};

class EngineValidation
{
public:
    static ValidationResult ValidateForPreview(const EngineModel& model);
    static ValidationResult ValidateForCalculation(const EngineModel& model);
};