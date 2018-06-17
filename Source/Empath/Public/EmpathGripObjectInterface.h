// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EmpathTypes.h"
#include "EmpathGripObjectInterface.generated.h"

class AEmpathHandActor;

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UEmpathGripObjectInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/**
 * Interface to get mutually exclusive grip responses from actors
 */
class EMPATH_API IEmpathGripObjectInterface
{
	GENERATED_IINTERFACE_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/** Returns the grip response of this actor. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = EmpathGripObjectInterface)
	EEmpathGripType GetGripResponse(AEmpathHandActor* GrippingHand, UPrimitiveComponent* GrippedComponent);

	/** Called when this actor is gripped. */
	UFUNCTION(BlueprintNativeEvent, Category = EmpathGripObjectInterface)
	void OnGripped(AEmpathHandActor* GrippingHand, const UPrimitiveComponent* GrippedComponent);
	
};