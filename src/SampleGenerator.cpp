#include "DummyDataGenerator/SampleGenerator.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace DummyDataGenerator
{
    namespace
    {
        // Realistic semiconductor sample names, matching the style used in
        // storedData/samples.json (Wafer-8in, GaN-4in, SiC-6in, ...).
        const std::vector<std::string> kSampleNamePool = {
            "Wafer-8in", "GaN-4in", "SiC-6in", "Photo-PR7", "Wafer2-Si02",
            "Wafer-12in", "GaN-6in", "SiC-8in", "Photo-PR9", "Epi-Si4in",
        };

        enum class StockBucket
        {
            Depleted,
            Low,
            Sufficient,
        };

        // Builds the list of stock buckets to assign, one per sample, according
        // to the requested ratios, then shuffles it so the order in which
        // buckets appear is not always "all depleted first".
        std::vector<StockBucket> BuildStockBucketPlan(const SampleGenerationOptions& options, std::mt19937& rng)
        {
            const double depletedRatio = std::clamp(options.depletedStockRatio, 0.0, 1.0);
            const double lowRatio = std::clamp(options.lowStockRatio, 0.0, 1.0 - depletedRatio);

            const int depletedCount = static_cast<int>(std::round(options.sampleCount * depletedRatio));
            const int lowCount = static_cast<int>(std::round(options.sampleCount * lowRatio));
            const int sufficientCount = std::max(0, options.sampleCount - depletedCount - lowCount);

            std::vector<StockBucket> plan;
            plan.reserve(options.sampleCount);
            plan.insert(plan.end(), depletedCount, StockBucket::Depleted);
            plan.insert(plan.end(), lowCount, StockBucket::Low);
            plan.insert(plan.end(), sufficientCount, StockBucket::Sufficient);

            // Ratios may round to fewer/more than sampleCount entries; pad or
            // trim with "Sufficient" so the plan always has exactly sampleCount.
            while (static_cast<int>(plan.size()) < options.sampleCount)
            {
                plan.push_back(StockBucket::Sufficient);
            }
            plan.resize(options.sampleCount);

            std::shuffle(plan.begin(), plan.end(), rng);
            return plan;
        }

        int GenerateStockQuantity(StockBucket bucket, const SampleGenerationOptions& options, std::mt19937& rng)
        {
            switch (bucket)
            {
            case StockBucket::Depleted:
                return 0;
            case StockBucket::Low:
            {
                std::uniform_int_distribution<int> distribution(
                    options.lowStockRange.minValue, options.lowStockRange.maxValue);
                return distribution(rng);
            }
            case StockBucket::Sufficient:
            default:
            {
                std::uniform_int_distribution<int> distribution(
                    options.sufficientStockRange.minValue, options.sufficientStockRange.maxValue);
                return distribution(rng);
            }
            }
        }

        std::string GenerateSampleName(int indexInBatch, std::mt19937& rng)
        {
            std::uniform_int_distribution<std::size_t> distribution(0, kSampleNamePool.size() - 1);
            const std::string& baseName = kSampleNamePool[distribution(rng)];

            // Keep names readable while still being unique enough for a batch
            // larger than the predefined name pool.
            if (static_cast<std::size_t>(indexInBatch) < kSampleNamePool.size())
            {
                return baseName;
            }
            return baseName + "-" + std::to_string(indexInBatch);
        }
    }

    std::vector<DataPersistence::Model::Sample> GenerateSamples(const SampleGenerationOptions& options)
    {
        std::vector<DataPersistence::Model::Sample> samples;
        if (options.sampleCount <= 0)
        {
            return samples;
        }
        samples.reserve(options.sampleCount);

        std::mt19937 rng(options.seed);
        std::uniform_real_distribution<double> productionTimeDistribution(
            options.minAverageProductionTimePerUnit, options.maxAverageProductionTimePerUnit);
        std::uniform_real_distribution<double> yieldRatioDistribution(
            options.minYieldRatio, options.maxYieldRatio);

        const std::vector<StockBucket> stockBucketPlan = BuildStockBucketPlan(options, rng);

        for (int index = 0; index < options.sampleCount; ++index)
        {
            DataPersistence::Model::Sample sample;
            sample.id = options.startId + index;
            sample.name = GenerateSampleName(index, rng);
            sample.averageProductionTimePerUnit = productionTimeDistribution(rng);
            sample.yieldRatio = yieldRatioDistribution(rng);
            sample.stockQuantity = GenerateStockQuantity(stockBucketPlan[index], options, rng);

            samples.push_back(sample);
        }

        return samples;
    }
}
