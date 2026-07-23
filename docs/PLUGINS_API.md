# OpenDriver-VR Plugin API Guide

> Document version: 1.4 · Last updated: May 2026

## Introduction

The OpenDriver-VR plugin system allows developers to add new VR hardware support (HMDs, controllers, trackers) without modifying the core driver. Plugins are compiled as dynamic libraries (`.dll` on Windows, `.so` on Linux) and loaded at runtime by the `opendriver_runner` process.

This guide explains how to create, configure, and integrate a plugin.

## Plugin Structure

A plugin consists of two main parts:
1. **The Plugin Manifest (`plugin.json`)**
2. **The Shared Library (`.dll` / `.so`)**

Plugins must be placed in the OpenDriver configuration directory:
- Windows: `%APPDATA%\opendriver\plugins\<your_plugin_name>\`
- Linux: `~/.config/opendriver/plugins/<your_plugin_name>/`

### 1. The Manifest (`plugin.json`)

Every plugin directory must contain a `plugin.json` file. This tells the core runtime how to load the plugin and provides metadata for the Dashboard.

```json
{
    "name": "my_custom_plugin",
    "version": "1.0.0",
    "author": "Your Name",
    "description": "Adds support for custom DIY hardware.",
    "entry_point": "my_custom_plugin.dll",
    "enabled": true
}
```

- **name**: Must be unique, lowercase, with no spaces.
- **entry_point**: The filename of the compiled shared library.

### 2. The Shared Library

The shared library must implement the `opendriver::core::IPlugin` interface and export two C-linkage factory functions: `CreatePlugin` and `DestroyPlugin`.

## Core API Interfaces

To write a plugin, you need to include the OpenDriver core headers. The primary interface is defined in `include/opendriver/core/plugin_interface.h`.

### `IPlugin` Interface

This is the main class your plugin must inherit from.

```cpp
#include <opendriver/core/plugin_interface.h>

using namespace opendriver::core;

class MyPlugin : public IPlugin {
public:
    // --- Metadata ---
    const char* GetName() const override { return "my_custom_plugin"; }
    const char* GetVersion() const override { return "1.0.0"; }
    const char* GetDescription() const override { return "Custom hardware plugin"; }
    const char* GetAuthor() const override { return "Your Name"; }

    // --- Lifecycle ---
    bool OnInitialize(IPluginContext* context) override;
    void OnShutdown() override;

    // --- Per-frame Update ---
    void OnTick(float delta_time) override;

    // --- Event Handling ---
    void OnEvent(const Event& event) override;

    // --- Status & UI ---
    bool IsActive() const override { return true; }
    std::string GetStatus() const override { return "Running normally"; }
    IUIProvider* GetUIProvider() override { return nullptr; } // Optional Qt UI

