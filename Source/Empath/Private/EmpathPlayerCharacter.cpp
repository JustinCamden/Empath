// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathPlayerCharacter.h"
#include "EmpathPlayerController.h"
#include "EmpathDamageType.h"
#include "EmpathFunctionLibrary.h"
#include "EmpathHandActor.h"
#include "EmpathTeleportBeacon.h"
#include "AI/Navigation/NavigationData.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "EmpathCharacter.h"
#include "DrawDebugHelpers.h"
#include "EmpathTeleportMarker.h"
#include "Runtime/HeadMountedDisplay/Public/HeadMountedDisplayFunctionLibrary.h"

// Stats for UE Profiler
DECLARE_CYCLE_STAT(TEXT("Empath VR Char Take Damage"), STAT_EMPATH_TakeDamage, STATGROUP_EMPATH_VRCharacter);
DECLARE_CYCLE_STAT(TEXT("Empath VR Char Teleport Trace"), STAT_EMPATH_TraceTeleport, STATGROUP_EMPATH_VRCharacter);

// Log categories
DEFINE_LOG_CATEGORY_STATIC(LogTeleportTrace, Log, All);

// Console variable setup so we can enable and disable debugging from the console
static TAutoConsoleVariable<int32> CVarEmpathTeleportDrawDebug(
	TEXT("Empath.TeleportDrawDebug"),
	0,
	TEXT("Whether to enable teleportation debug.\n")
	TEXT("0: Disabled, 1: Enabled"),
	ECVF_Scalability | ECVF_RenderThreadSafe);
static const auto TeleportDrawDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("Empath.TeleportDrawDebug"));


static TAutoConsoleVariable<float> CVarEmpathTeleportDebugLifetime(
	TEXT("Empath.TeleportDebugLifetime"),
	0.04f,
	TEXT("Duration of debug drawing for teleporting, in seconds."),
	ECVF_Scalability | ECVF_RenderThreadSafe);
static const auto TeleportDebugLifetime = IConsoleManager::Get().FindConsoleVariable(TEXT("Empath.TeleportDebugLifetime"));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define TELEPORT_LOC(_Loc, _Radius, _Color)				if (TeleportDrawDebug->GetInt()) { DrawDebugSphere(GetWorld(), _Loc, _Radius, 16, _Color, false, -1.0f, 0, 3.0f); }
#define TELEPORT_LINE(_Loc, _Dest, _Color)				if (TeleportDrawDebug->GetInt()) { DrawDebugLine(GetWorld(), _Loc, _Dest, _Color, false,  -1.0f, 0, 3.0f); }
#define TELEPORT_LOC_DURATION(_Loc, _Radius, _Color)	if (TeleportDrawDebug->GetInt()) { DrawDebugSphere(GetWorld(), _Loc, _Radius, 16, _Color, false, TeleportDebugLifetime->GetFloat(), 0, 3.0f); }
#define TELEPORT_LINE_DURATION(_Loc, _Dest, _Color)		if (TeleportDrawDebug->GetInt()) { DrawDebugLine(GetWorld(), _Loc, _Dest, _Color, false, TeleportDebugLifetime->GetFloat(), 0, 3.0f); }
#define TELEPORT_TRACE_DEBUG_TYPE(_Params)				if (TeleportDrawDebug->GetInt()) { _Params.DrawDebugType = EDrawDebugTrace::ForOneFrame; }
#else
#define TELEPORT_LOC(_Loc, _Radius, _Color)				/* nothing */
#define TELEPORT_LINE(_Loc, _Dest, _Color)				/* nothing */
#define TELEPORT_LOC_DURATION(_Loc, _Radius, _Color)	/* nothing */
#define TELEPORT_LINE_DURATION(_Loc, _Dest, _Color)		/* nothing */
#define TELEPORT_TRACE_DEBUG_TYPE()						/* nothing */
#endif

AEmpathPlayerCharacter::AEmpathPlayerCharacter(const FObjectInitializer& ObjectInitializer)
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
	TeleportMagnitude = 1500.0f;
	DashMagnitude = 800.0f;
	TeleportRadius = 4.0f;
	TeleportVelocityLerpSpeed = 20.0f;
	TeleportBeaconMinDistance = 20.0f;
	TeleportProjectQueryExtent = FVector(150.0f, 150.0f, 150.0f);
	TeleportTraceObjectTypes.Add((TEnumAsByte<EObjectTypeQuery>)ECC_WorldStatic); // World Statics
	TeleportTraceObjectTypes.Add((TEnumAsByte<EObjectTypeQuery>)ECC_Teleport); // Teleport channel
	InputAxisEventThreshold = 0.6f;
	InputAxisLocomotionWalkThreshold = 0.2f;
	TeleportHand = EEmpathBinaryHand::Right;
	DashTraceSettings.bTraceForBeacons = false;
	DashTraceSettings.bTraceForEmpathChars = false;
	TeleportMovementSpeed = 5000.0f;
	TeleportRotationSpeed = 720.0f;
	TeleportTimoutTime = 0.5f;
	TeleportDistTolerance = 5.0f;
	TeleportRotTolerance = 5.0f;
	TeleportPivotStep = 60.0f;
	DefaultLocomotionMode = EEmpathLocomotionMode::Dash;
	DashOrientation = EEmpathOrientation::Head;
	WalkOrientation = EEmpathOrientation::Hand;

	// Initialize damage capsule
	DamageCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("DamageCapsule"));
	DamageCapsule->SetupAttachment(VRReplicatedCamera);
	DamageCapsule->SetCapsuleHalfHeight(30.0f);
	DamageCapsule->SetCapsuleRadius(8.5f);
	DamageCapsule->SetRelativeLocation(FVector(1.75f, 0.0f, -18.5f));
	DamageCapsule->SetCollisionProfileName(FEmpathCollisionProfiles::DamageCollision);
	DamageCapsule->SetEnableGravity(false);

	// Set default class type
	RightHandClass = AEmpathHandActor::StaticClass();
	LeftHandClass = AEmpathHandActor::StaticClass();
	TeleportMarkerClass = AEmpathTeleportMarker::StaticClass();
}

