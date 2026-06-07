// Copyright (c) 2024 Nikita Petrov (https://github.com/NikkittaP)
// SPDX-License-Identifier: MIT

#include "StreamManagerMJPEG.h"
#include "MJPEGStreamerImpl.h"

DEFINE_LOG_CATEGORY(LogStreamMJPEG);

// #include "Engine.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"

#include "UnrealClient.h"
#include "TextureResource.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "ShowFlags.h"

#include "Materials/Material.h"

#include "RHICommandList.h"
#include "RenderingThread.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

#include "ImageUtils.h"

#include "Modules/ModuleManager.h"

#include "Slate/WidgetRenderer.h"
#include "Blueprint/UserWidget.h"

AStreamManagerMJPEG::AStreamManagerMJPEG()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    
    // Initialize Pimpl
    StreamerImpl = MakeUnique<FMJPEGStreamerImpl>();
}

// Called when the game starts or when spawned
void AStreamManagerMJPEG::BeginPlay()
{
    Super::BeginPlay();

    if (CaptureComponent)
    {
        SetupCaptureComponent();

        StreamerImpl->Start(ServerPort);
    }
    else
    {
        UE_LOG(LogStreamMJPEG, Error, TEXT("No CaptureComponent set!"));
    }
}

void AStreamManagerMJPEG::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Drain render request queue to prevent memory leaks
    int32 DrainedCount = 0;
    while (!RenderRequestQueue.IsEmpty())
    {
        FRenderRequestStreamMJPEGStruct* Request = nullptr;
        RenderRequestQueue.Dequeue(Request);
        if (Request)
        {
            Request->RenderFence.Wait();
            delete Request;
            QueueSize--;
            DrainedCount++;
        }
    }
    if (DrainedCount > 0)
    {
        UE_LOG(LogStreamMJPEG, Verbose, TEXT("EndPlay: Drained %d pending render requests"), DrainedCount);
    }

    // Release reusable resources
    ImageWrapper.Reset();
    CachedOverlayPixels.Empty();
    WidgetRenderer.Reset();
    OverlaySlateWidget.Reset();

    StreamerImpl->Stop();
    Super::EndPlay(EndPlayReason);
}

