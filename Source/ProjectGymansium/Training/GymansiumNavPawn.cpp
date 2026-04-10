// Copyright Epic Games, Inc. All Rights Reserved.

#include "GymansiumNavPawn.h"

#include "Camera/CameraComponent.h"
#include "ImageCaptureComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AGymansiumNavPawn::AGymansiumNavPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	CapsuleComponent->InitCapsuleSize(34.0f, 44.0f);
	CapsuleComponent->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = CapsuleComponent;

	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMeshComponent->SetupAttachment(CapsuleComponent);
	VisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -30.0f));
	VisualMeshComponent->SetRelativeScale3D(FVector(0.75f, 0.45f, 0.3f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		VisualMeshComponent->SetStaticMesh(CubeMesh.Object);
	}

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComponent->SetupAttachment(CapsuleComponent);
	CameraComponent->SetRelativeLocation(FVector(-160.0f, 0.0f, 110.0f));
	CameraComponent->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));

	// ImageCaptureComponent = CreateDefaultSubobject<UImageCaptureComponent>(TEXT("ImageCapture"));

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AGymansiumNavPawn::ApplyAction(float ThrottleInput, float TurnInput, float StepSeconds)
{
	const float ClampedStepSeconds = FMath::Max(StepSeconds, KINDA_SMALL_NUMBER);
	const float ClampedThrottle = FMath::Clamp(ThrottleInput, -1.0f, 1.0f);
	const float ClampedTurn = FMath::Clamp(TurnInput, -1.0f, 1.0f);
	const float EffectiveThrottle = (ClampedThrottle >= 0.0f)
		? ClampedThrottle
		: (ClampedThrottle * FMath::Clamp(ReverseSpeedMultiplier, 0.0f, 1.0f));

	AddActorLocalRotation(FRotator(0.0f, ClampedTurn * TurnSpeedDegrees * ClampedStepSeconds, 0.0f));

	const FVector StartLocation = GetActorLocation();
	const FVector Delta = GetActorForwardVector() * (EffectiveThrottle * MoveSpeed * ClampedStepSeconds);

	FHitResult HitResult;
	AddActorWorldOffset(Delta, true, &HitResult);

	const FVector EndLocation = GetActorLocation();
	LastVelocity = (EndLocation - StartLocation) / ClampedStepSeconds;
	bLastMoveBlocked = HitResult.IsValidBlockingHit();
}

void AGymansiumNavPawn::ResetToTransform(const FTransform& SpawnTransform)
{
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::ResetPhysics);
	LastVelocity = FVector::ZeroVector;
	bLastMoveBlocked = false;
}

FVector AGymansiumNavPawn::GetLastVelocity() const
{
	return LastVelocity;
}

bool AGymansiumNavPawn::WasLastMoveBlocked() const
{
	return bLastMoveBlocked;
}
