// Copyright Epic Games, Inc. All Rights Reserved.

#include "GymansiumConnectorManager.h"

#include "Environment/EnvironmentInterface.h"
#include "GymConnectors/gRPC/gRPCGymConnector.h"
#include "GymConnectors/AbstractGymConnector.h"

AGymansiumConnectorManager::AGymansiumConnectorManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	RPCConnector = CreateDefaultSubobject<URPCGymConnector>(TEXT("RPCConnector"));
	Connector = RPCConnector;
}

void AGymansiumConnectorManager::BeginPlay()
{
	Super::BeginPlay();

	if (Connector)
	{
		TArray<TScriptInterface<IBaseScholaEnvironment>> Environments;
		Connector->CollectEnvironments(Environments);
		Connector->Init(Environments);
	}
}

void AGymansiumConnectorManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Connector)
	{
		Connector->Step();
	}
}
