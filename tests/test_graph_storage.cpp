#include "app/graphing/graph_storage.h"
#include "platform/host/axiom_fs_host.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace {

std::filesystem::path graphStorageRoot() {
    return std::filesystem::temp_directory_path() / "mi23_graph_storage_tests";
}

GraphSessionData sampleSession() {
    GraphSessionData session;
    session.name = "Parabola";
    session.functions.push_back({"x^2", true});
    session.functions.push_back({"sin(x)", false});
    session.window = {-10.0, 10.0, -8.0, 12.0, 1.0, 1.0};
    session.angleRadians = true;
    return session;
}

} // namespace

TEST(GraphSessionStorage, SavesAndLoadsReadableGraphSession) {
    const std::filesystem::path root = graphStorageRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(AxiomFS::initialize(fs).status, AxiomFS::FilesystemStatus::Healthy);

    std::string savedName;
    ASSERT_TRUE(GraphSessionStorage::save(fs, "Parabola", sampleSession(), &savedName));
    EXPECT_EQ(savedName, "Parabola.mi23graph");

    const AxiomFS::ReadResult raw = fs.readFile("graphs/Parabola.mi23graph");
    ASSERT_TRUE(raw.ok());
    const std::string text(raw.data.begin(), raw.data.end());
    EXPECT_NE(text.find("\"version\": 1"), std::string::npos);
    EXPECT_NE(text.find("\"expression\": \"x^2\""), std::string::npos);

    GraphSessionData loaded;
    ASSERT_TRUE(GraphSessionStorage::load(fs, savedName, loaded));
    ASSERT_EQ(loaded.functions.size(), 2u);
    EXPECT_EQ(loaded.name, "Parabola");
    EXPECT_EQ(loaded.functions[0].expression, "x^2");
    EXPECT_TRUE(loaded.functions[0].enabled);
    EXPECT_EQ(loaded.window.yMax, 12.0);

    std::filesystem::remove_all(root, error);
}

TEST(GraphSessionStorage, SaveCreatesMissingGraphsDirectory) {
    const std::filesystem::path root = graphStorageRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(fs.mount(), AxiomFS::Status::Ok);

    ASSERT_TRUE(GraphSessionStorage::save(fs, "NoFolder", sampleSession()));
    EXPECT_TRUE(std::filesystem::is_directory(root / "graphs"));

    std::filesystem::remove_all(root, error);
}

TEST(GraphSessionStorage, SanitizesFilenamesInsideGraphsDirectory) {
    EXPECT_EQ(GraphSessionStorage::sanitizeFileName("../Bad Graph!?"),
              "Bad_Graph.mi23graph");
    EXPECT_EQ(GraphSessionStorage::graphPath("../Bad Graph!?"),
              "graphs/Bad_Graph.mi23graph");
}

TEST(GraphSessionStorage, DuplicateNamesAutoIncrement) {
    const std::filesystem::path root = graphStorageRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(AxiomFS::initialize(fs).status, AxiomFS::FilesystemStatus::Healthy);

    std::string first;
    std::string second;
    ASSERT_TRUE(GraphSessionStorage::save(fs, "Repeat", sampleSession(), &first));
    ASSERT_TRUE(GraphSessionStorage::save(fs, "Repeat", sampleSession(), &second));
    EXPECT_EQ(first, "Repeat.mi23graph");
    EXPECT_EQ(second, "Repeat_001.mi23graph");

    std::filesystem::remove_all(root, error);
}

TEST(GraphSessionStorage, CorruptedGraphFileFailsGracefully) {
    const std::filesystem::path root = graphStorageRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(AxiomFS::initialize(fs).status, AxiomFS::FilesystemStatus::Healthy);
    ASSERT_EQ(fs.writeFile("graphs/bad.mi23graph", "not json"), AxiomFS::Status::Ok);

    GraphSessionData loaded;
    EXPECT_FALSE(GraphSessionStorage::load(fs, "bad.mi23graph", loaded));

    std::filesystem::remove_all(root, error);
}

TEST(GraphSessionStorage, DeletesGraphFile) {
    const std::filesystem::path root = graphStorageRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(AxiomFS::initialize(fs).status, AxiomFS::FilesystemStatus::Healthy);
    ASSERT_TRUE(GraphSessionStorage::save(fs, "DeleteMe", sampleSession()));

    EXPECT_TRUE(GraphSessionStorage::deleteFile(fs, "DeleteMe.mi23graph"));
    bool exists = true;
    EXPECT_EQ(fs.exists("graphs/DeleteMe.mi23graph", exists), AxiomFS::Status::Ok);
    EXPECT_FALSE(exists);

    std::filesystem::remove_all(root, error);
}
