#include <gtest/gtest.h>

#include "core/companion/CompanionProtocol.h"
#include "core/companion/CompanionSystemActions.h"
#include "core/companion/CompanionUtils.h"
#include "platform/host/axiom_fs_host.h"

#include <algorithm>
#include <filesystem>

namespace {

class TestSettingsStore : public SettingsStore {
public:
    bool load(SettingsState& settings) override {
        settings = savedSettings;
        return true;
    }

    bool save(const SettingsState& settings) override {
        savedSettings = settings;
        saveCount++;
        return saveResult;
    }

    SettingsState savedSettings{};
    int saveCount = 0;
    bool saveResult = true;
};

class TestSystemActions : public Companion::CompanionSystemActions {
public:
    Companion::SystemActionResult requestReboot() override {
        rebootRequests++;
        return Companion::SystemActionResult::acceptedWithOutput("mock reboot scheduled\n");
    }

    Companion::SystemActionResult requestBootloader() override {
        bootloaderRequests++;
        return Companion::SystemActionResult::acceptedWithOutput("mock bootloader scheduled\n");
    }

    int rebootRequests = 0;
    int bootloaderRequests = 0;
};

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

    Companion::CompanionProtocol makeProtocol(Companion::CompanionSystemActions* actions = nullptr) {
        return Companion::CompanionProtocol(fs, settings, settingsStore, {"test-fw", "rev-a", "serial-1"}, actions);
    }

    std::filesystem::path root =
        std::filesystem::temp_directory_path() / "mi23_companion_protocol_tests";
    HostAxiomFSBackend backend{root};
    AxiomFS::FileSystem fs{backend};
    SettingsState settings{};
    TestSettingsStore settingsStore{};
};

} // namespace

TEST(CompanionBase64, EncodesAndDecodesBinaryData) {
    const uint8_t bytes[] = {0x00u, 0x01u, 0x02u, 0xFAu, 0xFFu};
    const std::string encoded = Companion::base64Encode(bytes, sizeof(bytes));
    EXPECT_EQ(encoded, "AAEC+v8=");

    std::vector<uint8_t> decoded;
    ASSERT_TRUE(Companion::base64Decode(encoded, decoded));
    ASSERT_EQ(decoded.size(), sizeof(bytes));
    EXPECT_TRUE(std::equal(decoded.begin(), decoded.end(), bytes));

    EXPECT_FALSE(Companion::base64Decode("bad?", decoded));
}

TEST(CompanionPaths, ValidatesAbsoluteVirtualPaths) {
    std::string fsPath;
    EXPECT_TRUE(Companion::validateVirtualPath("/", true, fsPath));
    EXPECT_EQ(fsPath, "");

    EXPECT_TRUE(Companion::validateVirtualPath("/graphs/graph1.mi23graph", false, fsPath));
    EXPECT_EQ(fsPath, "graphs/graph1.mi23graph");

    EXPECT_FALSE(Companion::validateVirtualPath("graphs/graph1.mi23graph", false, fsPath));
    EXPECT_FALSE(Companion::validateVirtualPath("/graphs/../settings.bin", false, fsPath));
    EXPECT_FALSE(Companion::validateVirtualPath("/graphs//bad", false, fsPath));
    EXPECT_FALSE(Companion::validateVirtualPath("/", false, fsPath));
}

TEST_F(CompanionProtocolFixture, HandlesPingDeviceInfoAndCapabilities) {
    Companion::CompanionProtocol protocol = makeProtocol();

    std::string response;
    EXPECT_TRUE(protocol.handleCommand("{\"id\":1,\"cmd\":\"protocol.ping\"}", response));
    EXPECT_EQ(response, "{\"id\":1,\"ok\":true,\"result\":{\"pong\":true}}\n");

    EXPECT_TRUE(protocol.handleCommand("{\"id\":2,\"cmd\":\"device.info\"}", response));
    EXPECT_NE(response.find("\"id\":2"), std::string::npos);
    EXPECT_NE(response.find("\"model\":\"MI-23\""), std::string::npos);
    EXPECT_NE(response.find("\"firmware\":\"test-fw\""), std::string::npos);
    EXPECT_NE(response.find("\"protocol\":1"), std::string::npos);
    EXPECT_NE(response.find("\"transport\":\"usb_cdc\""), std::string::npos);
    EXPECT_NE(response.find("\"serial\":\"serial-1\""), std::string::npos);

    EXPECT_TRUE(protocol.handleCommand("{\"id\":3,\"cmd\":\"device.capabilities\"}", response));
    EXPECT_NE(response.find("\"filesystem\":true"), std::string::npos);
    EXPECT_NE(response.find("\"settings\":true"), std::string::npos);
    EXPECT_NE(response.find("\"terminal\":true"), std::string::npos);
    EXPECT_NE(response.find("\"graphs\":true"), std::string::npos);
    EXPECT_NE(response.find("\"battery\":false"), std::string::npos);
    EXPECT_NE(response.find("\"screenshots\":false"), std::string::npos);
    EXPECT_NE(response.find("\"firmware_update\":false"), std::string::npos);
}

