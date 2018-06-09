// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EmpathTeamAgentInterface.h"
#include "EmpathAimLocationInterface.h"
#include "VRCharacter.h"
#include "EmpathPlayerCharacter.generated.h"

// Stat groups for UE Profiler
DECLARE_STATS_GROUP(TEXT("EmpathPlayerCharacter"), STATGROUP_EMPATH_VRCharacter, STATCAT_Advanced);

// Notify delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnVRTeleportDelegate, AActor*, TeleportingActor, FVector, Origin, FVector, Destination);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnPlayerCharacterDeathDelegate, FHitResult const&, KillingHitInfo, FVector, KillingHitImpulseDir, const AController*, DeathInstigator, const AActor*, DeathCauser, const UDamageType*, DeathDamageType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerCharacterStunnedDelegate, const AController*, StunInstigator, const AActor*, StunCauser, const float, StunDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerCharacterStunEndDelegate);

class AEmpathPlayerController;
class AEmpathHandActor;
class AEmpathTeleportBeacon;
class ANavigationData;
class AEmpathCharacter;

/**
*
*/
UCLASS()
class EMPATH_API AEmpathPlayerCharacter : public AVRCharacter, public IEmpathTeamAgentInterface, public IEmpathAimLocationInterface
{
	GENERATED_BODY()
public:
	// ---------------------------------------------------------
	//	Misc 

	// Override for tick
	virtual void Tick(float DeltaTime) override;
	
	// Override for begin player
	virtual void BeginPlay() override;

	// Constructor to set default variables
	AEmpathPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Pawn overrides for vr.
	virtual FVector GetPawnViewLocation() const override;
	virtual FRotator GetViewRotation() const override;

	// Override for Take Damage that calls our own custom Process Damage script (Since we can't override the OnAnyDamage event in c++)
	virtual float TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Wrapper for GetDistanceTo function that takes into account that this is a VR character */
	UFUNCTION(Category = "Empath|Utility", BlueprintCallable, BlueprintPure)
	float GetDistanceToVR(AActor* OtherActor) const;

	/** Gets the position of the player character at a specific height,
	where 1 is their eye height, and 0 is their feet. */
	UFUNCTION(BlueprintCallable, Category = "Empath|VRCharacter")
	FVector GetScaledHeightLocation(float HeightScale = 0.9f);


	// ---------------------------------------------------------
	//	Setters and getters

	/** Returns the controlling Empath Player Controller. */
	UFUNCTION(Category = "Empath|VRCharacter", BlueprintCallable, BlueprintPure)
	AEmpathPlayerController* GetEmpathPlayerCon() const;

	/** Returns whether this character is currently dead. */
	UFUNCTION(Category = "Empath|Health", BlueprintCallable, BlueprintPure)
	bool IsDead() const { return bDead; }

	/** Overridable function for whether the character can die.
	By default, only true if not Invincible and not Dead. */
	UFUNCTION(Category = "Empath|Health", BlueprintCallable, BlueprintNativeEvent)
	bool CanDie();

	/** Whether the character is currently stunned. */
	UFUNCTION(Category = "Empath|Combat", BlueprintCallable)
	bool IsStunned() const { return bStunned; }

	/** Overridable function for whether the character can be stunned.
	By default, only true if IsStunnable, not Stunned, Dead, and more than
	StunImmunityTimeAfterStunRecovery has passed since the last time we were stunned. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Empath|Combat")
	bool CanBeStunned();

	/** Returns whether the current character is in the process of teleporting. */
	UFUNCTION(Category = "Empath|VRCharacter", BlueprintCallable, BlueprintPure)
	bool IsTeleporting() const { return TeleportState != EEmpathTeleportState::NotTeleporting; }


	// ---------------------------------------------------------
	//	Events and receives

	/** Called when the character dies or their health depletes to 0. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Health", meta = (DisplayName = "Die"))
	void ReceiveDie(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType);

	/** Called when the character dies or their health depletes to 0. */
	UPROPERTY(BlueprintAssignable, Category = "Empath|Health")
	FOnPlayerCharacterDeathDelegate OnDeath;

	/** Called this when this character teleports. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|VRCharacter", meta = (DisplayName = "OnTeleport"))
	void ReceiveTeleport(FVector Origin, FVector Destination, float DeltaYaw);

	/** Called when character becomes stunned. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Combat", meta = (DisplayName = "BeStunned"))
	void ReceiveStunned(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration);

	/** Called when character becomes stunned. */
	UPROPERTY(BlueprintAssignable, Category = "Empath|Combat")
	FOnPlayerCharacterStunnedDelegate OnStunned;

