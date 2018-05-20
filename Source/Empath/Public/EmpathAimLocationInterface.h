// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EmpathAimLocationInterface.generated.h"

/** Interface used so characters can override their aim location or provide one of multiple aim locations depending on direction.*/
UINTERFACE(MinimalAPI)
class UEmpathAimLocationInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
 * 
 */
class EMPATH_API IEmpathAimLocationInterface
{
	GENERATED_IINTERFACE_BODY()
	/** Returns the aim location of the actor. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Empath|Utility")
	FVector GetCustomAimLocationOnActor(FVector LookOrigin, FVector LookDirection) const;
};
