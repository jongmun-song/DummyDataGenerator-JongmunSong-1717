#pragma once

// Data model for JSON persistence (see docs/PRE.md, docs/feature/json-parsing.md).
//
// Fields mirror ConsoleMVC's PoC verification example
// (ConsoleMVC/Example/Model/Sample.h), which models requirements.pdf
// Chapter 2's sample data: an identity, descriptive attributes, and a
// stock quantity. Unlike that example, this type carries no business-rule
// guard (e.g. QuantityGuard) - it is a plain data-transfer struct whose
// only responsibility is to serialize to/from JSON via the repository
// (see docs/feature/json-file-storage.md). Stock-quantity invariants are
// the consuming project's responsibility (e.g. SampleOrderSystem).

#include <string>

#include <nlohmann/json.hpp>

namespace DataPersistence::Model
{
    struct Sample
    {
        int id = 0;
        std::string name;
        double averageProductionTimePerUnit = 0.0;
        double yieldRatio = 0.0;
        int stockQuantity = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Sample, id, name, averageProductionTimePerUnit, yieldRatio, stockQuantity)
    };
}