void AEmpathPlayerCharacter::SetupPlayerInputComponent(class UInputComponent* NewInputComponent)
{
	Super::SetupPlayerInputComponent(NewInputComponent);

	// Dash
	NewInputComponent->BindAxis("LocomotionAxisUpDown", this, &AEmpathPlayerCharacter::LocomotionAxisUpDown);
	NewInputComponent->BindAxis("LocomotionAxisRightLeft", this, &AEmpathPlayerCharacter::LocomotionAxisRightLeft);

	// Teleport
	NewInputComponent->BindAxis("TeleportAxisUpDown", this, &AEmpathPlayerCharacter::TeleportAxisUpDown);
	NewInputComponent->BindAxis("TeleportAxisRightLeft", this, &AEmpathPlayerCharacter::TeleportAxisRightLeft);

	// Alt movement
	NewInputComponent->BindAction("AltMovementMode", IE_Pressed, this, &AEmpathPlayerCharacter::OnAltMovementPressed);
	NewInputComponent->BindAction("AltMovementMode", IE_Released, this, &AEmpathPlayerCharacter::OnAltMovementReleased);

	// Grip
	NewInputComponent->BindAction("GripRight", IE_Pressed, this, &AEmpathPlayerCharacter::OnGripRightPressed);
	NewInputComponent->BindAction("GripRight", IE_Released, this, &AEmpathPlayerCharacter::OnGripRightReleased);
	NewInputComponent->BindAction("GripLeft", IE_Pressed, this, &AEmpathPlayerCharacter::OnGripLeftPressed);
	NewInputComponent->BindAction("GripLeft", IE_Released, this, &AEmpathPlayerCharacter::OnGripLeftReleased);
}

void AEmpathPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Spawn and attach hands if their classes are set
	// Variables
	FTransform SpawnTransform;
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	FAttachmentTransformRules SpawnAttachRules(EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		EAttachmentRule::SnapToTarget,
		false);

	// Right hand
	if (RightHandClass)
	{
		SpawnTransform = RightMotionController->GetComponentTransform();
		RightHandActor = GetWorld()->SpawnActor<AEmpathHandActor>(RightHandClass, SpawnTransform, SpawnParams);
		RightHandActor->AttachToComponent(RootComponent, SpawnAttachRules);
	}

	// Left hand
	if (LeftHandClass)
	{
		SpawnTransform = RightMotionController->GetComponentTransform();
		LeftHandActor = GetWorld()->SpawnActor<AEmpathHandActor>(LeftHandClass, SpawnTransform, SpawnParams);
		LeftHandActor->AttachToComponent(RootComponent, SpawnAttachRules);
	}

	// Register hands
	if (RightHandActor)
	{
		RightHandActor->RegisterHand(LeftHandActor, this, RightMotionController, EEmpathBinaryHand::Right);
	}
	if (LeftHandActor)
	{
		LeftHandActor->RegisterHand(RightHandActor, this, LeftMotionController, EEmpathBinaryHand::Left);
	}

	// Spawn, attach, and hide the teleport marker
	if (TeleportMarkerClass)
	{
		SpawnTransform = GetTransform();
		TeleportMarker = GetWorld()->SpawnActor<AEmpathTeleportMarker>(TeleportMarkerClass, SpawnTransform, SpawnParams);
		TeleportMarker->OwningCharacter = this;
		TeleportMarker->AttachToComponent(RootComponent, SpawnAttachRules);
		TeleportMarker->SetActorHiddenInGame(true);
	}

	// Cache the player navmeshes
	CacheNavMesh();

	// Ensure we start on the default locomotion mode
	CurrentLocomotionMode = DefaultLocomotionMode;


	EHMDTrackingOrigin::Type TrackingOrigin = EHMDTrackingOrigin::Type::Floor;
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(TrackingOrigin);
}

void AEmpathPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	TickUpdateHealthRegen(DeltaTime);
	TickUpdateInputAxisEvents();
	TickUpdateTeleportState(DeltaTime);
	TickUpdateWalk();
}

float AEmpathPlayerCharacter::GetDistanceToVR(AActor* OtherActor) const
{
	return (GetVRLocation() - OtherActor->GetActorLocation()).Size();
}

EEmpathTeam AEmpathPlayerCharacter::GetTeamNum_Implementation() const
{
	// We should always return player for VR characters
	return EEmpathTeam::Player;
}


FVector AEmpathPlayerCharacter::GetCustomAimLocationOnActor_Implementation(FVector LookOrigin, FVector LookDirection) const
{
	return DamageCapsule->GetComponentLocation();
}

FVector AEmpathPlayerCharacter::GetScaledHeightLocation(float HeightScale)
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

FVector AEmpathPlayerCharacter::GetPawnViewLocation() const
{
	if (VRReplicatedCamera)
	{
		return VRReplicatedCamera->GetComponentLocation();
	}
	return Super::GetPawnViewLocation();
}

FRotator AEmpathPlayerCharacter::GetViewRotation() const
{
	if (VRReplicatedCamera)
	{
		return VRReplicatedCamera->GetComponentRotation();
	}
	return Super::GetViewRotation();
}

AEmpathPlayerController* AEmpathPlayerCharacter::GetEmpathPlayerCon() const
{
	if (EmpathPlayerController)
	{
		return EmpathPlayerController;
	}
	return Cast<AEmpathPlayerController>(GetController());
}

