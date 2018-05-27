// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "EmpathTypes.h"
#include "CoreMinimal.h"
#include "VRAIController.h"
#include "EmpathTeamAgentInterface.h"
#include "EmpathAIController.generated.h"

// Stat groups for UE Profiler
DECLARE_STATS_GROUP(TEXT("EmpathAICon"), STATGROUP_EMPATH_AICon, STATCAT_Advanced);

class AEmpathAIManager;
class AEmpathCharacter;
class AEmpathPlayerCharacter;

/**
*
*/
UCLASS()
class EMPATH_API AEmpathAIController : public AVRAIController, public IEmpathTeamAgentInterface
{
	GENERATED_BODY()
public:

	// ---------------------------------------------------------
	//	Static conts
	static const float MinTargetSelectionScore;
	static const FName AIVisionTraceTag;
	static const float MinDefenseGuardRadius;
	static const float MinDefensePursuitRadius;
	static const float MinFleeTargetRadius;

	// ---------------------------------------------------------
	//	Important Misc

	/** Constructor like behavior. */
	AEmpathAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Override for BeginPlay to register delegates on the VR Character. */
	virtual void BeginPlay() override;

	/** Override for EndPlay that enures we are unregistered with the AI manager. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Registers this AI controller with the AI Manager. */
	void RegisterAIManager(AEmpathAIManager* RegisteredAIManager);

	/** Returns whether we have registered with the AI Manager. */
	bool IsRegisteredWithAIManager() { return (AIManager != nullptr); }

	// ---------------------------------------------------------
	//	TeamAgent Interface

	/** Returns the team number of the actor. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Game")
	EEmpathTeam GetTeamNum() const;

	// ---------------------------------------------------------
	//	Getters and setters

	/** Gets the world AI Manager. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable, BlueprintPure)
	AEmpathAIManager* GetAIManager() const { return AIManager; }

	/** Returns the controlled Empath Character. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable, BlueprintPure)
	AEmpathCharacter* GetEmpathChar() const;

	/** Returns whether the current attack target's location has been lost i.e. no AI can see it. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsTargetLost() const;

	/** Returns whether we are facing the attack target. */
	UFUNCTION(BlueprintCallable, Category = "AI")
	bool IsFacingTarget(FVector TargetLoc, float AngleToleranceDeg = 10.f) const;

	/** Sets the attack target. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable)
	void SetAttackTarget(AActor* NewTarget);

	/** Returns the AI's current attack target. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	AActor* GetAttackTarget() const;

	/** Sets the AI's current defend target. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetDefendTarget(AActor* NewDefendTarget);

	/** Returns the AI's current defend target. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	AActor* GetDefendTarget() const;

	/** Sets the AI's current flee target. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetFleeTarget(AActor* NewFleeTarget);

	/** Returns the AI's current flee target. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	AActor* GetFleeTarget() const;

	/** Sets the target visibility and updates the AI manager if necessary. */
	void SetCanSeeTarget(bool bNewCanSeeTarget);

	/** Returns whether we can see the current attack target in the Blackboard. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	bool CanSeeTarget() const;

	/** Gets the eyes viewpoint for vision. Uses the named socket or skeletal bone as named by our Vision Bone name. */
	virtual void GetActorEyesViewPoint(FVector& out_Location, FRotator& out_Rotation) const override;

	/** Set the goal location for the most recent move request. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetGoalLocation(FVector GoalLocation);

	/** Get the goal location for the most recent move request. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	FVector GetGoalLocation() const;

	/** Sets whether the AI controller is passive. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable)
	void SetIsPassive(bool bNewPassive);

	/** Returns whether the AI controller is passive. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable, BlueprintPure)
	bool IsPassive() const;

	/** Returns the maximum distance to the target fixed for effective range and radius of the target, set in the Empath Character. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	float GetMaxAttackRange() const;

	/** Returns the maximum distance to the target fixed for effective range and radius of the target, set in the Empath Character. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	float GetMinAttackRange() const;

	/** Returns distance to "surface" of target -- target radius is factored in. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	float GetRangeToTarget() const;

	/** Returns true if this AI should move. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool WantsToReposition(float DesiredMaxAttackRange, float DesiredMinAttackRange) const;

	/** Returns the point to aim at (for shooting, etc). */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	virtual FVector GetAimLocation() const;

	/** Sets a custom override aim location. Otherwise, will be calculated the normal way. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetCustomAimLocation(FVector AimLocation);

	/** Clears the custom aim location. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void ClearCustomAimLocation();

	/** Returns the currently set custom aim location of this actor and wether it is currently using a custom aim location */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsUsingCustomAimLocation() const {return bUseCustomAimLocation;}

