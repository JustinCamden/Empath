// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathVRCharacter.h"
#include "EmpathPlayerController.h"
#include "EmpathDamageType.h"
#include "EmpathFunctionLibrary.h"

// Stats for UE Profiler
DECLARE_CYCLE_STAT(TEXT("Empath VR Char Take Damage"), STAT_EMPATH_TakeDamage, STATGROUP_EMPATH_VRCharacter);

AEmpathVRCharacter::AEmpathVRCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Initialize default variables
	bDead = false;
	bHealthRegenActive = true;
	HealthRegenDelay = 3.0f;
	HealthRegenRate = 1.0f / 3.0f;	// 3 seconds to full regen
	bInvincible = false;
	MaxHealth = 100.0f;
	CurrentHealth = MaxHealth;
	bStunnable = false;
	StunDamageThreshold = 50.0f;
	StunTimeThreshold = 0.5f;
	StunDurationDefault = 3.0f;
	StunImmunityTimeAfterStunRecovery = 3.0f;

	// Initialize damage capsule
	DamageCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("DamageCapsule"));
	DamageCapsule->SetupAttachment(VRReplicatedCamera);
	DamageCapsule->SetCapsuleHalfHeight(30.0f);
	DamageCapsule->SetCapsuleRadius(8.5f);
	DamageCapsule->SetRelativeLocation(FVector(1.75f, 0.0f, -18.5f));
}

float AEmpathVRCharacter::GetDistanceToVR(AActor* OtherActor) const
{
	return (GetVRLocation() - OtherActor->GetActorLocation()).Size();
}

EEmpathTeam AEmpathVRCharacter::GetTeamNum_Implementation() const
{
	// We should always return player for VR characters
	return EEmpathTeam::Player;
}


FVector AEmpathVRCharacter::GetCustomAimLocationOnActor_Implementation(FVector LookOrigin, FVector LookDirection) const
{
	return DamageCapsule->GetComponentLocation();
}

FVector AEmpathVRCharacter::GetScaledHeightLocation(float HeightScale)
{
	// Get the scaled height
	FVector Height = VRReplicatedCamera->GetRelativeTransform().GetLocation();
	Height.Z *= HeightScale;
	Height = GetTransform().TransformPosition(Height);

	// Get the VR location with the adjusted height
	FVector VRLoc = GetVRLocation();
	VRLoc.Z = Height.Z;
	return VRLoc;
}

FVector AEmpathVRCharacter::GetPawnViewLocation() const
{
	if (VRReplicatedCamera)
	{
		return VRReplicatedCamera->GetComponentLocation();
	}
	return Super::GetPawnViewLocation();
}

FRotator AEmpathVRCharacter::GetViewRotation() const
{
	if (VRReplicatedCamera)
	{
		return VRReplicatedCamera->GetComponentRotation();
	}
	return Super::GetViewRotation();
}

AEmpathPlayerController* AEmpathVRCharacter::GetEmpathPlayerCon() const
{
	if (EmpathPlayerController)
	{
		return EmpathPlayerController;
	}
	return Cast<AEmpathPlayerController>(GetController());
}

bool AEmpathVRCharacter::IsTeleporting_Implementation() const
{
	// TODO: Hook this up with our teleportation script
	return false;
}

void AEmpathVRCharacter::TeleportToVR(FVector Destination, float DeltaYaw)
{
	// Cache variables
	FVector Origin = GetVRLocation();

	// Fire notifies
	ReceiveTeleport(Origin, Destination, DeltaYaw);
	OnTeleport.Broadcast(this, Origin, Destination);

	// TODO: Implement our actual teleportation in C++. 
	// For now, we can use the blueprint implementation of "OnTeleport"
	return;
}

bool AEmpathVRCharacter::CanDie_Implementation()
{
	return (!bInvincible && !bDead);
}

void AEmpathVRCharacter::Die(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType)
{
	if (CanDie())
	{
		// Update variables
		bDead = true;

		// End stunned state flow
		if (bStunned)
		{
			EndStun();
		}
		else
		{
			GetWorldTimerManager().ClearTimer(StunTimerHandle);
		}

		// Signal Player controller
		AEmpathPlayerController* EmpathPlayerCon = GetEmpathPlayerCon();
		if (EmpathPlayerCon)
		{
			EmpathPlayerCon->ReceiveCharacterDeath(KillingHitInfo, KillingHitImpulseDir, DeathInstigator, DeathCauser, DeathDamageType);
		}

		// Signal notifies
		ReceiveDie(KillingHitInfo, KillingHitImpulseDir, DeathInstigator, DeathCauser, DeathDamageType);
		OnDeath.Broadcast(KillingHitInfo, KillingHitImpulseDir, DeathInstigator, DeathCauser, DeathDamageType);
	}
}

float AEmpathVRCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Scope these functions for the UE4 profiler
	SCOPE_CYCLE_COUNTER(STAT_EMPATH_TakeDamage);

	// If we're invincible, dead, or this is no damage, do nothing
	if (bInvincible || bDead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	// Cache the inital damage amount
	float AdjustedDamage = DamageAmount;

	// Grab the damage type
	UDamageType const* const DamageTypeCDO = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
	UEmpathDamageType const* EmpathDamageTypeCDO = Cast<UEmpathDamageType>(DamageTypeCDO);

	// Friendly fire damage adjustment
	EEmpathTeam const InstigatorTeam = UEmpathFunctionLibrary::GetActorTeam(EventInstigator);
	EEmpathTeam const MyTeam = GetTeamNum();
	if (InstigatorTeam == MyTeam && bCanTakeFriendlyFire && EmpathDamageTypeCDO)
	{
		// If this damage came from our team and we can't take friendly fire, do nothing
		if (!bCanTakeFriendlyFire)
		{
			return 0.0f;
		}

		// Otherwise, scale the damage by the friendly fire damage multiplier
		else if (EmpathDamageTypeCDO)
		{
			AdjustedDamage *= EmpathDamageTypeCDO->FriendlyFireDamageMultiplier;
		}
	}

	// Setup variables for hit direction and info
	FVector HitImpulseDir;
	FHitResult HitInfo;
	DamageEvent.GetBestHitInfo(this, (EventInstigator ? EventInstigator->GetPawn() : DamageCauser), HitInfo, HitImpulseDir);

	// Process point damage
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		// Point damage event, pass off to helper function
		FPointDamageEvent* const PointDamageEvent = (FPointDamageEvent*)&DamageEvent;

		// Update hit info
		HitInfo = PointDamageEvent->HitInfo;
		HitImpulseDir = PointDamageEvent->ShotDirection;

		// Allow modification of any damage amount in children or blueprint classes
		AdjustedDamage = ModifyAnyDamage(AdjustedDamage, EventInstigator, DamageCauser, DamageTypeCDO);

		// Allow modification of point damage specifically in children and blueprint classes
		AdjustedDamage = ModifyPointDamage(AdjustedDamage, PointDamageEvent->HitInfo, PointDamageEvent->ShotDirection, EventInstigator, DamageCauser, DamageTypeCDO);
	}

	// Process radial damage
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		// Radial damage event, pass off to helper function
		FRadialDamageEvent* const RadialDamageEvent = (FRadialDamageEvent*)&DamageEvent;

		// Get best hit info we can under the circumstances
		DamageEvent.GetBestHitInfo(this, (EventInstigator ? EventInstigator->GetPawn() : DamageCauser), HitInfo, HitImpulseDir);

		// Allow modification of any damage amount in children or blueprint classes
		AdjustedDamage = ModifyAnyDamage(AdjustedDamage, EventInstigator, DamageCauser, DamageTypeCDO);

		// Allow modification of specifically radial damage amount in children or blueprint classes
		AdjustedDamage = ModifyRadialDamage(AdjustedDamage, RadialDamageEvent->Origin, RadialDamageEvent->ComponentHits, RadialDamageEvent->Params.InnerRadius, RadialDamageEvent->Params.OuterRadius, EventInstigator, DamageCauser, DamageTypeCDO);
	}
	else
	{
		// Get best hit info we can under the circumstances
		DamageEvent.GetBestHitInfo(this, (EventInstigator ? EventInstigator->GetPawn() : DamageCauser), HitInfo, HitImpulseDir);

		// Allow modification of any damage amount in children or blueprint classes
		AdjustedDamage = ModifyAnyDamage(AdjustedDamage, EventInstigator, DamageCauser, DamageTypeCDO);
	}

	// Check again if our damage is <= 0
	if (AdjustedDamage <= 0.0f)
	{
		return 0.0f;
	}

	// Fire parent damage command. Do not modify damage amount after this
	const float ActualDamage = Super::TakeDamage(AdjustedDamage, DamageEvent, EventInstigator, DamageCauser);

	// Respond to the damage
	if (ActualDamage >= 0.f)
	{
		// Process damage to update health and death state
		ProcessFinalDamage(ActualDamage, HitInfo, HitImpulseDir, DamageTypeCDO, EventInstigator, DamageCauser);
		return ActualDamage;
	}
	return 0.0f;
}

float AEmpathVRCharacter::ModifyAnyDamage_Implementation(float DamageAmount, const AController* EventInstigator, const AActor* DamageCauser, const class UDamageType* DamageType)
{
	return DamageAmount;
}

float AEmpathVRCharacter::ModifyPointDamage_Implementation(float DamageAmount, const FHitResult& HitInfo, const FVector& ShotDirection, const AController* EventInstigator, const AActor* DamageCauser, const class UDamageType* DamageTypeCDO)
{
	return DamageAmount;
}

