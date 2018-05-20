// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathVRCharacter.h"

AEmpathVRCharacter::AEmpathVRCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDead = false;
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
	ReceiveTeleport(Origin, Destination, DeltaYaw);
	OnTeleport.Broadcast(this, Origin, Destination);

	// TODO: Implement our actual teleportation in C++. 
	// For now, we can use the blueprint implementation of "OnTeleport"
	return;
}

void AEmpathVRCharacter::Die()
{
	// Fire delegates and notifies
	ReceiveDeath();
	OnDeath.Broadcast();

	// TODO: Implement actual death
	bDead = true;
}

FVector AEmpathVRCharacter::GetCustomAimLocationOnActor_Implementation(FVector LookOrigin, FVector LookDirection) const
{
	return GetVRLocation();
}
