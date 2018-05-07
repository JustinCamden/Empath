// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathAIController.h"
#include "EmpathAIManager.h"
#include "EmpathGameModeBase.h"



AEmpathAIController::AEmpathAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void AEmpathAIController::BeginPlay()
{
	// Use the game mode to register us with the world ai manager
	AEmpathGameModeBase* EmpathGameMode = (AEmpathGameModeBase*)GetWorld()->GetAuthGameMode();
	if (EmpathGameMode)
	{
		AIManager = EmpathGameMode->GetAIManager();
		if (AIManager)
		{
			AIManager->EmpathAIControllers.Add(this);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ERROR: AI Manager not initialized"));
		}
	}
}

