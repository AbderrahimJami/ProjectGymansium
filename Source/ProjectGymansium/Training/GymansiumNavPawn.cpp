// Copyright Epic Games, Inc. All Rights Reserved.

#include "GymansiumNavPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
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

	VisionCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("VisionCapture"));
	VisionCaptureComponent->SetupAttachment(CapsuleComponent);
	VisionCaptureComponent->SetRelativeLocation(FVector(45.0f, 0.0f, 45.0f));
	VisionCaptureComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	VisionCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	VisionCaptureComponent->bCaptureEveryFrame = false;
	VisionCaptureComponent->bCaptureOnMovement = false;
	VisionCaptureComponent->bAlwaysPersistRenderingState = true;

	VisionRenderTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("VisionRenderTarget"));
	EnsureVisionRenderTarget();
	VisionCaptureComponent->TextureTarget = VisionRenderTarget;

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

void AGymansiumNavPawn::ConfigureVisionCapture(int32 InWidth, int32 InHeight)
{
	VisionWidth = FMath::Max(InWidth, 1);
	VisionHeight = FMath::Max(InHeight, 1);
	EnsureVisionRenderTarget();
}

bool AGymansiumNavPawn::CaptureVisionObservation(TArray<float>& OutImageValues, TArray<int>& OutShape)
{
	EnsureVisionRenderTarget();
	if (!VisionCaptureComponent || !VisionRenderTarget)
	{
		return false;
	}

	VisionCaptureComponent->CaptureScene();

	TArray<FColor> Bitmap;
	VisionRenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(Bitmap);

	const int32 Width = VisionRenderTarget->GetSurfaceWidth();
	const int32 Height = VisionRenderTarget->GetSurfaceHeight();
	const int32 NumPixels = Width * Height;
	if (Bitmap.Num() != NumPixels)
	{
		return false;
	}

	OutImageValues.SetNumUninitialized(NumPixels * 3);
	OutShape = { 3, Height, Width };

	for (int32 PixelIndex = 0; PixelIndex < NumPixels; ++PixelIndex)
	{
		OutImageValues[PixelIndex] = static_cast<float>(Bitmap[PixelIndex].R) / 255.0f;
		OutImageValues[PixelIndex + NumPixels] = static_cast<float>(Bitmap[PixelIndex].G) / 255.0f;
		OutImageValues[PixelIndex + (2 * NumPixels)] = static_cast<float>(Bitmap[PixelIndex].B) / 255.0f;
	}

	return true;
}

void AGymansiumNavPawn::GetRaycastDistances(int32 NumRays, float MaxRange, TArray<float>& OutDistances) const
{
	OutDistances.SetNum(NumRays);

	UWorld* World = GetWorld();
	if (!World || NumRays <= 0 || MaxRange <= 0.0f)
	{
		for (float& D : OutDistances) { D = 1.0f; }
		return;
	}

	const FVector Origin = GetActorLocation();
	const float PawnYaw = GetActorRotation().Yaw;
	const float AngleStep = 360.0f / static_cast<float>(NumRays);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	for (int32 i = 0; i < NumRays; ++i)
	{
		const float RayYaw = PawnYaw + (i * AngleStep);
		const FVector Direction = FRotator(0.0f, RayYaw, 0.0f).Vector();
		const FVector End = Origin + Direction * MaxRange;

		FHitResult HitResult;
		const bool bHit = World->LineTraceSingleByChannel(HitResult, Origin, End, ECC_WorldStatic, QueryParams);
		OutDistances[i] = bHit ? FMath::Clamp(HitResult.Distance / MaxRange, 0.0f, 1.0f) : 1.0f;
	}
}

void AGymansiumNavPawn::EnsureVisionRenderTarget()
{
	if (!VisionRenderTarget)
	{
		return;
	}

	const int32 TargetWidth = FMath::Max(VisionWidth, 1);
	const int32 TargetHeight = FMath::Max(VisionHeight, 1);
	if (VisionRenderTarget->GetSurfaceWidth() != TargetWidth || VisionRenderTarget->GetSurfaceHeight() != TargetHeight)
	{
		VisionRenderTarget->RenderTargetFormat = RTF_RGBA8;
		VisionRenderTarget->bGPUSharedFlag = true;
		VisionRenderTarget->InitAutoFormat(TargetWidth, TargetHeight);
		VisionRenderTarget->UpdateResourceImmediate(true);
	}

	if (VisionCaptureComponent)
	{
		VisionCaptureComponent->TextureTarget = VisionRenderTarget;
	}
}
