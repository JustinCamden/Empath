// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EmpathTeamAgentInterface.h"
#include "EmpathAimLocationInterface.h"
#include "VRCharacter.h"
#include "EmpathVRCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnVRTeleportDelegate, AActor*, TeleportingActor, FVector, Origin, FVector, Destination);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVRCharacterDeathDelegate);

/**
 * 
 */
UCLASS()
class EMPATH_API AEmpathVRCharacter : public AVRCharacter, public IEmpathTeamAgentInterface, public IEmpathAimLocationInterface
{
	GENERATED_BODY()
public:
	// ---------------------------------------------------------
	//	TeamAgent Interface

	/** Returns the team number of the actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Team")
	EEmpathTeam GetTeamNum() const;

	// ---------------------------------------------------------
	//	TeamAgent Interface

	/** Returns the team number of the actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Team")
	FVector GetCustomAimLocationOnActor(FVector LookOrigin) const;

	AEmpathVRCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(Category = "Empath|Health", BlueprintCallable, BlueprintPure)
	bool IsDead() const { return bDead; }
	
	/** Wrapper for GetDistanceTo function that takes into account that this is a VR character */
	UFUNCTION(Category = "Empath|Utility", BlueprintCallable, BlueprintPure)
	float GetDistanceToVR(AActor* OtherActor) const;

	/** Returns whether the current character is in the process of teleporting. */
	UFUNCTION(Category = "Empath|VRCharacter", BlueprintNativeEvent, BlueprintPure)
	bool IsTeleporting() const;

	/** Implementation of the teleport function */
	UFUNCTION(BlueprintCallable, Category = "Empath|VRCharacter")
	void TeleportToVR(FVector Destination, float DeltaYaw);

	/** Called this when this character teleports. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|VRCharacter", meta = (DisplayName = "OnTeleport"))
	void ReceiveTeleport(FVector Origin, FVector Destination, float DeltaYaw);

	/** Called this when this character teleports. */
	UPROPERTY(BlueprintAssignable, Category = "Empath|Health")
	FOnVRTeleportDelegate OnTeleport;

	/** Implementation of the teleport function */
	UFUNCTION(BlueprintCallable, Category = "Empath|VRCharacter")
	void Die();

	/** Called this when this character teleports. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|VRCharacter", meta = (DisplayName = "OnDeath"))
	void ReceiveDeath();

	/** Called this when the player teleports. */
	UPROPERTY(BlueprintAssignable, Category = "Empath|Health")
	FOnVRCharacterDeathDelegate OnDeath;

protected:
	/** Override for center mass of the character, used for aiming at the character and misc functions */
	UPROPERTY(Category = "Empath|VRCharacter", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	USceneComponent* AimLocationComponent;

	bool bDead;
};
