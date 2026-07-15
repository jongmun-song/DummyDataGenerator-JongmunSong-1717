// Unit tests for DummyDataGenerator::JsonRepository<TModel> and the three
// concrete repository factories (Stage 6 of the DummyDataGenerator PoC).
// See docs/PRD.md's persistence/reproducibility rules and CLAUDE.md's
// "data consistency" section (id auto-numbering vs. natural keys) for the
// domain rules being verified here.
//
// All tests read/write JSON files under a unique subdirectory of
// std::filesystem::temp_directory_path() (never storedData/) and remove
// that directory in TearDown so no test run leaves files behind.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "DummyDataGenerator/JsonRepository.h"
#include "DummyDataGenerator/OrderJsonRepository.h"
#include "DummyDataGenerator/ProductionQueueEntryJsonRepository.h"
#include "DummyDataGenerator/SampleJsonRepository.h"
#include "dataModel/Order.h"
#include "dataModel/ProductionQueueEntry.h"
#include "dataModel/Sample.h"

using DataPersistence::Model::Order;
using DataPersistence::Model::OrderState;
using DataPersistence::Model::ProductionQueueEntry;
using DataPersistence::Model::ProductionState;
using DataPersistence::Model::Sample;
using DummyDataGenerator::MakeOrderJsonRepository;
using DummyDataGenerator::MakeProductionQueueEntryJsonRepository;
using DummyDataGenerator::MakeSampleJsonRepository;

namespace
{
    class JsonRepositoryTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            tempDir_ = std::filesystem::temp_directory_path()
                / ("DummyDataGeneratorTests_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed())
                    + "_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
            std::filesystem::create_directories(tempDir_);
        }

        void TearDown() override
        {
            std::error_code ignored;
            std::filesystem::remove_all(tempDir_, ignored);
        }

        std::filesystem::path PathFor(const std::string& fileName) const
        {
            return tempDir_ / fileName;
        }

    private:
        std::filesystem::path tempDir_;
    };

    Sample MakeSample(int id, const std::string& name)
    {
        Sample sample;
        sample.id = id;
        sample.name = name;
        sample.averageProductionTimePerUnit = 0.5;
        sample.yieldRatio = 0.9;
        sample.stockQuantity = 100;
        return sample;
    }

    Order MakeOrder(int id, int sampleId, const std::string& customerName)
    {
        Order order;
        order.id = id;
        order.sampleId = sampleId;
        order.customerName = customerName;
        order.orderedQuantity = 50;
        order.state = OrderState::RESERVED;
        return order;
    }

    ProductionQueueEntry MakeEntry(int orderId, int sampleId)
    {
        ProductionQueueEntry entry;
        entry.orderId = orderId;
        entry.sampleId = sampleId;
        entry.orderedQuantity = 50;
        entry.shortageQuantity = 30;
        entry.actualProductionQuantity = 34;
        entry.totalProductionTime = 17.0;
        entry.state = ProductionState::WAITING;
        return entry;
    }
}

// --- Missing file -> empty collection ------------------------------------

TEST_F(JsonRepositoryTest, LoadWithMissingFileYieldsEmptyCollection)
{
    auto repository = MakeSampleJsonRepository(PathFor("missing_samples.json"));

    ASSERT_FALSE(std::filesystem::exists(PathFor("missing_samples.json")));
    EXPECT_NO_THROW(repository.load());
    EXPECT_TRUE(repository.all().empty());
}

TEST_F(JsonRepositoryTest, LoadWithMissingFileYieldsEmptyCollection_Order)
{
    auto repository = MakeOrderJsonRepository(PathFor("missing_orders.json"));

    repository.load();
    EXPECT_TRUE(repository.all().empty());
}

TEST_F(JsonRepositoryTest, LoadWithMissingFileYieldsEmptyCollection_ProductionQueueEntry)
{
    auto repository = MakeProductionQueueEntryJsonRepository(PathFor("missing_queue.json"));

    repository.load();
    EXPECT_TRUE(repository.all().empty());
}

