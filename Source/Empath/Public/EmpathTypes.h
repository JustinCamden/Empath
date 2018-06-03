// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EmpathTypes.generated.h"

// Collision definitions
#define ECC_Damage	ECC_GameTraceChannel1

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
	static const FName NavRecoveryDestination;
	static const FName NavRecoverySearchInnerRadius;
	static const FName NavRecoverySearchOuterRadius;
};

struct FEmpathCollisionProfiles
{
	static const FName Ragdoll;
	static const FName PawnIgnoreAll;
	static const FName DamageCollision;
	static const FName HandCollision;
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

UENUM(BlueprintType)
enum class EEmpathCharacterPhysicsState : uint8
{
	/** No physics. */
	Kinematic,

	/** Fully ragdolled. */
	FullRagdoll,

	/** Physical hit reactions */
	HitReact,

	/** Can be hit by the player -- should only on when very close to player */
	PlayerHittable,

	/** Getting up from ragdoll */
	GettingUp,
};

USTRUCT(BlueprintType)
struct FEmpathCharPhysicsStateSettings
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, Category = "Empath|Physics")
	bool bSimulatePhysics;

	UPROPERTY(EditAnywhere, Category = "Empath|Physics")
	bool bEnableGravity;

	UPROPERTY(EditAnywhere, Category = "Empath|Physics")
	FName PhysicalAnimationProfileName;

	UPROPERTY(EditAnywhere, Category = "Empath|Physics")
	FName PhysicalAnimationBodyName;

	UPROPERTY(EditAnywhere, Category = "Empath|Physics")
	FName SimulatePhysicsBodyBelowName;

	UPROPERTY(EditAnywhere, Category = "Empath|Physics")
	FName ConstraintProfileJointName;

	UPROPERTY(EditAnywhere, Category = "Empath|Physics")
	FName ConstraintProfileName;

	FEmpathCharPhysicsStateSettings()
		: bSimulatePhysics(false),
		bEnableGravity(false)
	{}
};

USTRUCT(BlueprintType)
struct FEmpathCharPhysicsStateSettingsEntry
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, Category = "Physics")
	EEmpathCharacterPhysicsState PhysicsState;

	UPROPERTY(EditAnywhere, Category = "Physics")
	FEmpathCharPhysicsStateSettings Settings;
};

struct FVelocityFrame
{
public:

	FVector Velocity;
	FVector AngularVelocity;
	float FrameTimeStamp;

	FVelocityFrame(FVector InVelocity = FVector::ZeroVector, FVector InAngularVelocity = FVector::ZeroVector, float InEventTimestamp = 0.0f)
		: Velocity(InVelocity),
		AngularVelocity(InAngularVelocity),
		FrameTimeStamp(InEventTimestamp)
	{}
};

namespace EmpathNavAreaFlags
{
	const int16 Navigable = (1 << 1);		// this one is defined by the system
	const int16 Climb = (1 << 2);
	const int16 Jump = (1 << 3);
	const int16 Fly = (1 << 4);
};

UENUM(BlueprintType)
enum class EEmpathNavRecoveryAbility : uint8
{
	/** Can recover if starting off nav mesh. Recovered if nav mesh is below us. Generally won't handle "islands" of nav mesh. */
	OffNavMesh,

	/** Can recover when pathing fails while on valid nav mesh. Recovers when a path exists to the recovery destination. Handles "islands" but assumes destination is off the island (was found requiring no path). Implies ability to recover from off nav mesh. */
	OnNavMeshIsland,
};


USTRUCT(BlueprintType)
struct FEmpathNavRecoverySettings
{
	GENERATED_USTRUCT_BODY();

	/** How many pathing failures until we try to regain navmesh? Ignored if <= 0. However if we failed while off navmesh, we'll immediately start recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FailureCountUntilRecovery;

	/** How much time after the first pathing failure until we try to regain navmesh on subsequent failures? Ignored if <= 0 or if FailureCountUntilRecovery > 0. However if we failed while off navmesh, we'll immediately start recovery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FailureTimeUntilRecovery;

	/** How much time since starting recovery to allow until we trigger the failure event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxRecoveryAttemptTime;

	/** The inner radius within which to search for a navmesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SearchInnerRadius;

	/** The outer radius within which to search for a navmesh*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SearchOuterRadius;

	/** How fast the inner search radius should expand*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SearchRadiusGrowthRateInner;

	/** How fast the outer search radius should expand*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SearchRadiusGrowthRateOuter;

	/** Number of frames to wait between recovery attempts. 0=no delay, 1=try every other frame, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (UIMin = 0, ClampMin = 0))
	int32 NavRecoverySkipFrames;

	FEmpathNavRecoverySettings()
		:FailureCountUntilRecovery(0),
		FailureTimeUntilRecovery(4.0f),
		MaxRecoveryAttemptTime(5.0f),
		SearchInnerRadius(100.0f),
		SearchOuterRadius(300.0f),
		SearchRadiusGrowthRateInner(200.0f),
		SearchRadiusGrowthRateOuter(500.0f),
		NavRecoverySkipFrames(1)
	{}
};