TEST_F(CompanionProtocolFixture, RejectsMalformedRequestsWithStructuredErrors) {
    Companion::CompanionProtocol protocol = makeProtocol();

    std::string response;
    EXPECT_FALSE(protocol.handleCommand("PING", response));
    EXPECT_NE(response.find("\"code\":\"bad_json\""), std::string::npos);

    EXPECT_FALSE(protocol.handleCommand("{\"cmd\":\"protocol.ping\"}", response));
    EXPECT_NE(response.find("\"code\":\"missing_id\""), std::string::npos);

    EXPECT_FALSE(protocol.handleCommand("{\"id\":7,\"cmd\":\"nope\"}", response));
    EXPECT_NE(response.find("\"id\":7"), std::string::npos);
    EXPECT_NE(response.find("\"code\":\"unknown_command\""), std::string::npos);
}

TEST_F(CompanionProtocolFixture, ListsReadsWritesAndDeletesFiles) {
    Companion::CompanionProtocol protocol = makeProtocol();

    std::string response;
    EXPECT_TRUE(protocol.handleCommand(
        "{\"id\":1,\"cmd\":\"fs.write\",\"path\":\"/test.txt\",\"offset\":0,"
        "\"data_b64\":\"SGVsbG8=\",\"truncate\":true}",
        response));
    EXPECT_NE(response.find("\"bytes_written\":5"), std::string::npos);

    EXPECT_TRUE(protocol.handleCommand(
        "{\"id\":2,\"cmd\":\"fs.read\",\"path\":\"/test.txt\",\"offset\":0,\"length\":16}",
        response));
    EXPECT_NE(response.find("\"data_b64\":\"SGVsbG8=\""), std::string::npos);
    EXPECT_NE(response.find("\"bytes_read\":5"), std::string::npos);
    EXPECT_NE(response.find("\"eof\":true"), std::string::npos);

    EXPECT_TRUE(protocol.handleCommand("{\"id\":3,\"cmd\":\"fs.list\",\"path\":\"/\"}", response));
    EXPECT_NE(response.find("\"name\":\"test.txt\""), std::string::npos);
    EXPECT_NE(response.find("\"type\":\"file\""), std::string::npos);

    EXPECT_TRUE(protocol.handleCommand("{\"id\":4,\"cmd\":\"fs.delete\",\"path\":\"/test.txt\"}", response));
    EXPECT_NE(response.find("\"deleted\":true"), std::string::npos);

    EXPECT_FALSE(protocol.handleCommand(
        "{\"id\":5,\"cmd\":\"fs.write\",\"path\":\"/missing/file.txt\",\"offset\":0,"
        "\"data_b64\":\"QQ==\",\"truncate\":true}",
        response));
    EXPECT_NE(response.find("\"code\":\"not_found\""), std::string::npos);
}

TEST_F(CompanionProtocolFixture, CreatesDirectoriesAndListsGraphs) {
    Companion::CompanionProtocol protocol = makeProtocol();

    std::string response;
    EXPECT_TRUE(protocol.handleCommand("{\"id\":1,\"cmd\":\"fs.mkdir\",\"path\":\"/notes/new\"}", response));
    EXPECT_NE(response.find("\"created\":true"), std::string::npos);

    ASSERT_EQ(fs.writeFile("graphs/Graph_001.mi23graph", "{\"version\":1}\n"), AxiomFS::Status::Ok);
    EXPECT_TRUE(protocol.handleCommand("{\"id\":2,\"cmd\":\"graphs.list\"}", response));
    EXPECT_NE(response.find("\"name\":\"Graph_001\""), std::string::npos);
    EXPECT_NE(response.find("\"path\":\"/graphs/Graph_001.mi23graph\""), std::string::npos);
}

