// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathCharacterMovementComponent.h"
#include "EmpathCharacter.h"
#include "GameFramework/NavMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/CapsuleComponent.h"

UEmpathCharacterMovementComponent::UEmpathCharacterMovementComponent()
{
	// Initialize variables
	MaxFallingSpeed = 5000.f;
	PhysicsForceScale = 1.0f;
	MaxSimulationTimeStep = 0.05f;
	MaxSimulationIterations = 2;

	// This physics interaction is for capsules pushing other objects, we don't want that at all.
	bEnablePhysicsInteraction = false;

	// These settings are *per frame* so we don't want huge adjustments that would affect physics velocity.
	MaxDepenetrationWithGeometry = 5.0f;
	MaxDepenetrationWithPawn = 2.0f;
}

float UEmpathCharacterMovementComponent::GetMaxSpeed() const
{
	return (MovementMode == MOVE_Falling) ? MaxFallingSpeed : Super::GetMaxSpeed();
}

void UEmpathCharacterMovementComponent::AddImpulse(FVector Impulse, bool bVelocityChange)
{
	if (PhysicsForceScale != 0.f)
	{
		Super::AddImpulse(Impulse * PhysicsForceScale, bVelocityChange);
	}
}

void UEmpathCharacterMovementComponent::AddForce(FVector Force)
{
	if (PhysicsForceScale != 0.f)
	{
		Super::AddForce(Force * PhysicsForceScale);
	}
}

void UEmpathCharacterMovementComponent::InitCollisionParams(FCollisionQueryParams &OutParams, FCollisionResponseParams& OutResponseParam) const
{
	Super::InitCollisionParams(OutParams, OutResponseParam);

	if (bInK2ComputeFloorDist && CharacterOwner)
	{
		// We may have collision disabled through a no-reponse to all channels, but we need a response for the traces to work.
		const AEmpathCharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<AEmpathCharacter>();
		if (DefaultCharacter)
		{
			OutResponseParam.CollisionResponse = DefaultCharacter->GetCapsuleComponent()->BodyInstance.GetResponseToChannels();
		}
	}
}

void UEmpathCharacterMovementComponent::EmpathComputeFloorDist(FVector CapsuleLocation, float LineDistance, float SweepDistance, float SweepRadius, FFindFloorResult& FloorResult) const
{
	bInK2ComputeFloorDist = true;
	Super::K2_ComputeFloorDist(CapsuleLocation, LineDistance, SweepDistance, SweepRadius, FloorResult);
	bInK2ComputeFloorDist = false;
}

void UEmpathCharacterMovementComponent::PerformAirControlForPathFollowing(FVector Direction, float ZDiff)
{
	if (PathFollowingComp.IsValid())
	{
		// Do nothing if we are on a custom nav link
		if (PathFollowingComp->GetCurrentCustomLinkOb() != nullptr)
		{
			return;
		};
	}

	Super::PerformAirControlForPathFollowing(Direction, ZDiff);
}


void UEmpathCharacterMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	// We don't want to affect physics volumes
	if (UpdatedComponent)
	{
		UpdatedComponent->bShouldUpdatePhysicsVolume = false;
		UpdatedComponent->PhysicsVolumeChangedDelegate.RemoveDynamic(this, &UCharacterMovementComponent::PhysicsVolumeChanged);
	}
}

FVector UEmpathCharacterMovementComponent::GetPathingSourceLocation() const
{
	return GetActorFeetLocation();
}
