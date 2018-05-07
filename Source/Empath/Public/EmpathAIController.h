// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "VRAIController.h"
#include "EmpathAIController.generated.h"

/**
*
*/
UCLASS()
class EMPATH_API AEmpathAIController : public AVRAIController
{
	GENERATED_BODY()
public:
	AEmpathAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Gets the world AI Manager */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable, BlueprintPure)
		class AEmpathAIManager*	GetAIManager() { return AIManager; }

	virtual void BeginPlay() override;

private:
	AEmpathAIManager * AIManager;
};
