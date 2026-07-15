#pragma once

// Stage 6 of the DummyDataGenerator PoC: a JsonRepository<Sample> (see
// JsonRepository.h) whose create() auto-assigns a fresh id, mirroring
// SampleRepository::create's id-autonumbering
// (../../DataPersistence/SampleRepository.h) and the schema in
// ../../storedData/samples.json.

#include <filesystem>

#include "DummyDataGenerator/JsonRepository.h"
#include "dataModel/Sample.h"

namespace DummyDataGenerator
{
    using SampleJsonRepository = JsonRepository<DataPersistence::Model::Sample>;

    // Builds a SampleJsonRepository backed by jsonPath. create() ignores
    // whatever id the input Sample carries and assigns
    // max(existing ids) + 1 (or 1 if the repository is empty), just like
    // SampleRepository::create.
    SampleJsonRepository MakeSampleJsonRepository(std::filesystem::path jsonPath);
}
