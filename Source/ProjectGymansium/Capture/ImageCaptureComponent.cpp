// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageCaptureComponent.h"

#include "ImageWriteQueue.h"
#include "ImageWriteTask.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/Paths.h"
#include "RHIGPUReadback.h"
#include "TextureResource.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogImageCapture, Log, All);

UImageCaptureComponent::UImageCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bAutoActivate = true;
}

void UImageCaptureComponent::BeginPlay()
{
	Super::BeginPlay();

	LoadSettings();

	if (Settings.bEnabled)
	{
		StartCapture();
	}
}

void UImageCaptureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCapture();
	Super::EndPlay(EndPlayReason);
}

void UImageCaptureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bReadbackInFlight && PendingReadback && PendingReadback->IsReady())
	{
		ProcessReadback();
	}
}

bool UImageCaptureComponent::LoadSettings()
{
	const FString FilePath = GetSettingsFilePath();
	return FImageCaptureSettings::LoadFromJsonFile(FilePath, Settings);
}

void UImageCaptureComponent::SetEnabled(bool bNewEnabled)
{
	if (bNewEnabled == Settings.bEnabled)
	{
		return;
	}

	Settings.bEnabled = bNewEnabled;

	if (bNewEnabled)
	{
		StartCapture();
	}
	else
	{
		StopCapture();
	}
}

void UImageCaptureComponent::StartCapture()
{
	StopCapture();
	CreateCaptureResources();

	if (!CaptureComponent || !RenderTarget)
	{
		UE_LOG(LogImageCapture, Error, TEXT("Failed to create capture resources — capture will not start"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FString OutputDir = GetOutputDirectory();
	IFileManager::Get().MakeDirectory(*OutputDir, true);

	CaptureSequenceNumber = 0;
	bReadbackInFlight = false;
	PendingReadback.Reset();

	SetComponentTickEnabled(true);

	World->GetTimerManager().SetTimer(
		CaptureTimerHandle,
		this,
		&UImageCaptureComponent::OnCaptureTimer,
		Settings.CaptureIntervalSeconds,
		true);

	UE_LOG(LogImageCapture, Log, TEXT("Image capture started — Mode=%s Res=%dx%d Interval=%.2fs Output=%s"),
		*Settings.CaptureMode,
		Settings.ResolutionWidth,
		Settings.ResolutionHeight,
		Settings.CaptureIntervalSeconds,
		*OutputDir);
}

void UImageCaptureComponent::StopCapture()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CaptureTimerHandle);
	}

	SetComponentTickEnabled(false);
	bReadbackInFlight = false;
	PendingReadback.Reset();
	DestroyCaptureResources();
}

void UImageCaptureComponent::CaptureNow()
{
	if (!CaptureComponent || !RenderTarget)
	{
		CreateCaptureResources();
	}

	if (!CaptureComponent || !RenderTarget)
	{
		UE_LOG(LogImageCapture, Warning, TEXT("Cannot capture — no capture resources available"));
		return;
	}

	OnCaptureTimer();
}

void UImageCaptureComponent::OnCaptureTimer()
{
	if (!CaptureComponent || !RenderTarget)
	{
		return;
	}

	// Skip if a readback is still in flight — don't pile up
	if (bReadbackInFlight)
	{
		return;
	}

	CaptureComponent->CaptureScene();
	PendingSequenceNumber = CaptureSequenceNumber;
	++CaptureSequenceNumber;
	EnqueueReadback();
}

void UImageCaptureComponent::EnqueueReadback()
{
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource)
	{
		return;
	}

	FTextureRHIRef TextureRHI = Resource->GetRenderTargetTexture();
	if (!TextureRHI.IsValid())
	{
		return;
	}

	PendingReadback = MakeUnique<FRHIGPUTextureReadback>(FName(TEXT("ImageCaptureReadback")));
	bReadbackInFlight = true;

	FRHIGPUTextureReadback* Readback = PendingReadback.Get();

	ENQUEUE_RENDER_COMMAND(ImageCaptureEnqueueCopy)(
		[Readback, TextureRHI](FRHICommandListImmediate& RHICmdList)
		{
			Readback->EnqueueCopy(RHICmdList, TextureRHI);
		});
}

