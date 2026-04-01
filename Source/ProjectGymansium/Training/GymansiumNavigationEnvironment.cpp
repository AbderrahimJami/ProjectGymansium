// Copyright Epic Games, Inc. All Rights Reserved.

#include "GymansiumNavigationEnvironment.h"

#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Points/BoxPoint.h"
#include "Spaces/BoxSpace.h"
#include "GymansiumGoalActor.h"
#include "GymansiumNavPawn.h"

AGymansiumNavigationEnvironment::AGymansiumNavigationEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;

	AgentPawnClass = AGymansiumNavPawn::StaticClass();
	GoalActorClass = AGymansiumGoalActor::StaticClass();

	RandomStream.Initialize(LastSeed);
}

void AGymansiumNavigationEnvironment::BeginPlay()
{
	Super::BeginPlay();
	EnsureActors();
}

void AGymansiumNavigationEnvironment::InitializeEnvironment_Implementation(FInteractionDefinition& OutAgentDefinition)
{
	OutAgentDefinition.ObsSpaceDefn.InitializeAs<FBoxSpace>();
	FBoxSpace& ObservationSpace = OutAgentDefinition.ObsSpaceDefn.GetMutable<FBoxSpace>();
	ObservationSpace.Add(0.0f, 1.0f);   // normalized distance to goal
	ObservationSpace.Add(-1.0f, 1.0f);  // signed bearing to goal
	ObservationSpace.Add(0.0f, 1.0f);   // normalized speed
	ObservationSpace.Add(0.0f, 1.0f);   // collision flag

	OutAgentDefinition.ActionSpaceDefn.InitializeAs<FBoxSpace>();
	FBoxSpace& ActionSpace = OutAgentDefinition.ActionSpaceDefn.GetMutable<FBoxSpace>();
	ActionSpace.Add(-1.0f, 1.0f); // throttle
	ActionSpace.Add(-1.0f, 1.0f); // turn
}

void AGymansiumNavigationEnvironment::Reset_Implementation(FInitialAgentState& OutAgentState)
{
	EnsureActors();

	if (!IsValid(AgentPawn) || !IsValid(GoalActor))
	{
		BuildObservation(OutAgentState.Observations);
		OutAgentState.Info.Add(TEXT("error"), TEXT("Environment actors are missing"));
		return;
	}

	CurrentStep = 0;
	CurrentEpisodeReward = 0.0f;
	LastReward = 0.0f;
	bLastStepCollided = false;
	LastEpisodeOutcome = TEXT("Running");

	const FTransform AgentTransform = MakeAgentSpawnTransform();
	AgentPawn->ResetToTransform(AgentTransform);
	GoalActor->SetActorLocation(MakeGoalLocation(AgentTransform.GetLocation()));

	PreviousDistanceToGoal = GetGoalDistance();
	LastDistanceToGoal = PreviousDistanceToGoal;
	LastBearingToGoal = GetSignedBearingToGoalDegrees();

	BuildObservation(OutAgentState.Observations);
	OutAgentState.Info.Add(TEXT("status"), TEXT("reset"));
	OutAgentState.Info.Add(TEXT("episode_reward"), FString::SanitizeFloat(CurrentEpisodeReward));
	UpdateDebugVisualization(false, false);
}

void AGymansiumNavigationEnvironment::Step_Implementation(const FInstancedStruct& InAction, FAgentState& OutAgentState)
{
	EnsureActors();

	if (!IsValid(AgentPawn) || !IsValid(GoalActor))
	{
		BuildObservation(OutAgentState.Observations);
		OutAgentState.bTruncated = true;
		OutAgentState.Info.Add(TEXT("error"), TEXT("Environment actors are missing"));
		return;
	}

	float Throttle = 0.0f;
	float Turn = 0.0f;

	if (InAction.IsValid() && InAction.GetScriptStruct() == FBoxPoint::StaticStruct())
	{
		const FBoxPoint& Action = InAction.Get<FBoxPoint>();
		if (Action.Values.Num() > 0)
		{
			Throttle = Action.Values[0];
		}
		if (Action.Values.Num() > 1)
		{
			Turn = Action.Values[1];
		}
	}

	AgentPawn->ApplyAction(Throttle, Turn, StepDurationSeconds);
	++CurrentStep;

	const float CurrentDistance = GetGoalDistance();
	const float Progress = PreviousDistanceToGoal - CurrentDistance;

	OutAgentState.Reward = (Progress * ProgressRewardScale) - TimePenalty;

	if (AgentPawn->WasLastMoveBlocked())
	{
		OutAgentState.Reward -= CollisionPenalty;
	}

	if (CurrentDistance <= GoalRadius)
	{
		OutAgentState.Reward += SuccessReward;
		OutAgentState.bTerminated = true;
	}

	if (CurrentStep >= MaxEpisodeSteps)
	{
		OutAgentState.bTruncated = true;
	}

	BuildObservation(OutAgentState.Observations);
	OutAgentState.Info.Add(TEXT("step"), FString::FromInt(CurrentStep));
	OutAgentState.Info.Add(TEXT("distance"), FString::SanitizeFloat(CurrentDistance));
	OutAgentState.Info.Add(TEXT("episode_reward"), FString::SanitizeFloat(CurrentEpisodeReward + OutAgentState.Reward));

	UpdateDebugState(OutAgentState.Reward);
	if (OutAgentState.bTerminated)
	{
		FinalizeEpisode(TEXT("Success"));
	}
	else if (OutAgentState.bTruncated)
	{
		FinalizeEpisode(TEXT("Timeout"));
	}
	UpdateDebugVisualization(OutAgentState.bTerminated, OutAgentState.bTruncated);

	PreviousDistanceToGoal = CurrentDistance;
}

