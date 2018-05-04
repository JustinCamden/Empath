// Fill out your copyright notice in the Description page of Project Settings.

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


