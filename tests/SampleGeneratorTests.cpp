// Unit tests for DummyDataGenerator::GenerateSamples (Stage 1 of the
// DummyDataGenerator PoC). See docs/PRD.md section 6.1/7.1 for the domain
// rules being verified here (stock buckets, reproducibility) and
// ../CLAUDE.md for the reusability requirements (seed reproducibility,
// parameterized ranges/distributions).

#include <gtest/gtest.h>

#include <algorithm>

#include "DummyDataGenerator/SampleGenerator.h"

using DummyDataGenerator::GenerateSamples;
using DummyDataGenerator::SampleGenerationOptions;
using DummyDataGenerator::StockQuantityRange;

namespace
{
    bool SamplesEqual(const DataPersistence::Model::Sample& lhs, const DataPersistence::Model::Sample& rhs)
    {
        return lhs.id == rhs.id
            && lhs.name == rhs.name
            && lhs.averageProductionTimePerUnit == rhs.averageProductionTimePerUnit
            && lhs.yieldRatio == rhs.yieldRatio
            && lhs.stockQuantity == rhs.stockQuantity;
    }
}

// --- Reproducibility ---------------------------------------------------

TEST(SampleGeneratorTest, SameSeedProducesIdenticalResults)
{
    SampleGenerationOptions options;
    options.sampleCount = 20;
    options.startId = 5;
    options.seed = 12345;

    const auto first = GenerateSamples(options);
    const auto second = GenerateSamples(options);

    ASSERT_EQ(first.size(), second.size());
    for (std::size_t i = 0; i < first.size(); ++i)
    {
        EXPECT_TRUE(SamplesEqual(first[i], second[i]))
            << "Mismatch at index " << i;
    }
}

TEST(SampleGeneratorTest, DifferentSeedsTypicallyProduceDifferentResults)
{
    SampleGenerationOptions optionsA;
    optionsA.sampleCount = 20;
    optionsA.seed = 1;

    SampleGenerationOptions optionsB = optionsA;
    optionsB.seed = 2;

    const auto samplesA = GenerateSamples(optionsA);
    const auto samplesB = GenerateSamples(optionsB);

    ASSERT_EQ(samplesA.size(), samplesB.size());

    bool anyDifference = false;
    for (std::size_t i = 0; i < samplesA.size(); ++i)
    {
        if (!SamplesEqual(samplesA[i], samplesB[i]))
        {
            anyDifference = true;
            break;
        }
    }
    EXPECT_TRUE(anyDifference);
}

// --- Count / id sequencing ----------------------------------------------

TEST(SampleGeneratorTest, GeneratesExactlyRequestedCountWithConsecutiveIds)
{
    SampleGenerationOptions options;
    options.sampleCount = 10;
    options.startId = 100;
    options.seed = 7;

    const auto samples = GenerateSamples(options);

    ASSERT_EQ(samples.size(), 10u);
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        EXPECT_EQ(samples[i].id, options.startId + static_cast<int>(i));
    }
}

TEST(SampleGeneratorTest, SingleSampleBoundaryWorks)
{
    SampleGenerationOptions options;
    options.sampleCount = 1;
    options.startId = 42;
    options.seed = 1;

    const auto samples = GenerateSamples(options);

    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].id, 42);
}

TEST(SampleGeneratorTest, ZeroSampleCountProducesEmptyResult)
{
    SampleGenerationOptions options;
    options.sampleCount = 0;
    options.seed = 1;

    const auto samples = GenerateSamples(options);

    EXPECT_TRUE(samples.empty());
}

// --- Stock bucket distribution / range consistency -----------------------

TEST(SampleGeneratorTest, StockBucketRatiosAreRespectedStatisticallyAndPerRange)
{
    SampleGenerationOptions options;
    options.sampleCount = 100;
    options.seed = 99;
    options.depletedStockRatio = 0.3;
    options.lowStockRatio = 0.3;
    options.lowStockRange = { 1, 50 };
    options.sufficientStockRange = { 100, 1000 };

    const auto samples = GenerateSamples(options);
    ASSERT_EQ(samples.size(), 100u);

    int depletedCount = 0;
    int lowCount = 0;
    int sufficientCount = 0;

    for (const auto& sample : samples)
    {
        if (sample.stockQuantity == 0)
        {
            ++depletedCount;
        }
        else if (sample.stockQuantity >= options.lowStockRange.minValue
            && sample.stockQuantity <= options.lowStockRange.maxValue)
        {
            ++lowCount;
        }
        else
        {
            ASSERT_GE(sample.stockQuantity, options.sufficientStockRange.minValue);
            ASSERT_LE(sample.stockQuantity, options.sufficientStockRange.maxValue);
            ++sufficientCount;
        }
    }

    // Ratios are rounded per-bucket internally, so allow a small tolerance
    // around the expected counts (30 / 30 / 40 out of 100).
    EXPECT_NEAR(depletedCount, 30, 2);
    EXPECT_NEAR(lowCount, 30, 2);
    EXPECT_NEAR(sufficientCount, 40, 2);
    EXPECT_EQ(depletedCount + lowCount + sufficientCount, 100);
}

TEST(SampleGeneratorTest, ZeroRatiosProduceOnlySufficientStock)
{
    SampleGenerationOptions options;
    options.sampleCount = 30;
    options.seed = 3;
    options.depletedStockRatio = 0.0;
    options.lowStockRatio = 0.0;
    options.sufficientStockRange = { 100, 1000 };

    const auto samples = GenerateSamples(options);

    ASSERT_EQ(samples.size(), 30u);
    for (const auto& sample : samples)
    {
        EXPECT_GE(sample.stockQuantity, options.sufficientStockRange.minValue);
        EXPECT_LE(sample.stockQuantity, options.sufficientStockRange.maxValue);
    }
}

TEST(SampleGeneratorTest, FullDepletedRatioProducesAllZeroStock)
{
    SampleGenerationOptions options;
    options.sampleCount = 30;
    options.seed = 4;
    options.depletedStockRatio = 1.0;
    options.lowStockRatio = 1.0; // clamped to 0 because depletedStockRatio already consumes the full ratio

    const auto samples = GenerateSamples(options);

    ASSERT_EQ(samples.size(), 30u);
    for (const auto& sample : samples)
    {
        EXPECT_EQ(sample.stockQuantity, 0);
    }
}

// --- Field range consistency ---------------------------------------------

TEST(SampleGeneratorTest, YieldRatioAndProductionTimeStayWithinRequestedRange)
{
    SampleGenerationOptions options;
    options.sampleCount = 50;
    options.seed = 11;
    options.minAverageProductionTimePerUnit = 0.5;
    options.maxAverageProductionTimePerUnit = 0.8;
    options.minYieldRatio = 0.75;
    options.maxYieldRatio = 0.9;

    const auto samples = GenerateSamples(options);

    ASSERT_EQ(samples.size(), 50u);
    for (const auto& sample : samples)
    {
        EXPECT_GE(sample.averageProductionTimePerUnit, options.minAverageProductionTimePerUnit);
        EXPECT_LE(sample.averageProductionTimePerUnit, options.maxAverageProductionTimePerUnit);
        EXPECT_GE(sample.yieldRatio, options.minYieldRatio);
        EXPECT_LE(sample.yieldRatio, options.maxYieldRatio);
    }
}
