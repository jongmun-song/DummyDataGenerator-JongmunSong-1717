#include "DummyDataGenerator/OrderGenerator.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace DummyDataGenerator
{
    namespace
    {
        using DataPersistence::Model::Order;
        using DataPersistence::Model::OrderState;
        using DataPersistence::Model::Sample;

        // Customer names, matching the style used in storedData/orders.json
        // (LG, SK, Sam, ...).
        const std::vector<std::string> kCustomerNamePool = {
            "LG", "SK", "Sam", "Hynix", "Amkor", "TSMC", "ASE", "JCET",
        };

        // Builds the list of OrderStates to assign, one per order, according
        // to the requested ratios, then shuffles it so the order in which
        // states appear is not always "all RESERVED first".
        std::vector<OrderState> BuildStatePlan(const OrderGenerationOptions& options, std::mt19937& rng)
        {
            const double reservedRatio = std::clamp(options.stateDistribution.reservedRatio, 0.0, 1.0);
            const double confirmedRatio = std::clamp(options.stateDistribution.confirmedRatio, 0.0, 1.0);
            const double producingRatio = std::clamp(options.stateDistribution.producingRatio, 0.0, 1.0);
            const double releaseRatio = std::clamp(options.stateDistribution.releaseRatio, 0.0, 1.0);

            const int reservedCount = static_cast<int>(std::round(options.orderCount * reservedRatio));
            const int confirmedCount = static_cast<int>(std::round(options.orderCount * confirmedRatio));
            const int producingCount = static_cast<int>(std::round(options.orderCount * producingRatio));
            const int releaseCount = static_cast<int>(std::round(options.orderCount * releaseRatio));
            const int rejectedCount = std::max(
                0, options.orderCount - reservedCount - confirmedCount - producingCount - releaseCount);

            std::vector<OrderState> plan;
            plan.reserve(options.orderCount);
            plan.insert(plan.end(), reservedCount, OrderState::RESERVED);
            plan.insert(plan.end(), confirmedCount, OrderState::CONFIRMED);
            plan.insert(plan.end(), producingCount, OrderState::PRODUCING);
            plan.insert(plan.end(), releaseCount, OrderState::RELEASE);
            plan.insert(plan.end(), rejectedCount, OrderState::REJECTED);

            // Ratios may round to fewer/more than orderCount entries; pad or
            // trim with RESERVED so the plan always has exactly orderCount.
            while (static_cast<int>(plan.size()) < options.orderCount)
            {
                plan.push_back(OrderState::RESERVED);
            }
            plan.resize(options.orderCount);

            std::shuffle(plan.begin(), plan.end(), rng);
            return plan;
        }

        // Picks any Sample at random; used for PRODUCING orders, since a
        // shortage can be reproduced against any Sample by choosing an
        // orderedQuantity above its current stockQuantity.
        const Sample& PickAnySample(const std::vector<Sample>& samples, std::mt19937& rng)
        {
            std::uniform_int_distribution<std::size_t> distribution(0, samples.size() - 1);
            return samples[distribution(rng)];
        }

        // Picks a Sample with stockQuantity > 0 when one exists; used for
        // CONFIRMED/RELEASE orders, which must reproduce a sufficient-stock
        // scenario (orderedQuantity <= stockQuantity, so stockQuantity must be
        // at least 1). Falls back to any Sample if every one is depleted -
        // an edge case that cannot fully satisfy the sufficient-stock rule.
        const Sample& PickSampleWithStock(const std::vector<Sample>& samples, std::mt19937& rng)
        {
            std::vector<const Sample*> inStock;
            for (const Sample& sample : samples)
            {
                if (sample.stockQuantity > 0)
                {
                    inStock.push_back(&sample);
                }
            }

            if (inStock.empty())
            {
                return PickAnySample(samples, rng);
            }

            std::uniform_int_distribution<std::size_t> distribution(0, inStock.size() - 1);
            return *inStock[distribution(rng)];
        }

        // orderedQuantity strictly greater than the referenced Sample's
        // stockQuantity, reproducing the "stock shortage" scenario required
        // for PRODUCING orders (PRD 6.4).
        int GenerateShortageQuantity(const Sample& sample, const OrderGenerationOptions& options, std::mt19937& rng)
        {
            std::uniform_int_distribution<int> extraDistribution(
                std::max(1, options.orderedQuantityRange.minValue), std::max(1, options.orderedQuantityRange.maxValue));
            return sample.stockQuantity + extraDistribution(rng);
        }

        // orderedQuantity within [1, Sample.stockQuantity], reproducing the
        // "stock sufficient" scenario required for CONFIRMED/RELEASE orders
        // (PRD 6.4). If stockQuantity is 0 (fallback case above), the best we
        // can do is order a single unit.
        int GenerateSufficientQuantity(const Sample& sample, const OrderGenerationOptions& options, std::mt19937& rng)
        {
            if (sample.stockQuantity <= 0)
            {
                return 1;
            }

            const int upperBound = std::min(sample.stockQuantity, options.orderedQuantityRange.maxValue);
            const int lowerBound = std::max(1, std::min(options.orderedQuantityRange.minValue, upperBound));

            std::uniform_int_distribution<int> distribution(lowerBound, upperBound);
            return distribution(rng);
        }

        // orderedQuantity with no stock constraint, for RESERVED/REJECTED
        // orders (not yet approved / rejected before production, per PRD 6.4).
        int GenerateUnconstrainedQuantity(const OrderGenerationOptions& options, std::mt19937& rng)
        {
            std::uniform_int_distribution<int> distribution(
                options.orderedQuantityRange.minValue, options.orderedQuantityRange.maxValue);
            return distribution(rng);
        }

        std::string GenerateCustomerName(std::mt19937& rng)
        {
            std::uniform_int_distribution<std::size_t> distribution(0, kCustomerNamePool.size() - 1);
            return kCustomerNamePool[distribution(rng)];
        }
    }

    std::vector<Order> GenerateOrders(
        const OrderGenerationOptions& options, const std::vector<Sample>& samples)
    {
        std::vector<Order> orders;
        if (options.orderCount <= 0 || samples.empty())
        {
            return orders;
        }
        orders.reserve(options.orderCount);

        std::mt19937 rng(options.seed);
        const std::vector<OrderState> statePlan = BuildStatePlan(options, rng);

        for (int index = 0; index < options.orderCount; ++index)
        {
            const OrderState state = statePlan[index];

            Order order;
            order.id = options.startId + index;
            order.state = state;
            order.customerName = GenerateCustomerName(rng);

            switch (state)
            {
            case OrderState::PRODUCING:
            {
                const Sample& sample = PickAnySample(samples, rng);
                order.sampleId = sample.id;
                order.orderedQuantity = GenerateShortageQuantity(sample, options, rng);
                break;
            }
            case OrderState::CONFIRMED:
            case OrderState::RELEASE:
            {
                const Sample& sample = PickSampleWithStock(samples, rng);
                order.sampleId = sample.id;
                order.orderedQuantity = GenerateSufficientQuantity(sample, options, rng);
                break;
            }
            case OrderState::RESERVED:
            case OrderState::REJECTED:
            default:
            {
                const Sample& sample = PickAnySample(samples, rng);
                order.sampleId = sample.id;
                order.orderedQuantity = GenerateUnconstrainedQuantity(options, rng);
                break;
            }
            }

            orders.push_back(order);
        }

        return orders;
    }
}
