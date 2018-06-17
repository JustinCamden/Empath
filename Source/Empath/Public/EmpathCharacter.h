// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EmpathTypes.h"
#include "EmpathTeamAgentInterface.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EmpathTypes.h"
#include "EmpathCharacter.generated.h"

// Stat groups for UE Profiler
DECLARE_STATS_GROUP(TEXT("EmpathCharacter"), STATGROUP_EMPATH_Character, STATCAT_Advanced);

// Notify delegate
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnCharacterDeathDelegate, FHitResult const&, KillingHitInfo, FVector, KillingHitImpulseDir, const AController*, DeathInstigator, const AActor*, DeathCauser, const UDamageType*, DeathDamageType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCharacterStunnedDelegate, const AController*, StunInstigator, const AActor*, StunCauser, const float, StunDuration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterStunEndDelegate);

class AEmpathAIController;
class UDamageType;
class UPhysicalAnimationComponent;
class ANavigationData;

UCLASS()
class EMPATH_API AEmpathCharacter : public ACharacter, public IEmpathTeamAgentInterface
{
	GENERATED_BODY()

public:	
	// Const declarations
	static const FName PhysicalAnimationComponentName;
	static const FName MeshCollisionProfileName;
	static const float RagdollCheckTimeMin;
	static const float RagdollCheckTimeMax;
	static const float RagdollRestThreshold_SingleBodyMax;
	static const float RagdollRestThreshold_AverageBodyMax;


