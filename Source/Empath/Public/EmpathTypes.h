// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EmpathTypes.generated.h"

/**
 Holder for all the custom data types and names used by the engine
 */

UENUM(BlueprintType)
enum class EEmpathTeam :uint8
{
	Neutral,
	Player,
	Enemy
};

struct FEmpathBBKeys
{
	static const FName AttackTarget;
	static const FName bCanSeeTarget;
	static const FName bAlert;
	static const FName GoalLocation;
	static const FName BehaviorMode;
	static const FName bIsPassive;
	static const FName DefendTarget;
	static const FName DefendGuardRadius;
	static const FName DefendPursuitRadius;
	static const FName FleeTarget;
	static const FName FleeTargetRadius;
};

USTRUCT(BlueprintType)
struct FSecondaryAttackTarget
{
	GENERATED_USTRUCT_BODY()

public:
	FSecondaryAttackTarget() :
		TargetActor(nullptr),
		TargetPreference(0.5f),
		TargetingRatio(0.3f),
		TargetRadius(0.0f)
	{}

	/**
	* @param TargetPreference	Determines how likely AI is to target this actor. Player is 0.f. Values >0 will emphasize targeting this actor, <0 will de-emphasize.
	*							Preference is one of several factors in target choice.
	* @param TargetingRatio		What (approx) percentage of AI should attack this target.
	* @param TargetRadius		Radius of the target, used to adjust various attack distances and such. Player is assumed to be radius 0.
	*/
	UPROPERTY(Category = "Empath|AI", EditAnywhere, BlueprintReadWrite)
	AActor* TargetActor;

	UPROPERTY(Category = "Empath|AI", EditAnywhere, BlueprintReadWrite)
	float TargetPreference;

	UPROPERTY(Category = "Empath|AI", EditAnywhere, BlueprintReadWrite)
	float TargetingRatio;

	UPROPERTY(Category = "Empath|AI", EditAnywhere, BlueprintReadWrite)
	float TargetRadius;

	bool IsValid() const;
};

UENUM(BlueprintType)
enum class EPlayerAwarenessState : uint8
{
	KnownLocation,
	PotentiallyLost,
	Lost,
	Searching,
	PresenceNotKnown,
};

UENUM(BlueprintType)
enum class EEmpathBehaviorMode : uint8
{
	SearchAndDestroy,
	Defend,
	Flee
};

USTRUCT(BlueprintType)
struct FPerBoneDamageScale
{
	GENERATED_USTRUCT_BODY()

	/**
	* @param BoneNames			Which bones should have this damage scale.
	* @param DamageScale		The value by which damage to the affected bones is multiplied.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Empath|PerBoneDamage")
	TSet<FName> BoneNames;

	UPROPERTY(EditDefaultsOnly, Category = "Empath|PerBoneDamage")
	float DamageScale;

	FPerBoneDamageScale() :
		DamageScale(1.0f)
	{}
};

USTRUCT(BlueprintType)
struct FDamageHistoryEvent
{
	GENERATED_USTRUCT_BODY()

public:
	/**
	* @param DamageAmount		The amount of damage received.
	* @param EventTimestamp		The time the damage was received.
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Empath|DamageHistory")
	float DamageAmount;
	UPROPERTY(EditDefaultsOnly, Category = "Empath|DamageHistory")
	float EventTimestamp;

	FDamageHistoryEvent(float InDamageAmount = 0.0f, float InEventTimestamp = 0.0f)
		: DamageAmount(InDamageAmount),
		EventTimestamp(InEventTimestamp)
	{}
};