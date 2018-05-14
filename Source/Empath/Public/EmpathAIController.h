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
class AEmpathVRCharacter;

/**
*
*/
UCLASS()
class EMPATH_API AEmpathAIController : public AVRAIController, public IEmpathTeamAgentInterface
{
	GENERATED_BODY()
public:
	AEmpathAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Empath|AI")
	EEmpathTeam Team;

	virtual void BeginPlay() override;

	// ---------------------------------------------------------
	//	TeamAgent Interface

	/** Returns the team number of the actor */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Game")
	EEmpathTeam GetTeamNum() const;

	/** Gets the world AI Manager. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable, BlueprintPure)
	AEmpathAIManager* GetAIManager() const { return AIManager; }

	/** Gets the controlled Empath Character. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable, BlueprintPure)
	AEmpathCharacter* GetEmpathChar() const { return EmpathChar; }

	void OnLostPlayerTarget();

	/** Called when the player target is lost. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnLostPlayerTarget"))
	void ReceiveLostPlayerTarget();

	void OnPlayerSearchStarted();

	/** Called when this AI should begin searching for the target. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnPlayerSearchStarted"))
	void ReceiveOnPlayerSearchStarted();

	/** Returns whether the current attack target's location has been lost i.e. no AI can see it. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsTargetLost() const;

	/** Sets the attack target. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable)
	void SetAttackTarget(AActor* NewTarget);

	/** Returns the AI's current attack target. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	AActor* GetAttackTarget() const;

	/** Returns whether we can see the current attack target in the Blackboard. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	bool CanSeeTarget() const;

	/** Returns the AI's current defend target. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	AActor* GetDefendTarget() const;

	/** Sets the AI's current defend target. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetDefendTarget(AActor* NewDefendTarget);

	/** Returns the AI's current flee target. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	AActor* GetFleeTarget() const;

	/** Sets the AI's current flee target. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetFleeTarget(AActor* NewFleeTarget);

	/** Get the goal location for the most recent move request. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	FVector GetGoalLocation() const;

	/** Set the goal location for the most recent move request. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetGoalLocation(FVector GoalLocation);

	/** Sets whether the AI controller is passive. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable)
	void SetIsPassive(bool bNewPassive);

	/** Returns whether the AI controller is passive. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable, BlueprintPure)
	bool IsPassive() const;

	/** The attack target radius of the currently set attack target. */
	UPROPERTY(Category = "Empath|AI", BlueprintReadOnly)
	float CurrentAttackTargetRadius;

	/** Updates what targets are visible and which is our current attack target */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void UpdateTargetingAndVision();

	/** Sets the known target location on this actor and the AI manager */
	void SetKnownTargetLocation(AActor const* AITarget);

	/** Gets the eyes viewpoint for vision. Uses the named socket or skeletal bone 
	as named by our Vision Bone name. */
	virtual void GetActorEyesViewPoint(FVector& out_Location, FRotator& out_Rotation) const override;

	/** Angle of our forward vision. Unlimited by range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI", meta = (Units = deg))
	float ForwardVisionAngle;

	/** Angle of our peripheral vision. Limited by range Peripheral Vision Range, */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI", meta = (Units = deg))
	float PeripheralVisionAngle;

	/** Range of our peripheral vision.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	float PeripheralVisionDistance;

	/** Range at which we will automatically see targets in line of sight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	float AutoSeeDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bDrawDebugVision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bDrawDebugLOSBlockingHits;

	UFUNCTION(BlueprintCallable, Category = "AI")
	bool IsFacingTarget(FVector TargetLoc, float AngleToleranceDeg = 10.f) const;

	/** Triggered when an AI has seen its target for the very first time and this AI is not passive. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnTargetSeenForFirstTime"))
	void ReceiveTargetSeenForFirstTime();

	/** Triggered when this AI found the player when no one else knew where he was. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnTargetSpotted"))
	void ReceiveTargetSpotted();

	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, 
	FNavPathSharedPtr* OutPath) override;

	/** Called when this AI has made a successful move request. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Empath|AI", meta = (DisplayName = "OnTargetSpotted"))
	void ReceiveMoveTo(FVector GoalLocation);

	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void PausePathFollowing();

	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void ResumePathFollowing();

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	virtual bool RunBehaviorTree(UBehaviorTree* BTAsset) override;

	/** Sets a custom override aim location. Otherwise, will be calculated the normal way. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void SetCustomAimLocation(FVector AimLocation);

	/** Clears the custom aim location. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void ClearCustomAimLocation();

	/** Returns the point to aim at (for shooting, etc). */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	virtual FVector GetAimLocation() const;

