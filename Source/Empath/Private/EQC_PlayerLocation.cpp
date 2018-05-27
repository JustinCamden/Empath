// Copyright 2018 Team Empath All Rights Reserved

#include "EQC_PlayerLocation.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EmpathAIController.h"
#include "EmpathPlayerCharacter.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "EnvironmentQuery/EQSTestingPawn.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"

void UEQC_PlayerLocation::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	FVector TargetLocation(0.f);
	APawn* const QueryOwner = Cast<APawn>(QueryInstance.Owner.Get());
	if (QueryOwner)
	{
		bool bFoundTarget = false;
		TargetLocation = QueryOwner->GetActorLocation(); // Fallback to self location if no target location.
		AController* PlayerCon = GetWorld()->GetFirstPlayerController();
		if (PlayerCon)
		{
			APawn* PlayerPawn = PlayerCon->GetPawn();
			if (PlayerPawn)
			{
				AEmpathPlayerCharacter* VRChar = Cast<AEmpathPlayerCharacter>(PlayerPawn);
				if (VRChar)
				{
					TargetLocation = VRChar->GetVRLocation();
				}
				else
				{
					TargetLocation = PlayerPawn->GetActorLocation();
				}
				bFoundTarget = true;
			}
		}

#if WITH_EDITOR
		// for debugging/eqstest pawns in editor
		if (bFoundTarget == false)
		{
			// look for a second testing pawn
			for (TActorIterator<AEQSTestingPawn> It(QueryOwner->GetWorld()); It; ++It)
			{
				AEQSTestingPawn* const P = *It;
				if (!P->IsPendingKill() && (P != QueryOwner))
				{
					TargetLocation = P->GetActorLocation();
					break;
				}
			}
		}
#endif
	}

	UEnvQueryItemType_Point::SetContextHelper(ContextData, TargetLocation);
}