// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathHandActor.h"
#include "Components/SphereComponent.h"
#include "EmpathTypes.h"
#include "EmpathKinematicVelocityComponent.h"

FName AEmpathHandActor::SphereComponentName(TEXT("SphereComponent0"));
FName AEmpathHandActor::KinematicVelocityComponentName(TEXT("KinematicVelocityComponent"));

// Sets default values
AEmpathHandActor::AEmpathHandActor(const FObjectInitializer& ObjectInitializer)
{
	// Enable tick
	PrimaryActorTick.bCanEverTick = true;

	// Initialize sphere collision component
	SphereComponent = CreateDefaultSubobject<USphereComponent>(SphereComponentName);
	SphereComponent->InitSphereRadius(7.75f);
	SphereComponent->SetCollisionProfileName(FEmpathCollisionProfiles::HandCollision);
	RootComponent = SphereComponent;

	// Initialize kinematic velocity component
	KinematicVelocityComponent = CreateDefaultSubobject<UEmpathKinematicVelocityComponent>(KinematicVelocityComponentName);
	KinematicVelocityComponent->SetupAttachment(RootComponent);

	// By default, we assume the right hand for positioning.
	// To change it to the left hand, invert the Y
	KinematicVelocityComponent->SetRelativeLocation(FVector(-6.5f, 1.0f, -2.0f));

}

// Called when the game starts or when spawned
void AEmpathHandActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AEmpathHandActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (FollowedComponent)
	{
		SetActorTransform(FollowedComponent->GetComponentTransform(), true);
	}
}

void AEmpathHandActor::RegisterHand(AEmpathHandActor* InOtherHand, 
	AEmpathPlayerCharacter* InOwningCharacter,
	USceneComponent* InFollowedComponent)
{
	OtherHand = InOtherHand;
	OwningCharacter = InOwningCharacter;
	FollowedComponent = InFollowedComponent;
	OnHandRegistered();
}

