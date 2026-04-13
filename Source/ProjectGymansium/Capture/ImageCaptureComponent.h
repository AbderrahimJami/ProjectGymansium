// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ImageCaptureSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHIGPUReadback.h"
#include "ImageCaptureComponent.generated.h"
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTGYMANSIUM_API UImageCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UImageCaptureComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Gymansium|ImageCapture")
	void StartCapture();

	UFUNCTION(BlueprintCallable, Category = "Gymansium|ImageCapture")
	void StopCapture();

	UFUNCTION(BlueprintCallable, Category = "Gymansium|ImageCapture")
	void CaptureNow();

	UFUNCTION(BlueprintCallable, Category = "Gymansium|ImageCapture")
	bool LoadSettings();

	UFUNCTION(BlueprintCallable, Category = "Gymansium|ImageCapture")
	void SetEnabled(bool bNewEnabled);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|ImageCapture")
	FImageCaptureSettings Settings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|ImageCapture")
	FString SettingsFileName = TEXT("Settings.json");

private:
	void OnCaptureTimer();
	void ProcessReadback();
	void EnqueueReadback();
	void EnqueueAsyncWrite(TArray<FColor>&& Bitmap, int32 Width, int32 Height, const FString& FilePath);
	FString GetSettingsFilePath() const;
	FString GetOutputDirectory() const;
	ESceneCaptureSource StringToCaptureSource(const FString& ModeName) const;
	ETextureRenderTargetFormat GetRenderTargetFormat(ESceneCaptureSource Source) const;
	void CreateCaptureResources();
	void DestroyCaptureResources();

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	FTimerHandle CaptureTimerHandle;
	int64 CaptureSequenceNumber = 0;

	TUniquePtr<FRHIGPUTextureReadback> PendingReadback;
	bool bReadbackInFlight = false;
	bool bCapturePending = false;
	int64 PendingSequenceNumber = 0;
};