	/** Called when a new attack target is selected */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnNewAttackTarget"))
	void ReceiveNewAttackTarget(AActor* OldTarget, AActor* NewTarget);

	/** Called when the current attack target dies */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI", meta = (DisplayName = "OnAttackTargetDied"))
	void ReceiveAttackTargetDied();



	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsDead() const;

	/** Returns true if this AI should move. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool ShouldAdvance(float DesiredMaxAttackRange) const;

	/** Returns the maximum distance to the target fixed for effective range and radius of the target. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	float GetMaxAttackRange() const;

	/** Returns distance to "surface" of target -- target radius is factored in. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	float GetRangeToTarget() const;

	/** Returns number of AIs in the given radius, excluding this one. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	int32 GetNumAIsNearby(float Radius) const;

	/** Asks this AI to move. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void RequestReposition() { bShouldReposition = true; };

	/** Gets whether this AI is currently running the behavior tree. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsAIRunning() const;

	/** Gets whether the attack target is teleporting. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsTargetTeleporting() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	UCurveFloat* TargetSelectionDistScoreCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	UCurveFloat* TargetSelectionAngleScoreCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float DistScoreWeight;

	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float AngleScoreWeight;

	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float TargetPrefScoreWeight;

	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float CurrentTargetPreferenceWeight;

	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	float TargetingRatioBonusScoreWeight;

	/** If true, this AI can target other targets besides the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bCanTargetSecondaryTargets;

	/** If true, this AI will not target the player*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bIgnorePlayer;

	/** If true, this AI will skip normal target selection checks and always target the player. 
	Overrides Ignore Player. 
	Useful for optimizing AI that will only encounter the player */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bAutoTargetPlayer;

	/** If true, this AI will skip vision cone checks for its target. Useful for 
	optimizing AI that should not be ambushable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bIgnoreVisionCone;

	float GetTargetSelectionScore(AActor* CandidateTarget, 
		float DesiredCandidateTargetingRatio, 
		float CandidateTargetPriority, 
		AActor* CurrentTarget) const;
	void UpdateAttackTarget();
	void UpdateVision(bool bTestImmediately = false);

	/** Bone name to use for origin of vision traces */
	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	FName VisionBoneName;

	/** Sets the target visibility and updates the AI manager if necessary. */
	void SetCanSeeTarget(bool bNewCanSeeTarget);

	FTraceHandle LOSTraceHandleToIgnore;
	FTraceHandle CurrentLOSTraceHandle;
	FTraceDelegate OnLOSTraceCompleteDelegate;
	void OnLOSTraceComplete(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);

	/** Whether we should detect bumping into other Empath Characters and as them to move. */
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

	/** If true, next WantsToAdvance will return true. Reset when WantsToAdvance is called,
	so that we don't keep returning true infinitely. */
	bool bShouldReposition;

	UFUNCTION()
	void OnCapsuleBumpDuringMove(UPrimitiveComponent* HitComp,
	AActor* OtherActor, UPrimitiveComponent* OtherComp, 
	FVector NormalImpulse, 
	const FHitResult& Hit);

	/** True to manually override aim location (via CustomAimLocation).
	False to determine aim location the normal way. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|AI")
	bool bUseCustomAimLocation;

	/** Aim location to use if bUseCustomAimLocation is true.
	Ignored otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|AI")
	FVector CustomAimLocation;

	/** Called when the attack target teleports. */
	UFUNCTION()
	void OnAttackTargetTeleported(AActor* Target, FTransform From, FTransform Destination);

	float LastSawAttackTargetTeleportTime;
	float GetTimeSinceLastSawAttackTargetTeleport() const;

	/** Called when the attack target teleports. */
	UFUNCTION(Category = "Empath|AI", BlueprintImplementableEvent, meta = (DisplayName = "OnAttackTargetTeleported"))
	void ReceiveAttackTargetTeleported(AActor* Target, FTransform From, FTransform Destination);

	/** When this AI has been on a path for this long and not reached the destination, 
	fail the path request so we try again. If set to 0, this feature is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	float TimeOnPathUntilRepath;

private:
	AEmpathAIManager* AIManager;
	AEmpathCharacter* EmpathChar;
};
