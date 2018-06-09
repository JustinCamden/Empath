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
 	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = true;

	// Initial variables
	FollowComponentLostThreshold = 10.0f;

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
	// Call the parent function
	Super::Tick(DeltaTime);

	// Update our position to follow the component
	MoveToFollowComponent();

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

void AEmpathHandActor::ActivateCasting_Implementation()
{
	KinematicVelocityComponent->Activate(false);
}

void AEmpathHandActor::DectivateCasting_Implementation()
{
	KinematicVelocityComponent->Deactivate();
}

void AEmpathHandActor::MoveToFollowComponent()
{
	if (FollowedComponent)
	{
		// Check the distance from the follow component
		SetActorTransform(FollowedComponent->GetComponentTransform(), true);
		float CurrentFollowDistance = (FollowedComponent->GetComponentTransform().GetLocation() - GetTransform().GetLocation()).Size();
		if (CurrentFollowDistance >= FollowComponentLostThreshold)
		{
			UpdateLostFollowComponent(true);
		}
		else
		{
			UpdateLostFollowComponent(false);
		}
	}
	else
	{
		// If we don't have a follow component, we've lost it
		UpdateLostFollowComponent(true);
	}
}

void AEmpathHandActor::UpdateLostFollowComponent(bool bLost)
{
	// Only process this function is there a change in result
	if (bLost != bLostFollowComponent)
	{
		// Update variable before firing notifies
		if (bLost)
		{
			bLostFollowComponent = true;
			OnLostFollowComponent();
		}
		else
		{
			bLostFollowComponent = false;
			OnFoundFollowComponent();
		}
	}
}

void AEmpathHandActor::OnLostFollowComponent_Implementation()
{
	DectivateCasting();
}

void AEmpathHandActor::OnFoundFollowComponent_Implementation()
{
	ActivateCasting();
}

FVector AEmpathHandActor::GetTeleportOrigin_Implementation() const
{
	return GetActorLocation();
}

FVector AEmpathHandActor::GetTeleportDirection_Implementation(FVector LocalDirection) const
{
	return GetTransform().TransformVectorNoScale(LocalDirection.GetSafeNormal());
}