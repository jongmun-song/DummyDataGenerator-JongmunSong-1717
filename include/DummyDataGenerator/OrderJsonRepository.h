#pragma once

// Stage 6 of the DummyDataGenerator PoC: a JsonRepository<Order> (see
// JsonRepository.h) whose create() auto-assigns a fresh id, mirroring
// OrderRepository::create's id-autonumbering
// (../../DataPersistence/OrderRepository.h) and the schema in
// ../../storedData/orders.json.

#include <filesystem>

#include "DummyDataGenerator/JsonRepository.h"
#include "dataModel/Order.h"

namespace DummyDataGenerator
{
    using OrderJsonRepository = JsonRepository<DataPersistence::Model::Order>;

    // Builds an OrderJsonRepository backed by jsonPath. create() ignores
    // whatever id the input Order carries and assigns
    // max(existing ids) + 1 (or 1 if the repository is empty), just like
    // OrderRepository::create. sampleId is left untouched - callers are
    // responsible for pointing it at an already-persisted Sample.id.
    OrderJsonRepository MakeOrderJsonRepository(std::filesystem::path jsonPath);
}
