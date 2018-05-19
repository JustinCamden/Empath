// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathFunctionLibrary.h"
#include "EmpathVRCharacter.h"
#include "EmpathGameModeBase.h"
#include "EmpathAimLocationInterface.h"

// Static consts
static const float MaxImpulsePerMass = 5000.f;
static const float MaxPointImpulseMag = 120000.f;

const FVector UEmpathFunctionLibrary::GetAimLocationOnActor(const AActor* Actor, FVector LookDirection)
{
	// First, check for the aim location interface
	if (Actor->GetClass()->ImplementsInterface(UEmpathAimLocationInterface::StaticClass()))
	{
		return IEmpathAimLocationInterface::Execute_GetCustomAimLocationOnActor(Actor, LookDirection);
	}

	// Nect, check if the object as a controller. If so, call this function in its pawn instead.
	const AController* ControllerTarget = Cast<AController>(Actor);
	if (ControllerTarget && ControllerTarget->GetPawn())
	{
		return GetAimLocationOnActor(ControllerTarget->GetPawn(), LookDirection);
	}
	
	return GetCenterMassLocationOnActor(Actor);
}

const FVector UEmpathFunctionLibrary::GetCenterMassLocationOnActor(const AActor* Actor)
{
	// Check if the object is the vr player
	const AEmpathVRCharacter* VRChar = Cast<AEmpathVRCharacter>(Actor);
	if (VRChar)
	{
		// Aim at the defined center mass of the vr character
		return VRChar->GetVRLocation();
	}

	// Else, get the center of the bounding box
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

const float UEmpathFunctionLibrary::AngleBetweenVectors(FVector A, FVector B)
{
	return FMath::RadiansToDegrees(FMath::Acos(A.GetSafeNormal() | B.GetSafeNormal()));
}

void UEmpathFunctionLibrary::AngleAndAxisBetweenVectors(FVector A, FVector B, float &Angle, FVector &Axis)
{
	FVector ANormalized = A.GetSafeNormal();
	FVector BNormalized = B.GetSafeNormal();
	Angle = FMath::RadiansToDegrees(FMath::Acos(ANormalized | BNormalized));
	Axis = FVector::CrossProduct(ANormalized, BNormalized);
	return;
}

AEmpathAIManager* UEmpathFunctionLibrary::GetAIManager(UObject* WorldContextObject)
{
	AEmpathGameModeBase* EmpathGMD = WorldContextObject->GetWorld()->GetAuthGameMode<AEmpathGameModeBase>();
	if (EmpathGMD)
	{
		return EmpathGMD->GetAIManager();
	}
	return nullptr;
}

const bool UEmpathFunctionLibrary::IsPlayer(AActor* Actor)
{
	if (Actor)
	{
		AController* PlayerCon = Actor->GetWorld()->GetFirstPlayerController();
		if (PlayerCon)
		{
			if (PlayerCon == Actor)
			{
				return true;
			}
			if (PlayerCon->GetPawn() == Actor)
			{
				return true;
			}
		}
	}
	return false;
}

EEmpathTeam UEmpathFunctionLibrary::GetActorTeam(const AActor* Actor)
{
	if (Actor)
	{
		// First check for the team agent interface on the actor
		{
			if (Actor->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()))
			{
				return IEmpathTeamAgentInterface::Execute_GetTeamNum(Actor);
			}
		}

		// Next, check on the controller
		{
			APawn const* const TargetPawn = Cast<APawn>(Actor);
			if (TargetPawn)
			{
				AController const* const TargetController = Cast<AController>(TargetPawn);
				if (TargetController && TargetController->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()))
				{
					return IEmpathTeamAgentInterface::Execute_GetTeamNum(TargetController);
				}
			}
		}

		// Next, check the instigator
		{
			AActor const* TargetInstigator = Actor->Instigator;
			if (TargetInstigator && TargetInstigator->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()))
			{
				return IEmpathTeamAgentInterface::Execute_GetTeamNum(Actor->Instigator);
			}
		}

		// Finally, check if the instigator controller has a team
		{
			AController* const InstigatorController = Actor->GetInstigatorController();
			if (InstigatorController && InstigatorController->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()))
			{
				return IEmpathTeamAgentInterface::Execute_GetTeamNum(InstigatorController);
			}
		}
	}

	// By default, return neutral
	return EEmpathTeam::Neutral;
}

