// Unit tests for DummyDataGenerator::GenerateOrders (Stage 2 of the
// DummyDataGenerator PoC). See docs/PRD.md section 6.2/6.4/7.2 for the
// domain rules being verified here (referential integrity, state/stock
// consistency, reproducibility) and ../CLAUDE.md for the reusability
// requirements (seed reproducibility, parameterized ranges/distributions).

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <vector>

#include "DummyDataGenerator/OrderGenerator.h"

using DataPersistence::Model::Order;
using DataPersistence::Model::OrderState;
using DataPersistence::Model::Sample;
using DummyDataGenerator::GenerateOrders;
using DummyDataGenerator::OrderedQuantityRange;
using DummyDataGenerator::OrderGenerationOptions;
using DummyDataGenerator::OrderStateDistribution;

namespace
{
    bool OrdersEqual(const Order& lhs, const Order& rhs)
    {
        return lhs.id == rhs.id
            && lhs.sampleId == rhs.sampleId
            && lhs.customerName == rhs.customerName
            && lhs.orderedQuantity == rhs.orderedQuantity
            && lhs.state == rhs.state;
    }

    // A fixed set of Samples with a variety of stockQuantity values (some
    // depleted, some in stock) so that state/stock consistency rules can be
    // exercised meaningfully, per the domain rule note in CLAUDE.md.
    std::vector<Sample> MakeFixtureSamples()
    {
        return {
            Sample{ 1, "SampleA", 0.5, 0.9, 0 },     // depleted
            Sample{ 2, "SampleB", 0.6, 0.8, 50 },    // in stock
            Sample{ 3, "SampleC", 0.7, 0.85, 200 },  // in stock
            Sample{ 4, "SampleD", 0.4, 0.95, 0 },    // depleted
            Sample{ 5, "SampleE", 0.55, 0.88, 500 }, // in stock
        };
    }
}

// --- Reproducibility ------------------------------------------------------

TEST(OrderGeneratorTest, SameSeedProducesIdenticalResults)
{
    const auto samples = MakeFixtureSamples();

    OrderGenerationOptions options;
    options.orderCount = 30;
    options.startId = 10;
    options.seed = 12345;

    const auto first = GenerateOrders(options, samples);
    const auto second = GenerateOrders(options, samples);

    ASSERT_EQ(first.size(), second.size());
    for (std::size_t i = 0; i < first.size(); ++i)
    {
        EXPECT_TRUE(OrdersEqual(first[i], second[i])) << "Mismatch at index " << i;
    }
}

// --- Count / id sequencing -------------------------------------------------

TEST(OrderGeneratorTest, GeneratesExactlyRequestedCountWithConsecutiveIds)
{
    const auto samples = MakeFixtureSamples();

    OrderGenerationOptions options;
    options.orderCount = 15;
    options.startId = 100;
    options.seed = 7;

    const auto orders = GenerateOrders(options, samples);

    ASSERT_EQ(orders.size(), 15u);
    for (std::size_t i = 0; i < orders.size(); ++i)
    {
        EXPECT_EQ(orders[i].id, options.startId + static_cast<int>(i));
    }
}

// --- Referential integrity --------------------------------------------------

TEST(OrderGeneratorTest, EverySampleIdReferencesAKnownSample)
{
    const auto samples = MakeFixtureSamples();
    std::set<int> knownSampleIds;
    for (const auto& sample : samples)
    {
        knownSampleIds.insert(sample.id);
    }

    OrderGenerationOptions options;
    options.orderCount = 50;
    options.seed = 42;

    const auto orders = GenerateOrders(options, samples);

    ASSERT_EQ(orders.size(), 50u);
    for (const auto& order : orders)
    {
        EXPECT_TRUE(knownSampleIds.count(order.sampleId) > 0)
            << "Order " << order.id << " references unknown sampleId " << order.sampleId;
    }
}

// --- State / stock consistency ----------------------------------------------

