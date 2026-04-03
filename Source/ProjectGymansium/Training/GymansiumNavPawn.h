// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GymansiumNavPawn.generated.h"

class UCameraComponent;
class UCapsuleComponent;
class UStaticMeshComponent;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Navigation")
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Navigation")
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Navigation")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float MoveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float TurnSpeedDegrees = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float ReverseSpeedMultiplier = 0.5f;

private:
	UPROPERTY()
	FVector LastVelocity = FVector::ZeroVector;

	UPROPERTY()
	bool bLastMoveBlocked = false;
};
