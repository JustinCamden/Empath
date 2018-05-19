// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "EmpathDamageType.generated.h"

/**
 * 
 */
UCLASS()
class EMPATH_API UEmpathDamageType : public UDamageType
{
	GENERATED_BODY()

public:
	UEmpathDamageType();

	/** How much stun damage is applied to the target, multiplied by the actual damage amount. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float StunDamageMultiplier;

	/** How much damage should be applied to targets of the same team, multiplied by the actual damage amount. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float FriendlyFireDamageMultiplier;

	/** Whether this damage should ignore per bone damage multipliers in the chracter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bIgnorePerBoneDamageScaling;
	
	/** How much of an we should apply to a character on death, in the direction of the killing hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DeathImpulse;

	/** How much of an additional upward impulse we should apply to a character on death, in the world Z direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float DeathImpulseUpkick;
};
