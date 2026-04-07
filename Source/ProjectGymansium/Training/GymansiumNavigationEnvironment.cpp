// Copyright Epic Games, Inc. All Rights Reserved.

#include "GymansiumNavigationEnvironment.h"

#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Points/BoxPoint.h"
#include "Points/DictPoint.h"
#include "Spaces/BoxSpace.h"
#include "Spaces/DictSpace.h"
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
	OutAgentDefinition.ObsSpaceDefn.InitializeAs<FDictSpace>();
	FDictSpace& ObservationSpace = OutAgentDefinition.ObsSpaceDefn.GetMutable<FDictSpace>();

	TInstancedStruct<FSpace> ImageObservationSpace;
	ImageObservationSpace.InitializeAs<FBoxSpace>();
	FBoxSpace& ImageBoxSpace = ImageObservationSpace.GetMutable<FBoxSpace>();
	ImageBoxSpace.Dimensions.Init(FBoxSpaceDimension(0.0f, 1.0f), VisionObservationWidth * VisionObservationHeight * 3);
	ImageBoxSpace.Shape = { 3, VisionObservationHeight, VisionObservationWidth };
	ObservationSpace.Spaces.Add(TEXT("image"), ImageObservationSpace);

	TInstancedStruct<FSpace> StateObservationSpace;
	StateObservationSpace.InitializeAs<FBoxSpace>();
	FBoxSpace& StateBoxSpace = StateObservationSpace.GetMutable<FBoxSpace>();
	StateBoxSpace.Add(0.0f, 1.0f);   // normalized distance to goal
	StateBoxSpace.Add(-1.0f, 1.0f);  // signed bearing to goal
	StateBoxSpace.Add(0.0f, 1.0f);   // normalized speed
	StateBoxSpace.Add(0.0f, 1.0f);   // collision flag
	ObservationSpace.Spaces.Add(TEXT("state"), StateObservationSpace);

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

	if (bEnableCurriculum && CurriculumOutcomeWindow.IsEmpty())
	{
		SpawnRadius = CurriculumInitialSpawnRadius;
	}

	CurrentStep = 0;
	CurrentEpisodeReward = 0.0f;
	LastReward = 0.0f;
	bLastStepCollided = false;
	EpisodeCollisionCount = 0;
	NearGoalOrbitSteps = 0;
	LastEpisodeOutcome = TEXT("Running");

	const FTransform AgentTransform = MakeAgentSpawnTransform();
	AgentPawn->ResetToTransform(AgentTransform);
	GoalActor->SetActorLocation(MakeGoalLocation(AgentTransform.GetLocation()));

	PreviousDistanceToGoal = GetGoalDistance();
	LastDistanceToGoal = PreviousDistanceToGoal;
	LastBearingToGoal = GetSignedBearingToGoalDegrees();
	PreviousAbsoluteBearingToGoal = FMath::Abs(LastBearingToGoal);
	bLastFacingGoal = FMath::Abs(LastBearingToGoal) <= SuccessFacingAngleDegrees;

	BuildObservation(OutAgentState.Observations);
	OutAgentState.Info.Add(TEXT("status"), TEXT("reset"));
	OutAgentState.Info.Add(TEXT("episode_reward"), FString::SanitizeFloat(CurrentEpisodeReward));
	OutAgentState.Info.Add(TEXT("bearing_degrees"), FString::SanitizeFloat(LastBearingToGoal));
	OutAgentState.Info.Add(TEXT("facing_goal"), bLastFacingGoal ? TEXT("true") : TEXT("false"));
	OutAgentState.Info.Add(TEXT("outcome"), LastEpisodeOutcome);
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
	const float CurrentBearing = GetSignedBearingToGoalDegrees();
	const float CurrentAbsoluteBearing = FMath::Abs(CurrentBearing);
	const float BearingImprovement = PreviousAbsoluteBearingToGoal - CurrentAbsoluteBearing;
	const bool bFacingGoal = CurrentAbsoluteBearing <= SuccessFacingAngleDegrees;
	const bool bNearGoal = CurrentDistance <= (GoalRadius * FMath::Max(NearGoalDistanceMultiplier, 1.0f));
	const bool bOrbitingNearGoal = bNearGoal
		&& !bFacingGoal
		&& Progress <= NearGoalProgressThreshold
		&& FMath::Abs(Turn) >= 0.35f;

	OutAgentState.Reward = (Progress * ProgressRewardScale) - TimePenalty;
	OutAgentState.Reward += BearingImprovement * AlignmentRewardScale;

	if (Throttle < 0.0f)
	{
		OutAgentState.Reward -= FMath::Abs(Throttle) * ReversePenaltyScale;
	}

	if (AgentPawn->WasLastMoveBlocked())
	{
		OutAgentState.Reward -= CollisionPenalty;
	}

	if (bOrbitingNearGoal)
	{
		++NearGoalOrbitSteps;
		OutAgentState.Reward -= FMath::Abs(Turn) * NearGoalTurnPenaltyScale;
	}
	else if (bNearGoal && !bFacingGoal && Progress > NearGoalProgressThreshold)
	{
		NearGoalOrbitSteps = FMath::Max(NearGoalOrbitSteps - 1, 0);
	}
	else if (!bNearGoal)
	{
		NearGoalOrbitSteps = 0;
	}

	if (CurrentDistance <= GoalRadius && (!bRequireFacingForSuccess || bFacingGoal))
	{
		OutAgentState.Reward += SuccessReward;
		OutAgentState.bTerminated = true;
	}

	if (!OutAgentState.bTerminated && NearGoalOrbitSteps >= NearGoalOrbitStepLimit)
	{
		OutAgentState.Reward -= OrbitTimeoutPenalty;
		OutAgentState.bTruncated = true;
	}

	if (!OutAgentState.bTerminated && CurrentStep >= MaxEpisodeSteps)
	{
		OutAgentState.bTruncated = true;
	}

	BuildObservation(OutAgentState.Observations);
	OutAgentState.Info.Add(TEXT("step"), FString::FromInt(CurrentStep));
	OutAgentState.Info.Add(TEXT("distance"), FString::SanitizeFloat(CurrentDistance));
	OutAgentState.Info.Add(TEXT("bearing_degrees"), FString::SanitizeFloat(CurrentBearing));
	OutAgentState.Info.Add(TEXT("facing_goal"), bFacingGoal ? TEXT("true") : TEXT("false"));
	OutAgentState.Info.Add(TEXT("episode_reward"), FString::SanitizeFloat(CurrentEpisodeReward + OutAgentState.Reward));

	UpdateDebugState(OutAgentState.Reward);
	if (OutAgentState.bTerminated)
	{
		OutAgentState.Info.Add(TEXT("outcome"), TEXT("Success"));
		FinalizeEpisode(TEXT("Success"));
	}
	else if (OutAgentState.bTruncated)
	{
		const FString OutcomeLabel = NearGoalOrbitSteps >= NearGoalOrbitStepLimit ? TEXT("OrbitTimeout") : TEXT("Timeout");
		OutAgentState.Info.Add(TEXT("outcome"), OutcomeLabel);
		FinalizeEpisode(OutcomeLabel);
	}
	else
	{
		OutAgentState.Info.Add(TEXT("outcome"), LastEpisodeOutcome);
	}
	UpdateDebugVisualization(OutAgentState.bTerminated, OutAgentState.bTruncated);

	PreviousDistanceToGoal = CurrentDistance;
	PreviousAbsoluteBearingToGoal = CurrentAbsoluteBearing;
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

	if (const FString* VisionWidthValue = Options.Find(TEXT("vision_observation_width")))
	{
		VisionObservationWidth = FMath::Max(FCString::Atoi(**VisionWidthValue), 1);
	}

	if (const FString* VisionHeightValue = Options.Find(TEXT("vision_observation_height")))
	{
		VisionObservationHeight = FMath::Max(FCString::Atoi(**VisionHeightValue), 1);
	}

	if (const FString* AlignmentRewardValue = Options.Find(TEXT("alignment_reward_scale")))
	{
		AlignmentRewardScale = FCString::Atof(**AlignmentRewardValue);
	}

	if (const FString* ReversePenaltyValue = Options.Find(TEXT("reverse_penalty_scale")))
	{
		ReversePenaltyScale = FCString::Atof(**ReversePenaltyValue);
	}

	if (const FString* FacingAngleValue = Options.Find(TEXT("success_facing_angle_degrees")))
	{
		SuccessFacingAngleDegrees = FCString::Atof(**FacingAngleValue);
	}

	if (const FString* RequireFacingValue = Options.Find(TEXT("require_facing_for_success")))
	{
		bRequireFacingForSuccess = RequireFacingValue->ToBool();
	}

	if (const FString* NearGoalDistanceMultiplierValue = Options.Find(TEXT("near_goal_distance_multiplier")))
	{
		NearGoalDistanceMultiplier = FCString::Atof(**NearGoalDistanceMultiplierValue);
	}

	if (const FString* NearGoalTurnPenaltyValue = Options.Find(TEXT("near_goal_turn_penalty_scale")))
	{
		NearGoalTurnPenaltyScale = FCString::Atof(**NearGoalTurnPenaltyValue);
	}

	if (const FString* NearGoalProgressValue = Options.Find(TEXT("near_goal_progress_threshold")))
	{
		NearGoalProgressThreshold = FCString::Atof(**NearGoalProgressValue);
	}

	if (const FString* OrbitStepLimitValue = Options.Find(TEXT("near_goal_orbit_step_limit")))
	{
		NearGoalOrbitStepLimit = FCString::Atoi(**OrbitStepLimitValue);
	}

	if (const FString* OrbitTimeoutPenaltyValue = Options.Find(TEXT("orbit_timeout_penalty")))
	{
		OrbitTimeoutPenalty = FCString::Atof(**OrbitTimeoutPenaltyValue);
	}

	if (IsValid(AgentPawn))
	{
		AgentPawn->ConfigureVisionCapture(VisionObservationWidth, VisionObservationHeight);
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

	if (IsValid(AgentPawn))
	{
		AgentPawn->ConfigureVisionCapture(VisionObservationWidth, VisionObservationHeight);
	}

	if (!IsValid(GoalActor) && GoalActorClass)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GoalActor = World->SpawnActor<AGymansiumGoalActor>(GoalActorClass, FTransform(MakeGoalLocation(GetEnvironmentCenter())), SpawnParameters);
	}
}

