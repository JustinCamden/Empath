// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EmpathCharacterMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class EMPATH_API UEmpathCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	UEmpathCharacterMovementComponent();
public:
	// Overrides
	virtual float GetMaxSpeed() const override;
	virtual void AddImpulse(FVector Impulse, bool bVelocityChange = false) override;
	virtual void AddForce(FVector Force) override;
	virtual void InitCollisionParams(FCollisionQueryParams &OutParams, FCollisionResponseParams& OutResponseParam) const;
	virtual void PerformAirControlForPathFollowing(FVector Direction, float ZDiff) override;
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;

	/** Wrapper for compute floor dist to ensure we get a trace channel response. */
	UFUNCTION(BlueprintCallable, Category = "Pawn|Components|CharacterMovement", meta = (DisplayName = "ComputeFloorDistance", ScriptName = "ComputeFloorDistance"))
	virtual void EmpathComputeFloorDist(FVector CapsuleLocation, float LineDistance, float SweepDistance, float SweepRadius, FFindFloorResult& FloorResult) const;

	/** Returns the source for pathing requests, typically just the character location but maybe something else for a hovering unit. */
	virtual FVector GetPathingSourceLocation() const;

	/** The maximum lateral speed when falling. */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float MaxFallingSpeed;

	/** Scale incoming physics forces by this amount. 0 disables movement from physics forces. */
	UPROPERTY(Category = "Character Movement: Physics Interaction", EditAnywhere, BlueprintReadWrite)
	float PhysicsForceScale;
	
private:
	mutable bool bInK2ComputeFloorDist;
	
};
