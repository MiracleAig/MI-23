#include <gtest/gtest.h>

#include "core/companion/CompanionProtocol.h"
#include "platform/host/axiom_fs_host.h"

#include <filesystem>

namespace {

class CompanionProtocolFixture : public ::testing::Test {
protected:
    void SetUp() override {
        std::error_code error;
        std::filesystem::remove_all(root, error);
        ASSERT_EQ(fs.mount(), AxiomFS::Status::Ok);
        ASSERT_EQ(AxiomFS::ensureDefaultLayout(fs), AxiomFS::Status::Ok);
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    std::filesystem::path root =
        std::filesystem::temp_directory_path() / "mi23_companion_protocol_tests";
    HostAxiomFSBackend backend{root};
    AxiomFS::FileSystem fs{backend};
};

} // namespace

TEST_F(CompanionProtocolFixture, HandlesPing) {
    Companion::CompanionProtocol protocol(fs, {"test-fw", "rev-a"});

    std::string response;
    EXPECT_TRUE(protocol.handleCommand("PING", response));
    EXPECT_EQ(response, "OK PONG\n");
}

TEST_F(CompanionProtocolFixture, HandlesHello) {
    Companion::CompanionProtocol protocol(fs, {"test-fw", "rev-a"});

    std::string response;
    EXPECT_TRUE(protocol.handleCommand("HELLO", response));
    EXPECT_NE(response.find("OK MIRACLE_PROTOCOL 1\n"), std::string::npos);
    EXPECT_NE(response.find("DEVICE_TYPE calculator\n"), std::string::npos);
    EXPECT_NE(response.find("MODEL MI-23\n"), std::string::npos);
    EXPECT_NE(response.find("FIRMWARE test-fw\n"), std::string::npos);
    EXPECT_NE(response.find("HARDWARE rev-a\n"), std::string::npos);
    EXPECT_NE(response.find("CAPABILITIES filesystem,graphs,settings,terminal\n"), std::string::npos);
    EXPECT_NE(response.find("END\n"), std::string::npos);
}

TEST_F(CompanionProtocolFixture, HandlesInfoFromFilesystemStats) {
    Companion::CompanionProtocol protocol(fs, {"test-fw", "rev-a"});

    std::string response;
    EXPECT_TRUE(protocol.handleCommand("INFO", response));
    EXPECT_NE(response.find("OK INFO\n"), std::string::npos);
    EXPECT_NE(response.find("STORAGE_TOTAL "), std::string::npos);
    EXPECT_NE(response.find("STORAGE_USED "), std::string::npos);
    EXPECT_NE(response.find("STORAGE_FREE "), std::string::npos);
    EXPECT_NE(response.find("FILESYSTEM littlefs\n"), std::string::npos);
    EXPECT_NE(response.find("END\n"), std::string::npos);
}

TEST_F(CompanionProtocolFixture, RejectsMalformedCommands) {
    Companion::CompanionProtocol protocol(fs, {"test-fw", "rev-a"});

    std::string response;
    EXPECT_FALSE(protocol.handleCommand("PING now", response));
    EXPECT_EQ(response, "ERR MALFORMED\n");

    EXPECT_FALSE(protocol.handleCommand("WHAT", response));
    EXPECT_EQ(response, "ERR UNKNOWN_COMMAND\n");
}
