#pragma once

// Stage 2 of the DummyDataGenerator PoC: generates DataPersistence::Model::Order
// dummy data (see ../../dataModel/Order.h and ../../storedData/orders.json for
// the target schema, and docs/PRD.md section 6.2/6.4/7.2).
//
// Orders reference a Sample list produced by Stage 1 (SampleGenerator.h) so
// referential integrity (Order.sampleId -> Sample.id) always holds. The state
// distribution also folds in Stage 4's stock-consistency rule up front (see
// CLAUDE.md's "data consistency" section):
//   - PRODUCING orders are generated with orderedQuantity > the referenced
//     Sample.stockQuantity (reproduces a stock-shortage scenario).
//   - CONFIRMED/RELEASE orders are generated with orderedQuantity <= the
//     referenced Sample.stockQuantity (reproduces a sufficient-stock /
//     already-handled scenario).
//   - RESERVED/REJECTED orders have no stock constraint (not yet approved /
//     rejected before production), per PRD 6.4.

#include <vector>

#include "dataModel/Order.h"
#include "dataModel/Sample.h"

namespace DummyDataGenerator
{
    // Inclusive [minValue, maxValue] range used for the ordered-quantity draw.
    struct OrderedQuantityRange
    {
        int minValue = 1;
        int maxValue = 300;
    };

    // Fraction of generated orders that should land in each OrderState. Ratios
    // are clamped/normalized internally; they do not need to sum to exactly 1.
    struct OrderStateDistribution
    {
        double reservedRatio = 0.3;
        double confirmedRatio = 0.2;
        double producingRatio = 0.2;
        double releaseRatio = 0.2;
        double rejectedRatio = 0.1;
    };

    // Parameters controlling how many Orders are generated and the value
    // ranges/distributions for each field. All fields have reasonable
    // defaults so callers can use OrderGenerationOptions{} as-is, or override
    // only what they need.
    struct OrderGenerationOptions
    {
        int orderCount = 5;
        int startId = 1;
        unsigned int seed = 0;

        OrderedQuantityRange orderedQuantityRange = { 50, 300 };

        OrderStateDistribution stateDistribution;
    };

    // Generates a reproducible list of Order values that reference the given
    // Samples by id. Calling this twice with the same options and the same
    // samples (in particular the same seed) yields an identical result.
    //
    // `samples` must be non-empty; an empty result is returned otherwise,
    // since an Order cannot be generated without a Sample to reference.
    std::vector<DataPersistence::Model::Order> GenerateOrders(
        const OrderGenerationOptions& options,
        const std::vector<DataPersistence::Model::Sample>& samples);
}