TEST(OrderGeneratorTest, ProducingOrdersReproduceStockShortage)
{
    const auto samples = MakeFixtureSamples();
    std::map<int, int> stockById;
    for (const auto& sample : samples)
    {
        stockById[sample.id] = sample.stockQuantity;
    }

    OrderGenerationOptions options;
    options.orderCount = 100;
    options.seed = 55;
    options.stateDistribution = OrderStateDistribution{ 0.0, 0.0, 1.0, 0.0, 0.0 };

    const auto orders = GenerateOrders(options, samples);

    bool sawProducing = false;
    for (const auto& order : orders)
    {
        if (order.state == OrderState::PRODUCING)
        {
            sawProducing = true;
            EXPECT_GT(order.orderedQuantity, stockById.at(order.sampleId))
                << "PRODUCING order " << order.id << " does not exceed stock of sample " << order.sampleId;
        }
    }
    EXPECT_TRUE(sawProducing);
}

TEST(OrderGeneratorTest, ConfirmedAndReleaseOrdersReproduceSufficientStock)
{
    // Fixture with at least one in-stock Sample so the fallback edge case
    // (all samples depleted) is not triggered here; that edge case is a
    // separate concern from the normal sufficient-stock invariant.
    const auto samples = MakeFixtureSamples();
    std::map<int, int> stockById;
    for (const auto& sample : samples)
    {
        stockById[sample.id] = sample.stockQuantity;
    }

    OrderGenerationOptions options;
    options.orderCount = 100;
    options.seed = 77;
    options.stateDistribution = OrderStateDistribution{ 0.0, 0.5, 0.0, 0.5, 0.0 };

    const auto orders = GenerateOrders(options, samples);

    bool sawConfirmedOrRelease = false;
    for (const auto& order : orders)
    {
        if (order.state == OrderState::CONFIRMED || order.state == OrderState::RELEASE)
        {
            sawConfirmedOrRelease = true;
            EXPECT_LE(order.orderedQuantity, stockById.at(order.sampleId))
                << "Order " << order.id << " (state " << static_cast<int>(order.state)
                << ") exceeds stock of sample " << order.sampleId;
        }
    }
    EXPECT_TRUE(sawConfirmedOrRelease);
}

// --- State distribution ------------------------------------------------------

TEST(OrderGeneratorTest, StateDistributionRatiosAreRespectedStatistically)
{
    const auto samples = MakeFixtureSamples();

    OrderGenerationOptions options;
    options.orderCount = 200;
    options.seed = 99;
    options.stateDistribution = OrderStateDistribution{ 0.4, 0.2, 0.2, 0.1, 0.1 };

    const auto orders = GenerateOrders(options, samples);
    ASSERT_EQ(orders.size(), 200u);

    std::map<OrderState, int> counts;
    for (const auto& order : orders)
    {
        ++counts[order.state];
    }

    // Ratios are rounded internally, so allow a small tolerance around the
    // expected counts (80 / 40 / 40 / 20 / 20 out of 200).
    EXPECT_NEAR(counts[OrderState::RESERVED], 80, 3);
    EXPECT_NEAR(counts[OrderState::CONFIRMED], 40, 3);
    EXPECT_NEAR(counts[OrderState::PRODUCING], 40, 3);
    EXPECT_NEAR(counts[OrderState::RELEASE], 20, 3);
    EXPECT_NEAR(counts[OrderState::REJECTED], 20, 3);
}

// --- Boundary values ----------------------------------------------------------

TEST(OrderGeneratorTest, EmptySampleListProducesEmptyResult)
{
    std::vector<Sample> emptySamples;

    OrderGenerationOptions options;
    options.orderCount = 10;
    options.seed = 1;

    const auto orders = GenerateOrders(options, emptySamples);

    EXPECT_TRUE(orders.empty());
}

TEST(OrderGeneratorTest, ZeroOrNegativeOrderCountProducesEmptyResult)
{
    const auto samples = MakeFixtureSamples();

    OrderGenerationOptions zeroOptions;
    zeroOptions.orderCount = 0;
    zeroOptions.seed = 1;
    EXPECT_TRUE(GenerateOrders(zeroOptions, samples).empty());

    OrderGenerationOptions negativeOptions;
    negativeOptions.orderCount = -5;
    negativeOptions.seed = 1;
    EXPECT_TRUE(GenerateOrders(negativeOptions, samples).empty());
}

TEST(OrderGeneratorTest, SingleOrderBoundaryWorks)
{
    const auto samples = MakeFixtureSamples();

    OrderGenerationOptions options;
    options.orderCount = 1;
    options.startId = 42;
    options.seed = 3;

    const auto orders = GenerateOrders(options, samples);

    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].id, 42);
}
