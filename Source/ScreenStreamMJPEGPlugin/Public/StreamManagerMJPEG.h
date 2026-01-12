// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

class ASceneCapture2D;
class UMaterial;

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Queue.h"
#include "Async/AsyncWork.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStreamMJPEG, Log, All);

#include "mjpeg_streamer.hpp"

#include "StreamManagerMJPEG.generated.h"

USTRUCT()
struct FRenderRequestStreamMJPEGStruct
{
    GENERATED_BODY()

    TArray<FColor> Image;
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

    UFUNCTION(BlueprintCallable, Category = "Stream")
    void UpdateRenderTargetAfterFrameSizeChanged();

protected:
    nadjieb::MJPEGStreamer streamer;

    // RenderRequest Queue
    TQueue<FRenderRequestStreamMJPEGStruct*> RenderRequestQueue;
    
    // Queue size tracker (TQueue doesn't expose size)
    std::atomic<int32> QueueSize{0};

    int ImgCounter = 0;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void SetupCaptureComponent();

public:
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "ImageCapture")
    void CaptureNonBlocking();
};