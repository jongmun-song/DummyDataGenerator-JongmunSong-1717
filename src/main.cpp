// Minimal console demo for DummyDataGenerator. This file is intentionally
// thin: it only calls the public API and prints the result, so the module
// itself stays reusable as a library by other repositories (see CLAUDE.md).
//
// Stage 6 (see docs/PRD.md section 8) is demonstrated here by actually
// persisting the generated Sample/Order/ProductionQueueEntry lists to JSON
// files via the *JsonRepository create() path - not just printing values.
// The demo writes to output/ (relative to the working directory), never to
// storedData/, so the checked-in example files stay untouched as the
// target-schema reference they are meant to be.

#include <filesystem>
#include <iostream>

#include "DummyDataGenerator.h"

int main()
{
    // Start each demo run from a clean output/ directory so the repository
    // ids assigned below are reproducible run-to-run.
    const std::filesystem::path outputDirectory = "output";
    std::filesystem::remove_all(outputDirectory);
    std::filesystem::create_directories(outputDirectory);

    DummyDataGenerator::SampleGenerationOptions sampleOptions;
    sampleOptions.sampleCount = 5;
    sampleOptions.seed = 42;

    const auto generatedSamples = DummyDataGenerator::GenerateSamples(sampleOptions);

    auto sampleRepository = DummyDataGenerator::MakeSampleJsonRepository(outputDirectory / "samples.json");
    const auto samples = DummyDataGenerator::PersistAll(sampleRepository, generatedSamples);

    std::cout << "Persisted " << samples.size() << " Sample(s) to " << (outputDirectory / "samples.json") << ":\n";
    for (const auto& sample : samples)
    {
        std::cout << "  id=" << sample.id
                   << " name=" << sample.name
                   << " avgProdTime=" << sample.averageProductionTimePerUnit
                   << " yieldRatio=" << sample.yieldRatio
                   << " stockQuantity=" << sample.stockQuantity
                   << '\n';
    }

    DummyDataGenerator::OrderGenerationOptions orderOptions;
    orderOptions.orderCount = 5;
    orderOptions.seed = 42;

    const auto generatedOrders = DummyDataGenerator::GenerateOrders(orderOptions, samples);

    auto orderRepository = DummyDataGenerator::MakeOrderJsonRepository(outputDirectory / "orders.json");
    const auto orders = DummyDataGenerator::PersistAll(orderRepository, generatedOrders);

    std::cout << "Persisted " << orders.size() << " Order(s) to " << (outputDirectory / "orders.json") << ":\n";
    for (const auto& order : orders)
    {
        std::cout << "  id=" << order.id
                   << " sampleId=" << order.sampleId
                   << " customerName=" << order.customerName
                   << " orderedQuantity=" << order.orderedQuantity
                   << " state=" << static_cast<int>(order.state)
                   << '\n';
    }

    DummyDataGenerator::ProductionQueueEntryGenerationOptions queueOptions;
    queueOptions.seed = 42;

    const auto generatedQueueEntries = DummyDataGenerator::GenerateProductionQueueEntries(queueOptions, orders, samples);

    auto queueRepository =
        DummyDataGenerator::MakeProductionQueueEntryJsonRepository(outputDirectory / "production_queue.json");
    const auto queueEntries = DummyDataGenerator::PersistAll(queueRepository, generatedQueueEntries);

    std::cout << "Persisted " << queueEntries.size() << " ProductionQueueEntry(ies) to "
              << (outputDirectory / "production_queue.json") << ":\n";
    for (const auto& entry : queueEntries)
    {
        std::cout << "  orderId=" << entry.orderId
                   << " sampleId=" << entry.sampleId
                   << " orderedQuantity=" << entry.orderedQuantity
                   << " shortageQuantity=" << entry.shortageQuantity
                   << " actualProductionQuantity=" << entry.actualProductionQuantity
                   << " totalProductionTime=" << entry.totalProductionTime
                   << " state=" << static_cast<int>(entry.state)
                   << '\n';
    }

    return 0;
}
