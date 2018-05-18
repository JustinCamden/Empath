// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathCharacter.h"
#include "EmpathVRCharacter.h"
#include "EmpathAIController.h"
#include "EmpathDamageType.h"
#include "EmpathFunctionLibrary.h"


// Sets default values
AEmpathCharacter::AEmpathCharacter(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	MaxHealth = 100.0f;
	CurrentHealth = MaxHealth;
	MaxEffectiveDistance = 250.0f;
	MinEffectiveDistance = 0.0f;
	DefaultTeam = EEmpathTeam::Enemy;
	bStunnable = true;
	bInvincible = false;
	bCanTakeFriendlyFire = false;
	StunDamageThreshold = 50.0f;
	StunTimeThreshold = 0.5f;
	StunDurationDefault = 3.0f;
}

// Called when the game starts or when spawned
void AEmpathCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AEmpathCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Ensure we signal our destruction to the Empath AI con if we have not already
	if (!bDead)
	{
		bDead = true;
		AEmpathAIController* EmpathAICon = GetEmpathAICon();
		if (EmpathAICon)
		{
			EmpathAICon->OnCharacterDeath();
		}
	}
	Super::EndPlay(EndPlayReason);
}

AEmpathAIController* AEmpathCharacter::GetEmpathAICon()
{
	if (CachedEmpathAICon)
	{
		return CachedEmpathAICon;
	}
	return Cast<AEmpathAIController>(GetController());
}

// Called every frame
void AEmpathCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float AEmpathCharacter::GetDistanceToVR(const AActor* OtherActor) const
{
	// Check if the other actor is a VR character
	const AEmpathVRCharacter* OtherVRChar = Cast<AEmpathVRCharacter>(OtherActor);
	if (OtherVRChar)
	{
		return (GetActorLocation() - OtherVRChar->GetVRLocation()).Size();
	}

	// Otherwise, use default behavior
	else 
	{
		return GetDistanceTo(OtherActor);
	}
}

bool AEmpathCharacter::CanDie_Implementation()
{
	return (!bInvincible && !bDead);
}

void AEmpathCharacter::Die()
{
	if (CanDie())
	{
		// Update variables
		bDead = true;

		// Signal AI controller
		AEmpathAIController* EmpathAICon = GetEmpathAICon();
		if (EmpathAICon)
		{
			EmpathAICon->OnCharacterDeath();
		}

		// Signal notifies
		ReceiveDie();
		OnDeath.Broadcast();
	}
}

void AEmpathCharacter::ReceiveDie_Implementation()
{
	SetLifeSpan(0.001f);
}

EEmpathTeam AEmpathCharacter::GetTeamNum_Implementation() const
{
	// Defer to the controller
	AController* const Controller = GetController();
	if (Controller && Controller->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()))
	{
		return IEmpathTeamAgentInterface::Execute_GetTeamNum(Controller);
	}

	return DefaultTeam;
}

bool AEmpathCharacter::CanBeStunned_Implementation()
{
	return (bStunnable && !bDead && !bStunned);
}

void AEmpathCharacter::BeStunned(float StunDuration)
{
	if (CanBeStunned())
	{
		ReceiveStunned(StunDuration);
		OnStunned.Broadcast(StunDuration);
	}
}

void AEmpathCharacter::ReceiveStunned_Implementation(float StunDuration)
{
	bStunned = true;
	GetWorldTimerManager().ClearTimer(StunTimerHandle);
	if (StunDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(StunTimerHandle, this, &AEmpathCharacter::EndStun, StunDuration, false);
	}
}

void AEmpathCharacter::EndStun()
{
	if (bStunned)
	{
		ReceiveStunEnd();
		OnStunEnd.Broadcast();
	}
}

void AEmpathCharacter::ReceiveStunEnd_Implementation()
{
	bStunned = false;
	StunDamage = 0.0f;
}

float AEmpathCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
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

	// Process point damage
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		// Point damage event, pass off to helper function
		FPointDamageEvent* const PointDamageEvent = (FPointDamageEvent*)&DamageEvent;

		// Adjust the damage according to our per bone damage scale
		if ((EmpathDamageTypeCDO == nullptr) || (EmpathDamageTypeCDO->bIgnorePerBoneDamageScaling == false))
		{
			AdjustedDamage *= GetPerBoneDamageScale(PointDamageEvent->HitInfo.BoneName);
		}

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

		// Allow modification of any damage amount in children or blueprint classes
		AdjustedDamage = ModifyAnyDamage(AdjustedDamage, EventInstigator, DamageCauser, DamageTypeCDO);

		// Allow modification of specifically radial damage amount in children or blueprint classes
		AdjustedDamage = ModifyRadialDamage(AdjustedDamage, RadialDamageEvent->Origin, RadialDamageEvent->ComponentHits, RadialDamageEvent->Params.InnerRadius, RadialDamageEvent->Params.OuterRadius, EventInstigator, DamageCauser, DamageTypeCDO);
	}
	else
	{
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
		ProcessFinalDamage(ActualDamage, DamageTypeCDO);

		// If hit our mesh, do physics impulses as appropriate
		USkeletalMeshComponent* const MyMesh = GetMesh();
		if (MyMesh && DamageTypeCDO)
		{
			// If point damage event, add an impulse at the location we were hit
			if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
			{
				FPointDamageEvent* const PointDamageEvent = (FPointDamageEvent*)&DamageEvent;
				if ((DamageTypeCDO->DamageImpulse > 0.f) && !PointDamageEvent->ShotDirection.IsNearlyZero())
				{
					if (MyMesh->IsSimulatingPhysics(PointDamageEvent->HitInfo.BoneName))
					{
						FVector const ImpulseToApply = PointDamageEvent->ShotDirection.GetSafeNormal() * DamageTypeCDO->DamageImpulse;
						UEmpathFunctionLibrary::AddDistributedImpulseAtLocation(MyMesh, ImpulseToApply, PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.BoneName, 0.5f);
					}
				}
			}

			// If its a radial damage event, apply a radial impulse across our body 
			else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
			{
				FRadialDamageEvent* const RadialDamageEvent = (FRadialDamageEvent*)&DamageEvent;
				if (DamageTypeCDO->DamageImpulse > 0.f)
				{
					MyMesh->AddRadialImpulse(RadialDamageEvent->Origin, RadialDamageEvent->Params.OuterRadius, DamageTypeCDO->DamageImpulse, RIF_Linear, DamageTypeCDO->bRadialDamageVelChange);
				}
			}
		}
		return ActualDamage;
	}
	return 0.0f;
}

float AEmpathCharacter::GetPerBoneDamageScale(FName BoneName) const
{
	// Check each per bone damage we've defined for this bone name
	for (FPerBoneDamageScale const& CurrPerBoneDamage : PerBoneDamage)
	{
		// If we find the bone name, return the damage scale
		if (CurrPerBoneDamage.BoneNames.Contains(BoneName))
		{
			return CurrPerBoneDamage.DamageScale;
		}
	}

	// By default, return 1.0f (umodified damage sale)
	return 1.0f;
}

float AEmpathCharacter::ModifyAnyDamage_Implementation(float DamageAmount, const AController* EventInstigator, const AActor* DamageCauser, const class UDamageType* DamageType)
{
	return DamageAmount;
}

float AEmpathCharacter::ModifyPointDamage_Implementation(float DamageAmount, const FHitResult& HitInfo, const FVector& ShotDirection, const AController* EventInstigator, const AActor* DamageCauser, const class UDamageType* DamageTypeCDO)
{
	return DamageAmount;
}

float AEmpathCharacter::ModifyRadialDamage_Implementation(float DamageAmount, const FVector& Origin, const TArray<struct FHitResult>& ComponentHits, float InnerRadius, float OuterRadius, const AController* EventInstigator, const AActor* DamageCauser, const class UDamageType* DamageTypeCDO)
{
	return DamageAmount;
}

void AEmpathCharacter::ProcessFinalDamage_Implementation(const float DamageAmount, const UDamageType* DamageType)
{
	// Decrement health and check for death
	CurrentHealth -= DamageAmount;
	if (CurrentHealth <= 0.0f && CanDie())
	{
		Die();
		return;
	}

	// If we didn't die, and we can be stunned, check for stun damage
	if (CanBeStunned())
	{
		UWorld* const World = GetWorld();
		const UEmpathDamageType* EmpathDamageTyeCDO = Cast<UEmpathDamageType>(DamageType);
		if (EmpathDamageTyeCDO && World)
		{
			// If this damage automatically stuns, then become stunned
			if (EmpathDamageTyeCDO->bAutoStun)
			{
				BeStunned(StunDurationDefault);
			}
			// Otherwise, log the stun damage and check for being stunned
			else if (EmpathDamageTyeCDO->StunDamageMultiplier > 0.0f)
			{
				TakeStunDamage(EmpathDamageTyeCDO->StunDamageMultiplier * DamageAmount);
			}
		}
		else
		{
			// Default implementation for if we weren't passed an Empath damage type
			TakeStunDamage(DamageAmount);
		}
	}

	return;
}

void AEmpathCharacter::TakeStunDamage(float StunDamageAmount)
{
	// Log stun event
	UWorld* const World = GetWorld();
	StunDamageHistory.Add(FDamageHistoryEvent(StunDamageAmount,- World->GetTimeSeconds()));

	// Clean old stun events. They are stored oldest->newest, so we can just iterate to find 
	// the transition point. This plus the next loop will still constitute at most one pass 
	// through the array.

	int32 NumToRemove = 0;
	for (int32 Idx = 0; Idx < StunDamageHistory.Num(); ++Idx)
	{
		FDamageHistoryEvent& DHE = StunDamageHistory[Idx];
		if (World->TimeSince(DHE.EventTimestamp) > StunTimeThreshold)
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
			BeStunned(StunDurationDefault);
		break;
		}
	}
}