void AEmpathPlayerCharacter::TeleportToLocation(FVector Destination)
{
	// Do nothing if we are already teleporting
	if (IsTeleporting())
	{
		return;
	}

	// Cache origin
	FVector Origin = GetVRLocation();

	// Update teleport state
	SetTeleportState(EEmpathTeleportState::TeleportingToLocation);
	TeleportActorLocationGoal = GetTeleportLocation(TeleportCurrentLocation);
	TeleportLastStartTime = GetWorld()->GetTimeSeconds();
	VRRootReference->SetEnableGravity(false);
	VRMovementReference->SetMovementMode(MOVE_None);

	// Hide the teleport trace
	HideTeleportTrace();

	// Temporarily disable collision
	TArray<AActor*> ControlledActors = GetControlledActors();
	for (AActor* CurrentActor : ControlledActors)
	{
		CurrentActor->SetActorEnableCollision(false);
	}
	
	// Fire notifies
	ReceiveTeleportToLocation(Origin, Destination);
	OnTeleport.Broadcast(this, Origin, Destination);
 
	return;
}

bool AEmpathPlayerCharacter::CanDie_Implementation() const
{
	return (!bInvincible && !bDead);
}

void AEmpathPlayerCharacter::Die(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType)
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

float AEmpathPlayerCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
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
		LastDamageTime = GetWorld()->GetTimeSeconds();
		ProcessFinalDamage(ActualDamage, HitInfo, HitImpulseDir, DamageTypeCDO, EventInstigator, DamageCauser);
		return ActualDamage;
	}
	return 0.0f;
}

float AEmpathPlayerCharacter::ModifyAnyDamage_Implementation(float DamageAmount, const AController* EventInstigator, const AActor* DamageCauser, const class UDamageType* DamageType)
{
	return DamageAmount;
}

float AEmpathPlayerCharacter::ModifyPointDamage_Implementation(float DamageAmount, const FHitResult& HitInfo, const FVector& ShotDirection, const AController* EventInstigator, const AActor* DamageCauser, const class UDamageType* DamageTypeCDO)
{
	return DamageAmount;
}

float AEmpathPlayerCharacter::ModifyRadialDamage_Implementation(float DamageAmount, const FVector& Origin, const TArray<struct FHitResult>& ComponentHits, float InnerRadius, float OuterRadius, const AController* EventInstigator, const AActor* DamageCauser, const class UDamageType* DamageTypeCDO)
{
	return DamageAmount;
}

void AEmpathPlayerCharacter::ProcessFinalDamage_Implementation(const float DamageAmount, FHitResult const& HitInfo, FVector HitImpulseDir, const UDamageType* DamageType, const AController* EventInstigator, const AActor* DamageCauser)
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

bool AEmpathPlayerCharacter::CanBeStunned_Implementation() const
{
	return (bStunnable
		&& !bDead
		&& !bStunned
		&& GetWorld()->TimeSince(LastStunTime) > StunImmunityTimeAfterStunRecovery);
}

