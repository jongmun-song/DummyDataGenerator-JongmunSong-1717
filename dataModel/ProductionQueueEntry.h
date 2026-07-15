#pragma once

// Data model for JSON persistence (see docs/PRE.md, docs/feature/json-parsing.md).
//
// Fields mirror ConsoleMVC's PoC verification example
// (ConsoleMVC/Example/Model/ProductionQueueEntry.h), which models
// requirements.pdf Chapter 2's production step: an order's shortage turned
// into an actual production quantity and total production time via the
// covering sample's yield ratio and average production time. Unlike that
// example, this type carries no derivation logic or state-machine
// enforcement (e.g. CalculateActualProductionQuantity,
// StatefulModel<ProductionState>::TryTransition) - it is a plain
// data-transfer struct whose only responsibility is to serialize to/from
// JSON via the repository (see docs/feature/json-file-storage.md).
// Deriving/validating these fields is the consuming project's
// responsibility (e.g. SampleOrderSystem).
//
// ProductionState adds a WAITING value (default for a newly enqueued
// entry) ahead of PRODUCING/CONFIRMED, matching requirements.pdf's FIFO
// production queue ("대기 주문 확인"): an entry waits in the queue before
// the production line actually starts working on it.

#include <nlohmann/json.hpp>

namespace DataPersistence::Model
{
    enum class ProductionState
    {
        WAITING,
        PRODUCING,
        CONFIRMED
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(ProductionState, {
        { ProductionState::WAITING, "WAITING" },
        { ProductionState::PRODUCING, "PRODUCING" },
        { ProductionState::CONFIRMED, "CONFIRMED" },
    })

    struct ProductionQueueEntry
    {
        int orderId = 0;
        int sampleId = 0;
        int orderedQuantity = 0;
        int shortageQuantity = 0;
        int actualProductionQuantity = 0;
        double totalProductionTime = 0.0;
        ProductionState state = ProductionState::WAITING;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(ProductionQueueEntry, orderId, sampleId, orderedQuantity,
            shortageQuantity, actualProductionQuantity, totalProductionTime, state)
    };
}
