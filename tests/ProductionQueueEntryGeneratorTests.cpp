// Unit tests for DummyDataGenerator::GenerateProductionQueueEntries (Stage 3
// of the DummyDataGenerator PoC). See docs/PRD.md section 6.3/6.4/7.3 for the
// domain rules being verified here (formula accuracy, referential integrity,
// PRODUCING-only filtering, reproducibility) and ../CLAUDE.md for the
// reusability requirements (seed reproducibility, parameterized
// distributions).

#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <set>
#include <vector>

#include "DummyDataGenerator/ProductionQueueEntryGenerator.h"

using DataPersistence::Model::Order;
using DataPersistence::Model::OrderState;
using DataPersistence::Model::ProductionQueueEntry;
using DataPersistence::Model::ProductionState;
using DataPersistence::Model::Sample;
using DummyDataGenerator::GenerateProductionQueueEntries;
using DummyDataGenerator::ProductionQueueEntryGenerationOptions;
using DummyDataGenerator::ProductionStateDistribution;

namespace
{
    bool EntriesEqual(const ProductionQueueEntry& lhs, const ProductionQueueEntry& rhs)
    {
        return lhs.orderId == rhs.orderId
            && lhs.sampleId == rhs.sampleId
            && lhs.orderedQuantity == rhs.orderedQuantity
            && lhs.shortageQuantity == rhs.shortageQuantity
            && lhs.actualProductionQuantity == rhs.actualProductionQuantity
            && lhs.totalProductionTime == rhs.totalProductionTime
            && lhs.state == rhs.state;
    }

    // A fixed set of Samples with varying stock/yield/production-time values,
    // and a fixed set of Orders mixing PRODUCING and non-PRODUCING states, so
    // filtering and referential integrity can be exercised meaningfully.
    std::vector<Sample> MakeFixtureSamples()
    {
        return {
            Sample{ 1, "SampleA", 0.5, 0.9, 0 },
            Sample{ 2, "SampleB", 0.6, 0.8, 50 },
            Sample{ 3, "SampleC", 0.7, 0.85, 200 },
            Sample{ 4, "SampleD", 0.4, 0.95, 0 },
            Sample{ 5, "SampleE", 0.55, 0.88, 500 },
        };
    }

    std::vector<Order> MakeFixtureOrders()
    {
        return {
            Order{ 100, 1, "Alice", 300, OrderState::PRODUCING },
            Order{ 101, 2, "Bob", 120, OrderState::PRODUCING },
            Order{ 102, 3, "Carol", 400, OrderState::CONFIRMED }, // non-PRODUCING, should be skipped
            Order{ 103, 4, "Dave", 50, OrderState::PRODUCING },
            Order{ 104, 5, "Eve", 600, OrderState::RESERVED },    // non-PRODUCING, should be skipped
            Order{ 105, 1, "Frank", 10, OrderState::REJECTED },   // non-PRODUCING, should be skipped
            Order{ 106, 2, "Grace", 90, OrderState::RELEASE },    // non-PRODUCING, should be skipped
        };
    }
}

// --- Reproducibility ------------------------------------------------------

TEST(ProductionQueueEntryGeneratorTest, SameSeedProducesIdenticalResults)
{
    const auto samples = MakeFixtureSamples();
    const auto orders = MakeFixtureOrders();

    ProductionQueueEntryGenerationOptions options;
    options.seed = 12345;

    const auto first = GenerateProductionQueueEntries(options, orders, samples);
    const auto second = GenerateProductionQueueEntries(options, orders, samples);

    ASSERT_EQ(first.size(), second.size());
    for (std::size_t i = 0; i < first.size(); ++i)
    {
        EXPECT_TRUE(EntriesEqual(first[i], second[i])) << "Mismatch at index " << i;
    }
}

// --- Count / filtering ------------------------------------------------------

TEST(ProductionQueueEntryGeneratorTest, GeneratesOneEntryPerProducingOrderOnly)
{
    const auto samples = MakeFixtureSamples();
    const auto orders = MakeFixtureOrders();

    std::size_t producingCount = 0;
    for (const auto& order : orders)
    {
        if (order.state == OrderState::PRODUCING)
        {
            ++producingCount;
        }
    }
    ASSERT_EQ(producingCount, 3u);

    ProductionQueueEntryGenerationOptions options;
    options.seed = 7;

    const auto entries = GenerateProductionQueueEntries(options, orders, samples);

    EXPECT_EQ(entries.size(), producingCount);
}

// --- Referential integrity --------------------------------------------------

TEST(ProductionQueueEntryGeneratorTest, EveryEntryReferencesItsSourceProducingOrder)
{
    const auto samples = MakeFixtureSamples();
    const auto orders = MakeFixtureOrders();

    std::map<int, int> producingOrderSampleIds; // orderId -> sampleId
    for (const auto& order : orders)
    {
        if (order.state == OrderState::PRODUCING)
        {
            producingOrderSampleIds[order.id] = order.sampleId;
        }
    }

    ProductionQueueEntryGenerationOptions options;
    options.seed = 42;

    const auto entries = GenerateProductionQueueEntries(options, orders, samples);

    ASSERT_EQ(entries.size(), producingOrderSampleIds.size());
    std::set<int> seenOrderIds;
    for (const auto& entry : entries)
    {
        ASSERT_TRUE(producingOrderSampleIds.count(entry.orderId) > 0)
            << "Entry references orderId " << entry.orderId << " which is not a PRODUCING order";
        EXPECT_EQ(entry.sampleId, producingOrderSampleIds.at(entry.orderId))
            << "Entry for order " << entry.orderId << " has mismatched sampleId";
        seenOrderIds.insert(entry.orderId);
    }
    EXPECT_EQ(seenOrderIds.size(), producingOrderSampleIds.size())
        << "Not every PRODUCING order produced exactly one entry";
}