void AEmpathPlayerCharacter::TakeStunDamage(const float StunDamageAmount, const AController* EventInstigator, const AActor* DamageCauser)
{
	// Log stun event
	StunDamageHistory.Add(FEmpathDamageHistoryEvent(StunDamageAmount, -GetWorld()->GetTimeSeconds()));

	// Clean old stun events. They are stored oldest->newest, so we can just iterate to find 
	// the transition point. This plus the next loop will still constitute at most one pass 
	// through the array.

	int32 NumToRemove = 0;
	for (int32 Idx = 0; Idx < StunDamageHistory.Num(); ++Idx)
	{
		FEmpathDamageHistoryEvent& DHE = StunDamageHistory[Idx];
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
	// Just add up and stun if necessary. This way we don't have to process on Tick.
	float AccumulatedDamage = 0.f;
	for (FEmpathDamageHistoryEvent& DHE : StunDamageHistory)
	{
		AccumulatedDamage += DHE.DamageAmount;
		if (AccumulatedDamage > StunDamageThreshold)
		{
			BeStunned(EventInstigator, DamageCauser, StunDurationDefault);
			break;
		}
	}
}

void AEmpathPlayerCharacter::BeStunned(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration)
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

void AEmpathPlayerCharacter::ReceiveStunned_Implementation(const AController* StunInstigator, const AActor* StunCauser, const float StunDuration)
{
	GetWorldTimerManager().ClearTimer(StunTimerHandle);
	if (StunDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(StunTimerHandle, this, &AEmpathPlayerCharacter::EndStun, StunDuration, false);
	}
}

void AEmpathPlayerCharacter::EndStun()
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

bool AEmpathPlayerCharacter::TraceTeleportLocation(FVector Origin, FVector Direction, float Magnitude, FEmpathTeleportTraceSettings TraceSettings, bool bInterpolateMagnitude)
{
	// Declare scope cycle for profiler
	SCOPE_CYCLE_COUNTER(STAT_EMPATH_TraceTeleport);

	// Update the velocity
	if (bInterpolateMagnitude)
	{
		TeleportCurrentVelocity = FMath::VInterpTo(TeleportCurrentVelocity,
			Direction.GetSafeNormal() * Magnitude,
			GetWorld()->GetDeltaSeconds(),
			TeleportVelocityLerpSpeed);
	}
	else
	{
		TeleportCurrentVelocity = Direction.GetSafeNormal() * Magnitude;
	}

	// Trace for any teleport locations we may already be inside of,
	// and remove them from our actual teleport trace
	TArray<AEmpathTeleportBeacon*> OverlappingTeleportTargets;
	TArray<FHitResult> BeaconTraceHits;
	const FVector BeaconTraceOrigin = Origin;
	const FVector BeaconTraceEnd = Origin + (Direction.GetSafeNormal() * TeleportBeaconMinDistance);
	FCollisionQueryParams BeaconTraceParams(FName(TEXT("BeaconTrace")), false, this);
	if (GetWorld()->LineTraceMultiByChannel(BeaconTraceHits, BeaconTraceOrigin, BeaconTraceEnd, ECC_Teleport, BeaconTraceParams))
	{
		// Add each overlapping beacon to the trace ignore list
		for (FHitResult BeaconTraceResult : BeaconTraceHits)
		{
			AEmpathTeleportBeacon* OverlappingBeacon = Cast<AEmpathTeleportBeacon>(BeaconTraceResult.Actor.Get());
			if (OverlappingBeacon)
			{
				OverlappingTeleportTargets.Add(OverlappingBeacon);
			}
		}
	}
	
	// Initialize and setup trace parameters
	FPredictProjectilePathParams TeleportTraceParams = FPredictProjectilePathParams(TeleportRadius, Origin, TeleportCurrentVelocity, 3.0f);
	TeleportTraceParams.bTraceWithCollision = true;
	TeleportTraceParams.bTraceWithChannel = true;
	TeleportTraceParams.ObjectTypes.Append(TeleportTraceObjectTypes);
	TeleportTraceParams.ActorsToIgnore.Append(GetControlledActors());
	TeleportTraceParams.ActorsToIgnore.Append(OverlappingTeleportTargets);
	TELEPORT_TRACE_DEBUG_TYPE(TeleportTraceParams)

	// Do the trace and update variables
	FPredictProjectilePathResult TraceResult;
	bool TraceHit = UGameplayStatics::PredictProjectilePath(this, TeleportTraceParams, TraceResult);
	TeleportTraceSplinePositions.Empty(TraceResult.PathData.Num());
	for (const FPredictProjectilePathPointData& PathPoint : TraceResult.PathData)
	{
		TeleportTraceSplinePositions.Add(PathPoint.Location);
	}
	
	// Check if the hit location is valid
	if (TraceHit)
	{
		TELEPORT_LOC(TraceResult.HitResult.ImpactPoint, 20.0f, FColor::Yellow)
		bool bNewIsValid = IsTeleportTraceValid(TraceResult.HitResult, Origin, TraceSettings);
		UpdateTeleportCurrLocValid(bNewIsValid);

		// Draw debug shapes if appropriate
		if (bNewIsValid)
		{
			UE_LOG(LogTeleportTrace, VeryVerbose, TEXT("%s: Teleport trace succeeded."), *GetNameSafe(this));
			TELEPORT_LOC(TeleportCurrentLocation, 25.0f, FColor::Green)
			TELEPORT_LINE(Origin, TeleportCurrentLocation, FColor::Green)
		}
		else
		{
			UE_LOG(LogTeleportTrace, VeryVerbose, TEXT("%s: Teleport trace failed."), *GetNameSafe(this));
			TELEPORT_LOC_DURATION(TraceResult.HitResult.ImpactPoint, 25.0f, FColor::Red)
			TELEPORT_LINE_DURATION(Origin, TraceResult.HitResult.ImpactPoint, FColor::Red)
		}
	}
	else
	{
		UpdateTeleportCurrLocValid(false);
	}

	return bIsTeleportCurrLocValid;
}

bool AEmpathPlayerCharacter::IsTeleportTraceValid(FHitResult TeleportHit, FVector TeleportOrigin, FEmpathTeleportTraceSettings TraceSettings)
{
	// Return false if there was an initial overlap or no trace hit
	if (TeleportHit.bStartPenetrating)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("%s: Teleport trace began overlapping [%s] "), *GetNameSafe(this), *GetNameSafe(TeleportHit.Actor.Get()));
		return false;
	}

	// Cache the actor hit
	AActor* HitActor = TeleportHit.Actor.Get();

	// Check if the object is a teleport beacon
	if (TraceSettings.bTraceForBeacons)
	{
		AEmpathTeleportBeacon* NewTeleportBeacon = Cast<AEmpathTeleportBeacon>(HitActor);
		if (NewTeleportBeacon)
		{
			// Update highlighting on new and old beacons
			if (TargetTeleportBeacon && TargetTeleportBeacon != NewTeleportBeacon)
			{
				TargetTeleportBeacon->OnReleasedForTeleport();
				OnTeleportBeaconReleased();
			}


			// If we can get a valid position from the teleport beacon, go with that
			if (NewTeleportBeacon->GetBestTeleportLocation(TeleportHit,
				TeleportOrigin,
				TeleportCurrentLocation,
				PlayerNavData,
				PlayerNavFilterClass))
			{
				if (ProjectPointToPlayerNavigation(TeleportCurrentLocation, TeleportCurrentLocation))
				{
					TargetTeleportBeacon = NewTeleportBeacon;
					TargetTeleportBeacon->OnTargetedForTeleport();
					OnTeleportBeaconTargeted();
					return true;
				}
			}
		}
	}

	// Otherwise, ensure any previously highlighted beacons are inactive
	if (TargetTeleportBeacon)
	{
		TargetTeleportBeacon->OnReleasedForTeleport();
		OnTeleportBeaconReleased();
		TargetTeleportBeacon = nullptr;
	}

	// Next, check if the hit target is an Empath Character
	if (TraceSettings.bTraceForEmpathChars)
	{
		AEmpathCharacter* NewTargetChar = Cast<AEmpathCharacter>(HitActor);
		if (NewTargetChar)
		{
			// Un-target any current teleport character
			if (TargetTeleportChar && TargetTeleportChar != NewTargetChar)
			{
				TargetTeleportChar->OnReleasedForTeleport();
				OnTeleportCharReleased();
			}

			// Check if this actor has a valid teleportation that is on the ground
			if (NewTargetChar->GetBestTeleportLocation(TeleportHit,
				TeleportOrigin,
				TeleportCurrentLocation,
				PlayerNavData,
				PlayerNavFilterClass))
			{
				if (ProjectPointToPlayerNavigation(TeleportCurrentLocation, TeleportCurrentLocation))
				{
					TargetTeleportChar = NewTargetChar;
					TargetTeleportBeacon->OnTargetedForTeleport();
					OnTeleportCharTargeted();
					return true;
				}
			}
			// Otherwise, ensure currently targeted empath character is un-targeted
			if (TargetTeleportChar)
			{
				TargetTeleportChar->OnReleasedForTeleport();
				TargetTeleportChar = nullptr;
				OnTeleportCharReleased();
			}
			UE_LOG(LogTeleportTrace, VeryVerbose, TEXT("%s: Teleport trace hit Empath Character [%s], but found no valid teleport locations."), *GetNameSafe(this), *GetNameSafe(HitActor));
			return false;
		}
	}
	

	// Again, ensure previous targeted empath character is un-targeted
	// (this is because we return false in the above case
	if (TargetTeleportChar)
	{
		TargetTeleportChar->OnReleasedForTeleport();
		TargetTeleportChar = nullptr;
		OnTeleportCharReleased();
	}

	// Finally, check if it is simply a space on the navmesh
	if (TraceSettings.bTraceForWorldStatic)
	{
		if (ProjectPointToPlayerNavigation(TeleportHit.Location, TeleportCurrentLocation))
		{
			return true;
		}
	}


	UE_LOG(LogTeleportTrace, VeryVerbose, TEXT("%s: Teleport trace found no valid locations on hit actor [%s]"), *GetNameSafe(this), *GetNameSafe(HitActor));
	TeleportCurrentLocation = FVector::ZeroVector;
	return false;
}

