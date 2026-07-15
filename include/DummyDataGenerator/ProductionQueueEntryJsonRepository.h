#pragma once

// Stage 6 of the DummyDataGenerator PoC: a JsonRepository<ProductionQueueEntry>
// (see JsonRepository.h) matching the schema in
// ../../storedData/production_queue.json.
//
// Unlike SampleJsonRepository/OrderJsonRepository, create() here does NOT
// autonumber anything: ProductionQueueEntry::orderId is a natural key that
// must already reference an existing, persisted Order.id (see
// ../../dataModel/ProductionQueueEntry.h and CLAUDE.md's "data consistency"
// section), so the caller-supplied orderId/sampleId are kept exactly as
// given.

#include <filesystem>

#include "DummyDataGenerator/JsonRepository.h"
#include "dataModel/ProductionQueueEntry.h"

namespace DummyDataGenerator
{
    using ProductionQueueEntryJsonRepository = JsonRepository<DataPersistence::Model::ProductionQueueEntry>;

    // Builds a ProductionQueueEntryJsonRepository backed by jsonPath.
    // create() leaves orderId/sampleId untouched (natural key), unlike
    // SampleJsonRepository/OrderJsonRepository's auto-incrementing id.
    ProductionQueueEntryJsonRepository MakeProductionQueueEntryJsonRepository(std::filesystem::path jsonPath);
}