// --- create() writes a file with the storedData/*.json schema -----------

TEST_F(JsonRepositoryTest, CreateSampleWritesFileWithExpectedSchema)
{
    const auto path = PathFor("samples.json");
    auto repository = MakeSampleJsonRepository(path);
    repository.load();

    repository.create(MakeSample(0, "Wafer-8in"));

    ASSERT_TRUE(std::filesystem::exists(path));

    std::ifstream input(path);
    nlohmann::json document;
    input >> document;

    ASSERT_TRUE(document.is_array());
    ASSERT_EQ(document.size(), 1u);
    const auto& first = document.at(0);
    EXPECT_TRUE(first.contains("id"));
    EXPECT_TRUE(first.contains("name"));
    EXPECT_TRUE(first.contains("averageProductionTimePerUnit"));
    EXPECT_TRUE(first.contains("yieldRatio"));
    EXPECT_TRUE(first.contains("stockQuantity"));
}

TEST_F(JsonRepositoryTest, CreateOrderWritesFileWithExpectedSchema)
{
    const auto path = PathFor("orders.json");
    auto repository = MakeOrderJsonRepository(path);
    repository.load();

    repository.create(MakeOrder(0, 1, "LG"));

    std::ifstream input(path);
    nlohmann::json document;
    input >> document;

    ASSERT_EQ(document.size(), 1u);
    const auto& first = document.at(0);
    EXPECT_TRUE(first.contains("id"));
    EXPECT_TRUE(first.contains("sampleId"));
    EXPECT_TRUE(first.contains("customerName"));
    EXPECT_TRUE(first.contains("orderedQuantity"));
    EXPECT_TRUE(first.contains("state"));
    EXPECT_EQ(first.at("state").get<std::string>(), "RESERVED");
}

TEST_F(JsonRepositoryTest, CreateProductionQueueEntryWritesFileWithExpectedSchema)
{
    const auto path = PathFor("production_queue.json");
    auto repository = MakeProductionQueueEntryJsonRepository(path);
    repository.load();

    repository.create(MakeEntry(1, 1));

    std::ifstream input(path);
    nlohmann::json document;
    input >> document;

    ASSERT_EQ(document.size(), 1u);
    const auto& first = document.at(0);
    EXPECT_TRUE(first.contains("orderId"));
    EXPECT_TRUE(first.contains("sampleId"));
    EXPECT_TRUE(first.contains("orderedQuantity"));
    EXPECT_TRUE(first.contains("shortageQuantity"));
    EXPECT_TRUE(first.contains("actualProductionQuantity"));
    EXPECT_TRUE(first.contains("totalProductionTime"));
    EXPECT_TRUE(first.contains("state"));
}

// --- Sample/Order: create() auto-assigns ids, ignoring caller input -----

TEST_F(JsonRepositoryTest, SampleCreateAutoNumbersIdsIgnoringInput)
{
    auto repository = MakeSampleJsonRepository(PathFor("samples.json"));
    repository.load();

    const auto first = repository.create(MakeSample(999, "First"));
    EXPECT_EQ(first.id, 1);

    const auto second = repository.create(MakeSample(999, "Second"));
    EXPECT_EQ(second.id, 2);

    const auto third = repository.create(MakeSample(-5, "Third"));
    EXPECT_EQ(third.id, 3);
}

TEST_F(JsonRepositoryTest, OrderCreateAutoNumbersIdsIgnoringInput)
{
    auto repository = MakeOrderJsonRepository(PathFor("orders.json"));
    repository.load();

    const auto first = repository.create(MakeOrder(999, 1, "LG"));
    EXPECT_EQ(first.id, 1);

    const auto second = repository.create(MakeOrder(42, 1, "SK"));
    EXPECT_EQ(second.id, 2);
}

// --- ProductionQueueEntry: natural key is preserved as-is ---------------

