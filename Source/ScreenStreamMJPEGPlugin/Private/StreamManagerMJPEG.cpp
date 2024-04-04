// Fill out your copyright notice in the Description page of Project Settings.

#include "StreamManagerMJPEG.h"

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

#include "mjpeg_streamer.hpp"

using MJPEGStreamer = nadjieb::MJPEGStreamer;

AStreamManagerMJPEG::AStreamManagerMJPEG()
{
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AStreamManagerMJPEG::BeginPlay()
{
    Super::BeginPlay();

    if (CaptureComponent)
    {
        SetupCaptureComponent();

        streamer.start(ServerPort);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("No CaptureComponent set!"));
    }
}

void AStreamManagerMJPEG::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    streamer.stop();
    Super::EndPlay(EndPlayReason);
}

// Called every frame
void AStreamManagerMJPEG::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!RenderRequestQueue.IsEmpty())
    {
        // Peek the next RenderRequest from queue
        FRenderRequestStreamMJPEGStruct *nextRenderRequest = nullptr;
        RenderRequestQueue.Peek(nextRenderRequest);

        if (nextRenderRequest)
        { // nullptr check
            if (nextRenderRequest->RenderFence.IsFenceComplete())
            {
                // Check if rendering is done, indicated by RenderFence
                // Load the image wrapper module
                IImageWrapperModule &ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

                // Prepare data to be JPEG
                static TSharedPtr<IImageWrapper> imageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG); // EImageFormat::JPEG
                imageWrapper->SetRaw(nextRenderRequest->Image.GetData(), nextRenderRequest->Image.GetAllocatedSize(), FrameWidth, FrameHeight, ERGBFormat::BGRA, 8);
                const TArray64<uint8> &ImgData = imageWrapper->GetCompressed(0);

                // Copy TArray64<uint8> to std::vector<uint8_t>
                std::vector<uint8_t> vectorBuffer(ImgData.GetData(), ImgData.GetData() + ImgData.Num());

                // Construct std::string from std::vector<uint8_t>
                streamer.publish("/stream.mjpg", std::string(vectorBuffer.begin(), vectorBuffer.end()));

                ImgCounter += 1;

                // Delete the first element from RenderQueue
                RenderRequestQueue.Pop();
                delete nextRenderRequest;
            }
        }
    }
}

void AStreamManagerMJPEG::SetupCaptureComponent()
{
    if (!IsValid(CaptureComponent))
    {
        UE_LOG(LogTemp, Error, TEXT("SetupCaptureComponent: CaptureComponent is not valid!"));
        return;
    }

    // Create RenderTargets
    UTextureRenderTarget2D *renderTarget2D = NewObject<UTextureRenderTarget2D>();

    // Color Capture
    renderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;   // 8-bit color format
    renderTarget2D->InitCustomFormat(FrameWidth, FrameHeight, PF_B8G8R8A8, true); // PF... disables HDR, which is most important since HDR gives gigantic overhead, and is not needed!
    UE_LOG(LogTemp, Warning, TEXT("Set Render Format for Color-Like-Captures"));

    renderTarget2D->bGPUSharedFlag = true; // demand buffer on GPU

    // Assign RenderTarget
    CaptureComponent->GetCaptureComponent2D()->TextureTarget = renderTarget2D;
    // Set Camera Properties
    CaptureComponent->GetCaptureComponent2D()->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    CaptureComponent->GetCaptureComponent2D()->TextureTarget->TargetGamma = GEngine->GetDisplayGamma();
    CaptureComponent->GetCaptureComponent2D()->ShowFlags.SetTemporalAA(true);
    // lookup more showflags in the UE4 documentation..

    UE_LOG(LogTemp, Warning, TEXT("Initialized RenderTarget!"));
}

void AStreamManagerMJPEG::CaptureNonBlocking()
{
    if (!IsValid(CaptureComponent))
    {
        UE_LOG(LogTemp, Error, TEXT("CaptureColorNonBlocking: CaptureComponent was not valid!"));
        return;
    }
    if (VerboseLogging)
    {
        UE_LOG(LogTemp, Warning, TEXT("Entering: CaptureNonBlocking"));
    }
    CaptureComponent->GetCaptureComponent2D()->TextureTarget->TargetGamma = GEngine->GetDisplayGamma();

    // Get RenderContext
    FTextureRenderTargetResource *renderTargetResource = CaptureComponent->GetCaptureComponent2D()->TextureTarget->GameThread_GetRenderTargetResource();
    if (VerboseLogging)
    {
        UE_LOG(LogTemp, Warning, TEXT("Got display gamma"));
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
        UE_LOG(LogTemp, Warning, TEXT("Inited ReadSurfaceContext"));
    }
    // Init new RenderRequest
    FRenderRequestStreamMJPEGStruct *renderRequest = new FRenderRequestStreamMJPEGStruct();
    if (VerboseLogging)
    {
        UE_LOG(LogTemp, Warning, TEXT("inited renderrequest"));
    }

    // Setup GPU command
    FReadSurfaceContext readSurfaceContext = {
        renderTargetResource,
        &(renderRequest->Image),
        FIntRect(0, 0, renderTargetResource->GetSizeXY().X, renderTargetResource->GetSizeXY().Y),
        FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)};
    if (VerboseLogging)
    {
        UE_LOG(LogTemp, Warning, TEXT("GPU Command complete"));
    }
    // Send command to GPU
    /* Up to version 4.22 use this
    ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
        SceneDrawCompletion,//ReadSurfaceCommand,
        FReadSurfaceContext, Context, readSurfaceContext,
    {
        RHICmdList.ReadSurfaceData(
            Context.SrcRenderTarget->GetRenderTargetTexture(),
            Context.Rect,
            *Context.OutData,
            Context.Flags
        );
    });
    */
    // Above 4.22 use this
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

    // Notifiy new task in RenderQueue
    RenderRequestQueue.Enqueue(renderRequest);

    // Set RenderCommandFence
    renderRequest->RenderFence.BeginFence();
}

void AStreamManagerMJPEG::UpdateRenderTargetAfterFrameSizeChanged()
{
    CaptureComponent->GetCaptureComponent2D()->TextureTarget->InitCustomFormat(FrameWidth, FrameHeight, PF_B8G8R8A8, true); // PF... disables HDR, which is most important since HDR gives gigantic overhead, and is not needed!
}