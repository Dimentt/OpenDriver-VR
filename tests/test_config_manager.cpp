#include <opendriver/core/config_manager.h>

#include <cassert>
#include <filesystem>

int main() {
    using namespace opendriver::core;
    namespace fs = std::filesystem;

    const fs::path temp_file =
        fs::temp_directory_path() / "opendriver_config_manager_test.json";
    const fs::path backup_file = temp_file.string() + ".bak";

    fs::remove(temp_file);
    fs::remove(backup_file);

    ConfigManager cfg;
    assert(cfg.Load(temp_file.string()) == true);

    cfg.SetString("plugins.android_hmd.device_id", "android_hmd_001");
    cfg.SetInt("plugins.android_hmd.udp_port", 6969);
    cfg.SetFloat("plugins.android_hmd.refresh_rate", 90.0f);
    cfg.SetBool("plugins.android_hmd.enabled", true);
    cfg.SetPluginConfig("virtual_controllers", json{
        {"enabled", false},
        {"role", "hands"}
    });
    cfg.SetPluginEnabled("persisted_tracker", false);

    assert(cfg.Save() == true);
    assert(!fs::exists(backup_file));

    cfg.SetInt("runtime.tick_rate_hz", 120);
    assert(cfg.Save() == true);
    assert(fs::exists(backup_file));

    ConfigManager cfg2;
    assert(cfg2.Load(temp_file.string()) == true);
    assert(cfg2.GetString("plugins.android_hmd.device_id", "") == "android_hmd_001");
    assert(cfg2.GetInt("plugins.android_hmd.udp_port", 0) == 6969);
    assert(cfg2.GetFloat("plugins.android_hmd.refresh_rate", 0.0f) == 90.0f);
    assert(cfg2.GetBool("plugins.android_hmd.enabled", false) == true);
    assert(cfg2.GetString("does.not.exist", "fallback") == "fallback");
    assert(cfg2.GetInt("runtime.tick_rate_hz", 0) == 120);
    assert(cfg2.IsPluginEnabled("persisted_tracker") == false);

    const json virtual_cfg = cfg2.GetPluginConfig("virtual_controllers");
    assert(virtual_cfg.is_object());
    assert(virtual_cfg["enabled"].get<bool>() == false);
    assert(virtual_cfg["role"].get<std::string>() == "hands");

    const json missing_cfg = cfg2.GetPluginConfig("missing_plugin");
    assert(missing_cfg.is_object());
    assert(missing_cfg.empty());

    fs::remove(temp_file);
    fs::remove(backup_file);
    return 0;
}