// --- Formula accuracy --------------------------------------------------------

TEST(ProductionQueueEntryGeneratorTest, DerivedFieldsMatchFormulaWhenShortageDoesNotDivideEvenly)
{
    // shortage = 300 - 167 = 133; yieldRatio = 0.87907 (chosen so 133/yield is
    // not an integer, exercising the ceil rounding-up behavior).
    std::vector<Sample> samples = {
        Sample{ 1, "SampleA", 2.5, 0.87907, 167 },
    };
    std::vector<Order> orders = {
        Order{ 100, 1, "Alice", 300, OrderState::PRODUCING },
    };

    const int expectedShortage = 300 - 167;
    const double expectedRawQuantity = expectedShortage / 0.87907;
    const int expectedActualQuantity = static_cast<int>(std::ceil(expectedRawQuantity));
    ASSERT_GT(expectedActualQuantity, expectedRawQuantity)
        << "Fixture should exercise a case where ceil actually rounds up";
    const double expectedTotalTime = 2.5 * expectedActualQuantity;

    ProductionQueueEntryGenerationOptions options;
    options.seed = 1;

    const auto entries = GenerateProductionQueueEntries(options, orders, samples);

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].orderedQuantity, 300);
    EXPECT_EQ(entries[0].shortageQuantity, expectedShortage);
    EXPECT_EQ(entries[0].actualProductionQuantity, expectedActualQuantity);
    EXPECT_DOUBLE_EQ(entries[0].totalProductionTime, expectedTotalTime);
}

TEST(ProductionQueueEntryGeneratorTest, DerivedFieldsMatchFormulaWhenShortageDividesEvenly)
{
    // shortage = 200 - 100 = 100; yieldRatio = 0.5 so 100/0.5 = 200 exactly,
    // i.e. ceil() is a no-op here.
    std::vector<Sample> samples = {
        Sample{ 2, "SampleB", 1.2, 0.5, 100 },
    };
    std::vector<Order> orders = {
        Order{ 200, 2, "Bob", 200, OrderState::PRODUCING },
    };

    const int expectedShortage = 200 - 100;
    const int expectedActualQuantity = 200; // 100 / 0.5
    const double expectedTotalTime = 1.2 * expectedActualQuantity;

    ProductionQueueEntryGenerationOptions options;
    options.seed = 2;

    const auto entries = GenerateProductionQueueEntries(options, orders, samples);

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].shortageQuantity, expectedShortage);
    EXPECT_EQ(entries[0].actualProductionQuantity, expectedActualQuantity);
    EXPECT_DOUBLE_EQ(entries[0].totalProductionTime, expectedTotalTime);
}

// --- Boundary values ----------------------------------------------------------

TEST(ProductionQueueEntryGeneratorTest, EmptyOrdersProducesEmptyResult)
{
    const auto samples = MakeFixtureSamples();
    std::vector<Order> emptyOrders;

    ProductionQueueEntryGenerationOptions options;
    options.seed = 1;

    const auto entries = GenerateProductionQueueEntries(options, emptyOrders, samples);

    EXPECT_TRUE(entries.empty());
}

TEST(ProductionQueueEntryGeneratorTest, EmptySamplesProducesEmptyResult)
{
    const auto orders = MakeFixtureOrders();
    std::vector<Sample> emptySamples;

    ProductionQueueEntryGenerationOptions options;
    options.seed = 1;

    const auto entries = GenerateProductionQueueEntries(options, orders, emptySamples);

    EXPECT_TRUE(entries.empty());
}

TEST(ProductionQueueEntryGeneratorTest, NoProducingOrdersProducesEmptyResult)
{
    const auto samples = MakeFixtureSamples();
    std::vector<Order> orders = {
        Order{ 100, 1, "Alice", 300, OrderState::RESERVED },
        Order{ 101, 2, "Bob", 120, OrderState::CONFIRMED },
        Order{ 102, 3, "Carol", 400, OrderState::RELEASE },
        Order{ 103, 4, "Dave", 50, OrderState::REJECTED },
    };

    ProductionQueueEntryGenerationOptions options;
    options.seed = 1;

    const auto entries = GenerateProductionQueueEntries(options, orders, samples);

    EXPECT_TRUE(entries.empty());
}

// --- State distribution ------------------------------------------------------

TEST(ProductionQueueEntryGeneratorTest, StateDistributionRatiosAreRespectedStatistically)
{
    const auto samples = MakeFixtureSamples();

    // Build 200 PRODUCING orders spread across the fixture samples so every
    // order has a valid stock/yield to derive from.
    std::vector<Order> orders;
    for (int i = 0; i < 200; ++i)
    {
        const int sampleId = (i % 5) + 1;
        orders.push_back(Order{ 1000 + i, sampleId, "Customer", 1000, OrderState::PRODUCING });
    }

    ProductionQueueEntryGenerationOptions options;
    options.seed = 99;
    options.stateDistribution = ProductionStateDistribution{ 0.5, 0.3, 0.2 };

    const auto entries = GenerateProductionQueueEntries(options, orders, samples);
    ASSERT_EQ(entries.size(), 200u);

    std::map<ProductionState, int> counts;
    for (const auto& entry : entries)
    {
        ++counts[entry.state];
    }

    // Ratios are rounded internally, so allow a small tolerance around the
    // expected counts (100 / 60 / 40 out of 200).
    EXPECT_NEAR(counts[ProductionState::WAITING], 100, 3);
    EXPECT_NEAR(counts[ProductionState::PRODUCING], 60, 3);
    EXPECT_NEAR(counts[ProductionState::CONFIRMED], 40, 3);
}
