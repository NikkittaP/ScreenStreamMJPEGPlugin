# ScreenStreamMJPEGPlugin

**Version:** 1.0.1  
**Engine Version:** Unreal Engine 5.2+ (tested up to 5.7)  
**License:** MIT License

## Description

ScreenStreamMJPEGPlugin is a plugin for Unreal Engine that enables real-time screen capture and streaming of MJPEG video over HTTP. It allows users to capture SceneCapture2D renders and serve the video stream to multiple clients over a network.

## Features

- Real-time SceneCapture2D capture and encoding
- MJPEG video streaming over HTTP
- Configurable resolution and server port
- Blueprint and C++ API support
- Low-latency streaming suitable for monitoring and debugging
- Memory-safe with built-in queue overflow protection
- Compatible with Unreal Engine 5.2 through 5.7

## Based on

- [UnrealImageCapture](https://github.com/TimmHess/UnrealImageCapture): Used for screen capturing functionality.
- [cpp-mjpeg-streamer](https://github.com/nadjieb/cpp-mjpeg-streamer): C++ code for MJPEG streaming server.

## Installation

1. Clone or download this repository.
2. Copy the `ScreenStreamMJPEGPlugin` folder into the `Plugins` directory of your Unreal Engine project.
3. Enable the plugin in your Unreal Engine project settings (Edit → Plugins → search for "ScreenStreamMJPEG").
4. Restart the editor if prompted.

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

### Blueprint Usage

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

**Example: Spawn and Configure Dynamically:**
```
Event Graph:
  [Event BeginPlay]
    → [Spawn Actor from Class] (StreamManagerMJPEG)
    → [Set Capture Component] = [Scene Capture 2D Reference]
    → [Set Server Port] = 8000
    → [Set Frame Width] = 1920
    → [Set Frame Height] = 1080
```

### C++ Usage

#### Include Header

```cpp
#include "StreamManagerMJPEG.h"
```

#### Spawn and Configure in Code

```cpp
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    // Find or create Scene Capture 2D
    ASceneCapture2D* SceneCapture = GetWorld()->SpawnActor<ASceneCapture2D>();
    
    // Spawn StreamManagerMJPEG
    AStreamManagerMJPEG* StreamManager = GetWorld()->SpawnActor<AStreamManagerMJPEG>();
    
    if (StreamManager && SceneCapture)
    {
        // Configure streaming parameters
        StreamManager->CaptureComponent = SceneCapture;
        StreamManager->FrameWidth = 1920;
        StreamManager->FrameHeight = 1080;
        StreamManager->ServerPort = 8000;
        
        // Update render target with new resolution
        StreamManager->UpdateRenderTargetAfterFrameSizeChanged();
    }
}
```

#### Manual Frame Capture

```cpp
// Trigger frame capture manually (useful for fixed frame rate capture)
void AMyGameMode::CaptureFrame()
{
    if (StreamManager)
    {
        StreamManager->CaptureNonBlocking();
    }
}

// Example: Capture at fixed intervals using timer
void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    // Capture at 30 FPS
    GetWorld()->GetTimerManager().SetTimer(
        CaptureTimerHandle,
        this,
        &AMyGameMode::CaptureFrame,
        1.0f / 30.0f,  // 30 FPS
        true           // Loop
    );
}
```

#### Find Existing StreamManagerMJPEG in Level

```cpp
#include "Kismet/GameplayStatics.h"

void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();
    
    // Find StreamManagerMJPEG placed in level
    AStreamManagerMJPEG* StreamManager = Cast<AStreamManagerMJPEG>(
        UGameplayStatics::GetActorOfClass(GetWorld(), AStreamManagerMJPEG::StaticClass())
    );
    
    if (StreamManager)
    {
        // Configure as needed
        StreamManager->FrameWidth = 1280;
        StreamManager->FrameHeight = 720;
        StreamManager->UpdateRenderTargetAfterFrameSizeChanged();
    }
}
```

### Accessing the Stream

Once the stream is running, you can access it from:

- **Web Browser:** `http://localhost:8000/stream.mjpg`
- **VLC Media Player:** Open Network Stream → `http://localhost:8000/stream.mjpg`
- **FFmpeg:** `ffplay http://localhost:8000/stream.mjpg`
- **Custom Client:** Any HTTP client that supports MJPEG streams

**For Remote Access:**
Replace `localhost` with the server's IP address: `http://192.168.1.100:8000/stream.mjpg`

### Configuration Options

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `ServerPort` | int | 8000 | HTTP port for MJPEG streaming server |
| `FrameWidth` | int | 640 | Width of captured frames in pixels |
| `FrameHeight` | int | 480 | Height of captured frames in pixels |
| `CaptureComponent` | ASceneCapture2D* | nullptr | Reference to Scene Capture 2D actor to stream |
| `VerboseLogging` | bool | false | Enable detailed logging for debugging |

### Best Practices

1. **Resolution:** Higher resolutions increase bandwidth and CPU usage. Start with 1280x720 for testing.
2. **Frame Rate:** Use manual capture with timers for consistent frame rates. Calling `CaptureNonBlocking()` every tick may be too frequent.
3. **Network:** For remote streaming, ensure firewall allows connections on the configured port.
4. **Performance:** Monitor CPU and memory usage. The plugin includes automatic memory protection (queue overflow detection).
5. **Scene Capture Position:** Attach Scene Capture 2D to moving actors or update its transform to follow cameras.

### Troubleshooting

**Stream not accessible:**
- Check that `CaptureComponent` is assigned
- Verify the port is not blocked by firewall
- Ensure BeginPlay has been called (press Play in editor)

**Low frame rate:**
- Reduce resolution (FrameWidth/FrameHeight)
- Decrease capture frequency if using manual capture
- Check CPU usage and GPU performance

**Memory issues:**
- Plugin includes automatic queue overflow protection
- Check logs for warnings about queue size
- Reduce capture frequency if warnings appear

## Contributing

Contributions are welcome! If you encounter any issues or have suggestions for improvements, please open an issue or submit a pull request on GitHub.

## License

This project is licensed under the [MIT License](LICENSE).

## Support

For support or inquiries, please contact [Nikita Petrov](mailto:nikitapetroff@gmail.com).
