// Copyright 2018 Team Empath All Rights Reserved

#include "EQC_DefendTarget.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EmpathAIController.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "EnvironmentQuery/EQSTestingPawn.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"

void UEQC_DefendTarget::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	AActor* DefendTarget = nullptr;

	APawn* const QueryOwner = Cast<APawn>(QueryInstance.Owner.Get());
	if (QueryOwner)
	{
		AEmpathAIController const* const AI = Cast<AEmpathAIController>(QueryOwner->GetController());
		if (AI)
		{
			DefendTarget = AI->GetDefendTarget();
		}

#if WITH_EDITOR
		// for debugging/eqstest pawns in editor
		if (DefendTarget == nullptr)
		{
			// look for a second testing pawn
			for (TActorIterator<AEQSTestingPawn> It(QueryOwner->GetWorld()); It; ++It)
			{
				AEQSTestingPawn* const P = *It;
				if (!P->IsPendingKill() && (P != QueryOwner))
				{
					DefendTarget = P;
					break;
				}
			}
		}
#endif
	}

	UEnvQueryItemType_Actor::SetContextHelper(ContextData, DefendTarget);
}