// Called every frame
void AStreamManagerMJPEG::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Check for queue overflow (memory leak detection)
    int32 CurrentQueueSize = QueueSize.load();
    
    if (CurrentQueueSize > 10)
    {
        UE_LOG(LogStreamMJPEG, Error, TEXT("StreamManagerMJPEG: RenderRequestQueue overflow! Size: %d. Memory leak detected!"), CurrentQueueSize);
        
        // Emergency cleanup: drain queue to prevent OOM
        while (!RenderRequestQueue.IsEmpty())
        {
            FRenderRequestStreamMJPEGStruct* Request = nullptr;
            RenderRequestQueue.Dequeue(Request);
            if (Request)
            {
                delete Request;
                QueueSize--;
            }
        }
        UE_LOG(LogStreamMJPEG, Verbose, TEXT("StreamManagerMJPEG: Emergency queue cleanup performed"));
        return;
    }

    if (!RenderRequestQueue.IsEmpty())
    {
        // ── Drain all completed requests, keep only the newest ──────────────
        // This prevents queue backup: during hitches multiple frames pile up.
        // We skip stale frames (just free them) and only JPEG-encode the latest.
        FRenderRequestStreamMJPEGStruct* latestReady = nullptr;
        int32 SkippedStale = 0;

        while (!RenderRequestQueue.IsEmpty())
        {
            FRenderRequestStreamMJPEGStruct* candidate = nullptr;
            RenderRequestQueue.Peek(candidate);
            if (!candidate) break;

            if (!candidate->RenderFence.IsFenceComplete())
                break;  // this and all subsequent are still GPU-pending

            // Dequeue this completed request
            RenderRequestQueue.Pop();
            QueueSize--;

            // If we already have a newer ready frame, discard the older one
            if (latestReady)
            {
                delete latestReady;
                SkippedStale++;
            }
            latestReady = candidate;
        }

        if (SkippedStale > 0)
        {
            UE_LOG(LogStreamMJPEG, Verbose, TEXT("Tick: Skipped %d stale frames, queue=%d"), SkippedStale, QueueSize.load());
        }

        if (latestReady)
        {
            double TickStartTime = FPlatformTime::Seconds();

            // ── Alpha-composite overlay onto scene image ────────────────
            if (latestReady->bHasOverlay &&
                latestReady->OverlayImage.Num() == latestReady->Image.Num())
            {
                // Cache overlay pixels only when throttling (interval > 1)
                if (OverlayRefreshInterval > 1 &&
                    latestReady->OverlayImage.GetData() != CachedOverlayPixels.GetData())
                {
                    CachedOverlayPixels = latestReady->OverlayImage;
                }

                const int32 PixelCount = latestReady->Image.Num();
                FColor* SceneData = latestReady->Image.GetData();
                const FColor* OverlayData = latestReady->OverlayImage.GetData();

                for (int32 i = 0; i < PixelCount; ++i)
                {
                    const uint8 A = OverlayData[i].A;
                    if (A == 0) continue;
                    if (A == 255)
                    {
                        SceneData[i] = OverlayData[i];
                    }
                    else
                    {
                        const uint32 InvA = 255 - A;
                        SceneData[i].R = (uint8)((OverlayData[i].R * A + SceneData[i].R * InvA + 127) / 255);
                        SceneData[i].G = (uint8)((OverlayData[i].G * A + SceneData[i].G * InvA + 127) / 255);
                        SceneData[i].B = (uint8)((OverlayData[i].B * A + SceneData[i].B * InvA + 127) / 255);
                        SceneData[i].A = 255;
                    }
                }
            }

            double AfterComposite = FPlatformTime::Seconds();

            // Lazy-init the reusable image wrapper
            if (!ImageWrapper)
            {
                IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
                ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
            }

            // JPEG encode
            ImageWrapper->SetRaw(latestReady->Image.GetData(), latestReady->Image.GetAllocatedSize(), FrameWidth, FrameHeight, ERGBFormat::BGRA, 8);
            const TArray64<uint8>& ImgData = ImageWrapper->GetCompressed(0);

            double AfterJpeg = FPlatformTime::Seconds();

            // Publish JPEG data — reuse std::string to avoid per-frame alloc
            StreamerImpl->Publish("/stream.mjpg", std::string(reinterpret_cast<const char*>(ImgData.GetData()), ImgData.Num()));

            double AfterPublish = FPlatformTime::Seconds();

            ImgCounter += 1;

            // Log timing every 100 frames
            if (ImgCounter % 100 == 0)
            {
                double CompMs = (AfterComposite - TickStartTime) * 1000.0;
                double JpegMs = (AfterJpeg - AfterComposite) * 1000.0;
                double PublishMs = (AfterPublish - AfterJpeg) * 1000.0;
                double TotalMs = (AfterPublish - TickStartTime) * 1000.0;
                UE_LOG(LogStreamMJPEG, Verbose, TEXT("Frame %d | Queue=%d | Composite=%.1fms JPEG=%.1fms Publish=%.1fms Total=%.1fms | Scene=%d Overlay=%d pixels | JPEG=%lldB"),
                    ImgCounter, QueueSize.load(),
                    CompMs, JpegMs, PublishMs, TotalMs,
                    latestReady->Image.Num(),
                    latestReady->OverlayImage.Num(),
                    ImgData.Num());
            }

            delete latestReady;
        }
    }
}

void AStreamManagerMJPEG::SetupCaptureComponent()
{
    if (!IsValid(CaptureComponent))
    {
        UE_LOG(LogStreamMJPEG, Error, TEXT("SetupCaptureComponent: CaptureComponent is not valid!"));
        return;
    }

    // Create RenderTargets
    UTextureRenderTarget2D *renderTarget2D = NewObject<UTextureRenderTarget2D>(this);

    // Color Capture
    renderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;   // 8-bit color format
    renderTarget2D->InitCustomFormat(FrameWidth, FrameHeight, PF_B8G8R8A8, true); // PF... disables HDR, which is most important since HDR gives gigantic overhead, and is not needed!
    UE_LOG(LogStreamMJPEG, Verbose, TEXT("Set Render Format for Color-Like-Captures"));

    renderTarget2D->bGPUSharedFlag = true; // demand buffer on GPU

    // Assign RenderTarget
    CaptureComponent->GetCaptureComponent2D()->TextureTarget = renderTarget2D;
    // Set Camera Properties
    CaptureComponent->GetCaptureComponent2D()->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    CaptureComponent->GetCaptureComponent2D()->TextureTarget->TargetGamma = GEngine->GetDisplayGamma();
    CaptureComponent->GetCaptureComponent2D()->ShowFlags.SetTemporalAA(true);
    // lookup more showflags in the UE4 documentation..

    UE_LOG(LogStreamMJPEG, Verbose, TEXT("Initialized RenderTarget!"));
}

