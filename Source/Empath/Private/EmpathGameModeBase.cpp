// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathGameModeBase.h"
#include "EmpathAIManager.h"
#include "Runtime/Engine/Classes/Engine/World.h"

AEmpathGameModeBase::AEmpathGameModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void AEmpathGameModeBase::BeginPlay()
{
	AIManager = (AEmpathAIManager*)GetWorld()->SpawnActor<AEmpathAIManager>();
}