// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathHandActor.h"
#include "Components/SphereComponent.h"
#include "EmpathTypes.h"
#include "EmpathKinematicVelocityComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EmpathGripObjectInterface.h"
#include "EmpathPlayerCharacter.h"

FName AEmpathHandActor::BlockingCollisionName(TEXT("BlockingCollision"));
FName AEmpathHandActor::KinematicVelocityComponentName(TEXT("KinematicVelocityComponent"));
FName AEmpathHandActor::MeshComponentName(TEXT("MeshComponent"));
FName AEmpathHandActor::GripCollisionName(TEXT("GripCollision"));
FName AEmpathHandActor::MotionControllerRootName(TEXT("MotionControllerRoot"));

// Sets default values
AEmpathHandActor::AEmpathHandActor(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = true;

	// Initial variables
	FollowComponentLostThreshold = 10.0f;
	ControllerOffsetLocation = FVector(8.5f, 1.0f, -2.5f);
	ControllerOffsetRotation = FRotator(-20.0f, -100.0f, -90.0f);

	// Initialize components
	// Root component
	MotionControllerRoot = CreateDefaultSubobject<USceneComponent>(MotionControllerRootName);
	RootComponent = MotionControllerRoot;

	// Mesh component
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(MeshComponentName);
	MeshComponent->SetEnableGravity(false);
	MeshComponent->SetCollisionProfileName(FEmpathCollisionProfiles::NoCollision);
	MeshComponent->SetupAttachment(RootComponent);

	// Blocking collision
	BlockingCollision = CreateDefaultSubobject<USphereComponent>(BlockingCollisionName);
	BlockingCollision->InitSphereRadius(7.75f);
	BlockingCollision->SetCollisionProfileName(FEmpathCollisionProfiles::HandCollision);
	BlockingCollision->SetEnableGravity(false);
	BlockingCollision->SetupAttachment(MeshComponent);

	// Grip collision
	GripCollision = CreateDefaultSubobject<USphereComponent>(GripCollisionName);
	GripCollision->InitSphereRadius(10.0f);
	GripCollision->SetCollisionProfileName(FEmpathCollisionProfiles::OverlapAllDynamic);
	GripCollision->SetEnableGravity(false);
	GripCollision->SetupAttachment(MeshComponent);

	// Kinematic velocity
	KinematicVelocityComponent = CreateDefaultSubobject<UEmpathKinematicVelocityComponent>(KinematicVelocityComponentName);
	KinematicVelocityComponent->SetupAttachment(MeshComponent);
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
	AEmpathPlayerCharacter* InOwningPlayerCharacter,
	USceneComponent* InFollowedComponent,
	EEmpathBinaryHand InOwningHand)
{
	// Cache inputted components
	OtherHand = InOtherHand;
	OwningPlayerCharacter = InOwningPlayerCharacter;
	FollowedComponent = InFollowedComponent;
	OwningHand = InOwningHand;

	// Invite the actor's left / right scale if it's the left hand
	if (OwningHand == EEmpathBinaryHand::Left)
	{
		FVector NewScale = GetActorScale3D();
		NewScale.Y *= -1.0f;
		SetActorScale3D(NewScale);
	}

	// We apply an initial controller offset to the mesh 
	// to compensate for the offset of the tracking origin and the actual center
	MeshComponent->SetRelativeLocationAndRotation(ControllerOffsetLocation, ControllerOffsetRotation);


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

void AEmpathHandActor::GetBestGripCandidate(AActor*& GripActor, UPrimitiveComponent*& GripComponent, EEmpathGripType& GripResponse)
{
	// Get all overlapping components
	TArray<UPrimitiveComponent*> OverlappingComponents;
	GripCollision->GetOverlappingComponents(OverlappingComponents);

	float BestDistance = 99999.0f;
	bool bFoundActor = false;

	// Check each overlapping component
	for (UPrimitiveComponent* CurrComponent : OverlappingComponents)
	{
		AActor* CurrActor = CurrComponent->GetOwner();

		// If this is a new actor, we need to check if it implements the interface, and if so, what the response is
		if (CurrActor != GripActor)
		{
			if (CurrActor->GetClass()->ImplementsInterface(UEmpathGripObjectInterface::StaticClass()))
			{
				EEmpathGripType CurrGripResponse = IEmpathGripObjectInterface::Execute_GetGripResponse(CurrActor, this, CurrComponent);
				if (CurrGripResponse != EEmpathGripType::NoGrip)
				{
					// Check the distance to this component is smaller than the current best. If so, update the current best
					float CurrDist = (CurrComponent->GetComponentLocation() - GripCollision->GetComponentLocation()).Size();
					if (CurrDist < BestDistance)
					{
						GripActor = CurrActor;
						GripComponent = CurrComponent;
						BestDistance = CurrDist;
						GripResponse = CurrGripResponse;
					}
				}
			}
		}

		// If this is our current best actor, then we can skip checking if it implements the interface
		else
		{
			EEmpathGripType CurrGripResponse = IEmpathGripObjectInterface::Execute_GetGripResponse(CurrActor, this, CurrComponent);
			if (CurrGripResponse != EEmpathGripType::NoGrip)
			{
				float CurrDist = (CurrComponent->GetComponentLocation() - GripCollision->GetComponentLocation()).Size();
				if (CurrDist < BestDistance)
				{
					GripComponent = CurrComponent;
					BestDistance = CurrDist;
					GripResponse = CurrGripResponse;
				}
			}
		}
	}
	return;

}

void AEmpathHandActor::OnGripPressed()
{
	// Only attempt to grip if we are not already gripping an object
	if (GripState == EEmpathGripType::NoGrip)
	{
		//Try and get a grip candidate
		AActor* GripCandidate;
		UPrimitiveComponent* GripComponent;
		EEmpathGripType GripResponse;
		GetBestGripCandidate(GripCandidate, GripComponent, GripResponse);

		if (GripCandidate)
		{
			// Branch functionality depending on the type of grip
			switch (GripResponse)
			{
			case EEmpathGripType::Climb:
			{
				// If we can climb then register the new climb point with the player character and update the grip state
				if (OwningPlayerCharacter->CanClimb())
				{
					FVector GripOffset = GripComponent->GetComponentTransform().InverseTransformPosition(GetActorLocation());
					OwningPlayerCharacter->SetClimbingGrip(this, GripComponent, GripOffset);
					GripState = EEmpathGripType::Climb;
				}
				break;
			}
			default:
			{
				break;
			}
			}
		}

	}
	ReceiveGripPressed();
}

void AEmpathHandActor::OnGripReleased()
{
	switch (GripState)
	{
	case EEmpathGripType::Climb:
	{
		// If this was our dominant climbing hand, but the other hand is still climbing, check and see if the
		// other hand can find a valid grip object
		if (OwningPlayerCharacter && OwningPlayerCharacter->ClimbHand == this && !OtherHand->CheckForClimbGrip())
		{
			// If not, then clear the player grip state
			OwningPlayerCharacter->ClearClimbingGrip();
		}
		break;
	}
	default:
	{
		break;
	}
	}
	// Update grip state
	GripState = EEmpathGripType::NoGrip;
	ReceiveGripReleased();
}

bool AEmpathHandActor::CheckForClimbGrip()
{
	// If we're climbing
	if (GripState == EEmpathGripType::Climb)
	{
		// Try and get a grip candidate
		AActor* GripCandidate;
		UPrimitiveComponent* GripComponent;
		EEmpathGripType GripResponse;
		GetBestGripCandidate(GripCandidate, GripComponent, GripResponse);

		// If we find a climbing grip, then update the climbing grip point on the owning player character
		if (GripResponse == EEmpathGripType::Climb)
		{
			FVector GripOffset = GripComponent->GetComponentTransform().InverseTransformPosition(GetActorLocation());
			OwningPlayerCharacter->SetClimbingGrip(this, GripComponent, GripOffset);
			return true;
		}
	}
	return false;
}