	/** Called when character stops being stunned. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Combat", meta = (DisplayName = "OnStunEnd"))
	void ReceiveStunEnd();

	/** Called when character stops being stunned. */
	UPROPERTY(BlueprintAssignable, Category = "Empath|Combat")
	FOnPlayerCharacterStunEndDelegate OnStunEnd;

	/** Called this when this character teleports. */
	UPROPERTY(BlueprintAssignable, Category = "Empath|Health")
	FOnVRTeleportDelegate OnTeleport;


	// ---------------------------------------------------------
	//	 Team Agent Interface

	/** Returns the team number of the actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Team")
	EEmpathTeam GetTeamNum() const;


	// ---------------------------------------------------------
	//	Aim location

	/** Returns the location of the damage collision capsule. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Team")
	FVector GetCustomAimLocationOnActor(FVector LookOrigin, FVector LookDirection) const;


	// ---------------------------------------------------------
	//	General Teleportation

	/** Implementation of the teleport function. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Teleportation")
	void TeleportToVR(FVector Destination, float DeltaYaw);

	/** Caches the player navmesh for later use by teleportation. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Teleportation")
	void CacheNavMesh();

	/** The the player's navmesh data. */
	UPROPERTY(BlueprintReadWrite, Category = "Empath|Teleportation")
	ANavigationData* PlayerNavData;

	/** The player's navigation filter. */
	TSubclassOf<UNavigationQueryFilter> PlayerNavFilterClass;

	/** The current teleport state of the character. */
	UPROPERTY(BlueprintReadWrite, Category = "Empath|Teleportation")
	EEmpathTeleportState TeleportState;

	/** The target magnitude of teleportation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Empath|Teleportation")
	float TeleportMagnitude;

	/** The target magnitude of dashing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Empath|Teleportation")
	float DashMagnitude;

	/** The current teleport velocity. */
	UPROPERTY(BlueprintReadWrite, Category = "Empath|Teleportation")
	FVector TeleportCurrentVelocity;

	/** The target magnitude of teleportation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Empath|Teleportation")
	float TeleportRadius;

	/** How fast to lerp the teleport velocity from its current to target velocity. */
	float TeleportVelocityLerpSpeed;

	/** The minimum distance from a teleport beacon for it to be a valid teleport target. */ 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Empath|Teleportation")
	float TeleportBeaconMinDistance;

	/** Traces a teleport location from the provided world origin in provided world direction, attempting to reach the provided magnitude. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Teleportation")
	void TraceTeleportLocation(FVector Origin, FVector Direction, float Magnitude);

	/** Object types to collide against when tracing for teleport locations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Teleportation")
	TArray<TEnumAsByte<EObjectTypeQuery>> TeleportTraceObjectTypes;

	/** Spline positions from our last teleport trace. Used to draw the spline if we wish to. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|Teleportation")
	TArray<FVector> TeleportTraceSplinePositions;

	/** Checks to see if our current teleport position is valid. */
	bool IsTeleportTraceValid(FHitResult TeleportHit, FVector TeleportOrigin);

	/** The teleport position currently targeted by a teleport trace. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|Teleportation")
	AEmpathTeleportBeacon* TargetTeleportBeacon;

	/** Called when we target a beacon while tracing teleport. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportation")
	void OnTeleportBeaconTargeted();

	/** Called when we un-target a beacon while tracing teleport. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportation")
	void OnTeleportBeaconReleased();

	/** The Empath Character currently targeted by a teleport trace. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|Teleportation")
	AEmpathCharacter* TargetTeleportChar;

	/** Called when we target an Empath Character while tracing teleport. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportation")
	void OnTeleportCharTargeted();

	/** Called when we un-target an Empath Character while tracing teleport. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportation")
	void OnTeleportCharReleased();

	/** The extent of our query when projecting a point to navigation for teleportation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Teleportation")
	FVector TeleportProjectQueryExtent;

	/** Our last updated teleport location. Zero vector if invalid. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|Teleportation")
	FVector TeleportCurrentLocation;

	/** Whether our teleport location is currently valid. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|Teleportation")
	bool bIsTeleportCurrLocValid;

	/** Used to project a point to the player's navigation mesh and check if it is on the ground. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Teleportation")
	bool ProjectPointToPlayerNavigation(FVector Point, FVector& OutPoint);

	/** Sets whether the currently traced location is valid and calls events as appropriate. */
	void UpdateTeleportCurrLocValid(const bool NewValid);

