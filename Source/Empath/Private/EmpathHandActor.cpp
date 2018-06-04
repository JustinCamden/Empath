// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathHandActor.h"
#include "Components/SphereComponent.h"
#include "EmpathTypes.h"
#include "EmpathKinematicVelocityComponent.h"
#include "Components/SkeletalMeshComponent.h"

FName AEmpathHandActor::SphereComponentName(TEXT("SphereComponent0"));
FName AEmpathHandActor::KinematicVelocityComponentName(TEXT("KinematicVelocityComponent"));
FName AEmpathHandActor::MeshComponentName(TEXT("MeshComponent"));

// Sets default values
AEmpathHandActor::AEmpathHandActor(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Initialize components
	// Root component
	SphereComponent = CreateDefaultSubobject<USphereComponent>(SphereComponentName);
	SphereComponent->InitSphereRadius(7.75f);
	SphereComponent->SetCollisionProfileName(FEmpathCollisionProfiles::HandCollision);
	RootComponent = SphereComponent;

	// Kinematic velocity
	KinematicVelocityComponent = CreateDefaultSubobject<UEmpathKinematicVelocityComponent>(KinematicVelocityComponentName);
	KinematicVelocityComponent->SetupAttachment(RootComponent);
	// By default, we set the position to be the wrist of a right hand.
	// To change to a left hand, simply invert the X
	KinematicVelocityComponent->SetRelativeLocation(FVector(-6.5f, 1.0f, -2.0f));

	// Mesh component
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(MeshComponentName);
	MeshComponent->SetupAttachment(RootComponent);
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

void AEmpathHandActor::OnHandRegistered_Implementation()
{
	ActivateCasting();
}

void AEmpathHandActor::ActivateCasting_Implementation()
{
	KinematicVelocityComponent->Activate(false);
}

void AEmpathHandActor::DectivateCasting_Implementation()
{
	KinematicVelocityComponent->Deactivate();
}

