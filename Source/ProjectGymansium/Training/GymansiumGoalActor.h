// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GymansiumGoalActor.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class PROJECTGYMANSIUM_API AGymansiumGoalActor : public AActor
{
	GENERATED_BODY()

public:
	AGymansiumGoalActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Navigation")
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;
};