void UImageCaptureComponent::ProcessReadback()
{
	if (!PendingReadback || !RenderTarget)
	{
		bReadbackInFlight = false;
		return;
	}

	const int32 Width = RenderTarget->GetSurfaceWidth();
	const int32 Height = RenderTarget->GetSurfaceHeight();
	const bool bIsDepth = CaptureComponent && CaptureComponent->CaptureSource == ESceneCaptureSource::SCS_SceneDepth;

	int32 RowPitchInPixels = 0;
	void* RawData = PendingReadback->Lock(RowPitchInPixels);

	if (!RawData)
	{
		UE_LOG(LogImageCapture, Warning, TEXT("Failed to lock readback buffer for capture #%lld"), PendingSequenceNumber);
		PendingReadback->Unlock();
		PendingReadback.Reset();
		bReadbackInFlight = false;
		return;
	}

	TArray<FColor> Bitmap;

	if (bIsDepth)
	{
		// Float16 RGBA data — read R channel as depth, normalize to grayscale
		const FFloat16Color* SrcData = static_cast<const FFloat16Color*>(RawData);

		// First pass: find max depth
		float MaxDepth = 1.0f;
		for (int32 Row = 0; Row < Height; ++Row)
		{
			const FFloat16Color* RowPtr = SrcData + (Row * RowPitchInPixels);
			for (int32 Col = 0; Col < Width; ++Col)
			{
				const float Depth = RowPtr[Col].R.GetFloat();
				if (FMath::IsFinite(Depth) && Depth > MaxDepth)
				{
					MaxDepth = Depth;
				}
			}
		}

		// Second pass: normalize to grayscale
		Bitmap.SetNum(Width * Height);
		for (int32 Row = 0; Row < Height; ++Row)
		{
			const FFloat16Color* RowPtr = SrcData + (Row * RowPitchInPixels);
			for (int32 Col = 0; Col < Width; ++Col)
			{
				const float Depth = RowPtr[Col].R.GetFloat();
				const float NormDepth = FMath::IsFinite(Depth) ? Depth : MaxDepth;
				const uint8 Gray = static_cast<uint8>(FMath::Clamp(NormDepth / MaxDepth, 0.0f, 1.0f) * 255.0f);
				Bitmap[Row * Width + Col] = FColor(Gray, Gray, Gray, 255);
			}
		}
	}
	else
	{
		// RGBA8 data
		const FColor* SrcData = static_cast<const FColor*>(RawData);
		Bitmap.SetNum(Width * Height);
		for (int32 Row = 0; Row < Height; ++Row)
		{
			const FColor* RowPtr = SrcData + (Row * RowPitchInPixels);
			FMemory::Memcpy(&Bitmap[Row * Width], RowPtr, Width * sizeof(FColor));
		}
	}

	PendingReadback->Unlock();
	PendingReadback.Reset();
	bReadbackInFlight = false;

	// Force alpha to fully opaque — some capture modes (Normal, BaseColor) return alpha=0
	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}

	const FDateTime Now = FDateTime::Now();
	const FString Timestamp = Now.ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString FileName = FString::Printf(TEXT("%s_%s_%06lld.png"), *Settings.CaptureMode, *Timestamp, PendingSequenceNumber);
	const FString FilePath = FPaths::Combine(GetOutputDirectory(), FileName);

	EnqueueAsyncWrite(MoveTemp(Bitmap), Width, Height, FilePath);
}

