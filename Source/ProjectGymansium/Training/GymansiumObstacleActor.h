// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GymansiumObstacleActor.generated.h"

class UMaterialInterface;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EGymansiumObstacleShape : uint8
{
	Cube,
	Cylinder,
	Wall
};

UCLASS(Blueprintable)
class PROJECTGYMANSIUM_API AGymansiumObstacleActor : public AActor
{
	GENERATED_BODY()

public:
	AGymansiumObstacleActor();

	UFUNCTION(BlueprintCallable, Category = "Gymansium|Obstacles")
	void SetShape(EGymansiumObstacleShape Shape);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Obstacles")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ObstacleMaterial;

private:
	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;
};