float AEmpathVRCharacter::ModifyRadialDamage_Implementation(float DamageAmount, const FVector& Origin, const TArray<struct FHitResult>& ComponentHits, float InnerRadius, float OuterRadius, const AController* EventInstigator, const AActor* DamageCauser, const class UDamageType* DamageTypeCDO)
{
	return DamageAmount;
}

void AEmpathVRCharacter::ProcessFinalDamage_Implementation(const float DamageAmount, FHitResult const& HitInfo, FVector HitImpulseDir, const UDamageType* DamageType, const AController* EventInstigator, const AActor* DamageCauser)
{
	// Decrement health and check for death
	CurrentHealth -= DamageAmount;
	if (CurrentHealth <= 0.0f && CanDie())
	{
		Die(HitInfo, HitImpulseDir, EventInstigator, DamageCauser, DamageType);
		return;
	}

	// If we didn't die, and we can be stunned, log stun damage
	if (CanBeStunned())
	{
		UWorld* const World = GetWorld();
		const UEmpathDamageType* EmpathDamageTyeCDO = Cast<UEmpathDamageType>(DamageType);
		if (EmpathDamageTyeCDO && World)
		{
			// Log the stun damage and check for being stunned
			if (EmpathDamageTyeCDO->StunDamageMultiplier > 0.0f)
			{
				TakeStunDamage(EmpathDamageTyeCDO->StunDamageMultiplier * DamageAmount, EventInstigator, DamageCauser);
			}
		}
		else
		{
			// Default implementation for if we weren't passed an Empath damage type
			TakeStunDamage(DamageAmount, EventInstigator, DamageCauser);
		}
	}

	return;
}

bool AEmpathVRCharacter::CanBeStunned_Implementation()
{
	return (bStunnable
		&& !bDead
		&& !bStunned
		&& GetWorld()->TimeSince(LastStunTime) > StunImmunityTimeAfterStunRecovery);
}

void AEmpathVRCharacter::TakeStunDamage(const float StunDamageAmount, const AController* EventInstigator, const AActor* DamageCauser)
{
	// Log stun event
	StunDamageHistory.Add(FDamageHistoryEvent(StunDamageAmount, -GetWorld()->GetTimeSeconds()));

	// Clean old stun events. They are stored oldest->newest, so we can just iterate to find 
	// the transition point. This plus the next loop will still constitute at most one pass 
	// through the array.

	int32 NumToRemove = 0;
	for (int32 Idx = 0; Idx < StunDamageHistory.Num(); ++Idx)
	{
		FDamageHistoryEvent& DHE = StunDamageHistory[Idx];
		if (GetWorld()->TimeSince(DHE.EventTimestamp) > StunTimeThreshold)
		{
			NumToRemove++;
		}
		else
		{
			break;
		}
	}

	if (NumToRemove > 0)
	{
		// Remove expired events
		StunDamageHistory.RemoveAt(0, NumToRemove);
	}


	// Remaining history array is now guaranteed to be inside the time threshold.
	// Just add up and stun if nececessary. This way we don't have to process on Tick.
	float AccumulatedDamage = 0.f;
	for (FDamageHistoryEvent& DHE : StunDamageHistory)
	{
		AccumulatedDamage += DHE.DamageAmount;
		if (AccumulatedDamage > StunDamageThreshold)
		{
			BeStunned(EventInstigator, DamageCauser, StunDurationDefault);
			break;
		}
	}
}

void AEmpathVRCharacter::BeStunned(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration)
{
	if (CanBeStunned())
	{
		bStunned = true;
		LastStunTime = GetWorld()->GetTimeSeconds();
		ReceiveStunned(StunInstigator, StunCauser, StunDuration);
		AEmpathPlayerController* EmpathPlayerCon = GetEmpathPlayerCon();
		if (EmpathPlayerCon)
		{
			EmpathPlayerCon->ReceiveCharacterStunEnd();
		}
		OnStunned.Broadcast(StunInstigator, StunCauser, StunDuration);
	}
}

void AEmpathVRCharacter::ReceiveStunned_Implementation(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration)
{
	GetWorldTimerManager().ClearTimer(StunTimerHandle);
	if (StunDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(StunTimerHandle, this, &AEmpathVRCharacter::EndStun, StunDuration, false);
	}
}

void AEmpathVRCharacter::EndStun()
{
	if (bStunned)
	{
		// Update variables
		bStunned = false;
		GetWorldTimerManager().ClearTimer(StunTimerHandle);
		StunDamageHistory.Empty();

		// Broadcast events and notifies
		ReceiveStunEnd();
		AEmpathPlayerController* EmpathPlayerCon = GetEmpathPlayerCon();
		if (EmpathPlayerCon)
		{
			EmpathPlayerCon->ReceiveCharacterStunEnd();
		}
		OnStunEnd.Broadcast();
	}
}

void AEmpathVRCharacter::ReceiveStunEnd_Implementation()
{
	return;
}