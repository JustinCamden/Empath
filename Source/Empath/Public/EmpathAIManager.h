// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "EmpathTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmpathAIManager.generated.h"

class AEmpathAIController;


UCLASS(Transient, BlueprintType)
class EMPATH_API AEmpathAIManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEmpathAIManager();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Returns the non-player attack targets in the scene
	TArray<FSecondaryAttackTarget> const& GetSecondaryAttackTargets() const { return SecondaryAttackTargets; };

	/**
	* @param TargetPreference	Determines how likely AI is to target this actor. Player is 0.f. Values >0 will emphasize targeting this actor, <0 will de-emphasize.
	*							Preference is one of several factors in target choice.
	* @param TargetingRatio	What (approx) percentage of AI should attack this target.
	* @param TargetRadius		Radius of the target, used to adjust various attack distances and such. Player is assumed to be radius 0.
	*/


	/** Adds a secondary attack target to the list. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void AddSecondaryTarget(AActor* NewTarget, float TargetRatio, float TargetPreference, float TargetRadius);

	/** Removes a secondary attack target from the list. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	void RemoveSecondaryTarget(AActor* TargetToRemove);

	/** Returns the number of AIs targeting an actor. */
	void GetNumAITargeting(AActor const* Target, int32& NumAITargetingCandiate, int32& NumTotalAI) const;

	/** Returns the radius of the attack target. */
	float GetAttackTargetRadius(AActor* AttackTarget) const;

	/** Returns whether the location of the target is known. */
	bool IsTargetLocationKnown(AActor const* Target) const;

	/** Sets the known location of the target. */
	void SetKnownTargetLocation(AActor const* Target);

	/** Returns whether the location of the target may be lost. */
	bool IsPlayerPotentiallyLost() const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// List of the non-player attack targets in the scene
	UPROPERTY(Category = "Empath|AI", EditAnywhere, BlueprintReadWrite)
	TArray<FSecondaryAttackTarget> SecondaryAttackTargets;

	// Variables governing player awareness
	bool bPlayerHasEverBeenSeen;
	bool bPlayerLocationIsKnown;
	EPlayerAwarenessState PlayerAwarenessState;
	FVector LastKnownPlayerLocation;
	FTimerHandle LostPlayerTimerHandle;

private:
	/** Removes any stale or dead secondary targets from the list. */
	void CleanUpSecondaryTargets();
};