void AGymansiumNavigationEnvironment::SeedEnvironment_Implementation(int Seed)
{
	LastSeed = Seed;
	RandomStream.Initialize(Seed);
}

void AGymansiumNavigationEnvironment::SetEnvironmentOptions_Implementation(const TMap<FString, FString>& Options)
{
	if (const FString* GoalRadiusValue = Options.Find(TEXT("goal_radius")))
	{
		GoalRadius = FCString::Atof(**GoalRadiusValue);
	}

	if (const FString* MaxStepsValue = Options.Find(TEXT("max_episode_steps")))
	{
		MaxEpisodeSteps = FCString::Atoi(**MaxStepsValue);
	}

	if (const FString* StepDurationValue = Options.Find(TEXT("step_duration_seconds")))
	{
		StepDurationSeconds = FCString::Atof(**StepDurationValue);
	}
}

void AGymansiumNavigationEnvironment::EnsureActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!IsValid(AgentPawn) && AgentPawnClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AgentPawn = World->SpawnActor<AGymansiumNavPawn>(AgentPawnClass, MakeAgentSpawnTransform(), SpawnParameters);
	}

	if (!IsValid(GoalActor) && GoalActorClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GoalActor = World->SpawnActor<AGymansiumGoalActor>(GoalActorClass, FTransform(MakeGoalLocation(GetEnvironmentCenter())), SpawnParameters);
	}
}

void AGymansiumNavigationEnvironment::BuildObservation(TInstancedStruct<FPoint>& OutObservation) const
{
	OutObservation.InitializeAs<FBoxPoint>();
	FBoxPoint& Observation = OutObservation.GetMutable<FBoxPoint>();

	float NormalizedDistance = 1.0f;
	float NormalizedBearing = 0.0f;
	float NormalizedSpeed = 0.0f;
	float CollisionFlag = 0.0f;

	if (IsValid(AgentPawn) && IsValid(GoalActor))
	{
		const FVector ToGoal = GoalActor->GetActorLocation() - AgentPawn->GetActorLocation();
		const float MaxDistance = FMath::Max(SpawnRadius * 2.0f, GoalRadius);
		NormalizedDistance = FMath::Clamp(ToGoal.Size() / MaxDistance, 0.0f, 1.0f);

		const float GoalYaw = ToGoal.Rotation().Yaw;
		const float PawnYaw = AgentPawn->GetActorRotation().Yaw;
		const float DeltaYaw = FMath::FindDeltaAngleDegrees(PawnYaw, GoalYaw);
		NormalizedBearing = FMath::Clamp(DeltaYaw / 180.0f, -1.0f, 1.0f);

		NormalizedSpeed = FMath::Clamp(AgentPawn->GetLastVelocity().Size() / FMath::Max(AgentPawn->MoveSpeed, 1.0f), 0.0f, 1.0f);
		CollisionFlag = AgentPawn->WasLastMoveBlocked() ? 1.0f : 0.0f;
	}

	Observation.Values = { NormalizedDistance, NormalizedBearing, NormalizedSpeed, CollisionFlag };
}

FTransform AGymansiumNavigationEnvironment::MakeAgentSpawnTransform()
{
	const FVector Center = GetEnvironmentCenter();
	const float Distance = RandomStream.FRandRange(0.0f, SpawnRadius);
	const float Angle = RandomStream.FRandRange(0.0f, 2.0f * PI);
	const FVector Offset(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 50.0f);
	const FVector Location = Center + Offset;

	const float Yaw = RandomStream.FRandRange(-180.0f, 180.0f);
	return FTransform(FRotator(0.0f, Yaw, 0.0f), Location);
}

