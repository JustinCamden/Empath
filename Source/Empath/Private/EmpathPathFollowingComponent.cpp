// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathPathFollowingComponent.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "AI/Navigation/NavLinkCustomInterface.h"
#include "AI/Navigation/NavLinkCustomComponent.h"
#include "EmpathNavLinkProxy_Climb.h"
#include "EmpathNavLinkProxy_Jump.h"
#include "EmpathAIController.h"
#include "Components/ArrowComponent.h"

UEmpathPathFollowingComponent::UEmpathPathFollowingComponent()
{
	VelocityCompensationAccelAdjustmentScale = 0.0f;
}

void UEmpathPathFollowingComponent::BeginPlay()
{
	Super::BeginPlay();

	PostProcessMove.BindUObject(this, &UEmpathPathFollowingComponent::AdjustMove);
}

void UEmpathPathFollowingComponent::AdjustMove(UPathFollowingComponent* PathFollowComp, FVector& Velocity)
{
	if (bPausePathfollowingVelocity)
	{
		Velocity = FVector::ZeroVector;
	}

	// Default accel-based pathfollowing just accelerates directly toward the target, regardless of current velocity.
	// This can result in some overshoot and orbit-like paths, particularly when the pawn is moving quickly.
	// This adjustment applies an additional acceleration term in the velocity domain, designed to turn the pawn's
	// current velocity to be toward the target. 
	else if (MovementComp 
		&& MovementComp->UseAccelerationForPathFollowing()
		&& VelocityCompensationAccelAdjustmentScale != 0.0f
		&& !MovementComp->Velocity.IsNearlyZero()
		&& GetWorld())
	{
		const FVector CurrentLocation = MovementComp->GetActorFeetLocation();
		const FVector CurrentTargetLocation = GetCurrentTargetLocation();

		float const InputScale = Velocity.Size();

		FVector const DirToTarget = (CurrentTargetLocation - CurrentLocation).GetSafeNormal2D();
		FVector const DesiredVelocity = DirToTarget * MovementComp->Velocity.Size();
		FVector const DesiredDeltaVelocity = DesiredVelocity - MovementComp->Velocity;

		float const DeltaTime = GetWorld()->GetDeltaSeconds();
		FVector const Acceleration = DesiredDeltaVelocity / DeltaTime;

		Velocity += Acceleration.GetSafeNormal2D() * InputScale * VelocityCompensationAccelAdjustmentScale;
	}
}

void UEmpathPathFollowingComponent::AdvanceToNextMoveSegment()
{
	SetNextMoveSegment();
}

void UEmpathPathFollowingComponent::StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint)
{
	Super::StartUsingCustomLink(CustomNavLink, DestPoint);

	UObject* const NavLinkComp = CustomNavLink->GetLinkOwner();
	if (NavLinkComp)
	{
		// If Climbing nav link
		if (AEmpathNavLinkProxy_Climb* const ClimbLink = Cast<AEmpathNavLinkProxy_Climb>(NavLinkComp->GetOuter()))
		{
			// Cache the transform of the ledge
			FTransform const LedgeAnchorTransform = ClimbLink->LedgeAlignmentComponent->GetComponentTransform();


			// The edge alignment component faces away from ledge, 
			// but character should face towards it, so we need to flip it around.
			// This will also zero out any pitch to keep the character upright.
			FVector const NewForward = -(LedgeAnchorTransform.GetRotation().GetForwardVector());
			FTransform DestinationTransform = LedgeAnchorTransform;
			DestinationTransform.SetRotation(NewForward.ToOrientationQuat());

			// Signal our AI controller to handle climbing to the destination
			AEmpathAIController* const AI = Cast<AEmpathAIController>(GetOwner());
			if (AI)
			{
				AI->ClimbTo(DestinationTransform);
				AI->PausePathFollowing();
			}
		}

		// If jump nav link
		else if (AEmpathNavLinkProxy_Jump* JumpLink = Cast<AEmpathNavLinkProxy_Jump>(NavLinkComp->GetOuter()))
		{
			if (AEmpathAIController* const AI = Cast<AEmpathAIController>(GetOwner()))
			{
				if (APawn* const AIPawn = AI->GetPawn())
				{
					FQuat const ActorRotation = AIPawn->GetActorQuat();
					AI->JumpTo(FTransform(ActorRotation, DestPoint), JumpLink->JumpArc, JumpLink);
				}
			}
			
		}
	}
}


void UEmpathPathFollowingComponent::FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink)
{
	Super::FinishUsingCustomLink(CustomNavLink);

	// Release the claim on this link
	if (CustomNavLink)
	{
		AEmpathAIController* const AI = Cast<AEmpathAIController>(GetOwner());
		UObject* const NavLinkComp = CustomNavLink->GetLinkOwner();
		if (AI && NavLinkComp)
		{
			AEmpathNavLinkProxy* const NavLink = Cast<AEmpathNavLinkProxy>(NavLinkComp->GetOuter());
			if (NavLink)
			{
				AI->ReleaseNavLinkClaim(NavLink);
			}
		}
	}
}

bool UEmpathPathFollowingComponent::HasReachedCurrentTarget(const FVector& CurrentLocation) const
{
	bool bReached = Super::HasReachedCurrentTarget(CurrentLocation);

	if (bReached == false)
	{
		// Extra acceptance check here.
		// The base class says we've reached if we go past the destination point, using a 3D dot check,
		// but in cases where we are landing from a jump, that leaves a gap where we can land and still not
		// be "past" the point. Thus, we do another similar check, but discounting the Z axis.

		const FVector CurrentTarget = GetCurrentTargetLocation();
		FVector CurrentDirectionXY = GetCurrentDirection();
		CurrentDirectionXY.Z = 0.f;

		// Check if moved too far
		FVector ToTargetXY = (CurrentTarget - MovementComp->GetActorFeetLocation());
		ToTargetXY.Z = 0.f;
		const float SegmentDot = FVector::DotProduct(ToTargetXY, CurrentDirectionXY);

		// If we passed the destination then we reached it
		if (SegmentDot < 0.0)
		{
			bReached = true;
		}
	}

	return bReached;
}

FVector UEmpathPathFollowingComponent::GetLocationOnPath() const
{
	return MovementComp ? MovementComp->GetActorFeetLocation() : FVector::ZeroVector;
}

void UEmpathPathFollowingComponent::OnDecelerationPossiblyChanged()
{
	// Recalculate deceleration data
	CachedBrakingMaxSpeed = 0.f;
	UpdateDecelerationData();
}


void UEmpathPathFollowingComponent::FollowPathSegment(float DeltaTime)
{
	bool bWasDecelerating = bIsDecelerating;
	Super::FollowPathSegment(DeltaTime);

	if (bWasDecelerating)
	{
		bIsDecelerating = true;
	}
}

void UEmpathPathFollowingComponent::Reset()
{
	Super::Reset();
	bIsDecelerating = false;
}
