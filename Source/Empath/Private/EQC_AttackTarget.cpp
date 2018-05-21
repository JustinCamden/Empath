// Copyright 2018 Team Empath All Rights Reserved

#include "EQC_AttackTarget.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EmpathAIController.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "EnvironmentQuery/EQSTestingPawn.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"

void UEQC_AttackTarget::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	AActor* AttackTarget = nullptr;

	APawn* const QueryOwner = Cast<APawn>(QueryInstance.Owner.Get());
	if (QueryOwner)
	{
		AEmpathAIController const* const AI = Cast<AEmpathAIController>(QueryOwner->GetController());
		if (AI)
		{
			AttackTarget = AI->GetAttackTarget();
		}

#if WITH_EDITOR
		// for debugging/eqstest pawns in editor
		if (AttackTarget == nullptr)
		{
			// look for a second testing pawn
			for (TActorIterator<AEQSTestingPawn> It(QueryOwner->GetWorld()); It; ++It)
			{
				AEQSTestingPawn* const P = *It;
				if (!P->IsPendingKill() && (P != QueryOwner))
				{
					AttackTarget = P;
					break;
				}
			}
		}
#endif
	}

	UEnvQueryItemType_Actor::SetContextHelper(ContextData, AttackTarget);
}