const ETeamAttitude::Type UEmpathFunctionLibrary::GetTeamAttitudeBetween(const AActor* A, const AActor* B)
{
	// Get the teams
	EEmpathTeam TeamA = GetActorTeam(A);
	EEmpathTeam TeamB = GetActorTeam(B);

	// If either of them are neutral, we don't care about them
	if (TeamA == EEmpathTeam::Neutral || TeamB == EEmpathTeam::Neutral)
	{
		return ETeamAttitude::Neutral;
	}

	// If they're the same, return frendly
	if (TeamA == TeamB)
	{
		return ETeamAttitude::Friendly;
	}

	// Otherwise, they're hostile
	return ETeamAttitude::Hostile;
}

void UEmpathFunctionLibrary::AddDistributedImpulseAtLocation(USkeletalMeshComponent* SkelMesh, FVector Impulse, FVector Location, FName BoneName, float DistributionPct)
{
	if (Impulse.IsNearlyZero() || (SkelMesh == nullptr))
	{
		return;
	}

	// 
	// overall strategy here: 
	// add some portion of the overall impulse distributed across all bodies in a radius, with linear falloff, but preserving impulse direction
	// the remaining portion is applied as a straight impulse to the given body at the given location
	// Desired outcome is similar output to a simple hard whack at the loc/bone, but more stable.
	//

	// compute total mass, per-bone distance info
	TArray<float> BodyDistances;
	BodyDistances.AddZeroed(SkelMesh->Bodies.Num());
	float MaxDistance = 0.f;

	float TotalMass = 0.0f;
	for (int32 i = 0; i < SkelMesh->Bodies.Num(); ++i)
	{
		FBodyInstance* const BI = SkelMesh->Bodies[i];
		if (BI->IsValidBodyInstance())
		{
			TotalMass += BI->GetBodyMass();

			FVector const BodyLoc = BI->GetUnrealWorldTransform().GetLocation();
			float Dist = (Location - BodyLoc).Size();
			BodyDistances[i] = Dist;
			MaxDistance = FMath::Max(MaxDistance, Dist);
		}
	}

	// sanity, since we divide with these
	TotalMass = FMath::Max(TotalMass, KINDA_SMALL_NUMBER);
	MaxDistance = FMath::Max(MaxDistance, KINDA_SMALL_NUMBER);

	float const OriginalImpulseMag = Impulse.Size();
	FVector const ImpulseDir = Impulse / OriginalImpulseMag;
	float const DistributedImpulseMag = OriginalImpulseMag * DistributionPct;
	float const PointImpulseMag = FMath::Min((OriginalImpulseMag - DistributedImpulseMag), MaxPointImpulseMag);

	const float DistributedImpulseMagPerMass = FMath::Min((DistributedImpulseMag / TotalMass), MaxImpulsePerMass);
	for (int32 i = 0; i < SkelMesh->Bodies.Num(); ++i)
	{
		FBodyInstance* const BI = SkelMesh->Bodies[i];

		if (BI->IsValidBodyInstance())
		{
			// linear falloff
			float const ImpulseFalloffScale = FMath::Max(0.f, 1.f - (BodyDistances[i] / MaxDistance));
			const float ImpulseMagForThisBody = DistributedImpulseMagPerMass * BI->GetBodyMass() * ImpulseFalloffScale;
			if (ImpulseMagForThisBody > 0.f)
			{
				BI->AddImpulse(ImpulseDir*ImpulseMagForThisBody, false);
			}
		}
	}

	// add the rest as a point impulse on the loc
	SkelMesh->AddImpulseAtLocation(ImpulseDir * PointImpulseMag, Location, BoneName);
}

