#pragma once

// Stage 1 of the DummyDataGenerator PoC: generates DataPersistence::Model::Sample
// dummy data (see ../../dataModel/Sample.h and ../../storedData/samples.json for
// the target schema, and docs/PRD.md section 6.1/7.1).
//
// The generator produces a mix of depleted / low / sufficient stock quantities so
// downstream stages (Order, ProductionQueueEntry) and monitoring scenarios can be
// exercised without hand-crafted fixtures.

#include <vector>

#include "dataModel/Sample.h"

namespace DummyDataGenerator
{
    // Inclusive [minValue, maxValue] range used for stock-quantity buckets below.
    struct StockQuantityRange
    {
        int minValue = 0;
        int maxValue = 0;
    };

    // Parameters controlling how many Samples are generated and the value ranges
    // for each field. All fields have reasonable defaults so callers can use
    // SampleGenerationOptions{} as-is, or override only what they need.
    struct SampleGenerationOptions
    {
        int sampleCount = 5;
        int startId = 1;
        unsigned int seed = 0;

        double minAverageProductionTimePerUnit = 0.2;
        double maxAverageProductionTimePerUnit = 1.0;

        double minYieldRatio = 0.7;
        double maxYieldRatio = 0.99;

        // Fraction of generated samples that should land in each stock bucket.
        // The remainder (1.0 - depletedStockRatio - lowStockRatio) gets a
        // "sufficient" stock quantity. Ratios are clamped to [0, 1] internally.
        double depletedStockRatio = 0.2;
        double lowStockRatio = 0.2;

        StockQuantityRange lowStockRange = { 1, 50 };
        StockQuantityRange sufficientStockRange = { 100, 1000 };
    };

    // Generates a reproducible list of Sample values. Calling this twice with the
    // same options (in particular the same seed) yields an identical result.
    std::vector<DataPersistence::Model::Sample> GenerateSamples(
        const SampleGenerationOptions& options = SampleGenerationOptions{});
}
