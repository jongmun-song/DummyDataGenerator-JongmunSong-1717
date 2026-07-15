#pragma once

// Data model for JSON persistence (see docs/PRE.md, docs/feature/json-parsing.md).
//
// Fields mirror ConsoleMVC's PoC verification example
// (ConsoleMVC/Example/Model/Order.h), which models requirements.pdf
// Chapter 2's order data and its RESERVED/CONFIRMED/PRODUCING/RELEASE/
// REJECTED state. Unlike that example, this type carries no state-machine
// enforcement (e.g. StatefulModel<OrderState>::TryTransition) - it is a
// plain data-transfer struct whose only responsibility is to serialize
// to/from JSON via the repository (see docs/feature/json-file-storage.md).
// Transition validity is the consuming project's responsibility (e.g.
// SampleOrderSystem).

#include <string>

#include <nlohmann/json.hpp>

namespace DataPersistence::Model
{
    enum class OrderState
    {
        RESERVED,
        CONFIRMED,
        PRODUCING,
        RELEASE,
        REJECTED
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(OrderState, {
        { OrderState::RESERVED, "RESERVED" },
        { OrderState::CONFIRMED, "CONFIRMED" },
        { OrderState::PRODUCING, "PRODUCING" },
        { OrderState::RELEASE, "RELEASE" },
        { OrderState::REJECTED, "REJECTED" },
    })

    struct Order
    {
        int id = 0;
        int sampleId = 0;
        std::string customerName;
        int orderedQuantity = 0;
        OrderState state = OrderState::RESERVED;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Order, id, sampleId, customerName, orderedQuantity, state)
    };
}
