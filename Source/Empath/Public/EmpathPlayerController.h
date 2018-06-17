// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EmpathPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class EMPATH_API AEmpathPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	/** Called when the controlled VR character becomes stunned. */
	UFUNCTION(BlueprintImplementableEvent, Category = EmpathPlayerController, meta = (DisplayName = "OnCharacterStunned"))
	void ReceiveCharacterStunned(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration);

	/** Called when the controlled VR character is no longer stunned. */
	UFUNCTION(BlueprintImplementableEvent, Category = EmpathPlayerController, meta = (DisplayName = "OnCharacterStunEnd"))
	void ReceiveCharacterStunEnd();
	
	/** Called when the controlled VR character dies. */
	UFUNCTION(BlueprintImplementableEvent, Category = EmpathPlayerController, meta = (DisplayName = "OnCharacterStunEnd"))
	void ReceiveCharacterDeath(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType);
};
