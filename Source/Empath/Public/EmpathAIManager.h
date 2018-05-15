// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "EmpathTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmpathAIManager.generated.h"

class AEmpathAIController;
class AEmpathVRCharacter;


UCLASS(Transient, BlueprintType)
class EMPATH_API AEmpathAIManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEmpathAIManager();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	void OnPlayerTeleported(FTransform const& FromTransform);
	void OnPlayerDied(AEmpathVRCharacter* PlayerWhoDied);
	void OnLostPlayerTimerExpired();

	// Returns the non-player attack targets in the scene
	TArray<FSecondaryAttackTarget> const& GetSecondaryAttackTargets() const { return SecondaryAttackTargets; }

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

	/** Alerts us that the target has been spotted, and updates us as to its location. */
	void UpdateKnownTargetLocation(AActor const* Target);

	/** Returns whether the location of the target may be lost. */
	bool IsPlayerPotentiallyLost() const;

	/** List of the EmpathAIControllers in the scene. */
	UPROPERTY(Category = "Empath|AI", BlueprintReadOnly)
	TArray<AEmpathAIController*> EmpathAICons;

	/** Called when an AI controller dies, to calculate whether any remaining AIs are aware of the player. */
	void CheckForAwareAIs();

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