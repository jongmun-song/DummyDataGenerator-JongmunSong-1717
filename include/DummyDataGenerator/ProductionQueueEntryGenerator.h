#pragma once

// Stage 3 of the DummyDataGenerator PoC: generates
// DataPersistence::Model::ProductionQueueEntry dummy data (see
// ../../dataModel/ProductionQueueEntry.h and
// ../../storedData/production_queue.json for the target schema, and
// docs/PRD.md section 6.3/6.4/7.3).
//
// Entries are derived from the PRODUCING Orders produced by Stage 2
// (OrderGenerator.h) - requirements.pdf's production queue only tracks
// orders that were approved but could not be fulfilled from stock. Each
// entry references its source Order (orderId) and the Sample it covers
// (sampleId), so referential integrity always holds, and its derived
// fields follow the formulas mandated by PRD 6.4:
//   - shortageQuantity = orderedQuantity - Sample.stockQuantity
//   - actualProductionQuantity = ceil(shortageQuantity / Sample.yieldRatio)
//   - totalProductionTime = Sample.averageProductionTimePerUnit *
//                            actualProductionQuantity
//
// Because Stage 2's PRODUCING orders are always generated with
// orderedQuantity > Sample.stockQuantity, shortageQuantity is guaranteed
// to be positive here.

#include <vector>

#include "dataModel/Order.h"
#include "dataModel/ProductionQueueEntry.h"
#include "dataModel/Sample.h"

namespace DummyDataGenerator
{
    // Fraction of generated entries that should land in each
    // ProductionState. Ratios are clamped/normalized internally; they do
    // not need to sum to exactly 1. PRD/CLAUDE.md place no constraint
    // linking ProductionState to Order/Sample values, so the distribution
    // is free to be tuned by the caller.
    struct ProductionStateDistribution
    {
        double waitingRatio = 0.4;
        double producingRatio = 0.4;
        double confirmedRatio = 0.2;
    };

    // Parameters controlling the ProductionState distribution and the
    // random seed used to assign it. Quantity/time fields are always
    // derived from the referenced Order/Sample, so there is nothing else
    // to parameterize here.
    struct ProductionQueueEntryGenerationOptions
    {
        unsigned int seed = 0;

        ProductionStateDistribution stateDistribution;
    };

    // Generates a reproducible list of ProductionQueueEntry values, one per
    // PRODUCING Order in `orders`. Each entry's orderId/sampleId reference
    // the source Order and its Sample; shortageQuantity,
    // actualProductionQuantity and totalProductionTime are derived from
    // that Sample's stockQuantity/yieldRatio/averageProductionTimePerUnit
    // per PRD 6.3/6.4.
    //
    // Orders whose state is not PRODUCING are skipped (PRD 7.3: only
    // PRODUCING orders enter the production queue). An Order whose
    // sampleId does not resolve to any Sample in `samples` is also skipped
    // (defensive: this should not happen given Stage 2's referential
    // integrity guarantee, but a missing Sample means the derived fields
    // cannot be computed).
    std::vector<DataPersistence::Model::ProductionQueueEntry> GenerateProductionQueueEntries(
        const ProductionQueueEntryGenerationOptions& options,
        const std::vector<DataPersistence::Model::Order>& orders,
        const std::vector<DataPersistence::Model::Sample>& samples);
}