void UImageCaptureComponent::EnqueueAsyncWrite(TArray<FColor>&& Bitmap, int32 Width, int32 Height, const FString& FilePath)
{
	TArray64<FColor> Pixels64(MoveTemp(Bitmap));

	TUniquePtr<FImageWriteTask> Task = MakeUnique<FImageWriteTask>();
	Task->PixelData = MakeUnique<TImagePixelData<FColor>>(FIntPoint(Width, Height), MoveTemp(Pixels64));
	Task->Filename = FilePath;
	Task->Format = EImageFormat::PNG;
	Task->CompressionQuality = 100;
	Task->bOverwriteFile = true;

	IImageWriteQueueModule& WriteQueueModule = FModuleManager::LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue");
	WriteQueueModule.GetWriteQueue().Enqueue(MoveTemp(Task));

	UE_LOG(LogImageCapture, Verbose, TEXT("Enqueued async write: %s (%dx%d)"), *FilePath, Width, Height);
}

FString UImageCaptureComponent::GetSettingsFilePath() const
{
#if WITH_EDITOR
	return FPaths::Combine(FPaths::ProjectConfigDir(), SettingsFileName);
#else
	return FPaths::Combine(FPaths::LaunchDir(), TEXT("Config"), SettingsFileName);
#endif
}

FString UImageCaptureComponent::GetOutputDirectory() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), Settings.OutputSubfolder);
}

ESceneCaptureSource UImageCaptureComponent::StringToCaptureSource(const FString& ModeName) const
{
	if (ModeName.Equals(TEXT("FinalColor"), ESearchCase::IgnoreCase))
	{
		return ESceneCaptureSource::SCS_FinalColorLDR;
	}
	if (ModeName.Equals(TEXT("FinalColorHDR"), ESearchCase::IgnoreCase))
	{
		return ESceneCaptureSource::SCS_FinalColorHDR;
	}
	if (ModeName.Equals(TEXT("SceneDepth"), ESearchCase::IgnoreCase))
	{
		return ESceneCaptureSource::SCS_SceneDepth;
	}
	if (ModeName.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
	{
		return ESceneCaptureSource::SCS_Normal;
	}
	if (ModeName.Equals(TEXT("BaseColor"), ESearchCase::IgnoreCase))
	{
		return ESceneCaptureSource::SCS_BaseColor;
	}

	UE_LOG(LogImageCapture, Warning, TEXT("Unknown capture mode '%s' — defaulting to FinalColor"), *ModeName);
	return ESceneCaptureSource::SCS_FinalColorLDR;
}

ETextureRenderTargetFormat UImageCaptureComponent::GetRenderTargetFormat(ESceneCaptureSource Source) const
{
	if (Source == ESceneCaptureSource::SCS_SceneDepth)
	{
		return RTF_RGBA16f;
	}
	return RTF_RGBA8;
}

void UImageCaptureComponent::CreateCaptureResources()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const ESceneCaptureSource CaptureSource = StringToCaptureSource(Settings.CaptureMode);

	RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("CaptureRT"));
	RenderTarget->RenderTargetFormat = GetRenderTargetFormat(CaptureSource);
	RenderTarget->bGPUSharedFlag = true;
	RenderTarget->InitAutoFormat(
		FMath::Max(Settings.ResolutionWidth, 1),
		FMath::Max(Settings.ResolutionHeight, 1));
	RenderTarget->UpdateResourceImmediate(true);

	CaptureComponent = NewObject<USceneCaptureComponent2D>(Owner, TEXT("ImageCaptureSceneCapture"));
	CaptureComponent->SetupAttachment(Owner->GetRootComponent());
	CaptureComponent->SetRelativeLocation(FVector(45.0f, 0.0f, 45.0f));
	CaptureComponent->SetRelativeRotation(FRotator::ZeroRotator);
	CaptureComponent->CaptureSource = CaptureSource;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	CaptureComponent->TextureTarget = RenderTarget;
	CaptureComponent->RegisterComponent();
}

void UImageCaptureComponent::DestroyCaptureResources()
{
	if (CaptureComponent)
	{
		CaptureComponent->DestroyComponent();
		CaptureComponent = nullptr;
	}

	RenderTarget = nullptr;
}