void AGymansiumNavigationEnvironment::BuildObservation(TInstancedStruct<FPoint>& OutObservation)
{
	OutObservation.InitializeAs<FDictPoint>();
	FDictPoint& Observation = OutObservation.GetMutable<FDictPoint>();

	TInstancedStruct<FPoint> ImageObservation;
	ImageObservation.InitializeAs<FBoxPoint>();
	BuildImageObservation(ImageObservation.GetMutable<FBoxPoint>());
	Observation.Points.Add(TEXT("image"), MoveTemp(ImageObservation));

	TInstancedStruct<FPoint> StateObservation;
	StateObservation.InitializeAs<FBoxPoint>();
	BuildStateObservation(StateObservation.GetMutable<FBoxPoint>());
	Observation.Points.Add(TEXT("state"), MoveTemp(StateObservation));
}

void AGymansiumNavigationEnvironment::BuildStateObservation(FBoxPoint& OutStateObservation) const
{
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

	OutStateObservation.Shape = { 4 };
	OutStateObservation.Values = { NormalizedDistance, NormalizedBearing, NormalizedSpeed, CollisionFlag };
}

void AGymansiumNavigationEnvironment::BuildImageObservation(FBoxPoint& OutImageObservation)
{
	const int32 Width = FMath::Max(VisionObservationWidth, 1);
	const int32 Height = FMath::Max(VisionObservationHeight, 1);
	OutImageObservation.Shape = { 3, Height, Width };
	OutImageObservation.Values.Init(0.0f, Width * Height * 3);

	if (!IsValid(AgentPawn))
	{
		return;
	}

	TArray<int> ImageShape;
	if (!AgentPawn->CaptureVisionObservation(OutImageObservation.Values, ImageShape))
	{
		OutImageObservation.Values.Init(0.0f, Width * Height * 3);
		OutImageObservation.Shape = { 3, Height, Width };
		return;
	}

	if (ImageShape.Num() == 3)
	{
		OutImageObservation.Shape = ImageShape;
	}
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
	bLastFacingGoal = FMath::Abs(LastBearingToGoal) <= SuccessFacingAngleDegrees;
	bLastStepCollided = IsValid(AgentPawn) && AgentPawn->WasLastMoveBlocked();
	if (bLastStepCollided)
	{
		++EpisodeCollisionCount;
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
		FString Telemetry = FString::Printf(
			TEXT("GymNav | Step %d/%d | Dist %.1f | Bearing %.1f | Facing %s | Reward %.3f | EpReward %.3f | Collided %s | EpColl %d | Orbit %d | Success %d | Timeout %d | Outcome %s"),
			CurrentStep,
			MaxEpisodeSteps,
			LastDistanceToGoal,
			LastBearingToGoal,
			bLastFacingGoal ? TEXT("Yes") : TEXT("No"),
			LastReward,
			CurrentEpisodeReward,
			bLastStepCollided ? TEXT("Yes") : TEXT("No"),
			EpisodeCollisionCount,
			NearGoalOrbitSteps,
			SuccessCount,
			TimeoutCount,
			*LastEpisodeOutcome
		);
		if (bEnableCurriculum)
		{
			Telemetry += FString::Printf(
				TEXT(" | Curriculum: diff=%.2f spawn=%.0f rate=%.0f%%"),
				CurrentDifficulty,
				SpawnRadius,
				RollingSuccessRate * 100.0f
			);
		}

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
	else if (OutcomeLabel == TEXT("Timeout") || OutcomeLabel == TEXT("OrbitTimeout"))
	{
		++TimeoutCount;
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("GymNav episode %d ended: %s | steps=%d reward=%.3f distance=%.3f episode_collisions=%d orbit_steps=%d"),
		EpisodesCompleted,
		*OutcomeLabel,
		CurrentStep,
		CurrentEpisodeReward,
		LastDistanceToGoal,
		EpisodeCollisionCount,
		NearGoalOrbitSteps
	);

	if (bEnableCurriculum)
	{
		UpdateCurriculum(OutcomeLabel == TEXT("Success"));
	}
}

void AGymansiumNavigationEnvironment::UpdateCurriculum(bool bSuccess)
{
	CurriculumOutcomeWindow.Add(bSuccess);
	if (CurriculumOutcomeWindow.Num() > FMath::Max(CurriculumWindowSize, 1))
	{
		CurriculumOutcomeWindow.RemoveAt(0);
	}

	int32 Successes = 0;
	for (const bool bOutcome : CurriculumOutcomeWindow)
	{
		if (bOutcome) { ++Successes; }
	}
	RollingSuccessRate = static_cast<float>(Successes) / static_cast<float>(CurriculumOutcomeWindow.Num());

	if (RollingSuccessRate >= CurriculumAdvanceThreshold)
	{
		CurrentDifficulty = FMath::Min(CurrentDifficulty + CurriculumStepSize, 1.0f);
	}
	else if (RollingSuccessRate < CurriculumRetreatThreshold)
	{
		CurrentDifficulty = FMath::Max(CurrentDifficulty - CurriculumStepSize, 0.0f);
	}

	SpawnRadius = FMath::Lerp(CurriculumInitialSpawnRadius, CurriculumTargetSpawnRadius, CurrentDifficulty);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("GymNav curriculum: difficulty=%.2f spawn_radius=%.0f success_rate=%.2f (window=%d)"),
		CurrentDifficulty,
		SpawnRadius,
		RollingSuccessRate,
		CurriculumOutcomeWindow.Num()
	);
}
