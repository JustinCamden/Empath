// Fill out your copyright notice in the Description page of Project Settings.

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
	UFUNCTION(Category = EmpathVRCharacter, BlueprintCallable, BlueprintPure)
	FVector GetAimLocation();
private:

	/** Override for center mass of the character, used for aiming at the character and misc functions */
	UPROPERTY(Category = EmpathVRCharacter, EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	USceneComponent* AimLocationComponent;
};
