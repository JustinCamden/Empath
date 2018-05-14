// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathAIManager.h"
#include "EmpathAIController.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "EmpathVRCharacter.h"
#include "EmpathTypes.h"

// Allows us to get IsValid from SecondaryAttackTarget structs
bool FSecondaryAttackTarget::IsValid() const
{
	return (TargetActor != nullptr) && !TargetActor->IsPendingKill();
}


// Sets default values
AEmpathAIManager::AEmpathAIManager()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	// Initialize default settings
	PlayerAwarenessState = EPlayerAwarenessState::PresenceNotKnown;
	bPlayerHasEverBeenSeen = false;
	bIsPlayerLocationKnown = false;
	LostPlayerTimeThreshold = 0.5f;
	StartSearchingTimeThreshold = 3.0f;
}

void AEmpathAIManager::OnPlayerDied(AEmpathVRCharacter* PlayerWhoDied)
{
	PlayerAwarenessState = EPlayerAwarenessState::PresenceNotKnown;
	bIsPlayerLocationKnown = false;
}

// Called when the game starts or when spawned
void AEmpathAIManager::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AEmpathAIManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AEmpathAIManager::CleanUpSecondaryTargets()
{
	for (int32 Idx = 0; Idx < SecondaryAttackTargets.Num(); ++Idx)
	{
		FSecondaryAttackTarget& AITarget = SecondaryAttackTargets[Idx];
		if (AITarget.IsValid() == false)
		{
			SecondaryAttackTargets.RemoveAtSwap(Idx, 1, true);
			--Idx;
		}
	}
}

void AEmpathAIManager::AddSecondaryTarget(AActor* Target, float TargetRatio, float TargetPreference, float TargetRadius)
{
	CleanUpSecondaryTargets();

	if (Target)
	{
		FSecondaryAttackTarget NewAttackTarget;
		NewAttackTarget.TargetActor = Target;
		NewAttackTarget.TargetPreference= TargetPreference;
		NewAttackTarget.TargetingRatio = TargetRatio;
		NewAttackTarget.TargetRadius = TargetRadius;

		SecondaryAttackTargets.Add(NewAttackTarget);
	}
}

void AEmpathAIManager::RemoveSecondaryTarget(AActor* Target)
{
	for (int32 Idx = 0; Idx < SecondaryAttackTargets.Num(); ++Idx)
	{
		FSecondaryAttackTarget& AITarget = SecondaryAttackTargets[Idx];
		if (AITarget.TargetActor == Target)
		{
			SecondaryAttackTargets.RemoveAtSwap(Idx, 1, true);
			--Idx;
		}
	}
}

void AEmpathAIManager::GetNumAITargeting(AActor const* Target, int32& NumAITargetingCandiate, int32& NumTotalAI) const
{
	NumAITargetingCandiate = 0;
	NumTotalAI = 0;

	for (AEmpathAIController* AI : TActorRange<AEmpathAIController>(GetWorld()))
	{
		++NumTotalAI;

		if (AI->GetAttackTarget() == Target)
		{
			++NumAITargetingCandiate;
		}
	}
}

float AEmpathAIManager::GetAttackTargetRadius(AActor* AttackTarget) const
{
	// Check if it is a secondary attack target
	for (FSecondaryAttackTarget const& CurrTarget : SecondaryAttackTargets)
	{
		if (CurrTarget.TargetActor == AttackTarget)
		{
			return CurrTarget.TargetRadius;
		}
	}

	return 0.0f;
}

void AEmpathAIManager::SetIsTargetLocationKnown(AActor const* Target)
{
	// If the target is a player
	if (Cast<AEmpathVRCharacter>(Target) != nullptr)
	{
		// Update variables
		bIsPlayerLocationKnown = true;
		LastKnownPlayerLocation = Target->GetActorLocation();

		// Stop the "lost player" state flow
		PlayerAwarenessState = EPlayerAwarenessState::KnownLocation;
		GetWorldTimerManager().ClearTimer(LostPlayerTimerHandle);


		// If this is the first time the player has been seen, notify all active
		// AIs that the player is in the area
		if (bPlayerHasEverBeenSeen == false)
		{
			for (AEmpathAIController* AI : TActorRange<AEmpathAIController>(GetWorld()))
			{
				if (AI->IsPassive() == false)
				{
					AI->ReceiveTargetSeenForFirstTime();
				}
			}

			bPlayerHasEverBeenSeen = true;
		}
	}

	// Secondart targets are always known, so just ignore any other case
}

bool AEmpathAIManager::IsTargetLocationKnown(AActor const* Target) const
{
	if (Target)
	{
		// If this is the player, return whether the player's location is known.
		if (Cast<AEmpathVRCharacter>(Target) != nullptr)
		{
			return bIsPlayerLocationKnown;
		}

		// Else, return true for secondary targets
		for (FSecondaryAttackTarget const& CurrentTarget : SecondaryAttackTargets)
		{
			if (CurrentTarget.IsValid() && (CurrentTarget.TargetActor == Target))
			{
				// secondary targets are always known
				return true;
			}
		}
	}

	return false;
}

bool AEmpathAIManager::IsPlayerPotentiallyLost() const
{
	return PlayerAwarenessState == EPlayerAwarenessState::PotentiallyLost;
}

void AEmpathAIManager::OnPlayerTeleported(FTransform const& FromTransform)
{
	if (PlayerAwarenessState == EPlayerAwarenessState::KnownLocation)
	{
		PlayerAwarenessState = EPlayerAwarenessState::PotentiallyLost;

		// If this timer expires and no AI has found the player, he is officially "lost"
		GetWorldTimerManager().SetTimer(LostPlayerTimerHandle,
			FTimerDelegate::CreateUObject(this, &AEmpathAIManager::OnLostPlayerTimerExpired),
			LostPlayerTimeThreshold, false);
	}
}

void AEmpathAIManager::OnLostPlayerTimerExpired()
{
	switch (PlayerAwarenessState)
	{
	case EPlayerAwarenessState::KnownLocation:
		// Shouldn't happen, do nothing.
		break;

	case EPlayerAwarenessState::PotentiallyLost:
		// Move on to lost state
		PlayerAwarenessState = EPlayerAwarenessState::Lost;
		GetWorldTimerManager().SetTimer(LostPlayerTimerHandle,
			FTimerDelegate::CreateUObject(this, &AEmpathAIManager::OnLostPlayerTimerExpired),
			StartSearchingTimeThreshold, false);

		bIsPlayerLocationKnown = false;

		// Signal all AI to respond to losing player
		// #TODO flag the AI closest to players last known position
		for (AEmpathAIController* AI : TActorRange<AEmpathAIController>(GetWorld()))
		{
			if (AI->IsAIRunning())
			{
				AI->OnLostPlayerTarget();
			}
		}

		break;

	case EPlayerAwarenessState::Lost:
		// Start searching
		PlayerAwarenessState = EPlayerAwarenessState::Searching;

		// Start searching
		// #TODO have flagged AI check player's last known position
		for (AEmpathAIController* AI : TActorRange<AEmpathAIController>(GetWorld()))
		{
			if (AI->IsAIRunning())
			{
				AI->OnPlayerSearchStarted();
			}
		}
		break;

	case EPlayerAwarenessState::Searching:
	{
		// Shouldn't happen, do nothing
		break;
	}

	break;

	}
}