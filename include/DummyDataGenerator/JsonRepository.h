#pragma once

// Stage 6 of the DummyDataGenerator PoC: a JSON-file-backed repository that
// mirrors DataPersistence's SampleRepository/OrderRepository/
// ProductionQueueEntryRepository pattern (see
// ../../DataPersistence/docs/PRE.md, ../../DataPersistence/SampleRepository.h)
// so generated dummy data can actually be added to the "connected DB"
// (requirements.pdf 25p [mission 1]: "Dummy Data는 연결된 DB에 추가"), not just
// returned as in-memory values.
//
// load() reads the whole collection into memory (missing file -> empty
// collection, matching SampleRepository's documented behavior); save()
// writes it back atomically via a temp-file-then-rename, so a crash mid-write
// never corrupts the previously-saved file; create() appends one new entry,
// assigns its key, persists, and rolls back the in-memory append if save()
// throws.
//
// DataPersistence::Model::Sample/Order/ProductionQueueEntry differ only in
// how a newly created entry's key is assigned: Sample/Order auto-assign a
// fresh `id` (ignoring whatever the caller put there), while
// ProductionQueueEntry keeps its caller-supplied `orderId` as-is because it
// is a natural key referencing an already-created Order (see
// dataModel/ProductionQueueEntry.h). Rather than duplicating load/save/
// create three times, that single difference is isolated behind the
// `KeyAssigner` policy function injected at construction (see
// SampleJsonRepository.h / OrderJsonRepository.h /
// ProductionQueueEntryJsonRepository.h for the three concrete policies).

#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace DummyDataGenerator
{
    template <typename TModel>
    class JsonRepository
    {
    public:
        // Assigns whatever key field TModel uses (id, orderId, ...) on
        // newEntry, given the entries already on record. Sample/Order pass
        // an auto-incrementing policy; ProductionQueueEntry passes a no-op
        // (its orderId is a natural key, not autonumbered).
        using KeyAssigner = std::function<void(TModel& newEntry, const std::vector<TModel>& existingEntries)>;

        JsonRepository(std::filesystem::path jsonPath, KeyAssigner assignKey)
            : jsonPath_(std::move(jsonPath))
            , assignKey_(std::move(assignKey))
        {
        }

        // Reads jsonPath_ into memory. A missing file is not an error - the
        // repository simply starts out empty. A malformed file's parse
        // exception propagates to the caller.
        void load()
        {
            entries_.clear();
            if (!std::filesystem::exists(jsonPath_))
            {
                return;
            }

            std::ifstream input(jsonPath_);
            if (!input)
            {
                throw std::runtime_error("JsonRepository: failed to open " + jsonPath_.string());
            }

            nlohmann::json document;
            input >> document;
            entries_ = document.get<std::vector<TModel>>();
        }

        // Writes entries_ to jsonPath_ atomically: serialize to a temp file
        // first, then rename it over the real path. If either step fails,
        // the previously-saved file is left untouched and an exception
        // propagates to the caller.
        void save() const
        {
            const std::filesystem::path tempPath = jsonPath_.string() + ".tmp";

            {
                std::ofstream output(tempPath, std::ios::trunc);
                if (!output)
                {
                    throw std::runtime_error("JsonRepository: failed to open temp file " + tempPath.string());
                }

                output << nlohmann::json(entries_).dump(4);
                if (!output)
                {
                    throw std::runtime_error("JsonRepository: failed to write temp file " + tempPath.string());
                }
            }

            std::error_code renameError;
            std::filesystem::rename(tempPath, jsonPath_, renameError);
            if (renameError)
            {
                std::filesystem::remove(tempPath);
                throw std::runtime_error(
                    "JsonRepository: failed to replace " + jsonPath_.string() + ": " + renameError.message());
            }
        }

        const std::vector<TModel>& all() const
        {
            return entries_;
        }

        // Appends a copy of `input` (with its key assigned per assignKey_)
        // and persists the whole collection. If save() throws, the append
        // is rolled back so in-memory state never diverges from disk.
        TModel create(const TModel& input)
        {
            TModel newEntry = input;
            assignKey_(newEntry, entries_);

            entries_.push_back(newEntry);
            try
            {
                save();
            }
            catch (...)
            {
                entries_.pop_back();
                throw;
            }

            return newEntry;
        }

    private:
        std::filesystem::path jsonPath_;
        KeyAssigner assignKey_;
        std::vector<TModel> entries_;
    };

    // Convenience for the common "generate then add to the connected DB"
    // scenario (requirements.pdf 25p [mission 1]): loads whatever is
    // already on disk, appends each of `generatedEntries` via create() (so
    // each entry's key is assigned/persisted exactly as a real Create call
    // would), and returns the persisted entries (with their final,
    // repository-assigned keys) so callers can feed them into the next
    // generation stage.
    template <typename TModel>
    std::vector<TModel> PersistAll(JsonRepository<TModel>& repository, const std::vector<TModel>& generatedEntries)
    {
        repository.load();

        std::vector<TModel> persisted;
        persisted.reserve(generatedEntries.size());
        for (const auto& entry : generatedEntries)
        {
            persisted.push_back(repository.create(entry));
        }

        return persisted;
    }
}
