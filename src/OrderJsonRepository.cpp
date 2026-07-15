#include "DummyDataGenerator/OrderJsonRepository.h"

#include <algorithm>

namespace DummyDataGenerator
{
    namespace
    {
        void AssignNextId(
            DataPersistence::Model::Order& newEntry,
            const std::vector<DataPersistence::Model::Order>& existingEntries)
        {
            int nextId = 1;
            for (const auto& entry : existingEntries)
            {
                nextId = std::max(nextId, entry.id + 1);
            }
            newEntry.id = nextId;
        }
    }

    OrderJsonRepository MakeOrderJsonRepository(std::filesystem::path jsonPath)
    {
        return OrderJsonRepository(std::move(jsonPath), &AssignNextId);
    }
}
