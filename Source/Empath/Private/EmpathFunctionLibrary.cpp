// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathFunctionLibrary.h"
#include "EmpathVRCharacter.h"
#include "EmpathGameModeBase.h"

const FVector UEmpathFunctionLibrary::GetAimLocationOnActor(AActor const* Actor)
{
	// Check if the object is a pawn
	APawn const* const TargetPawn = Cast<APawn>(Actor);
	if (TargetPawn)
	{
		// Check if the object is the vr player
		AEmpathVRCharacter const* const TargetVRChar = Cast<AEmpathVRCharacter>(TargetPawn);
		if (TargetVRChar)
		{
			// Aim at the defined center mass of the vr character
			return TargetVRChar->GetAimLocation();
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
const ETeamAttitude::Type UEmpathFunctionLibrary::GetTeamAttitudeTowards(AActor* A, AActor* B)
{
	// Ensure the input is valid
	if (!A || !B)
	{
		return ETeamAttitude::Neutral;
	}

	// Get the teams
	if (A->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()) &&
		B->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()))
	{
		EEmpathTeam TeamA = IEmpathTeamAgentInterface::Execute_GetTeamNum(A);
		EEmpathTeam TeamB = IEmpathTeamAgentInterface::Execute_GetTeamNum(B);

		// If either team is neutral, then we don't care about them
		if (TeamA == EEmpathTeam::Neutral || TeamB == EEmpathTeam::Neutral)
		{
			return ETeamAttitude::Neutral;
		}

		return (TeamA == TeamB) ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
	}
	return ETeamAttitude::Neutral;

}

