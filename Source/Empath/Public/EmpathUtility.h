// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EmpathUtility.generated.h"

class AActor;
class AEmpathAIManager;

/**
 * 
 */
UCLASS()
class EMPATH_API UEmpathUtility : public UObject
{
public:
	GENERATED_BODY()

	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static const FVector GetAimLocationOnActor(AActor const* Actor);

	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static const float AngleBetweenVectors(FVector A, FVector B);
};
