// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathCharacter.h"
#include "EmpathVRCharacter.h"
#include "EmpathAIController.h"
#include "EmpathDamageType.h"
#include "EmpathFunctionLibrary.h"
#include "EmpathAIManager.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"


// Stats for UE Profiler
DECLARE_CYCLE_STAT(TEXT("Empath Char Take Damage"), STAT_EMPATH_TakeDamage, STATGROUP_EMPATH_Character);
DECLARE_CYCLE_STAT(TEXT("Empath Is Ragdoll At Rest Check"), STAT_EMPATH_IsRagdollAtRest, STATGROUP_EMPATH_Character);

const FName AEmpathCharacter::MeshCollisionProfileName(TEXT("CharacterMesh"));
const FName AEmpathCharacter::PhysicalAnimationComponentName(TEXT("PhysicalAnimationComponent"));
const float AEmpathCharacter::RagdollCheckTimeMin = 0.25;
const float AEmpathCharacter::RagdollCheckTimeMax = 0.75;
const float AEmpathCharacter::RagdollRestThreshold_SingleBodyMax = 150.f;
const float AEmpathCharacter::RagdollRestThreshold_AverageBodyMax = 75.f;

// Sets default values
AEmpathCharacter::AEmpathCharacter(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	// Enable tick events on this character
	PrimaryActorTick.bCanEverTick = true;

	// Set default variables
	MaxHealth = 100.0f;
	CurrentHealth = MaxHealth;
	MaxEffectiveDistance = 250.0f;
	MinEffectiveDistance = 0.0f;
	DefaultTeam = EEmpathTeam::Enemy;
	bInvincible = false;
	bAllowDamageImpulse = true;
	bAllowDeathImpulse = true;
	bShouldRagdollOnDeath = true;
	bCanTakeFriendlyFire = false;
	bAllowRagdoll = true;
	bStunnable = true;
	StunDamageThreshold = 50.0f;
	StunTimeThreshold = 0.5f;
	StunDurationDefault = 3.0f;
	StunImmunityTimeAfterStunRecovery = 3.0f;
	CurrentCharacterPhysicsState = EEmpathCharacterPhysicsState::Kinematic;

	// Set default physics states
	FEmpathCharPhysicsStateSettingsEntry Entry;

	Entry.PhysicsState = EEmpathCharacterPhysicsState::Kinematic;
	PhysicsSettingsEntries.Add(Entry);

	Entry.PhysicsState = EEmpathCharacterPhysicsState::FullRagdoll;
	Entry.Settings.bSimulatePhysics = true;
	Entry.Settings.bEnableGravity = true;
	PhysicsSettingsEntries.Add(Entry);

	Entry.PhysicsState = EEmpathCharacterPhysicsState::HitReact;
	Entry.Settings.bEnableGravity = false;
	PhysicsSettingsEntries.Add(Entry);

	Entry.PhysicsState = EEmpathCharacterPhysicsState::PlayerHittable;
	PhysicsSettingsEntries.Add(Entry);

	Entry.PhysicsState = EEmpathCharacterPhysicsState::GettingUp;
	PhysicsSettingsEntries.Add(Entry);
	

	// Setup components
	// Mesh
	GetMesh()->SetCollisionProfileName(AEmpathCharacter::MeshCollisionProfileName);
	GetMesh()->bSingleSampleShadowFromStationaryLights = true;
	GetMesh()->bGenerateOverlapEvents = false;
	GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	GetMesh()->bApplyImpulseOnDamage = false;
	GetMesh()->SetEnableGravity(false);
	GetMesh()->SetSimulatePhysics(false);

	// Physical animation component
	PhysicalAnimation = CreateDefaultSubobject<UPhysicalAnimationComponent>(AEmpathCharacter::PhysicalAnimationComponentName);

	// Movement
	GetCharacterMovement()->bAlwaysCheckFloor = false;
}

// Called when the game starts or when spawned
void AEmpathCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Set reference for physical animation component
	if (PhysicalAnimation)
	{
		PhysicalAnimation->SetSkeletalMeshComponent(GetMesh());
	}

	// Set up Physics settings map for faster lookups
	for (FEmpathCharPhysicsStateSettingsEntry const& Entry : PhysicsSettingsEntries)
	{
		PhysicsStateToSettingsMap.Add(Entry.PhysicsState, Entry.Settings);
	}
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
			EmpathAICon->OnCharacterDeath(FHitResult(), FVector::ZeroVector, nullptr, nullptr, nullptr);
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

	// We check for the get up from ragdoll signal on tick because timers execute post-physics, 
	// and the component repositioning during getup causes visual pops
	if (bDeferredGetUpFromRagdoll)
	{
		bDeferredGetUpFromRagdoll = false;
		StartRecoverFromRagdoll();
	}
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

