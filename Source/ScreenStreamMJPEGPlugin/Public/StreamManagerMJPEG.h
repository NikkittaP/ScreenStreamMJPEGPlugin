// Copyright (c) 2024 Nikita Petrov (https://github.com/NikkittaP)
// SPDX-License-Identifier: MIT

#pragma once

class ASceneCapture2D;
class UMaterial;
class FMJPEGStreamerImpl;
class FWidgetRenderer;
class UUserWidget;
class UTextureRenderTarget2D;
class IImageWrapper;

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Queue.h"
#include "Async/AsyncWork.h"
#include "Widgets/SWidget.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStreamMJPEG, Log, All);

#include "StreamManagerMJPEG.generated.h"

USTRUCT()
struct FRenderRequestStreamMJPEGStruct
{
    GENERATED_BODY()

    TArray<FColor> Image;
    TArray<FColor> OverlayImage;
    bool bHasOverlay = false;
    FRenderCommandFence RenderFence;

    FRenderRequestStreamMJPEGStruct()
    {
    }
};

UCLASS(Blueprintable)
class SCREENSTREAMMJPEGPLUGIN_API AStreamManagerMJPEG : public AActor
{
    GENERATED_BODY()

public:
    AStreamManagerMJPEG();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stream")
    int ServerPort = 8000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stream")
    int FrameWidth = 640;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stream")
    int FrameHeight = 480;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stream")
    ASceneCapture2D *CaptureComponent;

    UPROPERTY(EditAnywhere, Category = "Logging")
    bool VerboseLogging = false;

    // ── HUD Overlay Compositing ─────────────────────────────────────────────

    /** When set, pixels from this render target are alpha-composited over the
     *  scene capture before JPEG encoding. Expected to be RGBA8 with premultiplied
     *  alpha, same resolution as FrameWidth x FrameHeight. */
    UPROPERTY(BlueprintReadWrite, Category = "Stream|Overlay")
    UTextureRenderTarget2D* OverlayRenderTarget = nullptr;

    /** Assign a Slate widget tree that the stream manager will render into
     *  OverlayRenderTarget each capture.  Pass nullptr to stop rendering.
     *  The widget tree is rendered at (FrameWidth x FrameHeight) resolution. */
    UFUNCTION(BlueprintCallable, Category = "Stream|Overlay")
    void SetOverlayWidget(UUserWidget* InWidget);

    /** How often (in frames) the overlay GPU readback runs.
     *  1 = every frame (default, smoothest labels), higher = less GPU overhead
     *  but labels update less frequently. DrawWidget always runs every frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stream|Overlay", meta = (ClampMin = "1", ClampMax = "30"))
    int32 OverlayRefreshInterval = 1;

    UFUNCTION(BlueprintCallable, Category = "Stream")
    void UpdateRenderTargetAfterFrameSizeChanged();

protected:
    // Pimpl to hide MJPEG streamer implementation details
    TUniquePtr<FMJPEGStreamerImpl> StreamerImpl;

    // RenderRequest Queue
    TQueue<FRenderRequestStreamMJPEGStruct*> RenderRequestQueue;
    
    // Queue size tracker (TQueue doesn't expose size)
    std::atomic<int32> QueueSize{0};

    int ImgCounter = 0;

    // ── JPEG encoding (reused across frames to avoid per-frame allocation) ──
    TSharedPtr<IImageWrapper> ImageWrapper;

    // ── Overlay rendering ───────────────────────────────────────────────────
    TUniquePtr<FWidgetRenderer> WidgetRenderer;
    TSharedPtr<SWidget> OverlaySlateWidget;
    TArray<FColor> CachedOverlayPixels;
    int32 OverlayFrameCounter = 0;
    int32 OverlayDrawCount = 0;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void SetupCaptureComponent();

public:
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "ImageCapture")
    void CaptureNonBlocking();
};