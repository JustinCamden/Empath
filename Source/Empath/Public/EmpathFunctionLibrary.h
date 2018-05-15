// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EmpathFunctionLibrary.generated.h"

class AEmpathAIManager;

/**
 * 
 */
UCLASS()
class EMPATH_API UEmpathFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Gets the center mass location of an actor, or the overriden location on a VR character. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const FVector GetAimLocationOnActor(AActor const* Actor);

	/** Gets the angle between 2 vectors. (Input does not need to be normalized.) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const float AngleBetweenVectors(FVector A, FVector B);

	/** Gets the world's AI Manager. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static AEmpathAIManager* GetAIManager(UObject* WorldContextObject);
};