    // --- Hot Reload (Optional) ---
    void* ExportState() override { return nullptr; }
    void ImportState(void* state) override {}

private:
    IPluginContext* m_context = nullptr;
};
```

#### Lifecycle Methods

- **`OnInitialize(IPluginContext* context)`**: Called once when the plugin is loaded. This is where you should read configuration, initialize hardware connections, subscribe to events, and register virtual VR devices. Return `true` if successful. If you return `false`, the plugin will be immediately unloaded. Keep a pointer to `context` as you will need it.
- **`OnShutdown()`**: Called when the runtime is shutting down or when the user disables the plugin. Clean up all resources, threads, and network sockets here. Devices registered by this plugin are automatically unregistered by the core.
- **`OnTick(float delta_time)`**: Called every frame by the runtime (roughly 90 times per second for VR). You can poll hardware here, although for low-latency VR it is highly recommended to spawn a dedicated background thread in `OnInitialize` and use `OnTick` only for housekeeping.
- **`OnEvent(const Event& event)`**: Called when an event you subscribed to is published on the Event Bus.

### Factory Exports

You MUST export `CreatePlugin` and `DestroyPlugin` so the loader can instantiate your class.

```cpp
extern "C" {
    // Note: Use OD_EXPORT if defining the platform macros, or __declspec(dllexport)
    __declspec(dllexport) IPlugin* CreatePlugin() {
        return new MyPlugin();
    }

    __declspec(dllexport) void DestroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
```

### `IPluginContext` Interface

The `IPluginContext` is your gateway to the OpenDriver core. You receive it in `OnInitialize`.

```cpp
class IPluginContext {
public:
    // Central Event Bus
    virtual EventBus& GetEventBus() = 0;

    // JSON Configuration
    virtual ConfigManager& GetConfig() = 0;

    // Logging
    virtual void Log(int level, const char* message) = 0;
    virtual void LogInfo(const char* msg); // Helper
    virtual void LogError(const char* msg); // Helper

    // Device Management
    virtual void RegisterDevice(const Device& device) = 0;
    virtual void UnregisterDevice(const char* device_id) = 0;

    // Data Submission (to SteamVR)
    virtual void UpdatePose(const char* device_id, 
                             double x, double y, double z, 
                             double qw, double qx, double qy, double qz,
                             double vx = 0, double vy = 0, double vz = 0,
                             double avx = 0, double avy = 0, double avz = 0) = 0;
                             
    virtual void UpdateInput(const char* device_id, const char* component_name, float value) = 0;

    // Safe execution on main thread
    virtual void PostToMainThread(std::function<void()> callback) = 0;
};
```

## Step-by-Step Examples

### 1. Registering a Device

To make SteamVR aware of your hardware, you must register a `Device`.

```cpp
#include <opendriver/core/device_registry.h>

bool MyPlugin::OnInitialize(IPluginContext* context) {
    m_context = context;

    Device controller;
    controller.id = "my_custom_controller";
    controller.type = DeviceType::CONTROLLER; // Maps to vr::TrackedDeviceClass_Controller
    controller.name = "Custom VR Wand";
    controller.manufacturer = "DIY Co";
    controller.serial_number = "WAND-001";
    controller.owner_plugin = GetName();

    // Define Inputs
    InputComponent trigger;
    trigger.name = "trigger";
    trigger.type = InputType::SCALAR; // Float value 0.0 to 1.0
    
    InputComponent grip;
    grip.name = "grip";
    grip.type = InputType::BOOLEAN;   // 0 or 1

    controller.inputs.push_back(trigger);
    controller.inputs.push_back(grip);

    m_context->RegisterDevice(controller);
    m_context->LogInfo("Registered Custom VR Wand.");

    return true;
}
```

### 2. Updating Tracking Pose

Pose updates should be sent frequently (e.g., from a dedicated reading thread).

```cpp
// X, Y, Z in meters
// QW, QX, QY, QZ representing rotation
// Velocities (vx, vy, vz, avx, avy, avz) are optional but highly recommended for SteamVR prediction!

m_context->UpdatePose("my_custom_controller", 
                      0.5, 1.2, -0.3,   // Position
                      1.0, 0.0, 0.0, 0.0 // Quaternion (Identity)
);
```

### 3. Updating Inputs

Input updates are usually sent when a hardware state changes.

```cpp
// Button press
m_context->UpdateInput("my_custom_controller", "grip", 1.0f);

// Trigger pull (half-way)
m_context->UpdateInput("my_custom_controller", "trigger", 0.5f);
```

### 4. Reading Configuration

Configuration is stored in `%APPDATA%\opendriver\config.json`. You can read settings specific to your plugin.

```cpp
bool MyPlugin::OnInitialize(IPluginContext* context) {
    // ...
    auto& config = context->GetConfig();
    
    // config.json -> {"plugins": {"my_custom_plugin": {"smoothing_factor": 0.5}}}
    nlohmann::json my_cfg = config.GetPluginConfig(GetName());
    
    float smoothing = my_cfg.value("smoothing_factor", 0.1f);
    // ...
}
```

### 5. Receiving Video Frames (For HMDs)

If your plugin is implementing an HMD (like an Android streaming headset), you need to receive the encoded H.264 video frames from the core.

```cpp
bool MyPlugin::OnInitialize(IPluginContext* context) {
    m_context = context;
    // Subscribe to video frames
    m_context->GetEventBus().Subscribe(EventType::VIDEO_FRAME, this);
    return true;
}

void MyPlugin::OnEvent(const Event& event) {
    if (event.type == EventType::VIDEO_FRAME) {
        // Safe deserialization using vector<uint8_t> to avoid RTTI issues across DLLs
        auto buffer = std::any_cast<std::vector<uint8_t>>(event.data);
        
        VideoFrameData frame;
        if (VideoFrameData::Deserialize(buffer, frame)) {
            // frame.nal_data contains the H.264 NAL units!
            // frame.pts is the presentation timestamp
            SendOverNetworkToHeadset(frame.nal_data);
        }
    }
}
```

### 6. Providing a Custom UI

If your plugin needs configuration UI (e.g., IP address fields, calibration buttons), implement `opendriver::core::IUIProvider`.

```cpp
#include <opendriver/core/iui_provider.h>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

class MyPluginUI : public opendriver::core::IUIProvider {
public:
    QWidget* CreateSettingsWidget(QWidget* parent) override {
        QWidget* widget = new QWidget(parent);
        QVBoxLayout* layout = new QVBoxLayout(widget);
        
        m_statusLabel = new QLabel("Waiting for data...", widget);
        m_statusLabel->setStyleSheet("color: white;");
        layout->addWidget(m_statusLabel);
        
        return widget;
    }

    void RefreshUI() override {
        // Called by Dashboard every 500ms
        if (m_statusLabel) {
            m_statusLabel->setText("Ping: 12ms");
        }
    }
private:
    QLabel* m_statusLabel = nullptr;
};
```

Then return it in your plugin:

```cpp
IUIProvider* MyPlugin::GetUIProvider() {
    if (!m_ui) m_ui = new MyPluginUI();
    return m_ui;
}
```

## CMake Setup for Plugins

The easiest way to build a plugin is to add it as a subdirectory within the OpenDriver-VR source tree, but you can also build it standalone.

Example `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_custom_plugin)