void AEmpathCharacter::Die(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType)
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

		// Signal AI controller
		AEmpathAIController* EmpathAICon = GetEmpathAICon();
		if (EmpathAICon)
		{
			EmpathAICon->OnCharacterDeath(KillingHitInfo, KillingHitImpulseDir, DeathInstigator, DeathCauser, DeathDamageType);
		}

		// Signal notifies
		ReceiveDie(KillingHitInfo, KillingHitImpulseDir, DeathInstigator, DeathCauser, DeathDamageType);
		OnDeath.Broadcast(KillingHitInfo, KillingHitImpulseDir, DeathInstigator, DeathCauser, DeathDamageType);
	}
}

void AEmpathCharacter::ReceiveDie_Implementation(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* KillingDamageType)
{
	// Set clean up timer
	GetWorldTimerManager().SetTimer(CleanUpPostDeathTimerHandle, this, &AEmpathCharacter::CleanUpPostDeath, CleanUpPostDeathTime, false);
	

	// Begin ragdolling if appropriate
	if (bShouldRagdollOnDeath)
	{
		USkeletalMeshComponent* const MyMesh = GetMesh();
		if (MyMesh)
		{
			StartRagdoll();
			UEmpathDamageType const* EmpathDamageTypeCDO = Cast<UEmpathDamageType>(KillingDamageType);

			// Apply an additional death impulse if appropriate
			if (bAllowDeathImpulse && EmpathDamageTypeCDO && MyMesh->IsSimulatingPhysics())
			{
				// If a default hit was passed in, fill in some assumed data
				FHitResult RealKillingHitInfo = KillingHitInfo;
				if ((RealKillingHitInfo.ImpactPoint == RealKillingHitInfo.Location) && (RealKillingHitInfo.ImpactPoint == FVector::ZeroVector))
				{
					RealKillingHitInfo.ImpactPoint = GetActorLocation();
					RealKillingHitInfo.Location = RealKillingHitInfo.ImpactPoint;
				}

				// Actually apply the death impulse
				float const DeathImpulse = (EmpathDamageTypeCDO->DeathImpulse >= 0.f) ? EmpathDamageTypeCDO->DeathImpulse : 0.0f;
				FVector const Impulse = KillingHitImpulseDir * DeathImpulse + FVector(0, 0, EmpathDamageTypeCDO->DeathImpulseUpkick);
				UEmpathFunctionLibrary::AddDistributedImpulseAtLocation(MyMesh, Impulse, KillingHitInfo.ImpactPoint, KillingHitInfo.BoneName, 0.5f);
			}
		}
	}
}

void AEmpathCharacter::CleanUpPostDeath_Implementation()
{
	GetWorldTimerManager().ClearTimer(CleanUpPostDeathTimerHandle);
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
	return (bStunnable 
		&& !bDead 
		&& !bStunned 
		&& GetWorld()->TimeSince(LastStunTime) > StunImmunityTimeAfterStunRecovery
		&& !bRagdolling);
}

void AEmpathCharacter::BeStunned(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration)
{
	if (CanBeStunned())
	{
		bStunned = true;
		ReceiveStunned(StunInstigator, StunCauser, StunDuration);
		LastStunTime = GetWorld()->GetTimeSeconds();
		AEmpathAIController* EmpathAICon = GetEmpathAICon();
		if (EmpathAICon)
		{
			EmpathAICon->ReceiveCharacterStunned(StunInstigator, StunCauser, StunDuration);
		}
		OnStunned.Broadcast(StunInstigator, StunCauser, StunDuration);
	}
}

void AEmpathCharacter::ReceiveStunned_Implementation(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration)
{
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
		// Update variables
		bStunned = false;
		GetWorldTimerManager().ClearTimer(StunTimerHandle);
		StunDamageHistory.Empty();

		// Broadcast events and notifies
		ReceiveStunEnd();
		AEmpathAIController* EmpathAICon = GetEmpathAICon();
		if (EmpathAICon)
		{
			EmpathAICon->ReceiveCharacterStunEnd();
		}
		OnStunEnd.Broadcast();
	}
}

void AEmpathCharacter::ReceiveStunEnd_Implementation()
{
	return;
}

float AEmpathCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Scope these functions for the UE4 profiler
	SCOPE_CYCLE_COUNTER(STAT_EMPATH_TakeDamage);

	// If we're invincible, dead, or this is no damage, do nothing
	if (bInvincible || bDead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	// If the player inflicted this damage, and the player's location was previous unknown,
	// alert us to their location
	AEmpathVRCharacter* Player = Cast<AEmpathVRCharacter>(DamageCauser);
	if (!Player && EventInstigator)
	{
		Player = Cast<AEmpathVRCharacter>(EventInstigator->GetPawn());
	}
	if (Player)
	{
		AEmpathAIController* AICon = GetEmpathAICon();
		if (AICon)
		{
			AEmpathAIManager* AIManager = AICon->GetAIManager();
			if (AIManager && AIManager->IsPlayerPotentiallyLost())
			{
				AIManager->UpdateKnownTargetLocation(Player);
			}
		}
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

		// Do physics impulses as appropriate
		USkeletalMeshComponent* const MyMesh = GetMesh();
		if (MyMesh && DamageTypeCDO && bAllowDamageImpulse)
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

void AEmpathCharacter::ProcessFinalDamage_Implementation(const float DamageAmount, FHitResult const& HitInfo, FVector HitImpulseDir, const UDamageType* DamageType, const AController* EventInstigator, const AActor* DamageCauser)
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

void AEmpathCharacter::TakeStunDamage(const float StunDamageAmount, const AController* EventInstigator, const AActor* DamageCauser)
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
			BeStunned(EventInstigator, DamageCauser, StunDurationDefault);
			break;
		}
	}
}

bool AEmpathCharacter::SetCharacterPhysicsState(EEmpathCharacterPhysicsState NewState)
{
	if (NewState != CurrentCharacterPhysicsState)
	{
		// Notify state end
		ReceiveEndCharacterPhysicsState(CurrentCharacterPhysicsState);

		FEmpathCharPhysicsStateSettings NewSettings;

		// Look up new state settings
		{
			FEmpathCharPhysicsStateSettings* FoundNewSettings = PhysicsStateToSettingsMap.Find(NewState);
			if (FoundNewSettings)
			{
				NewSettings = *FoundNewSettings;
			}
			else
			{
				// Log error.
				FName EnumName;
				const UEnum* EnumPtr = FindObject<UEnum>(ANY_PACKAGE, TEXT("EEmpathCharacterPhysicsState"), true);
				if (EnumPtr)
				{
					EnumName = EnumPtr->GetNameByValue((int64)NewState);
				}
				else
				{
					EnumName = "Invalid Entry";
				}
				UE_LOG(LogTemp, Warning, TEXT("%s ERROR: Could not find new physics state settings %s!"), *GetName(), *EnumName.ToString());
				return false;
			}
		}

		USkeletalMeshComponent* const MyMesh = GetMesh();

		// Set simulate physics
		if (NewSettings.bSimulatePhysics == false)
		{
			MyMesh->SetSimulatePhysics(false);
		}
		else
		{
			if (NewSettings.SimulatePhysicsBodyBelowName != NAME_None)
			{
				// We need to set false first since the SetAllBodiesBelow call below doesn't affect bodies above.
				// so we want to ensure they are in the default state
				MyMesh->SetSimulatePhysics(false);
				MyMesh->SetAllBodiesBelowSimulatePhysics(NewSettings.SimulatePhysicsBodyBelowName, true, true);
			}
			else
			{
				MyMesh->SetSimulatePhysics(true);
			}
		}

		// Set gravity
		MyMesh->SetEnableGravity(NewSettings.bEnableGravity);

		// Set physical animation
		PhysicalAnimation->ApplyPhysicalAnimationProfileBelow(NewSettings.PhysicalAnimationBodyName, NewSettings.PhysicalAnimationProfileName, true, true);

		// Set constraint profile
		if (NewSettings.ConstraintProfileJointName == NAME_None)
		{
			MyMesh->SetConstraintProfileForAll(NewSettings.ConstraintProfileName, true);
		}
		else
		{
			MyMesh->SetConstraintProfile(NewSettings.ConstraintProfileJointName, NewSettings.ConstraintProfileName, true);
		}

		// Update state and signal notifies
		CurrentCharacterPhysicsState = NewState;
		ReceiveBeginCharacterPhysicsState(NewState);
	}

	return true;
}

bool AEmpathCharacter::CanRagdoll_Implementation()
{
	return (bAllowRagdoll && !bRagdolling);
}

