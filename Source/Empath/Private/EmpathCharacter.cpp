// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathCharacter.h"
#include "EmpathVRCharacter.h"
#include "EmpathAIController.h"


// Sets default values
AEmpathCharacter::AEmpathCharacter(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	MaxEffectiveDistance = 250.0f;
	MinEffectiveDistance = 0.0f;
	DefaultTeam = EEmpathTeam::Enemy;
}

// Called when the game starts or when spawned
void AEmpathCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Grab the Empath AI controller
	EmpathAICon = Cast<AEmpathAIController>(GetController());
	if (!EmpathAICon)
	{
		UE_LOG(LogTemp, Warning, TEXT("ERROR: Not possessed by an EmpathAIController!"));
	}
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

void AEmpathCharacter::Die()
{
	OnDeath.Broadcast();
	ReceiveDie();
}

void AEmpathCharacter::ReceiveDie_Implementation()
{
	SetLifeSpan(0.001f);
}

EEmpathTeam AEmpathCharacter::GetTeamNum_Implementation() const
{
	// Defer to the controller
	AController* const Controller = GetController();
	IEmpathTeamAgentInterface* const ControllerTeamAgent = Cast<IEmpathTeamAgentInterface>(Controller);

	if (ControllerTeamAgent)
	{
		return IEmpathTeamAgentInterface::Execute_GetTeamNum(Controller);
	}

	return DefaultTeam;
}