TEST_F(JsonRepositoryTest, ProductionQueueEntryCreateKeepsCallerSuppliedOrderIdAndSampleId)
{
    auto repository = MakeProductionQueueEntryJsonRepository(PathFor("production_queue.json"));
    repository.load();

    const auto entry = repository.create(MakeEntry(7, 3));

    EXPECT_EQ(entry.orderId, 7);
    EXPECT_EQ(entry.sampleId, 3);

    // A second create() with a different natural key must not be renumbered
    // either - it is not autonumbered at all.
    const auto entry2 = repository.create(MakeEntry(2, 9));
    EXPECT_EQ(entry2.orderId, 2);
    EXPECT_EQ(entry2.sampleId, 9);
}

// --- Persistence across "restart" (reload from disk) ---------------------

TEST_F(JsonRepositoryTest, ReloadingAfterMultipleCreatesReproducesAllEntriesInOrder)
{
    const auto path = PathFor("samples.json");

    {
        auto repository = MakeSampleJsonRepository(path);
        repository.load();
        repository.create(MakeSample(0, "Alpha"));
        repository.create(MakeSample(0, "Beta"));
        repository.create(MakeSample(0, "Gamma"));
    }

    // Simulate a fresh process by constructing a brand-new repository
    // instance over the same file and loading it.
    auto reopened = MakeSampleJsonRepository(path);
    reopened.load();

    const auto& all = reopened.all();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].id, 1);
    EXPECT_EQ(all[0].name, "Alpha");
    EXPECT_EQ(all[1].id, 2);
    EXPECT_EQ(all[1].name, "Beta");
    EXPECT_EQ(all[2].id, 3);
    EXPECT_EQ(all[2].name, "Gamma");
}

// --- Sequential create() calls append and accumulate in all() -----------

TEST_F(JsonRepositoryTest, SequentialCreatesAppendToAll)
{
    auto repository = MakeOrderJsonRepository(PathFor("orders.json"));
    repository.load();

    EXPECT_TRUE(repository.all().empty());

    repository.create(MakeOrder(0, 1, "LG"));
    EXPECT_EQ(repository.all().size(), 1u);

    repository.create(MakeOrder(0, 2, "SK"));
    EXPECT_EQ(repository.all().size(), 2u);

    repository.create(MakeOrder(0, 3, "Samsung"));
    ASSERT_EQ(repository.all().size(), 3u);
    EXPECT_EQ(repository.all()[0].customerName, "LG");
    EXPECT_EQ(repository.all()[1].customerName, "SK");
    EXPECT_EQ(repository.all()[2].customerName, "Samsung");
}

// --- PersistAll helper ----------------------------------------------------

TEST_F(JsonRepositoryTest, PersistAllAssignsKeysAndPersistsEveryEntry)
{
    auto repository = MakeSampleJsonRepository(PathFor("samples.json"));

    std::vector<Sample> generated = {
        MakeSample(0, "One"),
        MakeSample(0, "Two"),
        MakeSample(0, "Three"),
    };

    const auto persisted = DummyDataGenerator::PersistAll(repository, generated);

    ASSERT_EQ(persisted.size(), 3u);
    EXPECT_EQ(persisted[0].id, 1);
    EXPECT_EQ(persisted[1].id, 2);
    EXPECT_EQ(persisted[2].id, 3);
    EXPECT_EQ(repository.all().size(), 3u);
}

// --- Failure scenario: save() to a non-existent parent directory --------
//
// std::ofstream simply fails to open (no directory auto-creation), so
// save()/create() should throw and, per create()'s documented rollback
// behavior, the in-memory entry list must not retain the failed append.
TEST_F(JsonRepositoryTest, CreateWithUnwritablePathThrowsAndRollsBackInMemoryState)
{
    const auto path = PathFor("no_such_subdir") / "samples.json";
    auto repository = MakeSampleJsonRepository(path);
    repository.load();

    EXPECT_THROW(repository.create(MakeSample(0, "Orphan")), std::exception);
    EXPECT_TRUE(repository.all().empty());
}
