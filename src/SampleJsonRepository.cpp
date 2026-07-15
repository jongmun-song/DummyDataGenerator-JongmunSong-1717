#include "DummyDataGenerator/SampleJsonRepository.h"

#include <algorithm>

namespace DummyDataGenerator
{
    namespace
    {
        void AssignNextId(
            DataPersistence::Model::Sample& newEntry,
            const std::vector<DataPersistence::Model::Sample>& existingEntries)
        {
            int nextId = 1;
            for (const auto& entry : existingEntries)
            {
                nextId = std::max(nextId, entry.id + 1);
            }
            newEntry.id = nextId;
        }
    }

    SampleJsonRepository MakeSampleJsonRepository(std::filesystem::path jsonPath)
    {
        return SampleJsonRepository(std::move(jsonPath), &AssignNextId);
    }
}