FVector AGymansiumNavigationEnvironment::MakeGoalLocation(const FVector& AgentLocation)
{
	const FVector Center = GetEnvironmentCenter();
	FVector Candidate = Center;

	for (int32 Attempt = 0; Attempt < 16; ++Attempt)
	{
		const float Distance = RandomStream.FRandRange(0.0f, SpawnRadius);
		const float Angle = RandomStream.FRandRange(0.0f, 2.0f * PI);
		Candidate = Center + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 50.0f);
		if (FVector::Dist2D(Candidate, AgentLocation) >= MinimumSpawnSeparation)
		{
			return Candidate;
		}
	}

	return Candidate;
}

float AGymansiumNavigationEnvironment::GetGoalDistance() const
{
	if (!IsValid(AgentPawn) || !IsValid(GoalActor))
	{
		return 0.0f;
	}

	return FVector::Dist2D(AgentPawn->GetActorLocation(), GoalActor->GetActorLocation());
}

float AGymansiumNavigationEnvironment::GetSignedBearingToGoalDegrees() const
{
	if (!IsValid(AgentPawn) || !IsValid(GoalActor))
	{
		return 0.0f;
	}

	const FVector ToGoal = GoalActor->GetActorLocation() - AgentPawn->GetActorLocation();
	const float GoalYaw = ToGoal.Rotation().Yaw;
	const float PawnYaw = AgentPawn->GetActorRotation().Yaw;
	return FMath::FindDeltaAngleDegrees(PawnYaw, GoalYaw);
}

FVector AGymansiumNavigationEnvironment::GetEnvironmentCenter() const
{
	return GetActorLocation();
}

void AGymansiumNavigationEnvironment::UpdateDebugState(float RewardDelta)
{
	LastReward = RewardDelta;
	CurrentEpisodeReward += RewardDelta;
	LastDistanceToGoal = GetGoalDistance();
	LastBearingToGoal = GetSignedBearingToGoalDegrees();
	bLastStepCollided = IsValid(AgentPawn) && AgentPawn->WasLastMoveBlocked();
	if (bLastStepCollided)
	{
		++CollisionCount;
	}
}

void AGymansiumNavigationEnvironment::UpdateDebugVisualization(bool bTerminated, bool bTruncated)
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(AgentPawn) || !IsValid(GoalActor))
	{
		return;
	}

	const FVector PawnLocation = AgentPawn->GetActorLocation();
	const FVector GoalLocation = GoalActor->GetActorLocation();
	const float DrawDuration = FMath::Max(DebugDrawDurationSeconds, 0.0f);

	if (bEnableDebugDraw)
	{
		const FColor GoalColor = bTerminated ? FColor::Green : (bTruncated ? FColor::Red : FColor::Yellow);
		const FColor PathColor = bLastStepCollided ? FColor::Red : FColor::Cyan;
		const FVector ArrowEnd = PawnLocation + (AgentPawn->GetActorForwardVector() * 150.0f);

		DrawDebugSphere(World, GoalLocation, GoalRadius, 24, GoalColor, false, DrawDuration, 0, 2.0f);
		DrawDebugLine(World, PawnLocation, GoalLocation, PathColor, false, DrawDuration, 0, 2.0f);
		DrawDebugDirectionalArrow(World, PawnLocation, ArrowEnd, 60.0f, FColor::Blue, false, DrawDuration, 0, 2.0f);
	}

	if (bEnableOnScreenTelemetry && GEngine)
	{
		const FString Telemetry = FString::Printf(
			TEXT("GymNav | Step %d/%d | Dist %.1f | Bearing %.1f | Reward %.3f | EpReward %.3f | Collided %s | Success %d | Timeout %d | Outcome %s"),
			CurrentStep,
			MaxEpisodeSteps,
			LastDistanceToGoal,
			LastBearingToGoal,
			LastReward,
			CurrentEpisodeReward,
			bLastStepCollided ? TEXT("Yes") : TEXT("No"),
			SuccessCount,
			TimeoutCount,
			*LastEpisodeOutcome
		);

		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), DrawDuration + 0.05f, FColor::White, Telemetry);
	}
}

void AGymansiumNavigationEnvironment::FinalizeEpisode(const FString& OutcomeLabel)
{
	++EpisodesCompleted;
	LastEpisodeOutcome = OutcomeLabel;

	if (OutcomeLabel == TEXT("Success"))
	{
		++SuccessCount;
	}
	else if (OutcomeLabel == TEXT("Timeout"))
	{
		++TimeoutCount;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("GymNav episode %d ended: %s | steps=%d reward=%.3f distance=%.3f collisions=%d"),
		EpisodesCompleted,
		*OutcomeLabel,
		CurrentStep,
		CurrentEpisodeReward,
		LastDistanceToGoal,
		CollisionCount
	);
}
