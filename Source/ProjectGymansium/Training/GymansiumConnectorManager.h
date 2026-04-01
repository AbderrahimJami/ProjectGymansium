// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GymansiumConnectorManager.generated.h"

class UAbstractGymConnector;
class URPCGymConnector;

UCLASS(Blueprintable)
class PROJECTGYMANSIUM_API AGymansiumConnectorManager : public AActor
{
	GENERATED_BODY()

public:
	AGymansiumConnectorManager();

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Gymansium|Training")
	TObjectPtr<UAbstractGymConnector> Connector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gymansium|Training")
	TObjectPtr<URPCGymConnector> RPCConnector;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
};
