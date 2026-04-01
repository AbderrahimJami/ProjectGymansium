// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Environment/SingleAgentEnvironmentInterface.h"
#include "GameFramework/Actor.h"
#include "GymansiumNavigationEnvironment.generated.h"

class AGymansiumGoalActor;
class AGymansiumNavPawn;
class USceneComponent;

UCLASS(Blueprintable)
class PROJECTGYMANSIUM_API AGymansiumNavigationEnvironment : public AActor, public ISingleAgentScholaEnvironment
{
	GENERATED_BODY()

public:
	AGymansiumNavigationEnvironment();

	virtual void InitializeEnvironment_Implementation(FInteractionDefinition& OutAgentDefinition) override;
	virtual void Reset_Implementation(FInitialAgentState& OutAgentState) override;
	virtual void Step_Implementation(const FInstancedStruct& InAction, FAgentState& OutAgentState) override;
	virtual void SeedEnvironment_Implementation(int Seed) override;
	virtual void SetEnvironmentOptions_Implementation(const TMap<FString, FString>& Options) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Navigation")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Gymansium|Navigation")
	TObjectPtr<AGymansiumNavPawn> AgentPawn;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Gymansium|Navigation")
	TObjectPtr<AGymansiumGoalActor> GoalActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	TSubclassOf<AGymansiumNavPawn> AgentPawnClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	TSubclassOf<AGymansiumGoalActor> GoalActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float StepDurationSeconds = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	int32 MaxEpisodeSteps = 300;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float GoalRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float SpawnRadius = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float MinimumSpawnSeparation = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float ProgressRewardScale = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float TimePenalty = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float CollisionPenalty = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Navigation")
	float SuccessReward = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Debug")
	bool bEnableDebugDraw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Debug")
	bool bEnableOnScreenTelemetry = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gymansium|Debug")
	float DebugDrawDurationSeconds = 0.2f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Debug")
	float LastReward = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Debug")
	float CurrentEpisodeReward = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Debug")
	float LastDistanceToGoal = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Debug")
	float LastBearingToGoal = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Debug")
	bool bLastStepCollided = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Debug")
	int32 EpisodesCompleted = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Debug")
	int32 SuccessCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Debug")
	int32 TimeoutCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Debug")
	int32 CollisionCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Debug")
	FString LastEpisodeOutcome = TEXT("None");

protected:
	virtual void BeginPlay() override;

private:
	void EnsureActors();
	void BuildObservation(TInstancedStruct<FPoint>& OutObservation) const;
	FTransform MakeAgentSpawnTransform();
	FVector MakeGoalLocation(const FVector& AgentLocation);
	float GetGoalDistance() const;
	float GetSignedBearingToGoalDegrees() const;
	FVector GetEnvironmentCenter() const;
	void UpdateDebugState(float RewardDelta);
	void UpdateDebugVisualization(bool bTerminated, bool bTruncated);
	void FinalizeEpisode(const FString& OutcomeLabel);

	UPROPERTY()
	FRandomStream RandomStream;

	UPROPERTY()
	int32 CurrentStep = 0;

	UPROPERTY()
	float PreviousDistanceToGoal = 0.0f;

	UPROPERTY()
	int32 LastSeed = 1337;
};
