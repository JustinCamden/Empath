// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathUtility.h"
#include "EmpathVRCharacter.h"
#include "EmpathGameModeBase.h"

const FVector UEmpathUtility::GetAimLocationOnActor(AActor const* Actor)
{
	// Check if the object is a pawn
	APawn const* const TargetPawn = Cast<APawn>(Actor);
	if (TargetPawn)
	{
		// Check if the object is the vr player
		AEmpathVRCharacter const* const TargetPlayer = Cast<AEmpathVRCharacter>(TargetPawn);
		if (TargetPlayer)
		{
			// Aim at the defined center mass of the vr character
			return TargetPlayer->GetAimLocation();
		}
	}

	if (Actor)
	{
		// Aim at bounds center
		FBox BoundingBox = Actor->GetComponentsBoundingBox();

		// Add Child Actor components to bounding box (since we use them)
		// Workaround for GetActorBounds not considering child actor components
		for (const UActorComponent* ActorComponent : Actor->GetComponents())
		{
			UChildActorComponent const* const CAComp = Cast<const UChildActorComponent>(ActorComponent);
			AActor const* const CA = CAComp ? CAComp->GetChildActor() : nullptr;
			if (CA)
			{
				BoundingBox += CA->GetComponentsBoundingBox();
			}
		}

		if (BoundingBox.IsValid)
		{
			FVector BoundsOrigin = BoundingBox.GetCenter();
			return BoundsOrigin;
		}
	}

	// If all else fails, aim at the target's location
	return Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
}

const float UEmpathUtility::AngleBetweenVectors(FVector A, FVector B)
{
	return FMath::RadiansToDegrees(FMath::Acos(A.GetSafeNormal() | B.GetSafeNormal()));
}