void AStreamManagerMJPEG::CaptureNonBlocking()
{
    if (!IsValid(CaptureComponent))
    {
        UE_LOG(LogStreamMJPEG, Error, TEXT("CaptureColorNonBlocking: CaptureComponent was not valid!"));
        return;
    }
    
    // Prevent queue overflow - skip capture if queue is too large
    int32 CurrentQueueSize = QueueSize.load();
    if (CurrentQueueSize > 5)
    {
        UE_LOG(LogStreamMJPEG, Verbose, TEXT("CaptureNonBlocking: Skipping capture, queue size: %d"), CurrentQueueSize);
        return;
    }
    
    if (VerboseLogging)
    {
        UE_LOG(LogStreamMJPEG, Verbose, TEXT("Entering: CaptureNonBlocking"));
    }
    CaptureComponent->GetCaptureComponent2D()->TextureTarget->TargetGamma = GEngine->GetDisplayGamma();

    // Get RenderContext
    FTextureRenderTargetResource *renderTargetResource = CaptureComponent->GetCaptureComponent2D()->TextureTarget->GameThread_GetRenderTargetResource();
    if (VerboseLogging)
    {
        UE_LOG(LogStreamMJPEG, Verbose, TEXT("Got display gamma"));
    }
    struct FReadSurfaceContext
    {
        FRenderTarget *SrcRenderTarget;
        TArray<FColor> *OutData;
        FIntRect Rect;
        FReadSurfaceDataFlags Flags;
    };
    if (VerboseLogging)
    {
        UE_LOG(LogStreamMJPEG, Verbose, TEXT("Inited ReadSurfaceContext"));
    }
    // Init new RenderRequest
    FRenderRequestStreamMJPEGStruct *renderRequest = new FRenderRequestStreamMJPEGStruct();
    if (VerboseLogging)
    {
        UE_LOG(LogStreamMJPEG, Verbose, TEXT("inited renderrequest"));
    }

    // ── Render overlay widget every frame, but throttle GPU readback ──────
    // DrawWidget is cheap (~0.5ms) so we always re-render to keep label
    // positions current. The expensive GPU ReadSurfaceData is throttled.
    bool bFreshOverlayRender = false;
    if (OverlaySlateWidget.IsValid() && WidgetRenderer.IsValid() && OverlayRenderTarget)
    {
        FTextureRenderTargetResource* OverlayResource = OverlayRenderTarget->GameThread_GetRenderTargetResource();
        if (OverlayResource)
        {
            double DrawStart = FPlatformTime::Seconds();
            WidgetRenderer->DrawWidget(OverlayResource, OverlaySlateWidget.ToSharedRef(),
                FVector2D(FrameWidth, FrameHeight), GetWorld()->GetDeltaSeconds());
            double DrawEnd = FPlatformTime::Seconds();

            OverlayDrawCount++;
            if (OverlayDrawCount % 100 == 0)
            {
                UE_LOG(LogStreamMJPEG, Verbose, TEXT("DrawWidget took %.1fms (call #%d, queue=%d)"),
                    (DrawEnd - DrawStart) * 1000.0, OverlayDrawCount, CurrentQueueSize);
            }

            // Decide whether to do a GPU readback this frame
            OverlayFrameCounter++;
            if (OverlayFrameCounter >= OverlayRefreshInterval)
            {
                OverlayFrameCounter = 0;
                bFreshOverlayRender = true;
                renderRequest->bHasOverlay = true;
            }
            else if (CachedOverlayPixels.Num() > 0)
            {
                // Use cached pixels from last GPU readback
                renderRequest->OverlayImage = CachedOverlayPixels;
                renderRequest->bHasOverlay = true;
            }
        }
    }

    // Setup GPU command
    FReadSurfaceContext readSurfaceContext = {
        renderTargetResource,
        &(renderRequest->Image),
        FIntRect(0, 0, renderTargetResource->GetSizeXY().X, renderTargetResource->GetSizeXY().Y),
        FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)};
    if (VerboseLogging)
    {
        UE_LOG(LogStreamMJPEG, Verbose, TEXT("GPU Command complete"));
    }

    // ── Enqueue scene read ──────────────────────────────────────────────────
    ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)
    (
        [readSurfaceContext](FRHICommandListImmediate &RHICmdList)
        {
            RHICmdList.ReadSurfaceData(
                readSurfaceContext.SrcRenderTarget->GetRenderTargetTexture(),
                readSurfaceContext.Rect,
                *readSurfaceContext.OutData,
                readSurfaceContext.Flags);
        });

    // ── Enqueue overlay read (only on fresh renders, not cached frames) ───
    if (bFreshOverlayRender)
    {
        FTextureRenderTargetResource* OverlayResource = OverlayRenderTarget->GameThread_GetRenderTargetResource();
        FReadSurfaceContext overlayContext = {
            OverlayResource,
            &(renderRequest->OverlayImage),
            FIntRect(0, 0, OverlayResource->GetSizeXY().X, OverlayResource->GetSizeXY().Y),
            FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)};

        ENQUEUE_RENDER_COMMAND(OverlayDrawCompletion)
        (
            [overlayContext](FRHICommandListImmediate &RHICmdList)
            {
                RHICmdList.ReadSurfaceData(
                    overlayContext.SrcRenderTarget->GetRenderTargetTexture(),
                    overlayContext.Rect,
                    *overlayContext.OutData,
                    overlayContext.Flags);
            });
    }

    // Notifiy new task in RenderQueue
    RenderRequestQueue.Enqueue(renderRequest);
    QueueSize++;

    // Set RenderCommandFence
    renderRequest->RenderFence.BeginFence();
}

