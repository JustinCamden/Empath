// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathAIManager.h"
#include "EmpathAIController.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "EmpathVRCharacter.h"
#include "EmpathTypes.h"

// Stats for UE Profiler
DECLARE_CYCLE_STAT(TEXT("HearingChecks"), STAT_EMPATH_HearingChecks, STATGROUP_EMPATH_AIManager);

const float AEmpathAIManager::HearingDisconnectDist = 500.0f;

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
	OnNewPlayerAwarenessState.Broadcast(PlayerAwarenessState);
	bPlayerHasEverBeenSeen = false;
	bIsPlayerLocationKnown = false;
	LostPlayerTimeThreshold = 0.5f;
	StartSearchingTimeThreshold = 3.0f;
}

void AEmpathAIManager::OnPlayerDied()
{
	PlayerAwarenessState = EPlayerAwarenessState::PresenceNotKnown;
	OnNewPlayerAwarenessState.Broadcast(PlayerAwarenessState);
	bIsPlayerLocationKnown = false;
}

// Called when the game starts or when spawned
void AEmpathAIManager::BeginPlay()
{
	Super::BeginPlay();

	// Grab an AI controllers that were not already registered with us. Shouldn't ever happen, but just in case.
	for (AEmpathAIController* CurrAICon : TActorRange<AEmpathAIController>(GetWorld()))
	{
		CurrAICon->RegisterAIManager(this);
	}

	// Bind delegates to the player
	APlayerController* PlayerCon = GetWorld()->GetFirstPlayerController();
	if (PlayerCon)
	{
		APawn* PlayerPawn = PlayerCon->GetPawn();
		if (PlayerPawn)
		{
			AEmpathVRCharacter* VRPlayer = Cast<AEmpathVRCharacter>(PlayerPawn);
			if (VRPlayer)
			{
				VRPlayer->OnTeleport.AddDynamic(this, &AEmpathAIManager::OnPlayerTeleported);
				VRPlayer->OnDeath.AddDynamic(this, &AEmpathAIManager::OnPlayerDied);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("ERROR: Player is not an Empath VR Character!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ERROR: Player not possessing pawn!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ERROR: Player not found!"));
	}
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

	for (AEmpathAIController* AI : EmpathAICons)
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

void AEmpathAIManager::UpdateKnownTargetLocation(AActor const* Target)
{
	// If the target is a player
	if (Cast<AEmpathVRCharacter>(Target) != nullptr)
	{
		// Update variables
		bIsPlayerLocationKnown = true;
		LastKnownPlayerLocation = Target->GetActorLocation();

		// Stop the "lost player" state flow
		PlayerAwarenessState = EPlayerAwarenessState::KnownLocation;
		OnNewPlayerAwarenessState.Broadcast(PlayerAwarenessState);
		GetWorldTimerManager().ClearTimer(LostPlayerTimerHandle);


		// If this is the first time the player has been seen, notify all active
		// AIs that the player is in the area
		if (bPlayerHasEverBeenSeen == false)
		{
			for (AEmpathAIController* AI : EmpathAICons)
			{
				AI->OnTargetSeenForFirstTime();
			}

			bPlayerHasEverBeenSeen = true;
		}
	}

	// Secondary targets are always known, so just ignore any other case
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

void AEmpathAIManager::OnPlayerTeleported(AActor* Player, FVector Origin, FVector Destination)
{
	if (PlayerAwarenessState == EPlayerAwarenessState::KnownLocation)
	{
		PlayerAwarenessState = EPlayerAwarenessState::PotentiallyLost;
		OnNewPlayerAwarenessState.Broadcast(PlayerAwarenessState);

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
		OnNewPlayerAwarenessState.Broadcast(PlayerAwarenessState);
		GetWorldTimerManager().SetTimer(LostPlayerTimerHandle,
			FTimerDelegate::CreateUObject(this, &AEmpathAIManager::OnLostPlayerTimerExpired),
			StartSearchingTimeThreshold, false);

		bIsPlayerLocationKnown = false;

		// Signal all AI to respond to losing player
		// #TODO flag the AI closest to players last known position
		for (AEmpathAIController* AI : EmpathAICons)
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
		OnNewPlayerAwarenessState.Broadcast(PlayerAwarenessState);

		// Start searching
		// #TODO have flagged AI check player's last known position
		for (AEmpathAIController* AI : EmpathAICons)
		{
			if (AI->IsAIRunning())
			{
				AI->OnSearchForPlayerStarted();
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

void AEmpathAIManager::CheckForAwareAIs()
{
	if (bPlayerHasEverBeenSeen)
	{
		// Check whether any remaining AIs are active
		for (AEmpathAIController* AI : EmpathAICons)
		{
			if (!AI->IsPassive())
			{
				return;
			}
		}

		// If not, we are no longer aware of the player
		PlayerAwarenessState = EPlayerAwarenessState::PresenceNotKnown;
		OnNewPlayerAwarenessState.Broadcast(PlayerAwarenessState);
		bPlayerHasEverBeenSeen = false;
		bIsPlayerLocationKnown = false;
		return;
	}
}

void AEmpathAIManager::ReportNoise(AActor* NoiseInstigator, AActor* NoiseMaker, FVector Location, float HearingRadius)
{
	// Track how long it takes to complete this function for the profiler
	SCOPE_CYCLE_COUNTER(STAT_EMPATH_HearingChecks);

	// Since secondary targets are currently always known,
	// we only care about this if the instigator is the player, and we are not currently aware of them
	if (PlayerAwarenessState == EPlayerAwarenessState::KnownLocation)
	{
		return;
	}
	AEmpathVRCharacter* VRCharNoiseInstigator = Cast<AEmpathVRCharacter>(NoiseInstigator);
	if (!VRCharNoiseInstigator)
	{
		VRCharNoiseInstigator = Cast<AEmpathVRCharacter>(NoiseInstigator->Instigator);
		if (!VRCharNoiseInstigator)
		{
			return;
		}
	}

	// Otherwise, loop through the AIs and see if any of them are in the hearing radius
	bool bHeardSound = false;
	for (AEmpathAIController* AI : EmpathAICons)
	{
		// If the AI is valid
		if (!AI->IsPassive() && !AI->IsDead())
		{
			// Check if it is within hearing distance
			float DistFromSound = (AI->GetPawn()->GetActorLocation() - Location).Size();
			if (DistFromSound <= HearingRadius)
			{
				// If so, end the loop
				bHeardSound = true;
				break;
			}
		}
	}

	// If we heard the sound
	if (bHeardSound)
	{
		// Check if it close enough to the instigator to be effectively the same location
		float InstigatorDist = (Location - VRCharNoiseInstigator->GetVRLocation()).Size();
		if (InstigatorDist <= HearingDisconnectDist)
		{
			// If so, we have found the target
			UpdateKnownTargetLocation(VRCharNoiseInstigator);
		}
		
		// TODO: Flag area for AI to investigate
	}
	return;
}