// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathCharacter.h"
#include "EmpathVRCharacter.h"


// Sets default values
AEmpathCharacter::AEmpathCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	MaxEffectiveDistance = 250.0f;
}

// Called when the game starts or when spawned
void AEmpathCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AEmpathCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

float AEmpathCharacter::GetDistanceToVR(const AActor* OtherActor) const
{
	// Check if the other actor is a VR character
	const AEmpathVRCharacter* OtherVRChar = Cast<AEmpathVRCharacter>(OtherActor);
	if (OtherVRChar)
	{
		return (GetActorLocation() - OtherVRChar->GetVRLocation()).Size();
	}

	// Otherwise, use default behavior
	else 
	{
		return GetDistanceTo(OtherActor);
	}
}

