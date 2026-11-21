#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <launch_path.hpp>
#include <system_error>
namespace {
/// @brief An empty directory of its own under the system's temporary one, removed with the test.
class InstallDirectory : public ::testing::Test {
protected:
  auto SetUp() -> void override {
    const auto *test = ::testing::UnitTest::GetInstance()->current_test_info();
    directory = std::filesystem::temp_directory_path() / (std::string("kuki_") + test->name());
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory);
  }
  auto TearDown() -> void override {
    std::error_code error;
    std::filesystem::remove_all(directory, error);
  }
  std::filesystem::path directory;
};
} // namespace
TEST_F(InstallDirectory, WritesBesideItselfWithoutTheMarker) {
  EXPECT_EQ(kuki::GetUserDataPath(directory), directory);
}
TEST_F(InstallDirectory, WritesToTheUsersFolderOnceInstalled) {
  std::ofstream(directory / kuki::INSTALLED_MARKER) << "installed\n";
  const auto data = kuki::GetUserDataPath(directory);
  EXPECT_NE(data, directory);
  EXPECT_EQ(data.filename(), "Kuki");
}
// The tests run from the build tree, which the install's marker never reaches, so they keep writing
// beside themselves as a development build of a game does.
TEST(LaunchPath, ABuildTreeIsNotInstalled) {
  EXPECT_EQ(kuki::GetUserDataPath(), kuki::GetLaunchPath());
}
