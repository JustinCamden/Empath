// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "EmpathTypes.h"
#include "CoreMinimal.h"
#include "VRAIController.h"
#include "EmpathAIController.generated.h"

// Stat groups for UE Profiler
DECLARE_STATS_GROUP(TEXT("EmpathAICon"), STATGROUP_EMPATH_AICon, STATCAT_Advanced);

class AEmpathAIManager;

/**
*
*/
UCLASS()
class EMPATH_API AEmpathAIController : public AVRAIController
{
	GENERATED_BODY()
public:
	AEmpathAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditDefaultsOnly, Category = "Empath|AI")
	EEmpathTeam Team;

	/** Gets the world AI Manager. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable, BlueprintPure)
	AEmpathAIManager*	GetAIManager() { return AIManager; }

	/** Sets the attack target. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable)
	void SetAttackTarget(AActor* NewTarget);

	/** Returns the AI's current attack target. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	AActor* GetAttackTarget() const;

	/** Gets whether we can see the current attack target in the Blackboard. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable)
	bool CanSeeTarget() const;

	virtual void BeginPlay() override;

	/** The attack target radius of the currently set attack target. */
	float CurrentAttackTargetRadius;

	/** Updates what targets are visible and which is our current attack target */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void UpdateTargetingAndVision();

	/** Gets the eyes viewpoint for vision. Uses the named socket or skeletal bone 
	as named by our Vision Bone name*/
	virtual void GetActorEyesViewPoint(FVector& out_Location, FRotator& out_Rotation) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI", meta = (Units = deg))
	float ForwardVisionAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI", meta = (Units = deg))
	float PeripheralVisionAngle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	float PeripheralVisionDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	float AutoSeeDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bDrawDebugVision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|AI")
	bool bDrawDebugLOSBlockingHits;

	/** Sets the known target location on this actor and the AI manager */
	void SetKnownTargetLocation(AActor const* AITarget);

	/** Sets whether the AI controller is passive. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable)
	void SetIsPassive(bool bNewPassive);

	/** Returns whether the AI controller is passive. */
	UFUNCTION(Category = "Empath|AI", BlueprintCallable, BlueprintPure)
	bool IsPassive() const;

	/** Triggered when an AI has seen its target for the very first time and this AI is not passive. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI")
	void OnTargetSeenForFirstTime();

	/** Triggered when this AI found the player when no one else knew where he was. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|AI")
	void OnTargetSpotted();

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

	void UpdateTargetVisiblity(bool bNewVisibility);

	FTraceHandle LOSTraceHandleToIgnore;
	FTraceHandle CurrentLOSTraceHandle;
	FTraceDelegate OnLOSTraceCompleteDelegate;
	void OnLOSTraceComplete(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);

private:
	AEmpathAIManager* AIManager;
};
