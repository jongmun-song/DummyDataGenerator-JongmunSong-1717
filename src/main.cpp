// Minimal console demo for DummyDataGenerator. This file is intentionally
// thin: it only calls the public API and prints the result, so the module
// itself stays reusable as a library by other repositories (see CLAUDE.md).

#include <iostream>

#include "DummyDataGenerator.h"

int main()
{
    DummyDataGenerator::SampleGenerationOptions options;
    options.sampleCount = 5;
    options.seed = 42;

    const auto samples = DummyDataGenerator::GenerateSamples(options);

    std::cout << "Generated " << samples.size() << " Sample(s):\n";
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

    const auto orders = DummyDataGenerator::GenerateOrders(orderOptions, samples);

    std::cout << "Generated " << orders.size() << " Order(s):\n";
    for (const auto& order : orders)
    {
        std::cout << "  id=" << order.id
                   << " sampleId=" << order.sampleId
                   << " customerName=" << order.customerName
                   << " orderedQuantity=" << order.orderedQuantity
                   << " state=" << static_cast<int>(order.state)
                   << '\n';
    }

    return 0;
}
