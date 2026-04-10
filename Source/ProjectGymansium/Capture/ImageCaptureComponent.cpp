// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImageCaptureComponent.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogImageCapture, Log, All);

UImageCaptureComponent::UImageCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

	CaptureComponent->CaptureScene();

	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource)
	{
		return;
	}

	const int32 Width = RenderTarget->GetSurfaceWidth();
	const int32 Height = RenderTarget->GetSurfaceHeight();
	const bool bIsDepth = CaptureComponent->CaptureSource == ESceneCaptureSource::SCS_SceneDepth;

	TArray<FColor> Bitmap;

	if (bIsDepth)
	{
		TArray<FLinearColor> LinearBitmap;
		Resource->ReadLinearColorPixels(LinearBitmap);
		if (LinearBitmap.Num() != Width * Height)
		{
			UE_LOG(LogImageCapture, Warning, TEXT("ReadLinearColorPixels failed for capture #%lld"), CaptureSequenceNumber);
			return;
		}

		// Find max depth for normalization (R channel holds depth in world units)
		float MaxDepth = 1.0f;
		for (const FLinearColor& Pixel : LinearBitmap)
		{
			if (FMath::IsFinite(Pixel.R) && Pixel.R > MaxDepth)
			{
				MaxDepth = Pixel.R;
			}
		}

		Bitmap.SetNum(LinearBitmap.Num());
		for (int32 i = 0; i < LinearBitmap.Num(); ++i)
		{
			const float Depth = FMath::IsFinite(LinearBitmap[i].R) ? LinearBitmap[i].R : MaxDepth;
			const uint8 Gray = static_cast<uint8>(FMath::Clamp(Depth / MaxDepth, 0.0f, 1.0f) * 255.0f);
			Bitmap[i] = FColor(Gray, Gray, Gray, 255);
		}
	}
	else
	{
		if (!Resource->ReadPixels(Bitmap))
		{
			UE_LOG(LogImageCapture, Warning, TEXT("ReadPixels failed for capture #%lld"), CaptureSequenceNumber);
			return;
		}
	}

	// Force alpha to fully opaque — some capture modes (Normal, BaseColor) return alpha=0
	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}

	const FDateTime Now = FDateTime::Now();
	const FString Timestamp = Now.ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString FileName = FString::Printf(TEXT("%s_%s_%06lld.png"), *Settings.CaptureMode, *Timestamp, CaptureSequenceNumber);
	const FString FilePath = FPaths::Combine(GetOutputDirectory(), FileName);

	SaveImageToDisk(Bitmap, Width, Height, FilePath);
	++CaptureSequenceNumber;
}

void UImageCaptureComponent::SaveImageToDisk(const TArray<FColor>& Bitmap, int32 Width, int32 Height, const FString& FilePath)
{
	TArray64<uint8> CompressedData;
	FImageUtils::PNGCompressImageArray(Width, Height, Bitmap, CompressedData);

	if (CompressedData.Num() == 0)
	{
		UE_LOG(LogImageCapture, Warning, TEXT("PNG compression failed for: %s"), *FilePath);
		return;
	}

	if (!FFileHelper::SaveArrayToFile(CompressedData, *FilePath))
	{
		UE_LOG(LogImageCapture, Warning, TEXT("Failed to write file: %s"), *FilePath);
		return;
	}

	UE_LOG(LogImageCapture, Verbose, TEXT("Saved capture: %s (%dx%d)"), *FilePath, Width, Height);
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