	/** Returns whether the controlled character is dead. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsDead() const;

	/** Returns number of AIs in the given radius, excluding this one. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	int32 GetNumAIsNearby(float Radius) const;

	/** Gets whether this AI is currently running the behavior tree. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsAIRunning() const;

	/** Gets whether the attack target is teleporting. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsTargetTeleporting() const;

	/** Gets the time since the last time we saw the target teleport. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	float GetTimeSinceLastSawAttackTargetTeleport() const;

	// ---------------------------------------------------------
	//	Events and receives

	void OnLostPlayerTarget();

	/** Called when the player target is lost. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnLostPlayerTarget"))
	void ReceiveLostPlayerTarget();

	void OnSearchForPlayerStarted();

	/** Called when this AI should begin searching for the target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnSearchForPlayerStarted"))
	void ReceiveSearchForPlayerStarted();

	void OnTargetSeenForFirstTime();

	/** Triggered when an AI has seen its target for the very first time and this AI is not passive. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnTargetSeenForFirstTime"))
	void ReceiveTargetSeenForFirstTime();

	/** Triggered when an AI found a player whose location was previously unknown. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnTargetSpotted"))
	void ReceiveTargetSpotted();

	/** Override that updates the Blackboard with our goal location when successful, and that binds OnBump to our character's capsule collisions. */
	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest,
		FNavPathSharedPtr* OutPath) override;

	/** Called when this AI has made a successful move request. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Empath|AI", meta = (DisplayName = "OnTargetSpotted"))
	void ReceiveMoveTo(FVector GoalLocation);

	/** Override that unbinds the capsule bump request */
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	/** Called when a new attack target is selected */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnNewAttackTarget"))
	void ReceiveNewAttackTarget(AActor* OldTarget, AActor* NewTarget);

	/** Called when the current attack target dies */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnAttackTargetDied"))
	void ReceiveAttackTargetDied(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType);

	/** Called when the attack target teleports. */
	void OnAttackTargetTeleported(AActor* Target, FVector Origin, FVector Destination);

	/** Called when the attack target teleports. */
	UFUNCTION(Category = "Empath|AI", BlueprintImplementableEvent, meta = (DisplayName = "OnAttackTargetTeleported"))
	void ReceiveAttackTargetTeleported(AActor* Target, FVector Origin, FVector Destination);

	/** Called when the controlled character dies. */
	void OnCharacterDeath(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType);

	/** Called when the controlled character dies. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnCharacterDeath"))
	void ReceiveCharacterDeath(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType);

	/** Called when the controlled character becomes stunned. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnCharacterStunned"))
	void ReceiveCharacterStunned(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration);

	/** Called when the controlled character is no longer stunned. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnCharacterStunEnd"))
	void ReceiveCharacterStunEnd();

	// ---------------------------------------------------------
	//	State flow / Commands

	/** Updates what targets are visible and which is our current attack target */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void UpdateTargetingAndVision();

	/** Alerts us that the target has been spotted, and updates the AI manager as to its location. */
	void UpdateKnownTargetLocation(AActor const* AITarget);

	/** Exposes pausing movement along the path. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void PausePathFollowing();

	/** Exposes resuming movement along the path. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void ResumePathFollowing();

	/** Fires the OnAIInitialized event on the Empath Character. */
	virtual bool RunBehaviorTree(UBehaviorTree* BTAsset) override;

	/** Asks this AI to move. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void RequestReposition() { bShouldReposition = true; };

	// ---------------------------------------------------------
	//	Behavior modes

	/** Tells the AIs behavior to search and destroy. Should be default behavior for most AIs. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetBehaviorModeSearchAndDestroy(AActor* InitialAttackTarget);

	/** Tells the AIs behavior to defend the defend target, and pursue targets if they get too close. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetBehaviorModeDefend(AActor* DefendTarget, float GuardRadius = 500.0f, float PursuitRadius = 750.0f);

	/** Tells the AIs behavior to flee away from the flee target. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetBehaviorModeFlee(AActor* FleeTarget, float TargetRadius = 200.f);

	/** Gets the current behavior mode. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	EEmpathBehaviorMode GetBehaviorMode() const;

	/** Gets the current guard radius for defense mode. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	float GetDefendGuardRadius() const;

	/** Gets the current pursuit radius for defense mode. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	float GetDefendPursuitRadius() const;

	/** Gets the radius of the flee target. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	float GetFleeTargetRadius() const;

	/** Returns whether we want to engage our attack target, depending on whether we can see it and what behavior mode we are in. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool WantsToEngageAttackTarget() const;

protected:

	// ---------------------------------------------------------
	//	TeamAgent Interface

	/** The team of this actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Empath|AI")
		EEmpathTeam Team;

	// ---------------------------------------------------------
	//	Vision variables

	/** Angle of our forward vision. Unlimited by range. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI", meta = (Units = deg))
	float ForwardVisionAngle;

	/** Angle of our peripheral vision. Limited by range Peripheral Vision Range, */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI", meta = (Units = deg))
	float PeripheralVisionAngle;

	/** Range of our peripheral vision.*/
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float PeripheralVisionDistance;

	/** Range at which we will automatically see targets in line of sight. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float AutoSeeDistance;

	/** Whether we should draw debug vision cones when updating vision. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	bool bDrawDebugVision;

	/** Whether we should draw blocking hits when updating vision vision. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	bool bDrawDebugLOSBlockingHits;

	// ---------------------------------------------------------
	//	Target selection variables

	/** Target selection score curve for distance. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	UCurveFloat* TargetSelectionDistScoreCurve;

	/** Target selection distance score weight. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float DistScoreWeight;

	/** Target selection score curve for angle. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	UCurveFloat* TargetSelectionAngleScoreCurve;

	/** Target selection angle score weight. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float AngleScoreWeight;

	/** Target selection preferred target weight. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float TargetPrefScoreWeight;

	/** Target selection current target weight. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float CurrentTargetPreferenceWeight;

	/** Target selection ratio AIs target target weight. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float TargetingRatioBonusScoreWeight;

	/** If true, this AI can target other targets besides the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bCanTargetSecondaryTargets;

	/** If true, this AI will not target the player*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bIgnorePlayer;

	/** If true, this AI will skip normal target selection checks and always target the player. Overrides Ignore Player. Useful for optimizing AI that will only encounter the player */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bAutoTargetPlayer;

	/** If true, this AI will skip vision cone checks for its target. Useful for optimizing AI that should not be ambushable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bIgnoreVisionCone;

	/** Gets the selection score for a target based on the above weights. */
	float GetTargetSelectionScore(AActor* CandidateTarget, 
		float DesiredCandidateTargetingRatio, 
		float CandidateTargetPriority, 
		AActor* CurrentTarget) const;
	void UpdateAttackTarget();
	void UpdateVision(bool bTestImmediately = false);

	/** Bone name to use for origin of vision traces. */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	FName VisionBoneName;

	/** Handle for our async vision trace. */
	FTraceHandle CurrentLOSTraceHandle;

	/** Handle for stale vision trace we need to ignore. */
	FTraceHandle LOSTraceHandleToIgnore;

	/** Delegate four when our vision trace. */
	FTraceDelegate OnLOSTraceCompleteDelegate;

	/** Called when our async vision trace is complete. */
	void OnLOSTraceComplete(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);

	// ---------------------------------------------------------
	//	Obstance avoidance

	/** Whether we should detect bumping into other Empath Characters and ask them to move. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Empath|AI")
	bool bDetectStuckAgainstOtherAI;

	/** The last time we bumped another Empath Character's capsule. */
	float LastCapsuleBumpWhileMovingTime;

	/** The minimum number of bumps before we ask whoever is blocking us to move. */
	int32 MinCapsuleBumpsBeforeRepositioning;

	/** The maximum time between bumps before we reset the counter. */
	float MaxTimeBetweenConsecutiveBumps;

	/** The current number of bumps while attempting to move. */
	int32 NumConsecutiveBumpsWhileMoving;

	/** If true, next WantsToAdvance will return true. Reset when WantsToAdvance is called, so that we don't keep returning true infinitely. */
	bool bShouldReposition;

	void OnCapsuleBumpDuringMove(UPrimitiveComponent* HitComp,
		AActor* OtherActor, UPrimitiveComponent* OtherComp, 
		FVector NormalImpulse, 
		const FHitResult& Hit);

	/** When this AI has been on a path for this long and not reached the destination, fail the path request so we try again. If set to 0, this feature is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	float TimeOnPathUntilRepath;

	/** The attack target radius of the currently set attack target. */
	UPROPERTY(Category = "Empath|AI", BlueprintReadOnly)
	float CurrentAttackTargetRadius;

	// ---------------------------------------------------------
	//	Aiming

	/** True to manually override aim location (via CustomAimLocation). False to determine aim location the normal way. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|AI")
	bool bUseCustomAimLocation;

	/** Aim location to use if bUseCustomAimLocation is true. Ignored otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|AI")
	FVector CustomAimLocation;

	// ---------------------------------------------------------
	//	Misc functions and properties

	/* Time we last saw the attack target teleport*/
	float LastSawAttackTargetTeleportTime;

	/** Cleans up this AI controller from the list maintained in the AI Manager */
	void UnregisterAIManager();

private:
	/** The AI manager we grabbed when spawning. */
	AEmpathAIManager* AIManager;

	/** Our index inside the list of EmpathAICons stored in the AI manager. */
	int32 AIManagerIndex;

	/** Stored reference to the Empath Character we control */
	AEmpathCharacter* CachedEmpathChar;
};