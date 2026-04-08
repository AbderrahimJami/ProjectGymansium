// Copyright Epic Games, Inc. All Rights Reserved.

#include "GymansiumObstacleActor.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AGymansiumObstacleActor::AGymansiumObstacleActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObstacleMesh"));
	MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = MeshComponent;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		CubeMesh = CubeFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		CylinderMesh = CylinderFinder.Object;
	}

	// Default to cube
	if (CubeMesh)
	{
		MeshComponent->SetStaticMesh(CubeMesh);
	}
}

void AGymansiumObstacleActor::SetShape(EGymansiumObstacleShape Shape)
{
	if (!MeshComponent)
	{
		return;
	}

	switch (Shape)
	{
	case EGymansiumObstacleShape::Cube:
		if (CubeMesh) { MeshComponent->SetStaticMesh(CubeMesh); }
		SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
		break;

	case EGymansiumObstacleShape::Cylinder:
		if (CylinderMesh) { MeshComponent->SetStaticMesh(CylinderMesh); }
		SetActorScale3D(FVector(0.8f, 0.8f, 1.5f));
		break;

	case EGymansiumObstacleShape::Wall:
		if (CubeMesh) { MeshComponent->SetStaticMesh(CubeMesh); }
		SetActorScale3D(FVector(2.5f, 0.3f, 1.5f));
		break;
	}

	if (ObstacleMaterial)
	{
		MeshComponent->SetMaterial(0, ObstacleMaterial);
	}
}
