// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathHandActor.h"
#include "Components/SphereComponent.h"
#include "EmpathTypes.h"

FName AEmpathHandActor::SphereComponentName(TEXT("SphereComponent0"));

// Sets default values
AEmpathHandActor::AEmpathHandActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SphereComponent = CreateDefaultSubobject<USphereComponent>(SphereComponentName);
	SphereComponent->InitSphereRadius(7.75f);
	SphereComponent->SetCollisionProfileName(FEmpathCollisionProfiles::HandCollision);
	RootComponent = SphereComponent;

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

