#include "DummyDataGenerator/ProductionQueueEntryGenerator.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <random>
#include <vector>

namespace DummyDataGenerator
{
    namespace
    {
        using DataPersistence::Model::Order;
        using DataPersistence::Model::OrderState;
        using DataPersistence::Model::ProductionQueueEntry;
        using DataPersistence::Model::ProductionState;
        using DataPersistence::Model::Sample;

        // Finds the Sample referenced by sampleId, if any. Returns nullptr
        // when no such Sample exists (see header comment: defensive-only,
        // should not happen given Stage 2's referential integrity).
        const Sample* FindSampleById(const std::vector<Sample>& samples, int sampleId)
        {
            for (const Sample& sample : samples)
            {
                if (sample.id == sampleId)
                {
                    return &sample;
                }
            }
            return nullptr;
        }

        // Builds the list of ProductionStates to assign, one per entry,
        // according to the requested ratios, then shuffles it so the order
        // in which states appear is not always "all WAITING first".
        std::vector<ProductionState> BuildStatePlan(
            int entryCount, const ProductionStateDistribution& distribution, std::mt19937& rng)
        {
            const double waitingRatio = std::clamp(distribution.waitingRatio, 0.0, 1.0);
            const double producingRatio = std::clamp(distribution.producingRatio, 0.0, 1.0);

            const int waitingCount = static_cast<int>(std::round(entryCount * waitingRatio));
            const int producingCount = static_cast<int>(std::round(entryCount * producingRatio));
            const int confirmedCount = std::max(0, entryCount - waitingCount - producingCount);

            std::vector<ProductionState> plan;
            plan.reserve(entryCount);
            plan.insert(plan.end(), waitingCount, ProductionState::WAITING);
            plan.insert(plan.end(), producingCount, ProductionState::PRODUCING);
            plan.insert(plan.end(), confirmedCount, ProductionState::CONFIRMED);

            // Ratios may round to fewer/more than entryCount entries; pad or
            // trim with WAITING so the plan always has exactly entryCount.
            while (static_cast<int>(plan.size()) < entryCount)
            {
                plan.push_back(ProductionState::WAITING);
            }
            plan.resize(entryCount);

            std::shuffle(plan.begin(), plan.end(), rng);
            return plan;
        }

        // Builds a ProductionQueueEntry for a single PRODUCING Order,
        // deriving shortageQuantity/actualProductionQuantity/
        // totalProductionTime from the referenced Sample per PRD 6.3/6.4.
        ProductionQueueEntry BuildEntry(const Order& order, const Sample& sample, ProductionState state)
        {
            ProductionQueueEntry entry;
            entry.orderId = order.id;
            entry.sampleId = sample.id;
            entry.orderedQuantity = order.orderedQuantity;
            entry.shortageQuantity = order.orderedQuantity - sample.stockQuantity;

            // yieldRatio should be in (0, 1]; guard against a degenerate
            // zero value so we never divide by zero here.
            const double yieldRatio = sample.yieldRatio > 0.0 ? sample.yieldRatio : 1.0;
            entry.actualProductionQuantity = static_cast<int>(
                std::ceil(static_cast<double>(entry.shortageQuantity) / yieldRatio));

            entry.totalProductionTime = sample.averageProductionTimePerUnit * entry.actualProductionQuantity;
            entry.state = state;
            return entry;
        }
    }

    std::vector<ProductionQueueEntry> GenerateProductionQueueEntries(
        const ProductionQueueEntryGenerationOptions& options,
        const std::vector<Order>& orders,
        const std::vector<Sample>& samples)
    {
        std::vector<ProductionQueueEntry> entries;

        std::vector<const Order*> producingOrders;
        for (const Order& order : orders)
        {
            if (order.state == OrderState::PRODUCING)
            {
                producingOrders.push_back(&order);
            }
        }

        if (producingOrders.empty() || samples.empty())
        {
            return entries;
        }
        entries.reserve(producingOrders.size());

        std::mt19937 rng(options.seed);
        const std::vector<ProductionState> statePlan =
            BuildStatePlan(static_cast<int>(producingOrders.size()), options.stateDistribution, rng);

        for (std::size_t index = 0; index < producingOrders.size(); ++index)
        {
            const Order& order = *producingOrders[index];
            const Sample* sample = FindSampleById(samples, order.sampleId);
            if (sample == nullptr)
            {
                // Defensive skip: an Order referencing a non-existent
                // Sample cannot be turned into a queue entry, since the
                // derived fields require that Sample's stock/yield/time.
                continue;
            }

            entries.push_back(BuildEntry(order, *sample, statePlan[index]));
        }

        return entries;
    }
}