set(CMAKE_CXX_STANDARD 17)

# We are building a shared library (.dll / .so)
add_library(my_custom_plugin SHARED src/plugin.cpp)

# Link against the OpenDriver Core library
# If building standalone, you need to provide the path to OpenDriver headers and libopendriver_core
target_link_libraries(my_custom_plugin PRIVATE opendriver_core)

# (Optional) If you use Qt for UI
target_link_libraries(my_custom_plugin PRIVATE Qt6::Widgets)

# Don't add 'lib' prefix on Windows
if(WIN32)
    set_target_properties(my_custom_plugin PROPERTIES PREFIX "")
endif()
```

## Hot Reloading

When you recompile your plugin `.dll`/`.so`, the OpenDriver Runtime detects the file change and automatically reloads it.

If your plugin holds state (e.g., active network connections or calibration data) that shouldn't be lost during reload, implement `ExportState` and `ImportState`.

```cpp
void* MyPlugin::ExportState() {
    // Allocate a state object on the heap
    auto* state = new PluginState();
    state->ip_address = m_currentIp;
    state->calibrated = m_isCalibrated;
    return state;
}

void MyPlugin::ImportState(void* state) {
    if (state) {
        auto* saved = static_cast<PluginState*>(state);
        m_currentIp = saved->ip_address;
        m_isCalibrated = saved->calibrated;
        delete saved; // Clean up!
    }
}
```

## Best Practices

1. **Don't block `OnTick`**: `OnTick` runs on a tight loop. If you need to perform blocking I/O (sockets, serial port reading), spawn a `std::thread` in `OnInitialize`.
2. **Use the Core Logger**: Use `m_context->LogInfo()` instead of `std::cout`. This ensures your logs appear in the Dashboard UI and the central `opendriver.log` file.
3. **Catch Exceptions**: If your plugin throws an unhandled exception in `OnTick`, the core will catch it and forcibly disable your plugin to prevent the whole runtime from crashing. Catch exceptions yourself to recover gracefully.
4. **Use Unique Names**: Ensure your `plugin.json` name and `Device.id` are globally unique to avoid conflicts with other developers' plugins.
