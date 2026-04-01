// Copyright Epic Games, Inc. All Rights Reserved.

#include "GymansiumGoalActor.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AGymansiumGoalActor::AGymansiumGoalActor()
{
	PrimaryActorTick.bCanEverTick = false;

	VisualMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMeshComponent->SetRelativeScale3D(FVector(0.75f));
	RootComponent = VisualMeshComponent;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		VisualMeshComponent->SetStaticMesh(SphereMesh.Object);
	}
}