void AEmpathPlayerCharacter::CacheNavMesh()
{
	UWorld* World = GetWorld();
	if (World)
	{
		for (ANavigationData* CurrNavData : TActorRange<ANavigationData>(World))
		{
			if (GetNameSafe(CurrNavData) == "RecastNavMesh-Player")
			{
				PlayerNavData = CurrNavData;
				break;
			}
		}
	}
}

bool AEmpathPlayerCharacter::ProjectPointToPlayerNavigation(FVector Point, FVector& OutPoint)
{
	if (UEmpathFunctionLibrary::EmpathProjectPointToNavigation(this,
		OutPoint,
		Point,
		PlayerNavData,
		PlayerNavFilterClass,
		TeleportProjectQueryExtent))
	{
		// Ensure the projected point is on the ground
		FHitResult GroundTraceHit;
		const FVector GroundTraceOrigin = OutPoint;
		const FVector GroundTraceEnd = GroundTraceOrigin + FVector(0.0f, 0.0f, -200.0f);
		FCollisionQueryParams GroundTraceParams(FName(TEXT("GroundTrace")), false, this);
		if (GetWorld()->LineTraceSingleByChannel(GroundTraceHit, GroundTraceOrigin, GroundTraceEnd, ECC_WorldStatic, GroundTraceParams))
		{
			OutPoint = GroundTraceHit.ImpactPoint;
			return true;
		}
	}
	UE_LOG(LogTeleportTrace, VeryVerbose, TEXT("%s: Traced teleport location [%s] is not on the player nav mesh. "), *GetNameSafe(this), *Point.ToString());
	return false;
	OutPoint = FVector::ZeroVector;
}

void AEmpathPlayerCharacter::UpdateTeleportCurrLocValid(const bool bNewValid)
{
	if (bNewValid && !bIsTeleportCurrLocValid)
	{
		bIsTeleportCurrLocValid = true;
		OnTeleportLocationFound();
		if (TeleportMarker)
		{
			TeleportMarker->OnTeleportLocationFound();
		}
		return;
	}
	if (!bNewValid && bIsTeleportCurrLocValid)
	{
		bIsTeleportCurrLocValid = false;
		OnTeleportLocationLost();
		if (TeleportMarker)
		{
			TeleportMarker->OnTeleportLocationLost();
		}
		return;
	}
}

void AEmpathPlayerCharacter::LocomotionAxisUpDown(float AxisValue)
{
	LocomotionInputAxis.X = AxisValue;
}

void AEmpathPlayerCharacter::LocomotionAxisRightLeft(float AxisValue)
{
	LocomotionInputAxis.Y = AxisValue;
}

void AEmpathPlayerCharacter::TeleportAxisUpDown(float AxisValue)
{
	TeleportInputAxis.X = AxisValue;
}

void AEmpathPlayerCharacter::TeleportAxisRightLeft(float AxisValue)
{
	TeleportInputAxis.Y = AxisValue;
}

void AEmpathPlayerCharacter::OnLocomotionPressed_Implementation()
{
	if (CurrentLocomotionMode == EEmpathLocomotionMode::Dash && CanDash())
	{

		// Get the best XY direction
		// X and Y are swapped because in Unreal, Y is forward
		FVector2D DashDirectionLocal = FVector2D::ZeroVector;

		// Dashing left or right
		if (FMath::Abs(LocomotionInputAxis.Y) > FMath::Abs(LocomotionInputAxis.X))
		{
			// Right
			if (LocomotionInputAxis.Y > 0.0f)
			{
				DashDirectionLocal.Y = 1.0f;
			}

			// Left
			else
			{
				DashDirectionLocal.Y = -1.0f;
			}
		}

		// Dashing forward or backward
		else
		{
			// Forward
			if (LocomotionInputAxis.X > 0.0f)
			{
				DashDirectionLocal.X = 1.0f;
			}
			else
			{
				DashDirectionLocal.X = -1.0f;
			}
		}

		// Update direction vector based on our camera's facing direction
		FVector OrientedDashDirection = GetOrientedLocomotionAxis(DashDirectionLocal);

		// Perform the trace
		TraceTeleportLocation(GetVRLocation(), OrientedDashDirection, DashMagnitude, DashTraceSettings, false);

		// Quality of life feature. If the first dash trace is invalid, we do a single, shorter trace, 
		// to see if we can still move in the desired direction. This will cost us some performance,
		// but will make life easier on the player if they're in a high space with ledges that they would
		// otherwise dash off of
		if (!bIsTeleportCurrLocValid)
		{
			TraceTeleportLocation(GetVRLocation(), OrientedDashDirection, DashMagnitude / 2.0f, DashTraceSettings, false);
		}

		if (bIsTeleportCurrLocValid)
		{
			OnDashSuccess();
			TeleportToLocation(TeleportCurrentLocation);
		}
		else
		{
			OnDashFail();
		}	
	}
}