void AEmpathCharacter::StartRagdoll()
{
	if (CanRagdoll())
	{
		USkeletalMeshComponent* const MyMesh = GetMesh();
		if (MyMesh)
		{
			// Update physics state
			SetCharacterPhysicsState(EEmpathCharacterPhysicsState::FullRagdoll);
			MyMesh->SetCollisionProfileName(FEmpathCollisionrProfileNames::Ragdoll);

			// We set the capsule to ignore all interactions rather than turning off collision completely, 
			// since there's a good chance we will get back up,
			//and the physics state would have to be recreated again.
			GetCapsuleComponent()->SetCollisionProfileName(FEmpathCollisionrProfileNames::PawnIgnoreAll);
			bRagdolling = true;

			// If we're not dead, start timer for getting back up
			if (bDead == false)
			{
				StartAutomaticRecoverFromRagdoll();
			}

			// Otherwise, clear any current timer and signals
			else
			{
				StopAutomaticRecoverFromRagdoll();
				bDeferredGetUpFromRagdoll = false;
			}

			ReceiveStartRagdoll();
		}
	}
}

void AEmpathCharacter::StartAutomaticRecoverFromRagdoll()
{
	if (bRagdolling && !bDead)
	{
		// We want to wait a bit between checking whether our ragdoll is at rest for performance
		float const NextGetUpFromRagdollCheckInterval = FMath::RandRange(RagdollCheckTimeMin, RagdollCheckTimeMax);
		GetWorldTimerManager().SetTimer(GetUpFromRagdollTimerHandle, FTimerDelegate::CreateUObject(this, &AEmpathCharacter::CheckForEndRagdoll), NextGetUpFromRagdollCheckInterval, false);
	}
}

void AEmpathCharacter::StopAutomaticRecoverFromRagdoll()
{
	GetWorldTimerManager().ClearTimer(GetUpFromRagdollTimerHandle);
}

void AEmpathCharacter::CheckForEndRagdoll()
{
	if (bDead == false)
	{
		if (IsRagdollAtRest())
		{
			bDeferredGetUpFromRagdoll = true;
		}
		else
		{
			StartAutomaticRecoverFromRagdoll();
		}
	}
	else
	{
		StopAutomaticRecoverFromRagdoll();
		bDeferredGetUpFromRagdoll = false;
	}
}

bool AEmpathCharacter::IsRagdollAtRest() const
{
	SCOPE_CYCLE_COUNTER(STAT_EMPATH_IsRagdollAtRest);

	if (bRagdolling)
	{
		int32 NumSimulatingBodies = 0;
		float TotalBodySpeed = 0.f;
		USkeletalMeshComponent const* const MyMesh = GetMesh();

		// Calculate the current rate of movement of our physics bodies
		for (FBodyInstance const* BI : MyMesh->Bodies)
		{
			if (BI->IsInstanceSimulatingPhysics())
			{
				if (BI->GetUnrealWorldTransform().GetLocation().Z < GetWorldSettings()->KillZ)
				{
					// If at least one body below KillZ, we are "at rest" in the sense that it's ok to clean us up
					return true;
				}

				// If any one body exceeds the single-body max, we are NOT at rest
				float const BodySpeed = BI->GetUnrealWorldVelocity().Size();
				if (BodySpeed > RagdollRestThreshold_SingleBodyMax)
				{
					return false;
				}

				TotalBodySpeed += BodySpeed;
				++NumSimulatingBodies;
			}
		}

		// If the average body speed is too high, we are NOT at rest
		if (NumSimulatingBodies > 0)
		{
			float const AverageBodySpeed = TotalBodySpeed / float(NumSimulatingBodies);
			if (AverageBodySpeed > RagdollRestThreshold_AverageBodyMax)
			{
				return false;
			}
		}
	}

	// Not simulating physics on any bodies, so we are at rest
	return true;
}


void AEmpathCharacter::StartRecoverFromRagdoll()
{
	USkeletalMeshComponent* const MyMesh = GetMesh();
	if (MyMesh && !bDead)
	{
		GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
		ReceiveStartRecoverFromRagdoll();
	}
}

void AEmpathCharacter::ReceiveStartRecoverFromRagdoll_Implementation()
{
	StopRagdoll(EEmpathCharacterPhysicsState::Kinematic);
}

void AEmpathCharacter::StopRagdoll(EEmpathCharacterPhysicsState NewPhysicsState)
{
	USkeletalMeshComponent* const MyMesh = GetMesh();
	if (MyMesh && bRagdolling)
	{
		// Update physics state
		SetCharacterPhysicsState(NewPhysicsState);
		MyMesh->SetCollisionProfileName(AEmpathCharacter::MeshCollisionProfileName);
		GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

		// Reattach mesh to capsule
		MyMesh->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepWorldTransform);

		bRagdolling = false;
		ReceiveStopRagdoll();
	}
}