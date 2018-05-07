// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmpathAIManager.generated.h"

UCLASS()
class EMPATH_API AEmpathAIManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEmpathAIManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(Category = "Empath|AI", EditAnywhere, BlueprintReadWrite)
		TArray<class AEmpathAIController*> EmpathAIControllers;

};