void AEmpathPlayerCharacter::OnLocomotionReleased_Implementation()
{

}

void AEmpathPlayerCharacter::OnTeleportPressed_Implementation()
{
	// Get the best direction and then decide the action
	// X and Y are reversed because in Unreal, X is forward and Y is right

	// Pivoting left or right
	if (FMath::Abs(TeleportInputAxis.Y) > FMath::Abs(TeleportInputAxis.X))
	{
		if (CanPivot())
		{
			// Right
			if (TeleportInputAxis.Y > 0.0f)
			{
				TeleportToPivot(TeleportPivotStep);
			}

			// Left
			else
			{
				TeleportToPivot(TeleportPivotStep * -1.0f);
			}
		}
	}

	// Either teleporting or turning 180 degrees
	else
	{
		// Teleporting
		if (TeleportInputAxis.X > 0.0f)
		{
			if (CanTeleport())
			{
				// Trace the teleport once to update locations
				FVector TeleportOrigin = FVector::ZeroVector;
				FVector TeleportLocalDirection = FVector(1.0f, 0.0f, 0.0f);
				GetTeleportTraceOriginAndDirection(TeleportOrigin, TeleportLocalDirection);
				TraceTeleportLocation(TeleportOrigin, TeleportLocalDirection, TeleportMagnitude, TeleportTraceSettings, false);

				// Begin drawing the trace
				SetTeleportState(EEmpathTeleportState::TracingTeleportLocation);
				ShowTeleportTrace();
			}
		}

		// Pivoting 180 degrees
		else if (CanPivot())
		{
			TeleportToPivot(180.0f);
		}
	}
}

void AEmpathPlayerCharacter::OnTeleportReleased_Implementation()
{
	if (TeleportState == EEmpathTeleportState::TracingTeleportLocation)
	{
		TeleportToLocation(TeleportCurrentLocation);
	}
}

void AEmpathPlayerCharacter::OnAltMovementPressed()
{
	bAltMovementPressed = true;
	if (DefaultLocomotionMode == EEmpathLocomotionMode::Dash)
	{
		CurrentLocomotionMode = EEmpathLocomotionMode::Walk;
	}
	else
	{
		CurrentLocomotionMode = EEmpathLocomotionMode::Dash;
	}
	ReceiveAltMovementPressed();
}

void AEmpathPlayerCharacter::ReceiveAltMovementPressed_Implementation()
{

}

void AEmpathPlayerCharacter::OnAltMovementReleased()
{
	bAltMovementPressed = false;
	if (DefaultLocomotionMode == EEmpathLocomotionMode::Dash)
	{
		CurrentLocomotionMode = EEmpathLocomotionMode::Dash;
	}
	else
	{
		CurrentLocomotionMode = EEmpathLocomotionMode::Walk;
	}
	ReceiveAltMovementReleased();
}

void AEmpathPlayerCharacter::ReceiveAltMovementReleased_Implementation()
{

}

void AEmpathPlayerCharacter::TickUpdateInputAxisEvents()
{
	// Dash
	if (LocomotionInputAxis.Size() >= InputAxisEventThreshold)
	{
		if (!bLocomotionPressed)
		{
			bLocomotionPressed = true;
			OnLocomotionPressed();
		}
	}
	else
	{
		if (bLocomotionPressed)
		{
			bLocomotionPressed = false;
			OnLocomotionReleased();
		}
	}

	// Teleport
	if (TeleportInputAxis.Size() >= InputAxisEventThreshold)
	{
		if (!bTeleportPressed)
		{
			bTeleportPressed = true;
			OnTeleportPressed();
		}
	}
	else
	{
		if (bTeleportPressed)
		{
			bTeleportPressed = false;
			OnTeleportReleased();
		}
	}
}

bool AEmpathPlayerCharacter::CanPivot_Implementation() const
{
	return (bPivotEnabled && !bDead && !bStunned && !IsTeleporting());
}

bool AEmpathPlayerCharacter::CanDash_Implementation() const
{
	return (bDashEnabled && !bDead && !bStunned && !IsTeleporting());
}

bool AEmpathPlayerCharacter::CanTeleport_Implementation() const
{
	return (bTeleportEnabled && !bDead && !bStunned && !IsTeleporting());
}

bool AEmpathPlayerCharacter::CanWalk_Implementation() const
{
	return (bWalkEnabled && !bDead && !bStunned && !IsTeleporting());
}

void AEmpathPlayerCharacter::TickUpdateHealthRegen(float DeltaTime)
{
	// Regen health if appropriate
	if (bHealthRegenActive && GetWorld()->TimeSince(LastDamageTime) > HealthRegenDelay)
	{
		CurrentHealth = FMath::Min((CurrentHealth + (HealthRegenRate * MaxHealth * DeltaTime)), MaxHealth);
	}
}

