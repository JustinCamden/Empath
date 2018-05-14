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

bool AEmpathVRCharacter::IsTeleporting_Implementation() const
{
	// TODO: Hook this up to teleport state
	return false;
}


