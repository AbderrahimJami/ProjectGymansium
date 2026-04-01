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

protected:
	virtual void BeginPlay() override;

private:
	void EnsureActors();
	void BuildObservation(TInstancedStruct<FPoint>& OutObservation) const;
	FTransform MakeAgentSpawnTransform();
	FVector MakeGoalLocation(const FVector& AgentLocation);
	float GetGoalDistance() const;
	FVector GetEnvironmentCenter() const;

	UPROPERTY()
	FRandomStream RandomStream;

	UPROPERTY()
	int32 CurrentStep = 0;

	UPROPERTY()
	float PreviousDistanceToGoal = 0.0f;

	UPROPERTY()
	int32 LastSeed = 1337;
};
