#include "DummyDataGenerator/ProductionQueueEntryJsonRepository.h"

namespace DummyDataGenerator
{
    namespace
    {
        // No-op: orderId is a natural key that must already reference an
        // existing, persisted Order.id, so create() must not reassign it
        // the way Sample/Order auto-increment their id.
        void KeepNaturalKey(
            DataPersistence::Model::ProductionQueueEntry&,
            const std::vector<DataPersistence::Model::ProductionQueueEntry>&)
        {
        }
    }

    ProductionQueueEntryJsonRepository MakeProductionQueueEntryJsonRepository(std::filesystem::path jsonPath)
    {
        return ProductionQueueEntryJsonRepository(std::move(jsonPath), &KeepNaturalKey);
    }
}