void AEmpathPlayerCharacter::TickUpdateTeleportState(float DeltaSeconds)
{
	// Update our teleport state as appropriate
	switch (TeleportState)
	{
	case EEmpathTeleportState::NotTeleporting:
	{
		break;
	}

	case EEmpathTeleportState::TracingTeleportLocation:
	{
		if (CanTeleport())
		{
			// Get the correct location and direction depending on the teleport hand
			FVector TeleportOrigin;
			FVector TeleportLocalDirection = FVector(1.0f, 0.0f, 0.0f);
			GetTeleportTraceOriginAndDirection(TeleportOrigin, TeleportLocalDirection);

			// Perform the actual trace
			TraceTeleportLocation(TeleportOrigin, TeleportLocalDirection, TeleportMagnitude, TeleportTraceSettings, true);
			OnTickTeleportStateUpdated();
		}
		else
		{
			SetTeleportState(EEmpathTeleportState::NotTeleporting);
		}


		break;
	}

	case EEmpathTeleportState::TeleportingToLocation:
	{
		// Check if we have timed out
		if (GetWorld()->TimeSince(TeleportLastStartTime) > TeleportTimoutTime)
		{
			OnTeleportEnd();
			OnTeleportToLocationFail();
			OnTickTeleportStateUpdated();
			break;
		}

		// Cache the distance between the current and goal locations
		FVector CurrLoc = GetActorLocation();
		float CurrDist = (TeleportActorLocationGoal - CurrLoc).Size();

		// Lerp towards the target location
		SetActorLocation(FMath::VInterpConstantTo(CurrLoc, TeleportActorLocationGoal, DeltaSeconds, TeleportMovementSpeed));

		// Check if we have arrived at or overshot the target location
		float NewDist = (TeleportActorLocationGoal - GetActorLocation()).Size();
		if (NewDist <= TeleportDistTolerance || NewDist > CurrDist)
		{
			SetActorLocation(TeleportActorLocationGoal);
			OnTeleportEnd();
			OnTeleportToLocationSuccess();
		}
		OnTickTeleportStateUpdated();
		break;
	}

	case EEmpathTeleportState::TeleportingToRotation:
	{
		// Cache and setup variables
		float OldDeltaYaw = TeleportRemainingDeltaYaw;

		// Get the current delta yaw to apply on this frame by interping the remaining delta towards 0 and finding the difference
		TeleportRemainingDeltaYaw = FMath::FInterpConstantTo(TeleportRemainingDeltaYaw, 0.0f, DeltaSeconds, TeleportRotationSpeed);
		float DeltaYawToApply = OldDeltaYaw - TeleportRemainingDeltaYaw;

		// Ensure we did not overhsoot the target
		if (FMath::Abs(OldDeltaYaw) < FMath::Abs(TeleportRemainingDeltaYaw))
		{
			TeleportRemainingDeltaYaw = 0.0f;
			DeltaYawToApply = OldDeltaYaw;
		}

		// Apply the delta yaw to our rotation
		FRotator DeltaRotation = FRotator(0.0f, DeltaYawToApply, 0.0f);
		AddActorWorldRotationVR(DeltaRotation, true);

		// End the rotation as appropriate
		if (FMath::IsNearlyZero(TeleportRemainingDeltaYaw))
		{
			SetTeleportState(EEmpathTeleportState::EndingTeleport);
			OnTeleportEnd();
			OnTeleportToRotationEnd();
		}
		OnTickTeleportStateUpdated();
		break;
	}

	case EEmpathTeleportState::EndingTeleport:
	{
		OnTickTeleportStateUpdated();
		break;
	}

	default:
	{
		break;
	}
	}
}

bool AEmpathPlayerCharacter::IsTeleporting() const
{
	return (TeleportState != EEmpathTeleportState::NotTeleporting
		&& TeleportState != EEmpathTeleportState::TracingTeleportLocation);
}

void AEmpathPlayerCharacter::SetTeleportState(EEmpathTeleportState NewTeleportState)
{
	if (NewTeleportState != TeleportState)
	{
		EEmpathTeleportState OldTeleportState = TeleportState;
		TeleportState = NewTeleportState;

		// Hide the teleport trace if appropriate
		if (OldTeleportState == EEmpathTeleportState::TracingTeleportLocation)
		{
			HideTeleportTrace();
		}


		OnTeleportStateChanged(OldTeleportState);
	}
}

void AEmpathPlayerCharacter::OnTeleportEnd()
{
	VRRootReference->SetEnableGravity(true);
	SetTeleportState(EEmpathTeleportState::EndingTeleport);
	VRMovementReference->SetMovementMode(MOVE_Walking);
	TArray<AActor*> ControlledActors(GetControlledActors());
	for (AActor* CurrentActor : ControlledActors)
	{
		CurrentActor->SetActorEnableCollision(true);
	}
	ReceiveTeleportEnd();
}

void AEmpathPlayerCharacter::ReceiveTeleportEnd_Implementation()
{
	// By default, just end the teleport state
	SetTeleportState(EEmpathTeleportState::NotTeleporting);
}

void AEmpathPlayerCharacter::GetTeleportTraceOriginAndDirection(FVector& Origin, FVector& Direction)
{
	// Right hand
	if (TeleportHand == EEmpathBinaryHand::Right)
	{
		if (RightHandActor)
		{
			Origin = RightHandActor->GetTeleportOrigin();
			Direction = RightHandActor->GetTeleportDirection(FVector(1.0f, 0.0f, 0.0f));
		}

		else
		{
			Origin = RightMotionController->GetComponentLocation();
			Direction = RightMotionController->GetForwardVector();
		}
	}

	// Left hand
	else
	{
		if (LeftHandActor)
		{
			Origin = LeftHandActor->GetTeleportOrigin();
			Direction = LeftHandActor->GetTeleportDirection(Direction);
		}

		else
		{
			Origin = LeftMotionController->GetComponentLocation();
			Direction = LeftMotionController->GetForwardVector();
		}
	}
}

void AEmpathPlayerCharacter::TeleportToPivot(float DeltaYaw)
{
	// Do nothing if we are already teleporting
	if (IsTeleporting())
	{
		return;
	}

	// Update delta yaw and teleport state
	TeleportRemainingDeltaYaw = DeltaYaw;
	SetTeleportState(EEmpathTeleportState::TeleportingToRotation);

	// Fire notifies
	OnTeleportToRotation();

}

