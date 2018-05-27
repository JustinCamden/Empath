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
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Health")
		bool CanDie();

	/** Whether the character is currently stunned. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Combat")
		bool IsStunned() const { return bStunned; }

	/** Overridable function for whether the character can be stunned.
	By default, only true if IsStunnable, not Stunned, Dead, and more than
	StunImmunityTimeAfterStunRecovery has passed since the last time we were stunned. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Combat")
		bool CanBeStunned();

	/** Returns whether the current character is in the process of teleporting. */
	UFUNCTION(Category = "Empath|VRCharacter", BlueprintNativeEvent, BlueprintPure)
		bool IsTeleporting() const;


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
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Combat", meta = (DisplayName = "OnStunEnd"))
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
	//	Aim location and damage collision

	/** Returns the location of the damage collision capsule. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Team")
		FVector GetCustomAimLocationOnActor(FVector LookOrigin, FVector LookDirection) const;

	/** The capsule component being used for damage collision. */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
		UCapsuleComponent* DamageCapsule;


	// ---------------------------------------------------------
	//	Teleportation

	/** Implementation of the teleport function */
	UFUNCTION(BlueprintCallable, Category = "Empath|VRCharacter")
		void TeleportToVR(FVector Destination, float DeltaYaw);


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

	/** Normalized regen rate (1/timetoregen100pct) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Health")
		float HealthRegenRate;

	/** How long after last damage to begin regenerating health */
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
	TArray<FDamageHistoryEvent, TInlineAllocator<16>> StunDamageHistory;

	/** Checks whether we should become stunned */
	virtual void TakeStunDamage(const float StunDamageAmount, const AController* EventInstigator, const AActor* DamageCauser);

private:
	/** Whether this character is currently dead. */
	bool bDead;

	/** Whether this character is currently stunned.*/
	bool bStunned;

	/** The player controller for this VR character. */
	AEmpathPlayerController* EmpathPlayerController;

};
