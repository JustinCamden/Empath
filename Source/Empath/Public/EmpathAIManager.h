// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "EmpathTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmpathAIManager.generated.h"

// Stat groups for UE Profiler
DECLARE_STATS_GROUP(TEXT("EmpathAIManager"), STATGROUP_EMPATH_AIManager, STATCAT_Advanced);

class AEmpathAIController;
class AEmpathPlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNewPlayerAwarenessStateDelegate, EPlayerAwarenessState, NewAwarenessState);

UCLASS(Transient, BlueprintType)
class EMPATH_API AEmpathAIManager : public AActor
{
	GENERATED_BODY()

public:
	static const float HearingDisconnectDist;

	// Sets default values for this actor's properties
	AEmpathAIManager();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void OnPlayerTeleported(AActor* Player, FVector Origin, FVector Destination);
	void OnPlayerDied(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType);
	void OnLostPlayerTimerExpired();

	// Returns the non-player attack targets in the scene
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	TArray<FSecondaryAttackTarget> const& GetSecondaryAttackTargets() const { return SecondaryAttackTargets; }

	/** Adds a secondary attack target to the list. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void AddSecondaryTarget(AActor* NewTarget, float TargetRatio, float TargetPreference, float TargetRadius);

	/** Removes a secondary attack target from the list. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void RemoveSecondaryTarget(AActor* TargetToRemove);

	/** Returns the number of AIs targeting an actor. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void GetNumAITargeting(AActor const* Target, int32& NumAITargetingCandiate, int32& NumTotalAI) const;

	/** Returns the radius of the attack target. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	float GetAttackTargetRadius(AActor* AttackTarget) const;

	/** Returns whether the location of the target is known. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsTargetLocationKnown(AActor const* Target) const;

	/** Alerts us that the target has been spotted, and updates us as to its location. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void UpdateKnownTargetLocation(AActor const* Target);

	/** Returns whether the location of the target may be lost. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	bool IsPlayerPotentiallyLost() const;

	/** Returns whether the location of the target may be lost. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	FVector GetLastKnownPlayerLocation() const { return LastKnownPlayerLocation; }

	/** List of the EmpathAIControllers in the scene. */
	UPROPERTY(Category = "Empath|AI", BlueprintReadOnly)
	TArray<AEmpathAIController*> EmpathAICons;

	/** Called when an AI controller dies, to calculate whether any remaining AIs are aware of the player. */
	void CheckForAwareAIs();

	/** Called when we want to report a noise to the AI manager. This is a heavy operation so do not overuse it.
	* @param NoiseInstigator	The actor that that caused the noise event, even if they are not the source of the noise (i.e. whoever cast the fireball).
	* @param NoiseMaker			The actor directly responsible for the noise (i.e. the fireball).
	* @param Location			Where the sound occured.
	* @param HearingRadius		How far away the sound can be heard from.
	*/
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void ReportNoise(AActor* NoiseInstigator, AActor* NoiseMaker, FVector Location, float HearingRadius);

	/** Called when the player awareness state changes */
	FOnNewPlayerAwarenessStateDelegate OnNewPlayerAwarenessState;

protected:
	/** How long the AI has to find the player after teleporting before declaring him "lost", in seconds. */
	float LostPlayerTimeThreshold;
	float StartSearchingTimeThreshold;

	/** Called when the game starts or when spawned. */
	virtual void BeginPlay() override;

	/** List of the non-player attack targets in the scene. */
	UPROPERTY(Category = "Empath|AI", EditAnywhere, BlueprintReadWrite)
	TArray<FSecondaryAttackTarget> SecondaryAttackTargets;

	/** Variables governing player awareness. */
	bool bPlayerHasEverBeenSeen;
	bool bIsPlayerLocationKnown;
	EPlayerAwarenessState PlayerAwarenessState;
	FVector LastKnownPlayerLocation;
	FTimerHandle LostPlayerTimerHandle;


private:
	/** Removes any stale or dead secondary AI cons from the list. */
	void CleanUpSecondaryTargets();
};