TArray<AActor*> AEmpathPlayerCharacter::GetControlledActors()
{
	TArray<AActor*> ControlledActors;
	ControlledActors.Add(this);
	if (RightHandActor)
	{
		ControlledActors.Add(RightHandActor);
		if (RightHandActor->HeldObject)
		{
			ControlledActors.Add(RightHandActor->HeldObject);
		}
	}
	if (LeftHandActor)
	{
		ControlledActors.Add(LeftHandActor);
		if (LeftHandActor->HeldObject)
		{
			ControlledActors.Add(LeftHandActor->HeldObject);
		}
	}
	return ControlledActors;
}

void AEmpathPlayerCharacter::ShowTeleportTrace()
{
	if (TeleportMarker)
	{
		TeleportMarker->SetActorHiddenInGame(false);
		TeleportMarker->OnShowTeleportMarker();
	}
	OnShowTeleportTrace();
}

void AEmpathPlayerCharacter::UpdateTeleportTrace()
{
	if (TeleportMarker)
	{
		TeleportMarker->UpdateMarkerLocation(TeleportCurrentLocation, bTeleportEnabled, TargetTeleportChar, TargetTeleportBeacon);
	}
	OnUpdateTeleportTrace();
}

void AEmpathPlayerCharacter::HideTeleportTrace()
{
	if (TeleportMarker)
	{
		TeleportMarker->SetActorHiddenInGame(true);
		TeleportMarker->OnHideTeleportMarker();
	}
	OnHideTeleportTrace();
}

void AEmpathPlayerCharacter::SetDefaultLocomotionMode(EEmpathLocomotionMode LocomotionMode)
{
	if (bAltMovementPressed)
	{
		CurrentLocomotionMode = DefaultLocomotionMode;
	}
	else
	{
		CurrentLocomotionMode = LocomotionMode;
	}

	DefaultLocomotionMode = LocomotionMode;
}

void AEmpathPlayerCharacter::TickUpdateWalk()
{
	// Check for valid input
	float MoveScale = LocomotionInputAxis.Size();
	UVRCharacterMovementComponent* CMC = Cast<UVRCharacterMovementComponent>(GetMovementComponent());
	if (CMC && CurrentLocomotionMode == EEmpathLocomotionMode::Walk && CanWalk() && MoveScale > InputAxisLocomotionWalkThreshold)
	{
		// Call notify if appropriate
		if (!bWalking)
		{
			bWalking = true;
			OnWalkBegin();
		}

		// Convert the remaining magnitude of the analog stick into a scale from 0 to 1
		float MaxScale = 1.0f - InputAxisLocomotionWalkThreshold;
		MoveScale -= InputAxisLocomotionWalkThreshold;
		MoveScale /= InputAxisLocomotionWalkThreshold;
		FVector2D ScaledInput = LocomotionInputAxis.GetSafeNormal() * MoveScale;

		// Add the input to our movement component
		CMC->AddInputVector(GetOrientedLocomotionAxis(ScaledInput));
	}

	else if (bWalking)
	{
		bWalking = false;
		OnWalkEnd();
	}
}

FVector AEmpathPlayerCharacter::GetOrientedLocomotionAxis(const FVector2D InputAxis) const
{
	// Convert to a 3 dimensional vector
	FVector ThreeDInputAxis = FVector(InputAxis.X, InputAxis.Y, 0.0f);

	// Find the correct orientation
	EEmpathOrientation OrientationToUse;
	if (CurrentLocomotionMode == EEmpathLocomotionMode::Dash)
	{
		OrientationToUse = DashOrientation;
	}
	else
	{
		OrientationToUse = WalkOrientation;
	}

	// Get the appropriate transform according to the vector
	FTransform DirectionTransform;
	if (OrientationToUse == EEmpathOrientation::Head)
	{
		DirectionTransform = VRReplicatedCamera->GetComponentTransform();
	}
	else
	{
		// Switch depending on witch hand is walk
		// It will always be whichever one is not teleport
		if (TeleportHand == EEmpathBinaryHand::Right)
		{
			DirectionTransform = LeftMotionController->GetComponentTransform();
		}
		else
		{
			DirectionTransform = RightMotionController->GetComponentTransform();
		}
	}

	ThreeDInputAxis = DirectionTransform.TransformVectorNoScale(ThreeDInputAxis);
	ThreeDInputAxis.Z = 0.0f;
	return ThreeDInputAxis;
}

void AEmpathPlayerCharacter::OnGripRightPressed()
{
	bGripRightPressed = true;
	ReceiveGripRightPressed();
}

void AEmpathPlayerCharacter::OnGripRightReleased()
{
	bGripRightPressed = false;
	ReceiveGripRightReleased();
}

void AEmpathPlayerCharacter::OnGripLeftPressed()
{
	bGripLeftPressed = true;
	ReceiveGripLeftPressed();
}

void AEmpathPlayerCharacter::OnGripLeftReleased()
{
	bGripLeftPressed = false;
	ReceiveGripLeftReleased();
}

void AEmpathPlayerCharacter::ReceiveGripRightPressed_Implementation()
{
	if (bGripEnabled)
	{
		RightHandActor->OnGripPressed();
	}
}

void AEmpathPlayerCharacter::ReceiveGripRightReleased_Implementation()
{
	if (bGripEnabled)
	{
		RightHandActor->OnGripReleased();
	}
}

void AEmpathPlayerCharacter::ReceiveGripLeftPressed_Implementation()
{

}

void AEmpathPlayerCharacter::ReceiveGripLeftReleased_Implementation()
{

}

void AEmpathPlayerCharacter::SetGripEnabled(const bool bNewEnabled)
{
	if (bNewEnabled != bGripEnabled)
	{
		bGripEnabled = bNewEnabled;

		// Release any grips if we can no longer grip
		if (!bGripEnabled)
		{
			if (RightHandActor->GripState != EEmpathGripType::NoGrip)
			{
				RightHandActor->OnGripReleased();
			}
			if (LeftHandActor->GripState != EEmpathGripType::NoGrip)
			{
				LeftHandActor->OnGripReleased();
			}
		}
	}
	return;
}

