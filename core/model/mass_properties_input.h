#pragma once

struct MassPropertiesInput
{
    double cylinderDiameterMm = 30.0;
    double reciprocatingMassKg = 0.5;
    double rotatingMassKg = 1.5;

    double referenceXmm = 0.0;
    double referenceYmm = 0.0;
    double referenceZmm = 0.0;
};