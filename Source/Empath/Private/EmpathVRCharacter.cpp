// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathVRCharacter.h"

AEmpathVRCharacter::AEmpathVRCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

FVector AEmpathVRCharacter::GetAimLocation()
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


