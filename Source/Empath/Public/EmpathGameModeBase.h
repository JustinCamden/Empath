// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EmpathGameModeBase.generated.h"

class AEmpathAIManager;

/**
 * 
 */
UCLASS()
class EMPATH_API AEmpathGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
public:
	AEmpathGameModeBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Gets the world AI Manager */
	UFUNCTION(Category = EmpathGameMode, BlueprintCallable, BlueprintPure)
	AEmpathAIManager* GetAIManager() const { return AIManager; }

	virtual void BeginPlay() override;
private:
	AEmpathAIManager* AIManager;
	
};
