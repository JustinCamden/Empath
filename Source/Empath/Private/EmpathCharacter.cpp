// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathCharacter.h"
#include "EmpathPlayerCharacter.h"
#include "EmpathAIController.h"
#include "EmpathDamageType.h"
#include "EmpathFunctionLibrary.h"
#include "EmpathAIManager.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "EmpathCharacterMovementComponent.h"
#include "EmpathPathFollowingComponent.h"
#include "DrawDebugHelpers.h"

// Stats for UE Profiler
DECLARE_CYCLE_STAT(TEXT("Empath Char Take Damage"), STAT_EMPATH_TakeDamage, STATGROUP_EMPATH_Character);
DECLARE_CYCLE_STAT(TEXT("Empath Is Ragdoll At Rest Check"), STAT_EMPATH_IsRagdollAtRest, STATGROUP_EMPATH_Character);

// Log categories
DEFINE_LOG_CATEGORY_STATIC(LogNavRecovery, Log, All);

// Console variable setup so we can enable and disable nav recovery debugging from the console
static int32 EmpathNavRecoveryDrawDebug = 0;
FAutoConsoleVariableRef CVarEmpathNavRecoveryDrawDebug(
	TEXT("Empath.NavRecoveryDrawDebug"),
	EmpathNavRecoveryDrawDebug,
	TEXT("Whether to enable nav recovery debug.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Cheat);

static float EmpathNavRecoveryDebugLifetime = 3.0f;
FAutoConsoleVariableRef CVarEmpathNavRecoveryDebugLifetime(
	TEXT("Empath.NavRecoveryDrawLifetime"),
	EmpathNavRecoveryDebugLifetime,
	TEXT("Duration of debug drawing for nav recovery, in seconds."),
	ECVF_Cheat);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define NAVRECOVERY_LOC(_Loc, _Radius, _Color)				if (EmpathNavRecoveryDrawDebug) { DrawDebugSphere(GetWorld(), _Loc, _Radius, 16, _Color); }
#define NAVRECOVERY_LINE(_Loc, _Dest, _Color)				if (EmpathNavRecoveryDrawDebug) { DrawDebugLine(GetWorld(), _Loc, _Dest, _Color); }
#define NAVRECOVERY_LOC_DURATION(_Loc, _Radius, _Color)		if (EmpathNavRecoveryDrawDebug) { DrawDebugSphere(GetWorld(), _Loc, _Radius, 16, _Color, false, EmpathNavRecoveryDebugLifetime); }
#define NAVRECOVERY_LINE_DURATION(_Loc, _Dest, _Color)		if (EmpathNavRecoveryDrawDebug) { DrawDebugLine(GetWorld(), _Loc, _Dest, _Color, false, EmpathNavRecoveryDebugLifetime); }
#else
#define NAVRECOVERY_LOC(_Loc, _Radius, _Color)				/* nothing */
#define NAVRECOVERY_LINE(_Loc, _Dest, _Color)				/* nothing */
#define NAVRECOVERY_LOC_DURATION(_Loc, _Radius, _Color)		/* nothing */
#define NAVRECOVERY_LINE_DURATION(_Loc, _Dest, _Color)		/* nothing */
#endif

// Consts
const FName AEmpathCharacter::MeshCollisionProfileName(TEXT("CharacterMesh"));
const FName AEmpathCharacter::PhysicalAnimationComponentName(TEXT("PhysicalAnimationComponent"));
const float AEmpathCharacter::RagdollCheckTimeMin = 0.25;
const float AEmpathCharacter::RagdollCheckTimeMax = 0.75;
const float AEmpathCharacter::RagdollRestThreshold_SingleBodyMax = 150.f;
const float AEmpathCharacter::RagdollRestThreshold_AverageBodyMax = 75.f;

// Sets default values
AEmpathCharacter::AEmpathCharacter(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer
	.SetDefaultSubobjectClass<UEmpathCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Enable tick events on this character
	PrimaryActorTick.bCanEverTick = true;

	// Health and damage
	bInvincible = false;
	MaxHealth = 10.0f;
	CurrentHealth = MaxHealth;
	bAllowDamageImpulse = true;
	bAllowDeathImpulse = true;
	bShouldRagdollOnDeath = true;
	bCanTakeFriendlyFire = false;

	// Stunning
	bStunnable = true;
	StunDamageThreshold = 5.0f;
	StunTimeThreshold = 0.5f;
	StunDurationDefault = 3.0f;
	StunImmunityTimeAfterStunRecovery = 3.0f;


	// General combat
	MaxEffectiveDistance = 250.0f;
	MinEffectiveDistance = 0.0f;
	DefaultTeam = EEmpathTeam::Enemy;

	// Physics
	bAllowRagdoll = true;
	CurrentCharacterPhysicsState = EEmpathCharacterPhysicsState::Kinematic;
	
	// Initialize default physics state entries
	// Physical animation profiles will have to be set in blueprint
	// Kinematic
	FEmpathCharPhysicsStateSettingsEntry Entry;
	Entry.PhysicsState = EEmpathCharacterPhysicsState::Kinematic;
	PhysicsSettingsEntries.Add(Entry);

	// Full ragdoll
	Entry.PhysicsState = EEmpathCharacterPhysicsState::FullRagdoll;
	Entry.Settings.bSimulatePhysics = true;
	Entry.Settings.bEnableGravity = true;
	PhysicsSettingsEntries.Add(Entry);

	// Hit react
	Entry.PhysicsState = EEmpathCharacterPhysicsState::HitReact;
	Entry.Settings.bEnableGravity = false;
	PhysicsSettingsEntries.Add(Entry);

	// Player hittable
	Entry.PhysicsState = EEmpathCharacterPhysicsState::PlayerHittable;
	PhysicsSettingsEntries.Add(Entry);

	// Getting up
	Entry.PhysicsState = EEmpathCharacterPhysicsState::GettingUp;
	PhysicsSettingsEntries.Add(Entry);

	// Nav mesh recovery
	bShouldUseNavRecovery = true;
	NavRecoveryAbility = EEmpathNavRecoveryAbility::OnNavMeshIsland;
	bFailedNavigation = false;
	NavRecoveryJumpArcMin = 0.25f;
	NavRecoveryJumpArcMax = 0.5f;

	// Adjust On Island recovery settings
	NavRecoverySettingsOnIsland.MaxRecoveryAttemptTime = 3.0f;
	NavRecoverySettingsOnIsland.SearchInnerRadius = 200.0f;
	NavRecoverySettingsOnIsland.SearchOuterRadius = 400.0f;
	NavRecoverySettingsOnIsland.SearchRadiusGrowthRateInner = 25.0f;
	NavRecoverySettingsOnIsland.SearchRadiusGrowthRateOuter = 50.0f;
	NavRecoveryTestExtent = FVector(0.0f, 0.0f, 2048.0f);
	
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

AEmpathAIController* AEmpathCharacter::GetEmpathAICon() const
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

	// Navmesh recovery
	if (IsFailingNavigation() && !bDead)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			const FVector Location = GetActorLocation();
			const float TimeSinceStart = World->TimeSince(NavRecoveryStartTime);
			const FEmpathNavRecoverySettings& CurrentSettings = GetCurrentNavRecoverySettings();

			if (TimeSinceStart >= CurrentSettings.MaxRecoveryAttemptTime)
			{
				OnNavMeshRecoveryFailed(Location, TimeSinceStart);
			}
			else
			{
				// Require default movement mode.
				// Otherwise, we usually can't path, or jumping would look bad.
				if (ExpectsSuccessfulPathInCurrentState())
				{
					const FVector Destination = GetNavRecoveryDestination();

					// Destination is zero if not set yet by Behavior Tree.
					if (!Destination.IsZero())
					{
						OnTickNavMeshRecovery(CurrentSettings, DeltaTime, Location, Destination);
					}
					else
					{
						// Allow slight delay for BT to set the location.
						if (TimeSinceStart > 0.20f)
						{
							UE_LOG(LogNavRecovery, VeryVerbose, TEXT("%s: Tick recovery has no destination set from location [%s]"), *GetNameSafe(this), *Location.ToString());
							NAVRECOVERY_LOC(Location, GetCapsuleComponent()->GetScaledCapsuleRadius(), FColor::Red);
							ReceiveFailedToFindRecoveryDestination(DeltaTime, Location, TimeSinceStart);
						}
					}
				}
				else
				{
					UE_LOG(LogNavRecovery, VeryVerbose, TEXT("%s: Tick recovery skipped from location [%s] because we are in a non-default movement mode (%d)"), *GetNameSafe(this), *Location.ToString(), (int32)GetCharacterMovement()->MovementMode);
				}
			}
		}
	}
}

