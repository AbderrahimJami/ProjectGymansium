// Copyright Epic Games, Inc. All Rights Reserved.

#include "GymansiumNNEController.h"

#include "NNE.h"
#include "GymansiumNavPawn.h"
#include "GymansiumGoalActor.h"

AGymansiumNNEController::AGymansiumNNEController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGymansiumNNEController::BeginPlay()
{
	Super::BeginPlay();
	LoadModel();

	if (bModelLoaded && StepDurationSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(
			StepTimerHandle,
			this,
			&AGymansiumNNEController::OnStepTimer,
			StepDurationSeconds,
			true  // looping
		);
	}
}

void AGymansiumNNEController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StepTimerHandle);
	ModelInstance.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGymansiumNNEController::LoadModel()
{
	bModelLoaded = false;

	if (!IsValid(PolicyModelData))
	{
		UE_LOG(LogTemp, Warning, TEXT("GymNNE: PolicyModelData is not assigned."));
		return;
	}

	// Obtain the CPU runtime. Requires the NNEOnnxRuntime plugin to be enabled.
	TWeakInterfacePtr<INNERuntimeCPU> Runtime = UE::NNE::GetRuntime<INNERuntimeCPU>(FString("NNERuntimeORTCpu"));
	if (!Runtime.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("GymNNE: NNERuntimeORTCpu not available. Enable the NNEOnnxRuntime plugin in your .uproject."));
		return;
	}

	TSharedPtr<UE::NNE::IModelCPU> Model = Runtime->CreateModelCPU(PolicyModelData);
	if (!Model.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("GymNNE: Failed to create model from PolicyModelData."));
		return;
	}

	ModelInstance = Model->CreateModelInstanceCPU();
	if (!ModelInstance.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("GymNNE: Failed to create model instance."));
		return;
	}

	// Bind the concrete input shape: [1, ObservationDim]
	TArray<UE::NNE::FTensorDesc> InputDescs = ModelInstance->GetInputTensorDescs();
	if (InputDescs.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("GymNNE: Model has no inputs."));
		return;
	}

	const UE::NNE::FTensorShape InputShape = UE::NNE::FTensorShape::Make({ 1, static_cast<uint32>(ObservationDim) });
	if (ModelInstance->SetInputTensorShapes({ InputShape }) != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("GymNNE: Failed to set input shape [1, %d]."), ObservationDim);
		return;
	}

	bModelLoaded = true;
	UE_LOG(LogTemp, Log, TEXT("GymNNE: Policy loaded. obs_dim=%d step=%.2fs"), ObservationDim, StepDurationSeconds);
}

void AGymansiumNNEController::OnStepTimer()
{
	if (!bModelLoaded || !IsValid(AgentPawn) || !IsValid(GoalActor))
	{
		return;
	}

	const TArray<float> Obs = BuildObservation();
	float Throttle = 0.0f;
	float Turn = 0.0f;

	if (RunInference(Obs, Throttle, Turn))
	{
		AgentPawn->ApplyAction(Throttle, Turn, StepDurationSeconds);
	}
}

TArray<float> AGymansiumNNEController::BuildObservation() const
{
	TArray<float> Obs;
	Obs.SetNumZeroed(ObservationDim);

	if (!IsValid(AgentPawn) || !IsValid(GoalActor))
	{
		return Obs;
	}

	const FVector ToGoal = GoalActor->GetActorLocation() - AgentPawn->GetActorLocation();
	const float MaxDist = FMath::Max(MaxObservationDistance, 1.0f);
	const float NormalizedDist = FMath::Clamp(ToGoal.Size() / MaxDist, 0.0f, 1.0f);

	const float GoalYaw = ToGoal.Rotation().Yaw;
	const float PawnYaw = AgentPawn->GetActorRotation().Yaw;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(PawnYaw, GoalYaw);
	const float NormalizedBearing = FMath::Clamp(DeltaYaw / 180.0f, -1.0f, 1.0f);

	const float NormalizedSpeed = FMath::Clamp(
		AgentPawn->GetLastVelocity().Size() / FMath::Max(AgentPawn->MoveSpeed, 1.0f),
		0.0f, 1.0f
	);
	const float CollisionFlag = AgentPawn->WasLastMoveBlocked() ? 1.0f : 0.0f;

	// Fill the first 4 elements — same layout as training
	if (ObservationDim >= 1) Obs[0] = NormalizedDist;
	if (ObservationDim >= 2) Obs[1] = NormalizedBearing;
	if (ObservationDim >= 3) Obs[2] = NormalizedSpeed;
	if (ObservationDim >= 4) Obs[3] = CollisionFlag;
	// Elements [4..ObservationDim-1] remain 0 (e.g. unused raycast slots)

	return Obs;
}

bool AGymansiumNNEController::RunInference(const TArray<float>& Obs, float& OutThrottle, float& OutTurn)
{
	if (!ModelInstance.IsValid())
	{
		return false;
	}

	TArray<UE::NNE::FTensorDesc> OutputDescs = ModelInstance->GetOutputTensorDescs();
	if (OutputDescs.IsEmpty())
	{
		return false;
	}

	// Output: [1, action_dim] — we expect action_dim >= 2
	const int32 OutputSize = OutputDescs[0].GetShape().Volume();
	TArray<float> OutputData;
	OutputData.SetNumUninitialized(FMath::Max(OutputSize, 2));

	UE::NNE::FTensorBindingCPU InputBinding;
	InputBinding.Data = const_cast<float*>(Obs.GetData());
	InputBinding.SizeInBytes = Obs.Num() * sizeof(float);

	UE::NNE::FTensorBindingCPU OutputBinding;
	OutputBinding.Data = OutputData.GetData();
	OutputBinding.SizeInBytes = OutputData.Num() * sizeof(float);

	if (ModelInstance->RunSync({ InputBinding }, { OutputBinding }) != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GymNNE: Inference failed."));
		return false;
	}

	OutThrottle = FMath::Clamp(OutputData[0], -1.0f, 1.0f);
	OutTurn     = FMath::Clamp(OutputData[1], -1.0f, 1.0f);
	return true;
}
