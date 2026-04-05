# ScreenStreamMJPEGPlugin

**Version:** 1.0.2  
**Engine Version:** Unreal Engine 5.2+ (tested up to 5.7)  
**License:** MIT License

## Description

ScreenStreamMJPEGPlugin is a plugin for Unreal Engine that enables real-time screen capture and streaming of MJPEG video over HTTP. It captures SceneCapture2D renders, optionally composites a UMG `UUserWidget` overlay on top (useful for HUD labels, telemetry, or annotations), and serves the result to multiple HTTP clients simultaneously.

## Features

- Real-time SceneCapture2D capture and JPEG encoding
- MJPEG video streaming over HTTP
- **UMG/Slate widget overlay** — alpha-composite any `UUserWidget` onto every streamed frame
- Configurable resolution and server port
- Blueprint and C++ API support
- Low-latency streaming suitable for monitoring and debugging
- Memory-safe: queue drain on shutdown, per-frame allocations minimised
- Compatible with Unreal Engine 5.2 through 5.7

## Based on

- [UnrealImageCapture](https://github.com/TimmHess/UnrealImageCapture): Used for screen capturing functionality.
- [cpp-mjpeg-streamer](https://github.com/nadjieb/cpp-mjpeg-streamer): C++ MJPEG streaming server.

## Installation

1. Clone or download this repository.
2. Copy the `ScreenStreamMJPEGPlugin` folder into the `Plugins` directory of your Unreal Engine project.
3. Enable the plugin in your Unreal Engine project settings (Edit → Plugins → search for "ScreenStreamMJPEG").
4. Restart the editor if prompted.

If you use the UMG overlay feature, add `"UMG"` to your module's `PrivateDependencyModuleNames` in your project's `.Build.cs`.

## Usage

### Quick Start

1. **Add Required Actors to Your Level:**
   - Place a `Scene Capture 2D` actor in your level
   - Place a `StreamManagerMJPEG` actor in your level (found in Place Actors panel under "All Classes")

2. **Configure StreamManagerMJPEG:**
   - Select the StreamManagerMJPEG actor
   - In Details panel, set:
     - `Capture Component`: Reference to your Scene Capture 2D actor
     - `Frame Width`: Desired resolution width (default: 640)
     - `Frame Height`: Desired resolution height (default: 480)
     - `Server Port`: HTTP port for MJPEG stream (default: 8000)

3. **Start Capturing:**
   - The stream automatically starts when you press Play in the editor
   - Access the stream at: `http://localhost:8000/stream.mjpg`

---

### UMG Widget Overlay

The plugin can alpha-composite any `UUserWidget` onto each streamed frame before JPEG encoding. This lets you embed HUD elements — labels, telemetry readouts, icons, or annotations — directly into the MJPEG stream without affecting the in-game viewport.

#### How it works

Each capture tick the plugin:
1. Calls `FWidgetRenderer::DrawWidget` to render your widget into an RGBA8 render target at stream resolution.
2. Reads back the overlay pixels from the GPU (throttled by `OverlayRefreshInterval`).
3. Alpha-composites the overlay over the scene pixels on the CPU.
4. JPEG-encodes and publishes the combined image.

The widget is rendered off-screen — it does not appear in the viewport and does not need to be added to the viewport yourself.

#### Blueprint Usage

```
Event Graph (e.g. BeginPlay):

  [Create Widget] (your overlay widget class)
        ↓
  [Get Actor of Class] → StreamManagerMJPEG
        ↓
  [Set Overlay Widget] (pass the widget reference)
```

To stop the overlay, call `Set Overlay Widget` with a `None` / `nullptr` reference.

#### C++ Usage

```cpp
#include "StreamManagerMJPEG.h"
#include "Blueprint/UserWidget.h"

// In BeginPlay (after StreamManagerMJPEG is ready):
UUserWidget* OverlayWidget = CreateWidget<UUserWidget>(GetWorld(), MyOverlayWidgetClass);

AStreamManagerMJPEG* StreamManager = Cast<AStreamManagerMJPEG>(
    UGameplayStatics::GetActorOfClass(GetWorld(), AStreamManagerMJPEG::StaticClass())
);

if (StreamManager && OverlayWidget)
{
    StreamManager->SetOverlayWidget(OverlayWidget);
}

// To remove the overlay later:
StreamManager->SetOverlayWidget(nullptr);
```

#### Design notes

- The widget is **not** added to the viewport. Create it with `CreateWidget` but do **not** call `AddToViewport`.
- The widget is rendered at `FrameWidth × FrameHeight` resolution, so design it at that canvas size for pixel-perfect output.
- The widget's **background must be fully transparent** (alpha = 0) so that only the elements you draw appear in the composite.
- `OverlayRefreshInterval` (default: `1`) controls how often a GPU readback of the overlay render target is performed. `1` = every captured frame (smoothest). Increase it to reduce GPU readback cost when the overlay content changes slowly.
- `UpdateLabelPositions()` or any per-frame widget logic must be called **before** `CaptureNonBlocking()` to ensure positions are current in the rendered frame.

#### Overlay configuration properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `OverlayRefreshInterval` | int32 | 1 | GPU readback frequency (frames). 1 = every frame. |
| `OverlayRenderTarget` | UTextureRenderTarget2D* | auto | Created automatically by `SetOverlayWidget`. Can be inspected or replaced. |

---

### Blueprint Usage (scene capture)

#### Setup in Level

1. **Add Scene Capture 2D:**
   - Place Actors → Cameras → Scene Capture 2D
   - Position and rotate it to capture the desired view

2. **Add StreamManagerMJPEG:**
   - Place Actors → All Classes → search "StreamManagerMJPEG"
   - In Details panel:
     - Set `Capture Component` to your Scene Capture 2D actor
     - Configure `Frame Width` and `Frame Height` (e.g., 1920x1080)
     - Set `Server Port` (default 8000 works well)

#### Runtime Configuration

**To Change Resolution at Runtime:**
```
Event Graph:
  [Event BeginPlay]
    → [Get Actor of Class] (StreamManagerMJPEG)
    → [Set Frame Width] = 1920
    → [Set Frame Height] = 1080
    → [Update Render Target After Frame Size Changed]
```

**To Manually Trigger Frame Capture:**
```
Event Graph:
  [Event Tick]
    → [Get Actor of Class] (StreamManagerMJPEG)
    → [Capture Non Blocking]
```

---

### C++ Usage (scene capture)

#### Include Header

```cpp
#include "StreamManagerMJPEG.h"
```

#### Spawn and Configure in Code

```cpp
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    ASceneCapture2D* SceneCapture = GetWorld()->SpawnActor<ASceneCapture2D>();
    AStreamManagerMJPEG* StreamManager = GetWorld()->SpawnActor<AStreamManagerMJPEG>();
    
    if (StreamManager && SceneCapture)
    {
        StreamManager->CaptureComponent = SceneCapture;
        StreamManager->FrameWidth = 1920;
        StreamManager->FrameHeight = 1080;
        StreamManager->ServerPort = 8000;
        StreamManager->UpdateRenderTargetAfterFrameSizeChanged();
    }
}
```

#### Manual Frame Capture at Fixed Rate

```cpp
// Capture at 30 FPS using a timer
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    GetWorld()->GetTimerManager().SetTimer(
        CaptureTimerHandle,
        this,
        &AMyGameMode::CaptureFrame,
        1.0f / 30.0f,
        true
    );
}

void AMyGameMode::CaptureFrame()
{
    if (StreamManager)
    {
        StreamManager->CaptureNonBlocking();
    }
}
```

---

### Accessing the Stream

Once the stream is running, access it from any MJPEG-capable client:

- **Web Browser:** `http://localhost:8000/stream.mjpg`
- **VLC Media Player:** Open Network Stream → `http://localhost:8000/stream.mjpg`
- **FFmpeg:** `ffplay http://localhost:8000/stream.mjpg`
- **Custom client:** Any HTTP client that supports multipart/x-mixed-replace MJPEG

For remote access, replace `localhost` with the server's IP address.

---

### Configuration Reference

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `ServerPort` | int | 8000 | HTTP port for MJPEG streaming server |
| `FrameWidth` | int | 640 | Capture width in pixels |
| `FrameHeight` | int | 480 | Capture height in pixels |
| `CaptureComponent` | ASceneCapture2D* | nullptr | Scene Capture 2D actor to stream |
| `VerboseLogging` | bool | false | Enable verbose log output for debugging |
| `OverlayRefreshInterval` | int32 | 1 | Overlay GPU readback frequency in frames |

### Best Practices

1. **Resolution:** Higher resolutions increase bandwidth and CPU usage. Start with 1280x720 for testing.
2. **Frame Rate:** Use a timer for consistent capture rates rather than calling `CaptureNonBlocking()` every tick.
3. **Overlay widget:** Keep the widget background transparent (alpha = 0). Opaque backgrounds will cover the entire scene.
4. **Overlay design size:** Match your widget's design canvas to `FrameWidth × FrameHeight` so elements appear at the correct scale and position.
5. **Network:** Ensure your firewall allows inbound connections on the configured port for remote access.
6. **Logging:** Plugin logs are at `Verbose` level. Enable with `Log LogStreamMJPEG Verbose` in the UE console.

### Troubleshooting

**Stream not accessible:**
- Check that `CaptureComponent` is assigned
- Verify the port is not blocked by firewall
- Ensure the game has started (BeginPlay has been called)

**Overlay not appearing:**
- Confirm `SetOverlayWidget` was called with a valid (non-null) widget
- Make sure the widget was **not** added to the viewport — `CreateWidget` only, no `AddToViewport`
- Check that widget elements have non-zero alpha; a fully transparent widget produces no visible overlay
- Verify stream resolution matches the widget's design canvas size

**Low frame rate:**
- Reduce resolution (`FrameWidth`/`FrameHeight`)
- Decrease capture frequency if using manual capture
- Increase `OverlayRefreshInterval` to reduce GPU readback cost

**Memory growth:**
- Plugin drains the render request queue on EndPlay and caps queue depth at 10 frames
- Check for `LogStreamMJPEG` errors about queue overflow
- Ensure `CaptureNonBlocking()` is not called faster than frames are consumed

## Contributing

Contributions are welcome. If you encounter any issues or have suggestions for improvements, please open an issue or submit a pull request on GitHub.

## License

This project is licensed under the [MIT License](LICENSE).

## Support

For support or inquiries, please contact [Nikita Petrov](mailto:nikitapetroff@gmail.com).
