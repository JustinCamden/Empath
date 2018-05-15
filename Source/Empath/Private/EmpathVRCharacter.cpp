// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathVRCharacter.h"

AEmpathVRCharacter::AEmpathVRCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDead = false;
}

FVector AEmpathVRCharacter::GetAimLocation() const
{
	if (AimLocationComponent)
	{
		return AimLocationComponent->GetComponentLocation();
	}
	else
	{
		return OffsetComponentToWorld.GetLocation();
	}
}

float AEmpathVRCharacter::GetDistanceToVR(AActor* OtherActor) const
{
	return (GetVRLocation() - OtherActor->GetActorLocation()).Size();
}

EEmpathTeam AEmpathVRCharacter::GetTeamNum_Implementation() const
{
	// We should always return player for VR characters
	return EEmpathTeam::Player;
}

bool AEmpathVRCharacter::IsTeleporting_Implementation() const
{
	// TODO: Hook this up with our teleportation script
	return false;
}

void AEmpathVRCharacter::TeleportToVR(FVector Destination, float DeltaYaw)
{
	// Cache variables
	FVector Origin = GetVRLocation();

	// Fire notifies
	OnTeleport.Broadcast(this, Origin, Destination);
	ReceiveTeleport(Origin, Destination, DeltaYaw);

	// TODO: Implement our actual teleportation in C++. 
	// For now, we can use the blueprint implementation of "OnTeleport"
	return;
}

void AEmpathVRCharacter::Die()
{
	// Fire delegates and notifies
	OnDeath.Broadcast();
	ReceiveDeath();

	// TODO: Implement actual death
	bDead = true;
}
