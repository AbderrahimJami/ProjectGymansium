// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "GymansiumNNEController.generated.h"

class AGymansiumNavPawn;
class AGymansiumGoalActor;

/**
 * Drives AGymansiumNavPawn using a trained ONNX policy loaded via UE's NNE.
 *
 * Setup:
 *   1. Export a checkpoint with export_onnx.py
 *   2. Import the .onnx file into the Content Browser (creates a UNNEModelData asset)
 *   3. Place this actor in the level alongside the NavPawn and GoalActor
 *   4. Assign PolicyModelData, AgentPawn, and GoalActor in the Details panel
 *   5. Set ObservationDim to match your training obs size (e.g. 4 for state-only)
 *   6. Enable the NNEOnnxRuntime plugin in your .uproject if not already active
 *
 * The controller runs standalone — no Python or gRPC connection required.
 */
UCLASS(Blueprintable)
class PROJECTGYMANSIUM_API AGymansiumNNEController : public AActor
{
	GENERATED_BODY()

public:
	AGymansiumNNEController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The UNNEModelData asset created when you import the .onnx into the Content Browser.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|NNE")
	TObjectPtr<UNNEModelData> PolicyModelData;

	// Observation vector size — must match the obs_dim used during export_onnx.py.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|NNE")
	int32 ObservationDim = 4;

	// How often the policy is queried (seconds). Match StepDurationSeconds from training.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|NNE")
	float StepDurationSeconds = 0.1f;

	// Maximum distance for goal-bearing computation (should match training SpawnRadius * 2).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|NNE")
	float MaxObservationDistance = 1200.0f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Gymansium|NNE")
	TObjectPtr<AGymansiumNavPawn> AgentPawn;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Gymansium|NNE")
	TObjectPtr<AGymansiumGoalActor> GoalActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|NNE")
	bool bModelLoaded = false;

private:
	void LoadModel();
	void OnStepTimer();
	TArray<float> BuildObservation() const;
	bool RunInference(const TArray<float>& Obs, float& OutThrottle, float& OutTurn);

	TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance;
	FTimerHandle StepTimerHandle;
};