TEST_F(CompanionProtocolFixture, GetsAndSetsSafeSettings) {
    Companion::CompanionProtocol protocol = makeProtocol();

    std::string response;
    EXPECT_TRUE(protocol.handleCommand("{\"id\":1,\"cmd\":\"settings.get\"}", response));
    EXPECT_NE(response.find("\"angle_mode\":\"rad\""), std::string::npos);
    EXPECT_NE(response.find("\"theme\":\"dark\""), std::string::npos);

    EXPECT_TRUE(protocol.handleCommand(
        "{\"id\":2,\"cmd\":\"settings.set\",\"values\":{\"angle_mode\":\"deg\",\"theme\":\"light\"}}",
        response));
    EXPECT_NE(response.find("\"updated\":true"), std::string::npos);
    EXPECT_NE(response.find("\"angle_mode\":\"deg\""), std::string::npos);
    EXPECT_EQ(settings.angleMode, AngleMode::Degrees);
    EXPECT_EQ(settings.theme, ThemeMode::Light);
    EXPECT_EQ(settingsStore.saveCount, 1);

    EXPECT_FALSE(protocol.handleCommand(
        "{\"id\":3,\"cmd\":\"settings.set\",\"values\":{\"exam_mode\":true}}",
        response));
    EXPECT_NE(response.find("\"code\":\"invalid_argument\""), std::string::npos);
}

TEST_F(CompanionProtocolFixture, RunsSafeTerminalCommands) {
    Companion::CompanionProtocol protocol = makeProtocol();

    std::string response;
    EXPECT_TRUE(protocol.handleCommand(
        "{\"id\":1,\"cmd\":\"terminal.exec\",\"line\":\"help\"}",
        response));
    EXPECT_NE(response.find("available commands: help, info, storage, capabilities, uptime, version, reboot, bootloader\\n"),
              std::string::npos);

    EXPECT_TRUE(protocol.handleCommand(
        "{\"id\":2,\"cmd\":\"terminal.exec\",\"line\":\"info\"}",
        response));
    EXPECT_NE(response.find("MI-23 firmware test-fw protocol 1\\n"), std::string::npos);

    EXPECT_FALSE(protocol.handleCommand(
        "{\"id\":3,\"cmd\":\"terminal.exec\",\"line\":\"rm -rf /\"}",
        response));
    EXPECT_NE(response.find("\"code\":\"invalid_argument\""), std::string::npos);
}

TEST_F(CompanionProtocolFixture, TerminalRebootCommandsReturnUnsupportedWithoutPlatformActions) {
    Companion::CompanionProtocol protocol = makeProtocol();

    std::string response;
    EXPECT_FALSE(protocol.handleCommand(
        "{\"id\":1,\"cmd\":\"terminal.exec\",\"line\":\"reboot\"}",
        response));
    EXPECT_NE(response.find("\"code\":\"unsupported\""), std::string::npos);
    EXPECT_NE(response.find("Reboot is not supported on this platform."), std::string::npos);

    EXPECT_FALSE(protocol.handleCommand(
        "{\"id\":2,\"cmd\":\"terminal.exec\",\"line\":\"bootloader\"}",
        response));
    EXPECT_NE(response.find("\"code\":\"unsupported\""), std::string::npos);
    EXPECT_NE(response.find("USB BOOT mode is not supported on this platform."), std::string::npos);
}

TEST_F(CompanionProtocolFixture, TerminalRebootCommandsUsePlatformActionsWhenAvailable) {
    TestSystemActions actions;
    Companion::CompanionProtocol protocol = makeProtocol(&actions);

    std::string response;
    EXPECT_TRUE(protocol.handleCommand(
        "{\"id\":1,\"cmd\":\"terminal.exec\",\"line\":\"reboot\"}",
        response));
    EXPECT_NE(response.find("\"output\":\"mock reboot scheduled\\n\""), std::string::npos);
    EXPECT_EQ(actions.rebootRequests, 1);

    EXPECT_TRUE(protocol.handleCommand(
        "{\"id\":2,\"cmd\":\"terminal.exec\",\"line\":\"bootloader\"}",
        response));
    EXPECT_NE(response.find("\"output\":\"mock bootloader scheduled\\n\""), std::string::npos);
    EXPECT_EQ(actions.bootloaderRequests, 1);
}
