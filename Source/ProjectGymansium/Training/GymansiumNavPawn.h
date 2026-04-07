// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GymansiumNavPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

UCLASS(Blueprintable)
class PROJECTGYMANSIUM_API AGymansiumNavPawn : public APawn
{
	GENERATED_BODY()

public:
	AGymansiumNavPawn();

	UFUNCTION(BlueprintCallable, Category = "Gymansium|Navigation")
	void ApplyAction(float ThrottleInput, float TurnInput, float StepSeconds);

	UFUNCTION(BlueprintCallable, Category = "Gymansium|Navigation")
	void ResetToTransform(const FTransform& SpawnTransform);

	UFUNCTION(BlueprintPure, Category = "Gymansium|Navigation")
	FVector GetLastVelocity() const;

	UFUNCTION(BlueprintPure, Category = "Gymansium|Navigation")
	bool WasLastMoveBlocked() const;

	UFUNCTION(BlueprintCallable, Category = "Gymansium|Vision")
	void ConfigureVisionCapture(int32 InWidth, int32 InHeight);

	UFUNCTION(BlueprintCallable, Category = "Gymansium|Vision")
	bool CaptureVisionObservation(TArray<float>& OutImageValues, TArray<int>& OutShape);

	UFUNCTION(BlueprintCallable, Category = "Gymansium|Navigation")
	void GetRaycastDistances(int32 NumRays, float MaxRange, TArray<float>& OutDistances) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Navigation")
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Navigation")
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Navigation")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Vision")
	TObjectPtr<USceneCaptureComponent2D> VisionCaptureComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Vision")
	TObjectPtr<UTextureRenderTarget2D> VisionRenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float MoveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float TurnSpeedDegrees = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float ReverseSpeedMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Vision")
	int32 VisionWidth = 84;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Vision")
	int32 VisionHeight = 84;

private:
	void EnsureVisionRenderTarget();

	UPROPERTY()
	FVector LastVelocity = FVector::ZeroVector;

	UPROPERTY()
	bool bLastMoveBlocked = false;
};
