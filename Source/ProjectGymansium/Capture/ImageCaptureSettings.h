// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCaptureSettings.generated.h"

USTRUCT(BlueprintType)
struct FImageCaptureSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageCapture")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageCapture", meta = (ClampMin = "0.01"))
	float CaptureIntervalSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageCapture", meta = (ClampMin = "1"))
	int32 ResolutionWidth = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageCapture", meta = (ClampMin = "1"))
	int32 ResolutionHeight = 512;

	/** One of: FinalColor, FinalColorHDR, SceneDepth, Normal, BaseColor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageCapture")
	FString CaptureMode = TEXT("FinalColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ImageCapture")
	FString OutputSubfolder = TEXT("CameraCaptures");

	static bool LoadFromJsonFile(const FString& FilePath, FImageCaptureSettings& OutSettings);
};