void AStreamManagerMJPEG::UpdateRenderTargetAfterFrameSizeChanged()
{
    if (!IsValid(CaptureComponent))
    {
        UE_LOG(LogStreamMJPEG, Error, TEXT("UpdateRenderTargetAfterFrameSizeChanged: CaptureComponent is not valid!"));
        return;
    }

    CaptureComponent->GetCaptureComponent2D()->TextureTarget->InitCustomFormat(FrameWidth, FrameHeight, PF_B8G8R8A8, true); // PF... disables HDR, which is most important since HDR gives gigantic overhead, and is not needed!

    // Re-create overlay render target if an overlay widget is active
    if (OverlaySlateWidget.IsValid())
    {
        if (!OverlayRenderTarget)
        {
            OverlayRenderTarget = NewObject<UTextureRenderTarget2D>(this);
        }
        OverlayRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
        OverlayRenderTarget->ClearColor = FLinearColor::Transparent;
        OverlayRenderTarget->bGPUSharedFlag = true;
        OverlayRenderTarget->InitCustomFormat(FrameWidth, FrameHeight, PF_B8G8R8A8, true);
    }
}

void AStreamManagerMJPEG::SetOverlayWidget(UUserWidget* InWidget)
{
    if (InWidget)
    {
        OverlaySlateWidget = InWidget->TakeWidget();

        if (!WidgetRenderer)
        {
            // bUseGammaCorrection = true
            WidgetRenderer = MakeUnique<FWidgetRenderer>(/*bUseGammaCorrection=*/true);
        }

        if (!OverlayRenderTarget)
        {
            OverlayRenderTarget = NewObject<UTextureRenderTarget2D>(this);
            OverlayRenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
            OverlayRenderTarget->ClearColor = FLinearColor::Transparent;
            OverlayRenderTarget->bGPUSharedFlag = true;
            OverlayRenderTarget->InitCustomFormat(FrameWidth, FrameHeight, PF_B8G8R8A8, true);
        }

        UE_LOG(LogStreamMJPEG, Verbose, TEXT("Overlay widget set (%dx%d)"), FrameWidth, FrameHeight);
    }
    else
    {
        OverlaySlateWidget.Reset();
        UE_LOG(LogStreamMJPEG, Verbose, TEXT("Overlay widget cleared"));
    }
}