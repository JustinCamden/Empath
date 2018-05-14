// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "VRCharacter.h"
#include "EmpathVRCharacter.generated.h"

/**
 * 
 */
UCLASS()
class EMPATH_API AEmpathVRCharacter : public AVRCharacter
{
	GENERATED_BODY()
public:
	AEmpathVRCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the world location for the center mass of the character.
	 If set to a component, gets that component's location. Otherwise, uses the VR location */
	UFUNCTION(Category = "Empath|VRCharacter", BlueprintCallable, BlueprintPure)
	FVector GetAimLocation() const;

	UFUNCTION(Category = "Empath|VRCharacter", BlueprintCallable, BlueprintPure)
	bool IsDead() const { return bDead; }
	
	/** Wrapper for GetDistanceTo function that takes into account that this is a VR character */
	UFUNCTION(Category = "Empath|Utility", BlueprintCallable, BlueprintPure)
	float GetDistanceToVR(AActor* OtherActor) const;

	/** Returns whether the current character is in the process of teleporting. */
	UFUNCTION(Category = "Empath|VRCharacter", BlueprintNativeEvent, BlueprintPure)
	bool IsTeleporting() const;

protected:
	/** Override for center mass of the character, used for aiming at the character and misc functions */
	UPROPERTY(Category = "Empath|VRCharacter", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	USceneComponent* AimLocationComponent;

	bool bDead;
};