	/** Called when we find a valid teleport location. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportation")
	void OnTeleportLocationFound();

	/** Called when we lose a valid teleport location. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportation")
	void OnTeleportLocationLost();

	/** Called after a teleport trace is updated. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportation")
	void OnTeleportTraceUpdated();


	// ---------------------------------------------------------
	//	Health and damage

	/** Whether this character is currently invincible. We most likely want to begin with this on,
	and disable it after they have had a chance to load up.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	bool bInvincible;

	/** Whether this character can take damage from friendly fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	bool bCanTakeFriendlyFire;

	/** Our maximum health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	float MaxHealth;

	/** Our Current health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	float CurrentHealth;

	/** Normalized health regen rate, 
	* read as 1 / the time to regen to full health,
	* so 0.333 would be 3 seconds to full regen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	float HealthRegenRate;

	/** How long after the last damage was applied to begin regenerating health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	float HealthRegenDelay;

	/** True if health is regenerating, false otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
	bool bHealthRegenActive;

	/** Orders the character to die. Called when the character's health depletes to 0. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Health")
	void Die(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType);

	/** Modifies any damage received from Take Damage calls */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health")
	float ModifyAnyDamage(float DamageAmount, const AController* EventInstigator, const AActor* DamageCauser, const UDamageType* DamageType);

	/** Modifies any damage from Point Damage calls (Called after ModifyAnyDamage and per bone damage scaling). */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health")
	float ModifyPointDamage(float DamageAmount, const FHitResult& HitInfo, const FVector& ShotDirection, const AController* EventInstigator, const AActor* DamageCauser, const UDamageType* DamageTypeCDO);

	/** Modifies any damage from Radial Damage calls (Called after ModifyAnyDamage). */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health")
	float ModifyRadialDamage(float DamageAmount, const FVector& Origin, const TArray<struct FHitResult>& ComponentHits, float InnerRadius, float OuterRadius, const AController* EventInstigator, const AActor* DamageCauser, const UDamageType* DamageTypeCDO);

	/** Processes final damage after all calculations are complete. Includes signaling stun damage and death events. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health")
	void ProcessFinalDamage(const float DamageAmount, FHitResult const& HitInfo, FVector HitImpulseDir, const UDamageType* DamageType, const AController* EventInstigator, const AActor* DamageCauser);

	/** The last time that this character was stunned. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|Combat")
	float LastDamageTime;

	// ---------------------------------------------------------
	//	Stun handling

	/** Whether this character is stunnable in principle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	bool bStunnable;

	/** How much the character has to accrue the StunDamageThreshold and become stunned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float StunTimeThreshold;

	/** Instructs the character to become stunned. If StunDuration <= 0, then stun must be removed by EndStun(). */
	UFUNCTION(BlueprintCallable, Category = "Empath|Combat")
	void BeStunned(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration = 3.0);

	FTimerHandle StunTimerHandle;

	/** Ends the stun effect on the character. Called automatically at the end of stun duration if it was > 0. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Combat")
	void EndStun();

	/** How much damage needs to be done in the time defined in StunTimeThreshold for the character to become stunned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float StunDamageThreshold;

	/** How long a stun lasts by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float StunDurationDefault;

	/** How long after recovering from a stun that this character should be immune to being stunned again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Combat")
	float StunImmunityTimeAfterStunRecovery;

	/** The last time that this character was stunned. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|Combat")
	float LastStunTime;

	/** History of stun damage that has been applied to this character. */
	TArray<FEmpathDamageHistoryEvent, TInlineAllocator<16>> StunDamageHistory;

	/** Checks whether we should become stunned */
	virtual void TakeStunDamage(const float StunDamageAmount, const AController* EventInstigator, const AActor* DamageCauser);


private:
	/** Whether this character is currently dead. */
	bool bDead;

	/** Whether this character is currently stunned.*/
	bool bStunned;

	/** The player controller for this VR character. */
	AEmpathPlayerController* EmpathPlayerController;

	// ---------------------------------------------------------
	//	Hands

	/** The right hand actor class of the character. */
	UPROPERTY(Category = "Empath|Hand", EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AEmpathHandActor> RightHandClass;

	/** The right hand actor class of the character. */
	UPROPERTY(Category = "Empath|Hand", EditDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AEmpathHandActor> LeftHandClass;

	/** Reference to the right hand actor. */
	UPROPERTY(Category = "Empath|Hand", BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	AEmpathHandActor* RightHandReference;

	/** Reference to the left hand actor. */
	UPROPERTY(Category = "Empath|Hand", BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	AEmpathHandActor* LeftHandReference;

	/** The capsule component being used for damage collision. */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UCapsuleComponent* DamageCapsule;

};