	// Sets default values for this character's properties
	AEmpathCharacter(const FObjectInitializer& ObjectInitializer);

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Override for Take Damage that calls our own custom Process Damage script (Since we can't override the OnAnyDamage event in c++)
	virtual float TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;


	// ---------------------------------------------------------
	//	TeamAgent Interface

	/** Returns the team number of the actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = EmpathCharacter)
	EEmpathTeam GetTeamNum() const;

	// ---------------------------------------------------------
	//	Setters and getters

	/** Returns the controlling Empath AI controller. */
	UFUNCTION(BlueprintCallable, Category = EmpathCharacter)
	AEmpathAIController* GetEmpathAICon() const;

	/** Wrapper for Get Distance To function that accounts for VR characters. */
	UFUNCTION(BlueprintCallable, Category = EmpathCharacter)
	float GetDistanceToVR(const AActor* OtherActor) const;

	/** Overridable function for whether the character can currently die. By default, only true if not Invincible and not Dead. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Combat")
	bool CanDie();

	/** Whether the character is currently dead. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Combat")
	bool IsDead() const { return bDead; }

	/** Whether the character is currently stunned. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Combat")
	bool IsStunned() const { return bStunned; }

	/** Overridable function for whether the character can be stunned. 
	By default, only true if IsStunnable, not Stunned, Dead or Ragdolling, 
	and more than StunImmunityTimeAfterStunRecovery has passed since the last time we were stunned. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Combat")
	bool CanBeStunned();
	
	/** Overridable function for whether the character can ragdoll. 
	By default, only true if AllowRagdoll and not already Ragdolling. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Combat")
	bool CanRagdoll();

	/** Whether the character is currently ragdolling. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Combat")
	bool IsRagdolling() const { return bRagdolling; }

	/** Whether the ragdolled character is currently at rest. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Physics")
	bool IsRagdollAtRest() const;

	// ---------------------------------------------------------
	//	Events and receives

	/** Called when the behavior tree has begun running. */
	UFUNCTION(BlueprintImplementableEvent, Category = "EmpathCharacter", meta = (DisplayName = "On AI Initialized"))
	void ReceiveAIInitalized();

	/** Called when the character dies or their health depletes to 0. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Combat", meta = (DisplayName = "Die"))
	void ReceiveDie(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType);

	/** Called when the character dies or their health depletes to 0. */
	UPROPERTY(BlueprintAssignable, Category = "EmpathCharacter|Combat")
	FOnCharacterDeathDelegate OnDeath;

	/** Called when character becomes stunned. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Combat", meta = (DisplayName = "Be Stunned"))
	void ReceiveStunned(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration);

	/** Called when character becomes stunned. */
	UPROPERTY(BlueprintAssignable, Category = "EmpathCharacter|Combat")
	FOnCharacterStunnedDelegate OnStunned;

	/** Called when character stops being stunned. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Combat", meta = (DisplayName = "On Stun End"))
	void ReceiveStunEnd();

	/** Called when character stops being stunned. */
	UPROPERTY(BlueprintAssignable, Category = "EmpathCharacter|Combat")
	FOnCharacterStunEndDelegate OnStunEnd;

	/** Called on character physics state end. */
	UFUNCTION(BlueprintImplementableEvent, Category = "EmpathCharacter|Physics", meta = (DisplayName = "On End Character PhysicsState"))
	void ReceiveEndCharacterPhysicsState(EEmpathCharacterPhysicsState OldState);

	/** Called on character physics state begin. */
	UFUNCTION(BlueprintImplementableEvent, Category = "EmpathCharacter|Physics", meta = (DisplayName = "On Begin Character Physics State"))
	void ReceiveBeginCharacterPhysicsState(EEmpathCharacterPhysicsState NewState);

	/** Called when the character begins ragdolling. */
	UFUNCTION(BlueprintImplementableEvent, Category = "EmpathCharacter|Physics", meta = (DisplayName = "On Start Ragdoll"))
	void ReceiveStartRagdoll();

	/** Called when the character stops ragdolling. */
	UFUNCTION(BlueprintImplementableEvent, Category = "EmpathCharacter|Physics", meta = (DisplayName = "OnStopRagdoll"))
	void ReceiveStopRagdoll();

	/** Called when we begin recovering from the ragdoll. By default, calls StopRagdoll. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Physics", meta = (DisplayName = "On Start Recovery From Ragdoll"))
	void ReceiveStartRecoverFromRagdoll();


	// ---------------------------------------------------------
	//	General Combat

	/** The maximum effective combat range for the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combat)
	float MaxEffectiveDistance;

	/** The minimum effective combat range for the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Combat)
	float MinEffectiveDistance;

	/** The lowest arc of our nav recovery jump, where 1.0f is a straight line to the target, and 0.0f is straight up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	float NavRecoveryJumpArcMin;

	/** The highest arc of our nav recovery jump, where 1.0f is a straight line to the target, and 0.0f is straight up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	float NavRecoveryJumpArcMax;


	// ---------------------------------------------------------
	//	Stun handling

	/** Whether this character is stunnable in principle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	bool bStunnable;

	/** How much the character has to accrue the StunDamageThreshold and become stunned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	float StunTimeThreshold;

	/** Orders the character to be stunned. If StunDuration <= 0, then stun must be removed by EndStun(). */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Combat")
	void BeStunned(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration = 3.0);

	FTimerHandle StunTimerHandle;

	/** Ends the stun effect on the character. Called automatically at the end of stun duration if it was > 0. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Combat")
	void EndStun();

	/** How much damage needs to be done in the time defined in StunTimeThreshold for the character to become stunned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	float StunDamageThreshold;

	/** How long a stun lasts by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	float StunDurationDefault;

	/** How long after recovering from a stun that this character should be immune to being stunned again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	float StunImmunityTimeAfterStunRecovery;

	/** The last time that this character was stunned. */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|Combat")
	float LastStunTime;

	/** History of stun damage that has been applied to this character. */
	TArray<FEmpathDamageHistoryEvent, TInlineAllocator<16>> StunDamageHistory;

	/** Checks whether we should become stunned */
	virtual void TakeStunDamage(const float StunDamageAmount, const AController* EventInstigator, const AActor* DamageCauser);


	// ---------------------------------------------------------
	//	Health and damage

	/** The maximum health of the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	float MaxHealth;

	/** The current health of the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	float CurrentHealth;

	/** Whether this character can take damage or die, in principle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	bool bInvincible;

	/** Whether this character can take damage from friendly fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	bool bCanTakeFriendlyFire;

	/** Whether this character should receive physics impulses from damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	bool bAllowDamageImpulse;

	/** Whether this character should receive physics impulses upon death. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	bool bAllowDeathImpulse;

	/** Orders the character to die. Called when the character's health depletes to 0. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Combat")
	void Die(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType);

	/** How long we should wait after death to clean up / remove the actor from the world. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	float CleanUpPostDeathTime;

	FTimerHandle CleanUpPostDeathTimerHandle;

	/** Cleans up / removes dead actor from the world. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "EmpathCharacter|Combat")
	void CleanUpPostDeath();

	/** Allows us to define weak and strong spots on the character by inflicting different amounts of damage depending on the bone that was hit. */
	UPROPERTY(EditDefaultsOnly, Category = "EmpathCharacter|Combat")
	TArray<FEmpathPerBoneDamageScale> PerBoneDamage;

	/** Allows us to define weak and strong spots on the character by inflicting different amounts of damage depending on the bone that was hit. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Combat")
	float GetPerBoneDamageScale(FName BoneName) const;

	/** Modifies any damage received from Take Damage calls */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Combat")
	float ModifyAnyDamage(float DamageAmount, const AController* EventInstigator, const AActor* DamageCauser, const UDamageType* DamageType);

	/** Modifies any damage from Point Damage calls (Called after ModifyAnyDamage and per bone damage scaling). */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Combat")
	float ModifyPointDamage(float DamageAmount, const FHitResult& HitInfo, const FVector& ShotDirection, const AController* EventInstigator, const AActor* DamageCauser, const UDamageType* DamageTypeCDO);

	/** Modifies any damage from Radial Damage calls (Called after ModifyAnyDamage). */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Combat")
	float ModifyRadialDamage(float DamageAmount, const FVector& Origin, const TArray<struct FHitResult>& ComponentHits, float InnerRadius, float OuterRadius, const AController* EventInstigator, const AActor* DamageCauser, const UDamageType* DamageTypeCDO);

	/** Processes final damage after all calculations are complete. Includes signaling stun damage and death events. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|Combat")
	void ProcessFinalDamage(const float DamageAmount, FHitResult const& HitInfo, FVector HitImpulseDir, const UDamageType* DamageType, const AController* EventInstigator, const AActor* DamageCauser);


	// ---------------------------------------------------------
	//	Physics

	/** Current character physics state. */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|Physics")
	EEmpathCharacterPhysicsState CurrentCharacterPhysicsState;

	/** Physics state presets. */
	UPROPERTY(EditDefaultsOnly, Category = "EmpathCharacter|Physics")
	TArray<FEmpathCharPhysicsStateSettingsEntry> PhysicsSettingsEntries;

	/** Sets a new physics state preset. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Physics")
	bool SetCharacterPhysicsState(EEmpathCharacterPhysicsState NewState);


	// ---------------------------------------------------------
	//	Ragdoll handling

	/** Whether this character is can ragdoll in principle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	bool bAllowRagdoll;

	/** Whether this character should ragdoll on death. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Combat")
	bool bShouldRagdollOnDeath;

	/** Signals the character's to begin ragdollizing. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Physics")
	void StartRagdoll();

	/** Signals the character to stop ragdolling. 
	NewPhysicsState is what we will transition to, since FullRagdoll is done. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Physics")
	void StopRagdoll(EEmpathCharacterPhysicsState NewPhysicsState);

	/** Signals the character's to begin recovering from the ragdoll. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Physics")
	void StartRecoverFromRagdoll();

	/** Physical animation component for controlling the skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Physics")
	UPhysicalAnimationComponent* PhysicalAnimation;

	/** Timer for automatic recovery from ragdoll state. */
	FTimerHandle GetUpFromRagdollTimerHandle;

	/** If character is currently ragdolled, starts the auto-getup-when-at-rest feature. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Physics")
	void StartAutomaticRecoverFromRagdoll();

	/** If character is currently ragdolled, disables the auto-getup-when-at-rest feature. 
	Only affects this instance of ragdoll, 
	subsequent calls to StartRagdoll will restart automatic recovery */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Physics")
	void StopAutomaticRecoverFromRagdoll();

	/** Checks to see if our ragdoll is at rest and whether we are not dead. If so, signals us to get up. */
	void CheckForEndRagdoll();

	/** Updates our ragdoll recovery state. Should only be called on tick. */
	void TickUpdateRagdollRecoveryState();


	// ---------------------------------------------------------
	//	Movement

	/** Returns the source for pathing requests, typically just the character location but maybe something else for a hovering unit (eg the ground below them). */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Movement")
	FVector GetPathingSourceLocation() const;

	/**
	* Traces a jump from StartLocation to Destination.
	* @param JumpArc	In range (0..1). 0 is straight up, 1 is straight at target, linear in between. 0.5 would give 45 deg arc if Start and End were at same height.
	*/
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Movement")
	bool TraceJump(float Radius, FVector StartLocation, FVector Destination, float JumpArc, float PathTracePercent, FVector& OutLaunchVelocity, EDrawDebugTrace::Type DrawDebugType, float DrawDebugDuration);

	/**
	* Traces a jump from StartLocation to Destination and then actually performs the jump.
	* @param JumpArc	In range (0..1). 0 is straight up, 1 is straight at target, linear in between. 0.5 would give 45 deg arc if Start and End were at same height.
	*/
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Movement")
	bool DoJump(FVector GroundDestination, float JumpArc, float PathTracePercent, FVector& OutLaunchVelocity, EDrawDebugTrace::Type DrawDebugType, float DrawDebugDuration);

	/** Refreshes pathing data based on speed, which is used for acceleration/braking along the path. If speed limit or accel/decel data change during pathing, you need to call this. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Movement")
	void RefreshPathingSpeedData();

	/** Sets speed and deceleration for this character. Use this instead of setting directly. If either value is negative, current value will remain unchanged. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Movement")
	void SetWalkingSpeedData(float WalkingSpeed, float WalkingBrakingDeceleration);

	/**
	* Starts a climb action.
	* @param LedgeTransform is the transform of the very edge of the ledge, facing the direction the character will face when climbing.
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "EmpathCharacter|Movement")
	void ClimbTo(FTransform const& LedgeTransform);

	/** The destination of our climb action*/
	UPROPERTY(BlueprintReadWrite)
	FTransform ClimbEndLocation;

	/** The starting location our climb action*/
	UPROPERTY(BlueprintReadWrite)
	FTransform ClimbStartLocation;

	/** Whether we are currently climbing. */
	UPROPERTY(BlueprintReadWrite, Category = "EmpathCharacter|Movement")
	bool bIsClimbing;

	/** Our ending offset from the final climb location, in local space. */
	UPROPERTY(BlueprintReadWrite, Category = "EmpathCharacter|Movement")
	FVector ClimbingOffset;

	/** The default climbing speed of this character. */
	UPROPERTY(BlueprintReadWrite, Category = "EmpathCharacter|Movement")
	float ClimbSpeed;

	/** Calculates the climb duration based on the speed of the character. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EmpathCharacter|Movement")
	float CalcClimbDuration(float Distance) const;

	/** Called when the character begins a climb action. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "EmpathCharacter|Movement")
	void OnClimbTo();

	/** The total duration of the current climb action. */
	UPROPERTY(BlueprintReadWrite, Category = "EmpathCharacter|Movement")
	float CurrentClimbDuration;

	/** The current percent from 0 to 1 of our climb action movement. */
	UPROPERTY(BlueprintReadWrite, Category = "EmpathCharacter|Movement")
	float CurrentClimbPercent;

	/** Begins the actual climbing. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "EmpathCharacter|Movement")
	void ClimbInterp_Start(float ClimbDuration);

	/** Continues the climbing motion. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "EmpathCharacter|Movement")
	void ClimbInterp_Tick(float DeltaTime);

	/** Finishes the climbing motion. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "EmpathCharacter|Movement")
	void ClimbInterp_End();
	
	/** Ends the climbing event. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|Movement")
	void EndClimb(bool bInterrupted = false);

	/** Called when the climb event ends. */
	UFUNCTION(BlueprintImplementableEvent, Category = "EmpathCharacter|Movement")
	void OnClimbEnd(bool bInterrupted);


	// ---------------------------------------------------------
	//	Nav mesh recovery
	
	/** Whether this character should use the navigation recovery system. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|NavMeshRecovery")
	bool bShouldUseNavRecovery;

	/** The type of navigation recovery this character possesses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|NavMeshRecovery")
	EEmpathNavRecoveryAbility NavRecoveryAbility;

	/** Settings for how to search for the navmesh to recover when off the navmesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|NavMeshRecovery")
	FEmpathNavRecoverySettings NavRecoverySettingsOffMesh;

	/** Settings for how to search for the navmesh to recover when on a navmesh island */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|NavMeshRecovery")
	FEmpathNavRecoverySettings NavRecoverySettingsOnIsland;

	/** Gets current settings based on whether we started on an island or not (if we support island recovery). */
	const FEmpathNavRecoverySettings& GetCurrentNavRecoverySettings() const;

	/** The maximum extent of our test for being on the navmesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|NavMeshRecovery")
	FVector NavRecoveryTestExtent;

	/** Whether we expect navmesh pathing to be successful in our current state. */
	bool ExpectsSuccessfulPathInCurrentState() const;

	/** Used for tracking successful path and EQS queries. 
	* The latter doesn't allow us to check a return value or respond to the result, 
	* so we have to wrap requests with this and then see if bPathRequestSuccessful is set. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "EmpathCharacter|NavMeshRecovery")
	void OnRequestingPath();

	/** Called on path request success to clear any lingering nav recovery. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "EmpathCharacter|NavMeshRecovery")
	void OnPathRequestSuccess(FVector GoalLocation);

	/** Called after pathing failure, to determine if it's time to start regaining navmesh. 
	* If it's time to start, sets bFailingNavMesh to true and returns true. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "EmpathCharacter|NavMeshRecovery")
	bool OnPathRequestFailed(FVector GoalLocation);

	/** Begins nav recovery, including resetting the state variables and signaling the
	* AI controller to begin running an EQS query. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|NavMeshRecovery")
	void OnStartNavRecovery(FVector FailedGoalLocation, bool bCurrentlyOnNavMesh);

	/** Ends the lost navmesh state flow. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "EmpathCharacter|NavMeshRecovery")
	void OnNavMeshRecovered();

	/** Called when recovery state fails. By default, restarts the process. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|NavMeshRecovery", meta = (DisplayName = "On Nav Mesh Recovery Failed"))
	void ReceiveNavMeshRecoveryFailed(FVector Location, float TimeSinceStartRecovery, bool bWasOnNavMeshAtStart);

	/** Resets all state variables and clears all failures. */
	void ClearNavRecoveryState();

	/** Returns nav recovery destination. Zero if not set and invalid. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|NavMeshRecovery")
	FVector GetNavRecoveryDestination() const;

	/** Returns the current nav recovery search radius. Returns default if no current search radius s found. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|NavMeshRecovery")
	void GetNavSearchRadiiCurrent(float& CurrentInnerRadius, float& CurrentOuterRadius) const;

	/** Returns the default nav recovery search radius. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|NavMeshRecovery")
	void GetNavSearchRadiiDefault(float& DefaultInnerRadius, float& DefaultOuterRadius) const;

	/** Sets the current nav recovery search radius. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|NavMeshRecovery")
	void SetNavSearchRadiiCurrent(float NewInnerRadius, float NewOuterRadius);

	/** Expands the current nav recovery search radius, */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|NavMeshRecovery")
	void ExpandNavSearchRadiiCurrent(float InnerGrowth, float OuterGrowth);

	/** Sets the nav recovery destination. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|NavMeshRecovery")
	void SetNavRecoveryDestination(FVector Destination);

	/** Returns whether we are currently failing navigation. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|NavMeshRecovery")
	bool IsFailingNavigation() const { return bFailedNavigation; }

	/** Returns true if recovery started while on nav mesh. If so, it's probably on an island compared to the failed destinations. */
	UFUNCTION(BlueprintCallable, Category = "EmpathCharacter|NavMeshRecovery")
	bool IsFailingNavigationFromValidNavMesh() const;

	/** Checks to see whether we have recovered the navmesh. */
	virtual void OnTickNavMeshRecovery(FEmpathNavRecoverySettings const& CurrentSettings, float DeltaTime, FVector Location, FVector RecoveryDestination);

	/** Called after checking to see whether we have recovered the navmesh. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|NavMeshRecovery", meta = (DisplayName = "On Tick Nav Mesh Recovery"))
	void ReceiveTickNavMeshRecovery(float DeltaTime, FVector Location, FVector RecoveryDestination);

	/** Called when we failed to find a recovery destination. */
	virtual void OnNavMeshRecoveryFailed(FVector Location, float TimeSinceStartRecovery);

	/** Called when we failed to find a recovery destination. */
	UFUNCTION(BlueprintNativeEvent, Category = "EmpathCharacter|NavMeshRecovery", meta = (DisplayName = "On Failed To Find Recovery Destination"))
	void ReceiveFailedToFindRecoveryDestination(float DeltaTime, FVector Location, float TimeSinceStartRecovery);

	/** Used for tracking successful path and EQS queries. The latter doesn't allow us to check a return value or respond to the result, so we have to wrap requests with this. */
	UPROPERTY(BlueprintReadWrite, Category = "EmpathCharacter|NavMeshRecovery")
	bool bPathRequestSuccessful;

	/** Whether a path request is currently active. */
	UPROPERTY(BlueprintReadWrite, Category = "EmpathCharacter|NavMeshRecovery")
	bool bPathRequestActive;

	/** True if failing navmesh and trying to recover it. */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|NavMeshRecovery")
	bool bFailedNavigation;

	/** True if we started failing while on navmesh. */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|NavMeshRecovery")
	bool bFailedNavigationStartedOnNavMesh;

	/** The current count of nav mesh pathing failures since the last success. */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|NavMeshRecovery")
	int32 NavFailureCurrentCount;

	/** The first time we failed to find a path. */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|NavMeshRecovery")
	float NavFailureFirstFailTime;

	/** The time we began nav mesh recovery. */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|NavMeshRecovery")
	float NavRecoveryStartTime;

	/** Our location when we began recovery. */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|NavMeshRecovery")
	FVector NavRecoveryStartActorLocation;

	/** Our pathing location when we began recovery. */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|NavMeshRecovery")
	FVector NavRecoveryStartPathingLocation;

	/** Our goal location when we began recovery. */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|NavMeshRecovery")
	FVector NavRecoveryFailedGoalLocation;

	/** Counter that is incremented each time we call TickNavMeshRecovery. Reset to 0 when recovery completes/starts.  */
	UPROPERTY(BlueprintReadOnly, Category = "EmpathCharacter|NavMeshRecovery")
	int32 NavRecoveryCounter;

	/** Updates the current character's navmesh recovery state. Should only be called on Tick  */
	void TickUpdateNavMeshRecoveryState(float DeltaTime);


	// ---------------------------------------------------------
	//	Teleportation

	/** Whether this actor can be teleported to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Teleportation")
	bool bCanBeTeleportedTo;

	/** Gets the best teleport location for this actor. Returns false if there is no valid location or if teleporting to this actor is disabled. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "EmpathCharacter|Teleportation")
	bool GetBestTeleportLocation(FHitResult TeleportHit, 
		FVector TeleportOrigin, 
		FVector& OutTeleportLocation, 
		ANavigationData* NavData = nullptr, 
		TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr) const;

	/** The extent of the teleport location query test for teleporting to this actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Teleportation")
	FVector TeleportQueryTestExtent;

	/** The minimum distance from this actor for valid teleport-to positions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EmpathCharacter|Teleportation")
	float TeleportPositionMinXYDistance;

	/** Called when targeted for tracing teleport. */
	UFUNCTION(BlueprintImplementableEvent, Category = "EmpathCharacter|Teleportation")
	void OnTargetedForTeleport();

	/** Called when un-targeted for tracing teleport. */
	UFUNCTION(BlueprintImplementableEvent, Category = "EmpathCharacter|Teleportation")
	void OnReleasedForTeleport();


protected:

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Default team of this character if it is uncontrolled by a controller (The controller's team takes precedence). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "EmpathCharacter")
	EEmpathTeam DefaultTeam;

private:

	/** Whether the character is currently dead. */
	bool bDead;

	/** Whether the character is currently stunned. */
	bool bStunned;

	/** Whether the character is currently ragdolling. */
	bool bRagdolling;

	/** Whether the character has been signaled to get up from ragdoll. */
	bool bDeferredGetUpFromRagdoll;

	/** Stored reference to our control Empath AI controller */
	AEmpathAIController* CachedEmpathAICon;

	/** Map of physics states to settings for faster lookup */
	TMap<EEmpathCharacterPhysicsState, FEmpathCharPhysicsStateSettings> PhysicsStateToSettingsMap;
};