float AEmpathCharacter::GetDistanceToVR(const AActor* OtherActor) const
{
	// Check if the other actor is a VR character
	const AEmpathPlayerCharacter* OtherVRChar = Cast<AEmpathPlayerCharacter>(OtherActor);
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
		LastStunTime = GetWorld()->GetTimeSeconds();
		ReceiveStunned(StunInstigator, StunCauser, StunDuration);
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
	AEmpathPlayerCharacter* Player = Cast<AEmpathPlayerCharacter>(DamageCauser);
	if (!Player && EventInstigator)
	{
		Player = Cast<AEmpathPlayerCharacter>(EventInstigator->GetPawn());
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

	// By default, return 1.0f (umodified damage scale)
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
	StunDamageHistory.Add(FDamageHistoryEvent(StunDamageAmount,- GetWorld()->GetTimeSeconds()));

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
			MyMesh->SetCollisionProfileName(FEmpathCollisionrProfiles::Ragdoll);

			// We set the capsule to ignore all interactions rather than turning off collision completely, 
			// since there's a good chance we will get back up,
			//and the physics state would have to be recreated again.
			GetCapsuleComponent()->SetCollisionProfileName(FEmpathCollisionrProfiles::PawnIgnoreAll);
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

FVector AEmpathCharacter::GetPathingSourceLocation() const
{
	FVector AgentLocation = FNavigationSystem::InvalidLocation;
	if (UEmpathCharacterMovementComponent* MoveComp = Cast<UEmpathCharacterMovementComponent>(GetCharacterMovement()))
	{
		AgentLocation = MoveComp->GetPathingSourceLocation();
	}
	else
	{
		AgentLocation = Super::GetNavAgentLocation();
	}
	return AgentLocation;
}

bool AEmpathCharacter::TraceJump(float Radius, FVector StartLocation, FVector Destination, float JumpArc, float PathTracePercent, FVector& OutLaunchVelocity, EDrawDebugTrace::Type DrawDebugType, float DrawDebugDuration)
{
	FVector LaunchVel;
	bool bHit = false;
	JumpArc = FMath::Clamp(JumpArc, 0.f, 0.99f);
	Radius = FMath::Max(Radius, 0.f);

	// Check if the jump is a success
	bool bSuccess = UEmpathFunctionLibrary::SuggestProjectileVelocity(this, LaunchVel, StartLocation, Destination, JumpArc);
	if (bSuccess && !LaunchVel.IsZero())
	{
		// Calculate jump variables
		const float DistanceXY = FVector::Dist2D(StartLocation, Destination);
		const float VelocityXY = LaunchVel.Size2D();
		const float TimeToTarget = (VelocityXY > 0.f) ? DistanceXY / VelocityXY : 0.f;
		if (TimeToTarget == 0.f)
		{
			// We shouldn't really hit the case of zero horizontal velocity since we limit the jump arc limit
			OutLaunchVelocity = FVector::ZeroVector;
			return false;
		}

		PathTracePercent = FMath::Clamp(PathTracePercent, 0.f, 1.f);
		const float TraceSimTime = TimeToTarget * PathTracePercent;

		// Setup trace variables
		FPredictProjectilePathParams PredictParams(Radius, StartLocation, LaunchVel, TraceSimTime, ECC_Pawn, this);
		PredictParams.bTraceWithCollision = true;
		PredictParams.bTraceComplex = false;
		PredictParams.SimFrequency = 4.0f;
		PredictParams.DrawDebugType = DrawDebugType;
		PredictParams.DrawDebugTime = DrawDebugDuration;

		// Do the trace
		FPredictProjectilePathResult PathResult;
		const bool bHitSomething = UGameplayStatics::PredictProjectilePath(this, PredictParams, PathResult);
		bSuccess = !bHitSomething;
	}

	// Update variables and return
	OutLaunchVelocity = LaunchVel;
	return bSuccess;
}

bool AEmpathCharacter::DoJump(FVector GroundDestination, float JumpArc, float PathTracePercent, FVector& OutLaunchVelocity, EDrawDebugTrace::Type DrawDebugType, float DrawDebugDuration)
{
	bool bSuccess = false;

	float CapsuleRadius, CapsuleLinearHalfHeight;
	GetCapsuleComponent()->GetScaledCapsuleSize_WithoutHemisphere(CapsuleRadius, CapsuleLinearHalfHeight);

	const float TraceRadius = CapsuleRadius * 0.90f;
	const FVector StartLocation = GetActorLocation() - FVector(0.f, 0.f, CapsuleLinearHalfHeight);
	const FVector TraceDestination = GroundDestination + CapsuleRadius;

	const bool bTraceSuccess = TraceJump(TraceRadius, StartLocation, TraceDestination, JumpArc, PathTracePercent, OutLaunchVelocity, DrawDebugType, DrawDebugDuration);
	if (bTraceSuccess)
	{
		if (AEmpathAIController* const EmpathAI = GetEmpathAICon())
		{
			float AscendingTime, DescendingTime;
			bSuccess = EmpathAI->DoJumpLaunchWithPrecomputedVelocity(FTransform(TraceDestination), OutLaunchVelocity, JumpArc, nullptr, AscendingTime, DescendingTime);
		}
	}

	return bSuccess;
}

void AEmpathCharacter::RefreshPathingSpeedData()
{
	// Inform AI that the accel / deccel may have changed
	AEmpathAIController* const AI = GetEmpathAICon();
	if (AI)
	{
		UEmpathPathFollowingComponent* const PFC = Cast<UEmpathPathFollowingComponent>(AI->GetPathFollowingComponent());
		if (PFC)
		{
			PFC->OnDecelerationPossiblyChanged();
		}
	}
}


void AEmpathCharacter::SetWalkingSpeedData(float WalkingSpeed, float WalkingBrakingDeceleration)
{
	UCharacterMovementComponent* const CMC = GetCharacterMovement();
	if (CMC)
	{
		if (WalkingSpeed >= 0.f)
		{
			CMC->MaxWalkSpeed = WalkingSpeed;
		}

		if (WalkingBrakingDeceleration >= 0.f)
		{
			CMC->BrakingDecelerationWalking = WalkingBrakingDeceleration;
			RefreshPathingSpeedData();
		}
	}
}

bool AEmpathCharacter::ExpectsSuccessfulPathInCurrentState() const
{
	// We don't expect to move if we're limp or dead
	if (bRagdolling || bDead)
	{
		return false;
	}

	// Check if the character is off the navmesh
	if (GetCharacterMovement()->MovementMode != GetCharacterMovement()->DefaultLandMovementMode ||
		GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Falling)
	{
		return false;
	}

	return true;
}

void AEmpathCharacter::OnRequestingPath_Implementation()
{
	bPathRequestActive = true;
	bPathRequestSuccessful = false;
	UE_LOG(LogNavRecovery, VeryVerbose, TEXT("%s: Requesting path from location [%s]"), *GetNameSafe(this), *GetActorLocation().ToString());
}

void AEmpathCharacter::OnPathRequestSuccess_Implementation(FVector GoalLocation)
{
	// Note that we ignore the ExpectsSuccessfulPathInCurrentState(), 
	// because it could have been a success made when about to land, 
	// and we automatically fail when falling. 
	// We want to avoid counting failures when we wouldn't expect success.
	if (bFailedNavigation)
	{
		// Normally we are triggered by a successful MoveTo which already sets this,
		// but anims might want it and it won't be set from some recovery logic 
		// (such as jumps).
		AEmpathAIController* const AI = Cast<AEmpathAIController>(GetController());
		if (AI)
		{
			AI->SetGoalLocation(GoalLocation);
		}
		// Draw from current location to the goal when we recover
		NAVRECOVERY_LOC_DURATION(GetActorLocation(), GetCapsuleComponent()->GetScaledCapsuleRadius(), FColor::Green);
		NAVRECOVERY_LOC_DURATION(GoalLocation, GetCapsuleComponent()->GetScaledCapsuleRadius(), FColor::Green);
		NAVRECOVERY_LINE(GetActorLocation(), GoalLocation, FColor::Green);
	}

	// Reset the recovery state
	ClearNavRecoveryState();
	UE_LOG(LogNavRecovery, VeryVerbose, TEXT("%s: Path request success! Location [%s], goal [%s]"), *GetNameSafe(this), *GetActorLocation().ToString(), *GoalLocation.ToString());
}


bool AEmpathCharacter::OnPathRequestFailed_Implementation(FVector GoalLocation)
{
	// Reset state variables
	bPathRequestActive = false;
	bPathRequestSuccessful = false;

	// If we are not using nav recovery, do nothing
	if (!bShouldUseNavRecovery)
	{
		return false;
	}

	// If it is already being handled, we don't need to restart recovery.
	if (bFailedNavigation)
	{
		UE_LOG(LogNavRecovery, Warning, TEXT("%s: Notified failing nav mesh while trying to recover. Should not be trying to path while recovering."), *GetNameSafe(this));
		return false;
	}

	// Path failures while unable to path should be ignored.
	if (!ExpectsSuccessfulPathInCurrentState())
	{
		// Reset failure tracking.
		UE_LOG(LogNavRecovery, VeryVerbose, TEXT("%s: Ignoring failed Path request at location [%s], goal [%s] because path success is not expected."), *GetNameSafe(this), *GetActorLocation().ToString(), *GoalLocation.ToString());
		ClearNavRecoveryState();
		return false;
	}

	UWorld* World = GetWorld();
	if (!ensure(World != nullptr))
	{
		return false;
	}

	UE_LOG(LogNavRecovery, Verbose, TEXT("%s: Path request failed at location [%s], goal [%s]"), *GetNameSafe(this), *GetActorLocation().ToString(), *GoalLocation.ToString());
	NAVRECOVERY_LOC(GetActorLocation(), GetCapsuleComponent()->GetScaledCapsuleRadius(), FColor::Red);
	NAVRECOVERY_LOC(GoalLocation, GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.95f, FColor::Orange);
	NAVRECOVERY_LINE(GetActorLocation(), GoalLocation, FColor::Orange);

	// Update tracking
	NavFailureCurrentCount++;
	if (NavFailureFirstFailTime == 0.f)
	{
		NavFailureFirstFailTime = World->GetTimeSeconds();
	}

	// Check if it's time to switch to regaining.
	bool bShouldStartNavRecovery = false;
	const FEmpathNavRecoverySettings& CurrentSettings = GetCurrentNavRecoverySettings();
	if (CurrentSettings.FailureCountUntilRecovery > 0 && NavFailureCurrentCount >= CurrentSettings.FailureCountUntilRecovery)
	{
		bShouldStartNavRecovery = true;
	}
	else if (CurrentSettings.FailureTimeUntilRecovery > 0 && World->TimeSince(NavFailureFirstFailTime) >= CurrentSettings.FailureTimeUntilRecovery)
	{
		bShouldStartNavRecovery = true;
	}

	// If we are off navmesh, go to failure anyway
	FVector ProjectedPoint;
	const bool bOnNavMesh = UEmpathFunctionLibrary::EmpathProjectPointToNavigation(this, ProjectedPoint, GetActorLocation(), nullptr, nullptr, NavRecoveryTestExtent);
	if (bOnNavMesh == false)
	{
		bShouldStartNavRecovery = true;
	}
	else
	{
		// We are on navmesh. Can't try to do recovery if we only handle being off mesh.
		if (NavRecoveryAbility == EEmpathNavRecoveryAbility::OffNavMesh)
		{
			UE_LOG(LogNavRecovery, Verbose, TEXT("%s: Will not try nav recovery from location [%s], goal [%s] because we are on mesh and can only handle being off the mesh."), *GetNameSafe(this), *GetActorLocation().ToString(), *GoalLocation.ToString());
			bShouldStartNavRecovery = false;
		}
	}

	// Start if requested
	if (bShouldStartNavRecovery)
	{
		OnStartNavRecovery(GoalLocation, bOnNavMesh);
	}

	return bFailedNavigation;
}

void AEmpathCharacter::OnStartNavRecovery_Implementation(FVector FailedGoalLocation, bool bCurrentlyOnNavMesh)
{
	// Initialize nav recovery state variables
	ClearNavRecoveryState();
	bFailedNavigation = true;
	bFailedNavigationStartedOnNavMesh = bCurrentlyOnNavMesh;
	bPathRequestActive = false;
	bPathRequestSuccessful = false;
	NavRecoveryStartTime = GetWorld()->GetTimeSeconds();
	NavRecoveryStartActorLocation = GetActorLocation();
	NavRecoveryStartPathingLocation = GetPathingSourceLocation();
	NavRecoveryFailedGoalLocation = FailedGoalLocation;

	// Signal our AI to update its search radius. 
	// The AI should run an EQS query from the blackboard
	AEmpathAIController* const AI = Cast<AEmpathAIController>(GetController());
	if (AI)
	{
		const FEmpathNavRecoverySettings& CurrentSettings = GetCurrentNavRecoverySettings();
		AI->SetNavRecoverySearchRadii(CurrentSettings.SearchInnerRadius, CurrentSettings.SearchOuterRadius);
	}

	UE_LOG(LogNavRecovery, Warning, TEXT("%s: Failed Navigation, starting recovery from location [%s], goal [%s] (dist=%.2f, dist2D=%.2f) StartedOnMesh=%d"),
		*GetNameSafe(this), *NavRecoveryStartActorLocation.ToString(), *FailedGoalLocation.ToString(), (NavRecoveryStartActorLocation - FailedGoalLocation).Size(), (NavRecoveryStartActorLocation - FailedGoalLocation).Size2D(), bFailedNavigationStartedOnNavMesh);
	NAVRECOVERY_LOC_DURATION(NavRecoveryStartActorLocation, GetCapsuleComponent()->GetScaledCapsuleRadius(), FColor::Red);
	NAVRECOVERY_LOC_DURATION(FailedGoalLocation, GetCapsuleComponent()->GetScaledCapsuleRadius(), FColor::Red);
	NAVRECOVERY_LINE_DURATION(NavRecoveryStartActorLocation, FailedGoalLocation, FColor::Red);
}

void AEmpathCharacter::OnNavMeshRecovered_Implementation()
{
	if (bFailedNavigation)
	{
		UE_LOG(LogNavRecovery, Warning, TEXT("%s: Regained nav mesh! At location [%s], goal was [%s] (dist=%.2f, dist2D=%.2f) StartedOnMesh=%d"),
			*GetNameSafe(this), *GetActorLocation().ToString(), *GetNavRecoveryDestination().ToString(), (GetActorLocation() - GetNavRecoveryDestination()).Size(), (GetActorLocation() - GetNavRecoveryDestination()).Size2D(), bFailedNavigationStartedOnNavMesh);
		NAVRECOVERY_LOC_DURATION(GetActorLocation(), GetCapsuleComponent()->GetScaledCapsuleRadius(), FColor::Green);
		
		// End the lost nav mesh state flow
		bFailedNavigation = false;
		NavRecoveryCounter = 0;
		OnPathRequestSuccess(GetActorLocation());
	}
	else
	{
		UE_LOG(LogNavRecovery, Warning, TEXT("%s: Character was flagged as having regained the navmesh when navmesh was not believed to be lost."), *GetNameSafe(this));
	}
}

void AEmpathCharacter::ReceiveNavMeshRecoveryFailed_Implementation(FVector Location, float TimeSinceStartRecovery, bool bWasOnNavMeshAtStart)
{
	// By default, restart the process and clear the failure in case normal movement works again.
	ClearNavRecoveryState();
}

void AEmpathCharacter::ClearNavRecoveryState()
{
	bFailedNavigation = false;
	bFailedNavigationStartedOnNavMesh = false;
	bPathRequestActive = false;
	bPathRequestSuccessful = true;
	NavRecoveryStartActorLocation = FVector::ZeroVector;
	NavRecoveryStartPathingLocation = FVector::ZeroVector;
	NavFailureCurrentCount = 0;
	NavFailureFirstFailTime = 0.f;
	NavRecoveryCounter = 0;

	AEmpathAIController* const AI = GetEmpathAICon();
	if (AI)
	{
		AI->ClearNavRecoveryDestination();
	}
}

void AEmpathCharacter::ReceiveTickNavMeshRecovery_Implementation(float DeltaTime, FVector Location, FVector RecoveryDestination)
{
	// By default, jump to the location
	float JumpArc = FMath::RandRange(FMath::Min(NavRecoveryJumpArcMin, NavRecoveryJumpArcMax),
		FMath::Max(NavRecoveryJumpArcMax, NavRecoveryJumpArcMin));
	FVector JumpVector = FVector::ZeroVector;
	DoJump(RecoveryDestination, JumpArc, 0.95f, JumpVector, EDrawDebugTrace::None, 0.0f);
}

FVector AEmpathCharacter::GetNavRecoveryDestination() const
{
	AEmpathAIController* const AI = GetEmpathAICon();
	if (AI)
	{
		return AI->GetNavRecoveryDestination();
	}
	return FVector::ZeroVector;
}

void AEmpathCharacter::GetNavSearchRadiiCurrent(float& CurrentInnerRadius, float& CurrentOuterRadius) const
{
	AEmpathAIController* const AI = GetEmpathAICon();
	if (AI)
	{
		AI->GetNavRecoverySearchRadii(CurrentInnerRadius, CurrentOuterRadius);
	}
	else
	{
		GetNavSearchRadiiDefault(CurrentInnerRadius, CurrentOuterRadius);
	}
}

void AEmpathCharacter::GetNavSearchRadiiDefault(float& DefaultInnerRadius, float& DefaultOuterRadius) const
{
	const FEmpathNavRecoverySettings& CurrentSettings = GetCurrentNavRecoverySettings();
	DefaultInnerRadius = CurrentSettings.SearchInnerRadius;
	DefaultOuterRadius = CurrentSettings.SearchOuterRadius;
}

void AEmpathCharacter::SetNavSearchRadiiCurrent(float NewInnerRadius, float NewOuterRadius)
{
	AEmpathAIController* const AI = Cast<AEmpathAIController>(GetController());
	if (AI)
	{
		AI->SetNavRecoverySearchRadii(NewInnerRadius, NewOuterRadius);
	}
}

void AEmpathCharacter::ExpandNavSearchRadiiCurrent(float InnerGrowth, float OuterGrowth)
{
	// Do not allow the inner to outgrow the outer
	InnerGrowth = FMath::Min(InnerGrowth, OuterGrowth);
	if (InnerGrowth >= 0.f && OuterGrowth >= 0.f)
	{
		float CurrentInnerRadius;
		float CurrentOuterRadius;
		GetNavSearchRadiiCurrent(CurrentInnerRadius, CurrentOuterRadius);

		const float NewInnerRadius = CurrentInnerRadius + InnerGrowth;
		const float NewOuterRadius = CurrentOuterRadius + OuterGrowth;

		UE_LOG(LogNavRecovery, VeryVerbose, TEXT("%s: Growing search radii [%.2f, %.2f] -> [%.2f, %.2f]"), *GetNameSafe(this), CurrentInnerRadius, CurrentOuterRadius, NewInnerRadius, NewOuterRadius);
		SetNavSearchRadiiCurrent(NewInnerRadius, NewOuterRadius);
	}
}

const FEmpathNavRecoverySettings& AEmpathCharacter::GetCurrentNavRecoverySettings() const
{
	if (IsFailingNavigationFromValidNavMesh() && (NavRecoveryAbility == EEmpathNavRecoveryAbility::OnNavMeshIsland))
	{
		return NavRecoverySettingsOnIsland;
	}
	else
	{
		return NavRecoverySettingsOffMesh;
	}
}

bool AEmpathCharacter::IsFailingNavigationFromValidNavMesh() const
{
	return bFailedNavigation && bFailedNavigationStartedOnNavMesh;
}


void AEmpathCharacter::SetNavRecoveryDestination(FVector Destination)
{
	AEmpathAIController* const AI = Cast<AEmpathAIController>(GetController());
	if (AI)
	{
		AI->SetNavRecoveryDestination(Destination);
	}
}

void AEmpathCharacter::OnTickNavMeshRecovery(FEmpathNavRecoverySettings const& CurrentSettings, float DeltaTime, FVector Location, FVector RecoveryDestination)
{
	ensure(IsFailingNavigation());
	ensure(!RecoveryDestination.IsZero());

	// Try automatic recovery methods if requested.
	bool bRecovered = false;

	FVector ProjectedPoint = FVector::ZeroVector;
	if ((NavRecoveryAbility == EEmpathNavRecoveryAbility::OffNavMesh) || (IsFailingNavigationFromValidNavMesh() == false))
	{
		// Can only recover from off nav mesh, or we started off navmesh so just want to get there.
		bRecovered = UEmpathFunctionLibrary::EmpathProjectPointToNavigation(this, ProjectedPoint, Location, nullptr, nullptr, NavRecoveryTestExtent);
	}
	else if (NavRecoveryAbility == EEmpathNavRecoveryAbility::OnNavMeshIsland)
	{
		// Recovered if we can now path to the desired goal location (that was off the island).
		ProjectedPoint = GetPathingSourceLocation();
		bRecovered = UEmpathFunctionLibrary::EmpathHasPathToLocation(this, RecoveryDestination);
	}
	else
	{
		// Do nothing.
	}

	// See if we recovered.
	if (bRecovered)
	{
		NAVRECOVERY_LOC_DURATION(ProjectedPoint, GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.85f, FColor::Green);
		NAVRECOVERY_LOC_DURATION(RecoveryDestination, GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.85f, FColor::Green);
		NAVRECOVERY_LINE_DURATION(ProjectedPoint, RecoveryDestination, FColor::Green);
		OnNavMeshRecovered();
	}
	else
	{
		if (CurrentSettings.NavRecoverySkipFrames <= 0 || (NavRecoveryCounter % (CurrentSettings.NavRecoverySkipFrames + 1)) == 0)
		{
			UE_LOG(LogNavRecovery, VeryVerbose, TEXT("%s: Tick recovery from location [%s] -> [%s] (dist=%.2f, dist2D=%.2f) StartedOnMesh=%d"),
				*GetNameSafe(this), *Location.ToString(), *RecoveryDestination.ToString(), (Location - RecoveryDestination).Size(), (Location - RecoveryDestination).Size2D(), bFailedNavigationStartedOnNavMesh);
			NAVRECOVERY_LOC(Location, GetCapsuleComponent()->GetScaledCapsuleRadius(), FColor::Cyan);
			NAVRECOVERY_LOC(RecoveryDestination, GetCapsuleComponent()->GetScaledCapsuleRadius(), FColor::Cyan);
			NAVRECOVERY_LINE(Location, RecoveryDestination, FColor::Cyan);

			ReceiveTickNavMeshRecovery(DeltaTime, Location, RecoveryDestination);
		}
		else
		{
			UE_LOG(LogNavRecovery, VeryVerbose, TEXT("%s: Skipped recovery from location [%s] -> [%s] (dist=%.2f, dist2D=%.2f) StartedOnMesh=%d"),
				*GetNameSafe(this), *Location.ToString(), *RecoveryDestination.ToString(), (Location - RecoveryDestination).Size(), (Location - RecoveryDestination).Size2D(), bFailedNavigationStartedOnNavMesh);
		}
		NavRecoveryCounter++;
	}
}

void AEmpathCharacter::OnNavMeshRecoveryFailed(FVector Location, float TimeSinceRecoveryStart)
{
	UE_LOG(LogNavRecovery, Warning, TEXT("%s: Recovery has FAILED from location [%s] StartedOnMesh=%d"), *GetNameSafe(this), *Location.ToString(), bFailedNavigationStartedOnNavMesh);
	ReceiveNavMeshRecoveryFailed(Location, TimeSinceRecoveryStart, bFailedNavigationStartedOnNavMesh);
}

void AEmpathCharacter::ReceiveFailedToFindRecoveryDestination_Implementation(float DeltaTime, FVector Location, float TimeSinceStartRecovery)
{
	// Default is to expand out the search radii
	const FEmpathNavRecoverySettings& CurrentSettings = GetCurrentNavRecoverySettings();
	const float InnerGrowth = FMath::Max(0.f, CurrentSettings.SearchRadiusGrowthRateInner * DeltaTime);
	const float OuterGrowth = FMath::Max(0.f, CurrentSettings.SearchRadiusGrowthRateOuter * DeltaTime);
	ExpandNavSearchRadiiCurrent(InnerGrowth, OuterGrowth);
}