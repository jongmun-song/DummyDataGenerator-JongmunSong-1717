#pragma once

// Single public entry point for the DummyDataGenerator module.
//
// Other repositories (ConsoleMVC, DataPersistence, SampleOrderSystem, ...)
// should include only this header to pull in every dummy-data generation
// feature this module offers. Each stage of docs/PRD.md section 8 adds its
// generator header here as it is implemented.

#include "DummyDataGenerator/SampleGenerator.h"
#include "DummyDataGenerator/OrderGenerator.h"
#include "DummyDataGenerator/ProductionQueueEntryGenerator.h"

#include "DummyDataGenerator/JsonRepository.h"
#include "DummyDataGenerator/SampleJsonRepository.h"
#include "DummyDataGenerator/OrderJsonRepository.h"
#include "DummyDataGenerator/ProductionQueueEntryJsonRepository.h"
