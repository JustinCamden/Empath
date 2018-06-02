// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathAIController.h"
#include "EmpathAIManager.h"
#include "EmpathGameModeBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "EmpathPlayerCharacter.h"
#include "EmpathCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "EmpathFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "Runtime/Engine/Public/EngineUtils.h"
#include "EmpathPathFollowingComponent.h"
#include "AI/Navigation/NavLinkCustomComponent.h"
#include "EmpathNavLinkProxy_Jump.h"

// Stats for UE Profiler
DECLARE_CYCLE_STAT(TEXT("Empath Char Take Damage"), STAT_EMPATH_TakeDamage, STATGROUP_EMPATH_Character);
DECLARE_CYCLE_STAT(TEXT("Empath Is Ragdoll At Rest Check"), STAT_EMPATH_IsRagdollAtRest, STATGROUP_EMPATH_Character);

// Log categories
DEFINE_LOG_CATEGORY_STATIC(LogAIController, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogAIVision, Log, All);

// Console variable setup so we can enable and disable vision debugging from the console
static TAutoConsoleVariable<int32> CVarEmpathAIVisionDrawDebug(
	TEXT("Empath.AIVisionDrawDebug"),
	0,
	TEXT("Whether to enable AI vision debug.\n")
	TEXT("0: Disable, 1: Enabled"),
	ECVF_Scalability | ECVF_RenderThreadSafe);
static const auto AIVisionDrawDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("Empath.AIVisionDrawDebug"));


static TAutoConsoleVariable<float> CVarEmpathAIVisionDebugLifetime(
	TEXT("Empath.AIVisionDebugLifetime"),
	3.0f,
	TEXT("Duration of debug drawing for AI vision, in seconds."),
	ECVF_Scalability | ECVF_RenderThreadSafe);
static const auto AIVisionDebugLifetime = IConsoleManager::Get().FindConsoleVariable(TEXT("Empath.AIVisionDebugLifetime"));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#define AIVISION_LOC(_Loc, _Radius, _Color)									if (AIVisionDrawDebug->GetInt()) { DrawDebugSphere(GetWorld(), _Loc, _Radius, 16, _Color, -1.0f, 0, 3.0f); }
#define AIVISION_LINE(_Loc, _Dest, _Color)									if (AIVisionDrawDebug->GetInt()) { DrawDebugLine(GetWorld(), _Loc, _Dest, _Color, -1.0f, 0, 3.0f); }
#define AIVISION_CONE(_Loc, _Forward, _Dist, _Angle, _Color)				if (AIVisionDrawDebug->GetInt()) { DrawDebugCone(GetWorld(), _Loc, _Forward, _Dist, _Angle, _Angle, 12, _Color, -1.0f, 0, 3.0f);}
#define AIVISION_LOC_DURATION(_Loc, _Radius, _Color)						if (AIVisionDrawDebug->GetInt()) { DrawDebugSphere(GetWorld(), _Loc, _Radius, 16, _Color, false, AIVisionDebugLifetime->GetFloat(), 0, 3.0f); }
#define AIVISION_LINE_DURATION(_Loc, _Dest, _Color)							if (AIVisionDrawDebug->GetInt()) { DrawDebugLine(GetWorld(), _Loc, _Dest, _Color, false, AIVisionDebugLifetime->GetFloat(), 0, 3.0f); }
#define AIVISION_CONE_DURATION(_Loc, _Forward, _Dist, _Angle, _Color)		if (AIVisionDrawDebug->GetInt()) { DrawDebugCone(GetWorld(), _Loc, _Forward, _Dist, _Angle, _Angle, 12, _Color, false, AIVisionDebugLifetime->GetFloat(), 0, 3.0f);}
#else
#define AIVISION_LOC(_Loc, _Radius, _Color)				/* nothing */
#define AIVISION_LINE(_Loc, _Dest, _Color)				/* nothing */
#define AIVISION_CONE(_Loc, _Dest, _Color)				/* nothing */
#define AIVISION_LOC_DURATION(_Loc, _Radius, _Color)	/* nothing */
#define AIVISION_LINE_DURATION(_Loc, _Dest, _Color)		/* nothing */
#define AIVISION_CONE_DURATION(_Loc, _Dest, _Color)		/* nothing */
#endif

// Stats for UE Profiler
DECLARE_CYCLE_STAT(TEXT("AI Update Attack Target"), STAT_EMPATH_UpdateAttackTarget, STATGROUP_EMPATH_AICon);
DECLARE_CYCLE_STAT(TEXT("AI Update Vision"), STAT_EMPATH_UpdateVision, STATGROUP_EMPATH_AICon);
DECLARE_CYCLE_STAT(TEXT("AI Jump Anim Calculation"), STAT_EMPATH_JumpAnim, STATGROUP_EMPATH_AICon);

// Cannot statics in class initializer so initialize here
const float AEmpathAIController::MinTargetSelectionScore = -9999999.0f;
const FName AEmpathAIController::AIVisionTraceTag = FName(TEXT("AIVisionTrace"));
const float AEmpathAIController::MinDefenseGuardRadius = 100.f;
const float AEmpathAIController::MinDefensePursuitRadius = 150.f;
const float AEmpathAIController::MinFleeTargetRadius = 100.f;

AEmpathAIController::AEmpathAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		.SetDefaultSubobjectClass<UEmpathPathFollowingComponent>(TEXT("PathFollowingComponent")))
{
	Team = EEmpathTeam::Enemy;

	// Target selection
	bCanTargetSecondaryTargets = true;
	bIgnorePlayer = false;
	bAutoTargetPlayer = false;
	bIgnoreVisionCone = false;

	// Target selection score weights
	DistScoreWeight = 1.0f;
	AngleScoreWeight = 0.5f;
	TargetPrefScoreWeight = 1.0f;
	CurrentTargetPreferenceWeight = 0.7f;
	TargetingRatioBonusScoreWeight = 1.0f;

	// Vision variables
	ForwardVisionAngle = 65.0f;
	PeripheralVisionAngle = 85.0f;
	PeripheralVisionDistance = 1500.0f;
	AutoSeeDistance = 100.0f;
	//bDrawDebugVision = false;
	//bDrawDebugLOSBlockingHits = false;

	// Bind events and delegates
	OnLOSTraceCompleteDelegate.BindUObject(this, &AEmpathAIController::OnLOSTraceComplete);

	// Navigation variables
	bDetectStuckAgainstOtherAI = true;
	MinCapsuleBumpsBeforeRepositioning = 10;
	MaxTimeBetweenConsecutiveBumps = 0.2f;
	TimeOnPathUntilRepath = 15.0f;

	// Movement variables
	bClaimNavLinksOnMove = true;


	// Turn off perception component for performance, since we don't use it and don't want it ticking
	UAIPerceptionComponent* const PerceptionComp = GetPerceptionComponent();
	if (PerceptionComp)
	{
		PerceptionComp->PrimaryComponentTick.bCanEverTick = false;
	}
}

void AEmpathAIController::BeginPlay()
{
	Super::BeginPlay();
	
	// Grab the AI Manager
	AEmpathGameModeBase* EmpathGMD = GetWorld()->GetAuthGameMode<AEmpathGameModeBase>();
	if (EmpathGMD)
	{
		AEmpathAIManager* GrabbedAIManager = EmpathGMD->GetAIManager();
		RegisterAIManager(GrabbedAIManager);
	}
	else
	{
		UE_LOG(LogAIController, Warning, TEXT("%s ERROR: Not running in Empath game mode!"), *GetName());
	}
}

void AEmpathAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	// Ensure we are unregistered from the AI manager
	UnregisterAIManager();
}

void AEmpathAIController::UnPossess()
{
	UnregisterAIManager();
	ReleaseAllClaimedNavLinks();
	Super::UnPossess();
}

void AEmpathAIController::RegisterAIManager(AEmpathAIManager* RegisteringAIManager)
{
	if (!AIManager && RegisteringAIManager)
	{
		AIManager = RegisteringAIManager;
		AIManagerIndex = AIManager->EmpathAICons.AddUnique(this);
	}
}

EEmpathTeam AEmpathAIController::GetTeamNum_Implementation() const
{
	return Team;
}

AEmpathCharacter* AEmpathAIController::GetEmpathChar() const
{
	if (CachedEmpathChar)
	{
		return CachedEmpathChar;
	}
	return Cast<AEmpathCharacter>(GetPawn());
}

bool AEmpathAIController::IsTargetLost() const
{
	AActor* const Target = GetAttackTarget();
	if (Target && AIManager)
	{
		return !AIManager->IsTargetLocationKnown(Target);
	}

	return false;
}

bool AEmpathAIController::IsFacingTarget(FVector TargetLoc, float AngleToleranceDeg) const
{
	// Check if we have a pawn
	APawn const* const MyPawn = GetPawn();
	if (MyPawn)
	{
		// Check the angle from the pawn's forward vector to the target
		FVector const ToTarget = (TargetLoc - MyPawn->GetActorLocation()).GetSafeNormal2D();
		FVector ForwardDir = MyPawn->GetActorForwardVector();

		// Flatten the Z so we only take the left and right angle into account
		ForwardDir.Z = 0.f;
		float const Dot = ForwardDir | ToTarget;
		float const CosTolerance = FMath::Cos(FMath::DegreesToRadians(AngleToleranceDeg));
		return (Dot > CosTolerance);
	}

	return false;
}

void AEmpathAIController::SetAttackTarget(AActor* NewTarget)
{
	// Ensure we have a blackboard
	if (Blackboard)
	{
		// Ensure that this is a new target
		AActor* const OldTarget = Cast<AActor>(Blackboard->GetValueAsObject(FEmpathBBKeys::AttackTarget));
		if (NewTarget != OldTarget)
		{
			// Unregister delegates on old target
			AEmpathPlayerCharacter* OldVRCharTarget = Cast<AEmpathPlayerCharacter>(OldTarget);
			if (OldVRCharTarget)
			{
				OldVRCharTarget->OnTeleport.RemoveDynamic(this, &AEmpathAIController::OnAttackTargetTeleported);
				OldVRCharTarget->OnDeath.RemoveDynamic(this, &AEmpathAIController::ReceiveAttackTargetDied);
			}

			// Do the set in the blackboard
			Blackboard->SetValueAsObject(FEmpathBBKeys::AttackTarget, NewTarget);
			LastSawAttackTargetTeleportTime = 0.0f;

			// Update the target radius
			if (AIManager)
			{
				CurrentAttackTargetRadius = AIManager->GetAttackTargetRadius(NewTarget);
			}

			// Register delegates on new target
			AEmpathPlayerCharacter* NewVRCharTarget = Cast<AEmpathPlayerCharacter>(NewTarget);
			if (NewVRCharTarget)
			{
				NewVRCharTarget->OnTeleport.AddDynamic(this, &AEmpathAIController::OnAttackTargetTeleported);
				NewVRCharTarget->OnDeath.AddDynamic(this, &AEmpathAIController::ReceiveAttackTargetDied);
			}


			// If the new target is null, then we can no longer see the target
			if (NewTarget == nullptr)
			{
				Blackboard->SetValueAsBool(FEmpathBBKeys::bCanSeeTarget, false);
			}
			ReceiveNewAttackTarget(OldTarget, NewTarget);
		}
	}
}

AActor* AEmpathAIController::GetAttackTarget() const
{
	if (Blackboard)
	{
		return Cast<AActor>(Blackboard->GetValueAsObject(FEmpathBBKeys::AttackTarget));
	}

	return nullptr;
}

void AEmpathAIController::SetDefendTarget(AActor* NewDefendTarget)
{
	if (Blackboard)
	{
		Blackboard->SetValueAsObject(FEmpathBBKeys::DefendTarget, NewDefendTarget);
	}

	return;
}

AActor* AEmpathAIController::GetDefendTarget() const
{
	if (Blackboard)
	{
		return Cast<AActor>(Blackboard->GetValueAsObject(FEmpathBBKeys::DefendTarget));
	}

	return nullptr;
}

void AEmpathAIController::SetFleeTarget(AActor* NewFleeTarget)
{
	if (Blackboard)
	{
		Blackboard->SetValueAsObject(FEmpathBBKeys::FleeTarget, NewFleeTarget);
	}

	return;
}

AActor* AEmpathAIController::GetFleeTarget() const
{
	if (Blackboard)
	{
		return Cast<AActor>(Blackboard->GetValueAsObject(FEmpathBBKeys::FleeTarget));
	}

	return nullptr;
}

void AEmpathAIController::SetCanSeeTarget(bool bNewCanSeeTarget)
{
	if (Blackboard)
	{
		// Update the blackboard
		Blackboard->SetValueAsBool(FEmpathBBKeys::bCanSeeTarget, bNewCanSeeTarget);

		// If we can see the target, update shared knowledge of the target's location
		if (bNewCanSeeTarget)
		{
			// Update shared knowledge of target location
			AActor* const Target = GetAttackTarget();
			if (Target)
			{
				UpdateKnownTargetLocation(Target);
			}
		}
	}
}

bool AEmpathAIController::CanSeeTarget() const
{
	if (Blackboard)
	{
		return Blackboard->GetValueAsBool(FEmpathBBKeys::bCanSeeTarget);
	}

	return false;
}

void AEmpathAIController::GetActorEyesViewPoint(FVector& out_Location, FRotator& out_Rotation) const
{
	if (VisionBoneName != NAME_None)
	{
		// Check if we have a skeletal mesh
		AEmpathCharacter* EmpathChar = GetEmpathChar();
		USkeletalMeshComponent* const MySkelMesh = EmpathChar ? EmpathChar->GetMesh() : nullptr;
		if (MySkelMesh)
		{
			// Check first for a socket matching the name
			if (MySkelMesh->DoesSocketExist(VisionBoneName))
			{
				FTransform const SocketTransform = MySkelMesh->GetSocketTransform(VisionBoneName);
				out_Location = SocketTransform.GetLocation();
				out_Rotation = SocketTransform.Rotator();
				return;
			}

			// If there is no socket, check for a bone
			else
			{
				int32 const BoneIndex = MySkelMesh->GetBoneIndex(VisionBoneName);
				if (BoneIndex != INDEX_NONE)
				{
					FTransform const BoneTransform = MySkelMesh->GetBoneTransform(BoneIndex);
					out_Location = BoneTransform.GetLocation();
					out_Rotation = BoneTransform.Rotator();
					return;
				}
			}
		}
	}

	// Fallback for if we don't have a skeletal mesh or if there are no sockets or bones
	// matching the name
	return Super::GetActorEyesViewPoint(out_Location, out_Rotation);
}

void AEmpathAIController::SetGoalLocation(FVector GoalLocation)
{
	if (Blackboard)
	{
		Blackboard->SetValueAsVector(FEmpathBBKeys::GoalLocation, GoalLocation);
	}
}

FVector AEmpathAIController::GetGoalLocation() const
{
	if (Blackboard)
	{
		return Blackboard->GetValueAsVector(FEmpathBBKeys::GoalLocation);
	}

	// Default to current location.
	return GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
}

void AEmpathAIController::SetIsPassive(bool bNewPassive)
{
	if (Blackboard)
	{
		Blackboard->SetValueAsBool(FEmpathBBKeys::bIsPassive, bNewPassive);
	}

	return;
}

bool AEmpathAIController::IsPassive() const
{
	if (Blackboard)
	{
		return Blackboard->GetValueAsBool(FEmpathBBKeys::bIsPassive);
	}

	return false;
}

float AEmpathAIController::GetMaxAttackRange() const
{
	AEmpathCharacter*EmpathChar = GetEmpathChar();
	if (EmpathChar)
	{
		return EmpathChar->MaxEffectiveDistance + CurrentAttackTargetRadius;
	}
	return 0.0f;
}

float AEmpathAIController::GetMinAttackRange() const
{
	AEmpathCharacter*EmpathChar = GetEmpathChar();
	if (EmpathChar)
	{
		return EmpathChar->MinEffectiveDistance + CurrentAttackTargetRadius;
	}
	return 0.0f;
}

float AEmpathAIController::GetRangeToTarget() const
{
	AEmpathCharacter*EmpathChar = GetEmpathChar();
	AActor const* const AttackTarget = GetAttackTarget();
	if (EmpathChar && AttackTarget)
	{
		return EmpathChar->GetDistanceTo(AttackTarget) - CurrentAttackTargetRadius;
	}

	return 0.f;
}

bool AEmpathAIController::WantsToReposition(float DesiredMaxAttackRange, float DesiredMinAttackRange) const
{
	AEmpathCharacter*EmpathChar = GetEmpathChar();
	AActor* AttackTarget = GetAttackTarget();

	// If we were requested to move, return true
	if (bShouldReposition)
	{
		// We want to reset the request to move, since we'll probably attempt to move when this is called.
		// Since bShouldReposition is a protected variable, and this function is public,
		// we need to get around the protection level.
		AEmpathAIController* const MutableThis = const_cast<AEmpathAIController*>(this);
		MutableThis->bShouldReposition = false;

		return true;
	}

	// If we are too close or far away from attack target, we want to move
	else if (EmpathChar && AttackTarget)
	{
		float const RangeToTarget = EmpathChar->GetDistanceToVR(AttackTarget);
		if (RangeToTarget > DesiredMaxAttackRange || RangeToTarget < DesiredMinAttackRange)
		{
			return true;
		}
	}

	// Otherwise, we want to move if we cannot see the attack target
	return !CanSeeTarget();
}

FVector AEmpathAIController::GetAimLocation() const
{
	// Check for manually set override
	if (bUseCustomAimLocation)
	{
		return CustomAimLocation;
	}

	// Otherwise, aim for the attack target, so long as it is not teleporting
	AActor* const AttackTarget = GetAttackTarget();
	if (AttackTarget)
	{
		AEmpathPlayerCharacter* VRCharacterTarget = Cast<AEmpathPlayerCharacter>(AttackTarget);
		if (!(VRCharacterTarget && VRCharacterTarget->IsTeleporting()))
		{
			return UEmpathFunctionLibrary::GetAimLocationOnActor(AttackTarget);
		}
	}

	// Otherwise, aim straight ahead
	APawn* const MyPawn = GetPawn();
	if (MyPawn)
	{
		return (MyPawn->GetActorLocation() + (MyPawn->GetActorForwardVector() * 10000.f));
	}

	return FVector::ZeroVector;
}

void AEmpathAIController::SetCustomAimLocation(FVector AimLocation)
{
	CustomAimLocation = AimLocation;
	bUseCustomAimLocation = true;
}

void AEmpathAIController::ClearCustomAimLocation()
{
	CustomAimLocation = FVector::ZeroVector;
	bUseCustomAimLocation = false;
}

bool AEmpathAIController::IsDead() const
{
	AEmpathCharacter*EmpathChar = GetEmpathChar();
	if (IsPendingKill() || !EmpathChar || EmpathChar->IsDead())
	{
		return true;
	}
	return false;
}

bool AEmpathAIController::IsAIRunning() const
{
	UBrainComponent* const Brain = GetBrainComponent();
	if (Brain)
	{
		return Brain->IsRunning();
	}

	return false;
}

bool AEmpathAIController::IsTargetTeleporting() const
{
	AEmpathPlayerCharacter* VRCharTarget = Cast<AEmpathPlayerCharacter>(GetAttackTarget());
	if (VRCharTarget && VRCharTarget->IsTeleporting())
	{
		return true;
	}
	return false;
}

float AEmpathAIController::GetTimeSinceLastSawAttackTargetTeleport() const
{
	return GetWorld()->TimeSince(LastSawAttackTargetTeleportTime);
}

void AEmpathAIController::OnLostPlayerTarget()
{
	AEmpathPlayerCharacter* VRCharTarget = Cast<AEmpathPlayerCharacter>(GetAttackTarget());
	if (!IsPassive() && VRCharTarget)
	{
		ReceiveLostPlayerTarget();
	}
}

void AEmpathAIController::OnSearchForPlayerStarted()
{
	AEmpathPlayerCharacter* VRCharTarget = Cast<AEmpathPlayerCharacter>(GetAttackTarget());
	if (!IsPassive() && VRCharTarget)
	{
		ReceiveSearchForPlayerStarted();
	}
}

void AEmpathAIController::OnTargetSeenForFirstTime()
{
	if (!IsPassive())
	{
		ReceiveTargetSeenForFirstTime();
	}
}

FPathFollowingRequestResult AEmpathAIController::MoveTo(const FAIMoveRequest& MoveRequest,
	FNavPathSharedPtr* OutPath)
{
	FPathFollowingRequestResult const Result = Super::MoveTo(MoveRequest, OutPath);

	const FVector GoalLocation = MoveRequest.GetDestination();
	AEmpathCharacter* const MyChar = GetEmpathChar();

	// If success or already at the location, update the Blackboard, and the Empath character
	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful ||
		Result.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		if (Blackboard)
		{
			Blackboard->SetValueAsVector(FEmpathBBKeys::GoalLocation, GoalLocation);
		}
		if (MyChar)
		{
			MyChar->OnPathRequestSuccess(GoalLocation);
		}
	}

	// If success, fire our own notifies
	if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		ReceiveMoveTo(GoalLocation);

		// OnAIMoveTo can may deceleration values, such as when choosing to walk or run.
		// We need to warn path following component so it can re-run the code to cache various deceleration-related data.
		UEmpathPathFollowingComponent* const PFC = Cast<UEmpathPathFollowingComponent>(GetPathFollowingComponent());
		if (PFC)
		{
			PFC->OnDecelerationPossiblyChanged();
		}

		// Release any currently claimed nav links and claim all new ones on the path
		if (bClaimNavLinksOnMove)
		{
			ReleaseAllClaimedNavLinks();
			ClaimAllNavLinksOnPath();
		}

		// Bind capsule bump detection while not moving.
		if (bDetectStuckAgainstOtherAI)
		{
			AEmpathCharacter*EmpathChar = GetEmpathChar();
			UCapsuleComponent* const MyPawnCapsule = EmpathChar ? EmpathChar->GetCapsuleComponent() : nullptr;
			if (MyPawnCapsule)
			{
				MyPawnCapsule->OnComponentHit.AddDynamic(this, &AEmpathAIController::OnCapsuleBumpDuringMove);
			}
		}
	}

	// If failure, alert the empath character
	else if (Result.Code == EPathFollowingRequestResult::Failed)
	{
		if (MyChar)
		{
			MyChar->OnPathRequestFailed(GoalLocation);
		}
	}
	return Result;
}

void AEmpathAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	// Release any claimed nav links
	ReleaseAllClaimedNavLinks();

	// Unbind capsule bump detection while not moving
	AEmpathCharacter*EmpathChar = GetEmpathChar();
	UCapsuleComponent* const MyPawnCapsule = EmpathChar ? EmpathChar->GetCapsuleComponent() : nullptr;
	if (MyPawnCapsule && MyPawnCapsule->OnComponentHit.IsAlreadyBound(this, &AEmpathAIController::OnCapsuleBumpDuringMove))
	{
		MyPawnCapsule->OnComponentHit.RemoveDynamic(this, &AEmpathAIController::OnCapsuleBumpDuringMove);
	}

	Super::OnMoveCompleted(RequestID, Result);
}

void AEmpathAIController::OnCapsuleBumpDuringMove(UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	// Check if the colliding actor is an Empath Character
	AEmpathCharacter* const OtherEmpathChar = Cast<AEmpathCharacter>(OtherActor);
	if (OtherEmpathChar && (OtherEmpathChar->GetCapsuleComponent() == OtherComp))
	{
		UWorld const* const World = GetWorld();

		// If its been less than the max time between bumps, then increment the bump count
		if (World->TimeSince(LastCapsuleBumpWhileMovingTime) < MaxTimeBetweenConsecutiveBumps)
		{
			NumConsecutiveBumpsWhileMoving++;

			// If we've experienced several hits in a short time, and velocity is 0, we're stuck
			if (NumConsecutiveBumpsWhileMoving > MinCapsuleBumpsBeforeRepositioning)
			{
				AEmpathCharacter*EmpathChar = GetEmpathChar();
				if (EmpathChar && EmpathChar->GetVelocity().IsNearlyZero())
				{
					// If we're stuck, stop trying to path
					StopMovement();

					// Then tell the character blocking our path to move
					AEmpathAIController* const OtherAI = Cast<AEmpathAIController>(OtherEmpathChar->GetController());
					if (OtherAI)
					{
						OtherAI->bShouldReposition = true;
					}
				}
			}
		}
		else
		{
			// If its been longer than the max time between bumps, this is our first bump
			NumConsecutiveBumpsWhileMoving = 1;
		}

		// Update last bumped time
		LastCapsuleBumpWhileMovingTime = GetWorld()->GetTimeSeconds();
	}
}

void AEmpathAIController::UpdateTargetingAndVision()
{
	UpdateAttackTarget();
	UpdateVision();
}

void AEmpathAIController::UpdateAttackTarget()
{
	// Track how long it takes to complete this function for the profiler
	SCOPE_CYCLE_COUNTER(STAT_EMPATH_UpdateAttackTarget);

	// If we auto target the player, then simply set them as the attack target
	if (bAutoTargetPlayer)
	{
		AController* PlayerCon = GetWorld()->GetFirstPlayerController();
		if (PlayerCon)
		{
			APawn* PlayerPawn = PlayerCon->GetPawn();
			if (PlayerPawn)
			{
				SetAttackTarget(PlayerPawn);
				return;
			}
		}
	}

	// Initialize internal variables
	AActor* const CurrAttackTarget = GetAttackTarget();
	AActor* BestAttackTarget = CurrAttackTarget;

	// Score potential targets and choose the best one
	if (AIManager)
	{
		// Initialize the best score to very low to ensure that we calculate scores properly
		float BestScore = MinTargetSelectionScore;

		if (bCanTargetSecondaryTargets)
		{
			// Ensure there are secondary targets
			TArray<FSecondaryAttackTarget> const& SecondaryTargets = AIManager->GetSecondaryAttackTargets();
			if (SecondaryTargets.Num() > 0)
			{
				// Check each valid secondary target
				for (FSecondaryAttackTarget const& CurrentTarget : SecondaryTargets)
				{
					if (CurrentTarget.IsValid())
					{
						float const Score = GetTargetSelectionScore(CurrentTarget.TargetActor,
							CurrentTarget.TargetingRatio,
							CurrentTarget.TargetPreference,
							CurrAttackTarget);

						// If this is better than our current target, update accordingly
						if (Score > BestScore)
						{
							BestScore = Score;
							BestAttackTarget = CurrentTarget.TargetActor;
						}
					}
				}
			}
		}

		if (bIgnorePlayer == false)
		{
			// Check player score if player exists
			APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
			if (PlayerController)
			{
				// Ensure this is our VR Character and they are not dead
				AEmpathPlayerCharacter* const PlayerTarget = Cast<AEmpathPlayerCharacter>(PlayerController->GetPawn());
				if (PlayerTarget && !PlayerTarget->IsDead())
				{
					float const PlayerScore = GetTargetSelectionScore(PlayerTarget,
						0.0f,
						0.0f,
						CurrAttackTarget);

					// If the player is the best target, target them
					if (PlayerScore > BestScore)
					{
						BestScore = PlayerScore;
						BestAttackTarget = PlayerTarget;
					}
				}
			}
		}
	}

	// Update attack target
	SetAttackTarget(BestAttackTarget);
}

void AEmpathAIController::UpdateVision(bool bTestImmediately)
{
	// Track how long it takes to complete this function for the profiler
	SCOPE_CYCLE_COUNTER(STAT_EMPATH_UpdateVision);

	// Check if we can see the target
	UWorld* const World = GetWorld();
	AActor* const AttackTarget = GetAttackTarget();

	if (World && AttackTarget && AIManager)
	{
		// Check is the player is teleporting. If so, we can't see them
		AEmpathPlayerCharacter* const PlayerTarget = Cast<AEmpathPlayerCharacter>(AttackTarget);
		if (PlayerTarget && PlayerTarget->IsTeleporting())
		{
			SetCanSeeTarget(false);
			return;
		}

		// Set up raycasting params
		FCollisionQueryParams Params(AIVisionTraceTag);
		Params.AddIgnoredActor(GetPawn());
		Params.AddIgnoredActor(AttackTarget);

		// Get view location and rotation
		FVector ViewLoc;
		FRotator ViewRotation;
		GetActorEyesViewPoint(ViewLoc, ViewRotation);

		// Trace to where we are trying to aim
		FVector const TraceStart = ViewLoc;
		FVector const TraceEnd = UEmpathFunctionLibrary::GetAimLocationOnActor(AttackTarget);

		// Check whether the target's location is known or may be lost
		bool const bAlreadyKnowsTargetLoc = AIManager->IsTargetLocationKnown(AttackTarget);
		bool const bPlayerMayBeLost = AIManager->IsPlayerPotentiallyLost();

		// Only test the vision angle when the AI doesn't already know where the target is or the player might be lost
		bool const bTestVisionCone = (bPlayerMayBeLost || !bAlreadyKnowsTargetLoc) && (!bIgnoreVisionCone);

		bool bInVisionCone = true;
		if (bTestVisionCone)
		{
			// Cache distance to target
			FVector const ToTarget = (TraceEnd - ViewLoc);
			float ToTargetDist = ToTarget.Size();
			FVector const ToTargetNorm = ToTarget.GetSafeNormal();
			FVector const EyesForwardNorm = ViewRotation.Vector();

			// Is the target within our auto see range?
			if (ToTargetDist <= AutoSeeDistance)
			{
				// If so, automatically see them
				bInVisionCone = true;
			}

			// Else, check the angle of the target to our vision
			else
			{
				// Set the cos limit depending on distance from the target
				float const AngleCos = FVector::DotProduct(ToTargetNorm, EyesForwardNorm);
				float const CosLimit = ((ToTargetDist <= PeripheralVisionDistance) ? 
					FMath::Cos(FMath::DegreesToRadians(PeripheralVisionAngle)) : 
					FMath::Cos(FMath::DegreesToRadians(ForwardVisionAngle)));
				
				// Return whether we are in the vision cone
				bInVisionCone = (AngleCos >= CosLimit);
			}

			//// Draw debug shapes if appropriate
			//if (bDrawDebugVision)
			//{
			//	// Forward Vision
			AIVISION_CONE_DURATION(ViewLoc, EyesForwardNorm, PeripheralVisionDistance + 500.0f, FMath::DegreesToRadians(ForwardVisionAngle), FColor::Yellow);
			//	DrawDebugCone(World, ViewLoc, EyesForwardNorm, PeripheralVisionDistance + 500.0f, 
			//		FMath::DegreesToRadians(ForwardVisionAngle), 
			//		FMath::DegreesToRadians(ForwardVisionAngle), 
			//		12, FColor::Yellow, false, 1.0f);

			//	// Peripheral vision
			AIVISION_CONE_DURATION(ViewLoc, EyesForwardNorm, PeripheralVisionDistance, FMath::DegreesToRadians(PeripheralVisionAngle), FColor::Green);
			//	DrawDebugCone(World, ViewLoc, EyesForwardNorm, PeripheralVisionDistance,
			//		FMath::DegreesToRadians(PeripheralVisionAngle),
			//		FMath::DegreesToRadians(PeripheralVisionAngle),
			//		12, FColor::Green, false, 1.0f);

			//	// Auto see distance
			AIVISION_LOC_DURATION(ViewLoc, AutoSeeDistance, FColor::Red);
			//	DrawDebugSphere(World, ViewLoc, AutoSeeDistance, 12, FColor::Red, false, 1.0f);
			//}
		}

		// If we are in the vision cone, prepare to trace
		if (bInVisionCone)
		{
			FCollisionResponseParams const ResponseParams = FCollisionResponseParams::DefaultResponseParam;

			// If we want to trace immediately, conduct the trace this frame
			if (bTestImmediately)
			{
				bool bHasLOS = true;
				FHitResult OutHit(0.f);
				bool bHit = (World->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, 
					ECC_Visibility, Params, ResponseParams));

				// If we hit an object that is not us or the target, then our line of sight is blocked
				if (bHit)
				{
					if (OutHit.bBlockingHit && (OutHit.Actor != AttackTarget))
					{
						bHasLOS = false;
						//if (bDrawDebugLOSBlockingHits)
						//{
						UE_LOG(LogAIVision, Log, TEXT("Visual trace hit %s!"), *GetNameSafe(OutHit.GetActor()));
						AIVISION_LOC_DURATION(TraceStart, 8.0f, FColor::White);
						AIVISION_LOC_DURATION(TraceEnd, 8.0f, FColor::Yellow);
						AIVISION_LOC_DURATION(OutHit.ImpactPoint, 8.0f, FColor::Red);
						AIVISION_LINE_DURATION(TraceStart, TraceEnd, FColor::Yellow);
						//	DrawDebugBox(GetWorld(), TraceStart, FVector(8.f), FColor::White, false, 5.f);
						//	DrawDebugBox(GetWorld(), TraceEnd, FVector(8.f), FColor::Yellow, false, 5.f);
						//	DrawDebugSphere(GetWorld(), OutHit.ImpactPoint, 8.f, 10, FColor::Red, false, 5.f);
						//	DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Yellow, false, 5.f, 0, 3.f);
						//}
					}
					else
					{
						AIVISION_LINE_DURATION(TraceStart, TraceEnd, FColor::Green);
						//if (bDrawDebugLOSBlockingHits)
						//{
						//	DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Green, false, 5.f, 0, 3.f);
						//}
					}
				}
				SetCanSeeTarget(bHasLOS);
			}

			// Most of the time, we don't mind getting the results a few frames late, so we'll
			// conduct the trace asynchroniously for performance
			else
			{
				CurrentLOSTraceHandle = World->AsyncLineTraceByChannel(EAsyncTraceType::Single,
					TraceStart, TraceEnd, ECC_Visibility, Params, ResponseParams,
					&OnLOSTraceCompleteDelegate);
			}
		}
		else
		{
			// No line of sight
			SetCanSeeTarget(false);
		}
	}
}

float AEmpathAIController::GetTargetSelectionScore(AActor* CandidateTarget,
	float DesiredCandidateTargetingRatio,
	float CandidateTargetPreference,
	AActor* CurrentTarget) const
{
	// Ensure the target and controlled pawns are valid
	ACharacter* const AIChar = Cast<ACharacter>(GetPawn());
	if (AIManager && AIChar && CandidateTarget)
	{

		// Get the relative locations
		FVector AICharLoc = AIChar->GetActorLocation();
		FVector CandidateLoc = CandidateTarget->GetActorLocation();

		// Find the distance to the target
		FVector AItoCandidateDir;
		float AItoCandidateDist;
		(CandidateLoc - AICharLoc).ToDirectionAndLength(AItoCandidateDir, AItoCandidateDist);

		// Find angle
		float const Angle = FMath::RadiansToDegrees(FMath::Acos(AItoCandidateDir | AIChar->GetActorForwardVector()));

		// Find final score using the curves defined in editor
		float const DistScore = DistScoreWeight * (TargetSelectionDistScoreCurve ? TargetSelectionDistScoreCurve->GetFloatValue(AItoCandidateDist) : 0.f);
		float const AngleScore = AngleScoreWeight * (TargetSelectionAngleScoreCurve ? TargetSelectionAngleScoreCurve->GetFloatValue(Angle) : 0.f);
		float const CurrentTargetPrefScore = (CurrentTarget && CandidateTarget == CurrentTarget) ? CurrentTargetPreferenceWeight : 0.f;
		float const CandidateTargetPrefScore = TargetPrefScoreWeight * CandidateTargetPreference;

		// Apply a large penalty if we don't know the target location
		float const CandidateIsLostPenalty = AIManager->IsTargetLocationKnown(CandidateTarget) ? 0.f : -10.f;

		// Calculate bonus or penalty depending on the ratio of AI targeting this target
		float TargetingRatioBonusScore = 0.f;
		if (DesiredCandidateTargetingRatio > 0.f)
		{
			int32 NumAITargetingCandiate, NumTotalAI;
			AIManager->GetNumAITargeting(CandidateTarget, NumAITargetingCandiate, NumTotalAI);

			float CurrentTargetingRatio = (float)NumAITargetingCandiate / (float)NumTotalAI;

			// If this is a new target
			if (CandidateTarget != CurrentTarget)
			{
				// Would switching to this target get us closer to the desired ratio?
				if (CurrentTargetingRatio < DesiredCandidateTargetingRatio)
				{
					float ProposedTargetingRatio = (float)(NumAITargetingCandiate + 1) / (float)NumTotalAI;

					float CurDelta = FMath::Abs(CurrentTargetingRatio - DesiredCandidateTargetingRatio);
					float ProposedDelta = FMath::Abs(ProposedTargetingRatio - DesiredCandidateTargetingRatio);

					if (ProposedDelta < CurDelta)
					{
						// Switching to this target would get us closer to the ideal ratio, emphasize this target
						TargetingRatioBonusScore = TargetingRatioBonusScoreWeight;
					}
					else
					{
						// Switching targets would get us farther from the ideal ratio, de-emphasize this target
						TargetingRatioBonusScore = -TargetingRatioBonusScoreWeight;
					}
				}
			}

			// If this is our current target
			else
			{
				// Would switching away from this get us closer to desired ratio?
				if (CurrentTargetingRatio > DesiredCandidateTargetingRatio)
				{
					float ProposedTargetingRatio = (float)(NumAITargetingCandiate - 1) / (float)NumTotalAI;

					float CurDelta = FMath::Abs(CurrentTargetingRatio - DesiredCandidateTargetingRatio);
					float ProposedDelta = FMath::Abs(ProposedTargetingRatio - DesiredCandidateTargetingRatio);

					if (ProposedDelta < CurDelta)
					{
						// Switching targets would get us closer to the ideal ratio, add a score nudge
						TargetingRatioBonusScore = -TargetingRatioBonusScoreWeight;
					}

					if (ProposedDelta < CurDelta)
					{
						// Switching targets would get us farther from the ideal ratio, de-emphasize this target
						TargetingRatioBonusScore = -TargetingRatioBonusScoreWeight;
					}
					else
					{
						// staying on this target would keep us closer to the ideal ratio, emphasize this target
						TargetingRatioBonusScore = TargetingRatioBonusScoreWeight;
					}
				}
			}
		}

		return CurrentTargetPrefScore + CandidateTargetPrefScore + AngleScore + DistScore + TargetingRatioBonusScore + CandidateIsLostPenalty;
	}

	return MinTargetSelectionScore;
}

void AEmpathAIController::UpdateKnownTargetLocation(AActor const* AITarget)
{
	if (AIManager)
	{
		bool bNotifySpotted = false;

		// If we're not passive and we didn't already know the target location, 
		// then trigger a notify spotted event
		if (IsPassive() == false)
		{
			bool const bAlreadyKnewTargetLocation = AIManager->IsTargetLocationKnown(AITarget);
			if (!bAlreadyKnewTargetLocation)
			{
				bNotifySpotted = true;
			}
		}

		// Update shared knowledge of target location
		AIManager->UpdateKnownTargetLocation(AITarget);

		if (bNotifySpotted)
		{
			// We want to update vision immediately because this AI doesn't necessarily need to reposition, 
			// thinking it can't see the target for a few frames after hearing it.
			UpdateVision(true);

			// We didn't know where the player was before, so this AI triggers its "spotted" event
			ReceiveTargetSpotted();
		}
	}
}

void AEmpathAIController::OnLOSTraceComplete(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum)
{
	if ((TraceHandle == LOSTraceHandleToIgnore))
	{
		// We explicitly chose to invalidate this trace, so clear the handle and return
		LOSTraceHandleToIgnore = FTraceHandle();
		return;
	}

	// Look for any results that resulted in a blocking hit
	bool bHasLOS = true;
	FHitResult const* const OutHit = FHitResult::GetFirstBlockingHit(TraceDatum.OutHits);

	// If we hit an object that isn't us or the attack target, then we do not have line of sight
	if (OutHit)
	{
		if (OutHit->bBlockingHit && OutHit->GetActor() != GetAttackTarget())
		{
			bHasLOS = false;

			// Draw debug blocking hits if appropriate
			UE_LOG(LogAIVision, Log, TEXT("Visual trace hit %s!"), *GetNameSafe(OutHit->GetActor()));
			AIVISION_LOC_DURATION(TraceDatum.Start, 8.0f, FColor::White);
			AIVISION_LOC_DURATION(TraceDatum.End, 8.0f, FColor::Yellow);
			AIVISION_LOC_DURATION(OutHit->ImpactPoint, 8.0f, FColor::Red);
			AIVISION_LINE_DURATION(TraceDatum.Start, TraceDatum.End, FColor::Yellow);
			//if (bDrawDebugLOSBlockingHits)
			//{
			//	UE_LOG(LogTemp, Log, TEXT("Visual trace hit %s!"), *GetNameSafe(OutHit->GetActor()));

			//	DrawDebugBox(GetWorld(), TraceDatum.Start, FVector(8.f), FColor::White, false, 5.f);
			//	DrawDebugBox(GetWorld(), TraceDatum.End, FVector(8.f), FColor::Yellow, false, 5.f);
			//	DrawDebugSphere(GetWorld(), OutHit->ImpactPoint, 8.f, 10, FColor::Red, false, 5.f);
			//	DrawDebugLine(GetWorld(), TraceDatum.Start, TraceDatum.End, FColor::Yellow, false, 5.f, 0, 3.f);
			//}
		}
	}

	// If we see the target and are drawing debug hits, draw a line to the target to show we hit it
	else
	{
		AIVISION_LINE_DURATION(TraceDatum.Start, TraceDatum.End, FColor::Green);
		//if (bDrawDebugLOSBlockingHits)
		//{
		//	DrawDebugLine(GetWorld(), TraceDatum.Start, TraceDatum.End, FColor::Green, false, 5.f, 0, 3.f);
		//}
	}

	// Update the visibility
	SetCanSeeTarget(bHasLOS);
}

bool AEmpathAIController::RunBehaviorTree(UBehaviorTree* BTAsset)
{
	bool bRunBTRequest = Super::RunBehaviorTree(BTAsset);

	if (bRunBTRequest)
	{
		AEmpathCharacter*EmpathChar = GetEmpathChar();
		if (EmpathChar)
		{
			EmpathChar->ReceiveAIInitalized();
		}
	}
	return bRunBTRequest;
}

void AEmpathAIController::PausePathFollowing()
{
	PauseMove(GetCurrentMoveRequestID());
}

void AEmpathAIController::ResumePathFollowing()
{
	ResumeMove(GetCurrentMoveRequestID());
}

void AEmpathAIController::SetBehaviorModeSearchAndDestroy(AActor* InitialAttackTarget)
{
	if (Blackboard)
	{
		Blackboard->SetValueAsEnum(FEmpathBBKeys::BehaviorMode, 
			static_cast<uint8>(EEmpathBehaviorMode::SearchAndDestroy));
		
		// If initial attack target is not set, let the AI choose it itself
		if (InitialAttackTarget != nullptr)
		{
			SetAttackTarget(InitialAttackTarget);
		}
	}
}

void AEmpathAIController::SetBehaviorModeDefend(AActor* DefendTarget, float GuardRadius, float PursuitRadius)
{
	if (Blackboard && DefendTarget)
	{
		Blackboard->SetValueAsEnum(FEmpathBBKeys::BehaviorMode, static_cast<uint8>(EEmpathBehaviorMode::Defend));
		Blackboard->SetValueAsObject(FEmpathBBKeys::DefendTarget, DefendTarget);

		// Keep the radii above a base minimum value to avoid silliness
		float NewGuardRadius = FMath::Max(GuardRadius, MinDefenseGuardRadius);
		float NewPursuitRadius = FMath::Max3(PursuitRadius, MinDefensePursuitRadius, NewGuardRadius);

		// Pursuit radius must be greater than defend radius for points to be generated in queries using them.
		// Thus, we ensure it is larger here
		if (NewPursuitRadius <= NewGuardRadius + 50.0f)
		{
			NewPursuitRadius = NewGuardRadius + 50.0f;
		}

		// Update blackboard
		Blackboard->SetValueAsFloat(FEmpathBBKeys::DefendGuardRadius, NewGuardRadius);
		Blackboard->SetValueAsFloat(FEmpathBBKeys::DefendPursuitRadius, NewPursuitRadius);
	}
}

void AEmpathAIController::SetBehaviorModeFlee(AActor* FleeTarget, float TargetRadius)
{
	if (Blackboard && FleeTarget)
	{
		// Clear our AI focus
		ClearFocus(EAIFocusPriority::Default);
		SetAttackTarget(nullptr);

		// Update blackboard
		Blackboard->SetValueAsEnum(FEmpathBBKeys::BehaviorMode, static_cast<uint8>(EEmpathBehaviorMode::Flee));
		Blackboard->SetValueAsObject(FEmpathBBKeys::FleeTarget, FleeTarget);

		TargetRadius = FMath::Max(TargetRadius, MinFleeTargetRadius);
		Blackboard->SetValueAsFloat(FEmpathBBKeys::FleeTargetRadius, TargetRadius);
	}
}

EEmpathBehaviorMode AEmpathAIController::GetBehaviorMode() const
{
	return Blackboard ? static_cast<EEmpathBehaviorMode>(Blackboard->GetValueAsEnum(FEmpathBBKeys::BehaviorMode)) : EEmpathBehaviorMode::SearchAndDestroy;
}

float AEmpathAIController::GetDefendGuardRadius() const
{
	return Blackboard ? Blackboard->GetValueAsFloat(FEmpathBBKeys::DefendGuardRadius) : MinDefenseGuardRadius;
}

float AEmpathAIController::GetDefendPursuitRadius() const
{
	return Blackboard ? Blackboard->GetValueAsFloat(FEmpathBBKeys::DefendPursuitRadius) : MinDefensePursuitRadius;
}

float AEmpathAIController::GetFleeTargetRadius() const
{
	return Blackboard ? Blackboard->GetValueAsFloat(FEmpathBBKeys::FleeTargetRadius) : 0.f;
}

bool AEmpathAIController::WantsToEngageAttackTarget() const
{
	// Don't engage if the target is lost
	if (IsTargetLost())
	{
		return false;
	}

	// We don't necessarily want to engage if we are in defend mode
	EEmpathBehaviorMode const BehaviorMode = GetBehaviorMode();
	if (BehaviorMode == EEmpathBehaviorMode::Defend)
	{
		// Get character pointers
		AEmpathCharacter const* const EmpathChar = GetEmpathChar();
		AActor const* const AttackTarget = GetAttackTarget();
		AActor const* const DefendTarget = GetDefendTarget();
		if (EmpathChar && AttackTarget && DefendTarget)
		{
			// If attack target is inside the pursuit radius, return true
			float const PursuitRadius = GetDefendPursuitRadius();
			float const DistFromAttackTargetToDefendTarget = AttackTarget->GetDistanceTo(DefendTarget);
			if (DistFromAttackTargetToDefendTarget < PursuitRadius)
			{
				return true;
			}
			else
			{
				// Otherwise, check if our attacks can still reach it from inside the defense radius
				float const ClosestPossibleDistToAttackTarget = DistFromAttackTargetToDefendTarget - PursuitRadius;
				float const MaxAttackDist = GetMaxAttackRange();

				// If so return true
				if (ClosestPossibleDistToAttackTarget <= MaxAttackDist)
				{
					return true;
				}
			}
		}

		// Otherwise, return false
		return false;
	}

	// We don't want to engage if we are in flee mode
	else if (BehaviorMode == EEmpathBehaviorMode::Flee)
	{
		return false;
	}

	// Otherwise, we're in search and destroy, and should want to engage
	return true;
}

void AEmpathAIController::OnAttackTargetTeleported(AActor* Target, FVector Origin, FVector Destination)
{
	// If the target disappeared in front of me
	if (CanSeeTarget())
	{
		// Ignore any in progress async vision checks
		if (GetWorld()->IsTraceHandleValid(CurrentLOSTraceHandle, false))
		{
			LOSTraceHandleToIgnore = CurrentLOSTraceHandle;
		}
		// Check again if we can see the target
		UpdateTargetingAndVision();

		LastSawAttackTargetTeleportTime = GetWorld()->GetTimeSeconds();

		ReceiveAttackTargetTeleported(Target, Origin, Destination);
	}
}

int32 AEmpathAIController::GetNumAIsNearby(float Radius) const
{
	int32 Count = 0;
	APawn const* const MyPawn = GetPawn();

	// Check how close each existing nearby AI is to this pawn
	if (MyPawn && AIManager)
	{
		for (AEmpathAIController* CurrentAI : AIManager->EmpathAICons)
		{
			if (CurrentAI != this)
			{
				APawn const* const CurrentPawn = CurrentAI->GetPawn();
				if (CurrentPawn)
				{
					if (MyPawn->GetDistanceTo(CurrentPawn) <= Radius)
					{
						++Count;
					}
				}
			}
		}
	}
	return Count;
}

void AEmpathAIController::OnCharacterDeath(FHitResult const& KillingHitInfo, FVector KillingHitImpulseDir, const AController* DeathInstigator, const AActor* DeathCauser, const UDamageType* DeathDamageType)
{
	// Remove ourselves from the AI manager
	UnregisterAIManager();

	// Release nav links
	ReleaseAllClaimedNavLinks();

	// Clear our attack target
	SetAttackTarget(nullptr);

	// Turn off behavior tree
	UBrainComponent* const Brain = GetBrainComponent();
	if (Brain)
	{
		Brain->StopLogic(TEXT("Pawn Died"));
	}

	// Fire notifies
	ReceiveCharacterDeath(KillingHitInfo, KillingHitImpulseDir, DeathInstigator, DeathCauser, DeathDamageType);
}

void AEmpathAIController::UnregisterAIManager()
{
	if (AIManager)
	{
		//// Remove us from the list of AI cons
		AIManager->EmpathAICons.RemoveAtSwap(AIManagerIndex);

		// Update the index we swapped with
		if (AIManagerIndex < AIManager->EmpathAICons.Num())
		{
			AEmpathAIController* SwappedAICon = AIManager->EmpathAICons[AIManagerIndex];
			if (SwappedAICon)
			{
				//SwappedAICon->AIManagerIndex = AIManagerIndex;
			}
		}

		AIManager->CheckForAwareAIs();
		AIManager = nullptr;
	}
	return;
}

void AEmpathAIController::ClimbTo_Implementation(FTransform const& LedgeTransform)
{
	// By default, signal the character to begin climbing
	if (AEmpathCharacter* EmpathChar = GetEmpathChar())
	{
		EmpathChar->ClimbTo(LedgeTransform);
	}
	return;
}

void AEmpathAIController::FinishClimb()
{
	// Signal our path following component to continue on to the next segment.
	UEmpathPathFollowingComponent* const PFC = Cast<UEmpathPathFollowingComponent>(GetPathFollowingComponent());
	if (PFC)
	{
		PFC->AdvanceToNextMoveSegment();
		PFC->FinishUsingCustomLink(Cast<UNavLinkCustomComponent>(PFC->GetCurrentCustomLinkOb()));
	}

	ResumePathFollowing();
}

void AEmpathAIController::JumpTo_Implementation(FTransform const& Destination, float Arc, const AActor* JumpFromActor)
{
	float OutAscentTime;
	float OutDescentTime;
	DoJumpLaunch(Destination, Arc, JumpFromActor, OutAscentTime, OutDescentTime);
}

bool AEmpathAIController::DoJumpLaunch(FTransform const& Destination, float Arc, const AActor* JumpFromActor, float& OutAscendingTime, float& OutDescendingTime)
{
	return DoJumpLaunch_Internal(Destination, Arc, nullptr, JumpFromActor, OutAscendingTime, OutDescendingTime);
}

bool AEmpathAIController::DoJumpLaunchWithPrecomputedVelocity(FTransform const& Destination, FVector LaunchVelocity, float Arc, const AActor* JumpFromActor, float& OutAscendingTime, float& OutDescendingTime)
{
	return DoJumpLaunch_Internal(Destination, Arc, &LaunchVelocity, JumpFromActor, OutAscendingTime, OutDescendingTime);
}

bool AEmpathAIController::DoJumpLaunch_Internal(FTransform const& Destination, float Arc, const FVector* InLaunchVel, const AActor* JumpFromActor, float& OutAscendingTime, float& OutDescendingTime)
{
	bool bSuccess = false;
	OutAscendingTime = 0.f;
	OutDescendingTime = 0.f;

	ACharacter* const AIChar = Cast<ACharacter>(GetPawn());
	if (AIChar)
	{
		// Calculate launch direction
		FVector const StartLoc = AIChar->GetActorLocation();
		FVector const DestLoc = Destination.GetLocation();

		// Calculation launch velocity if it was not passed to us
		FVector LaunchVel;
		if (InLaunchVel == nullptr)
		{
			// Compute LaunchVel
			if (!UEmpathFunctionLibrary::SuggestProjectileVelocity(AIChar, LaunchVel, StartLoc, DestLoc, Arc))
			{
				// Return false if we didn't hit the destination
				return false;
			}
		}

		// Otherwise use the provided launch velocity
		else
		{
			LaunchVel = *InLaunchVel;
		}

		// Launch the character and update out variables
		AIChar->LaunchCharacter(LaunchVel, true, true);
		OnAIJumpTo.Broadcast(this, LaunchVel, StartLoc, DestLoc, JumpFromActor);
		UEmpathFunctionLibrary::CalculateJumpTimings(this, LaunchVel, StartLoc, DestLoc, OutAscendingTime, OutDescendingTime);
		bSuccess = true;
	}

	return bSuccess;
}

void AEmpathAIController::ClaimAllNavLinksOnPath()
{
	UEmpathPathFollowingComponent* const PFC = Cast<UEmpathPathFollowingComponent>(GetPathFollowingComponent());
	if (PFC)
	{
		// If the path is valid
		UNavigationSystem* const NavSys = UNavigationSystem::GetCurrent(GetWorld());
		const FNavPathSharedPtr& NewPath = PFC->GetPath();
		if (NewPath.IsValid())
		{
			// Loop through all nav links on the path and claim them
			TArray<FNavPathPoint>& PathPoints = NewPath->GetPathPoints();
			for (int32 Idx = 0; Idx < PathPoints.Num(); ++Idx)		// note start at next idx -- only return true for upcoming jumps
			{
				FNavPathPoint const& PathPt = PathPoints[Idx];
				INavLinkCustomInterface* const CustomNavLink = NavSys->GetCustomLink(PathPt.CustomLinkId);
				UObject* const NavLinkComp = CustomNavLink ? CustomNavLink->GetLinkOwner() : nullptr;
				AEmpathNavLinkProxy* const EmpathNavLink = NavLinkComp ? Cast<AEmpathNavLinkProxy>(NavLinkComp->GetOuter()) : nullptr;
				if (EmpathNavLink)
				{
					EmpathNavLink->Claim(this);
					ClaimedNavLinks.AddUnique(EmpathNavLink);
				}
			}
		}
	}
}

void AEmpathAIController::ReleaseNavLinkClaim(AEmpathNavLinkProxy* NavLink)
{
	if (NavLink)
	{
		NavLink->BeginReleaseClaim();
		ClaimedNavLinks.RemoveSwap(NavLink);
	}
}

void AEmpathAIController::ReleaseAllClaimedNavLinks()
{
	for (AEmpathNavLinkProxy* NavLink : ClaimedNavLinks)
	{
		if (NavLink)
		{
			NavLink->BeginReleaseClaim(true);
		}
	}

	ClaimedNavLinks.Reset();
}

bool AEmpathAIController::IsUsingCustomNavLink() const
{
	UPathFollowingComponent const* const PFC = GetPathFollowingComponent();
	return (PFC && PFC->GetCurrentCustomLinkOb() != nullptr);
}

bool AEmpathAIController::CurrentPathHasUpcomingJumpLink() const
{
	return (GetUpcomingJumpLink() != nullptr);
}

AEmpathNavLinkProxy_Jump* AEmpathAIController::GetUpcomingJumpLink() const
{
	UEmpathPathFollowingComponent* const PFC = Cast<UEmpathPathFollowingComponent>(GetPathFollowingComponent());
	if (PFC)
	{
		// Check if the upcoming path is valid
		int32 const NextPathIndex = PFC->GetNextPathIndex();
		UNavigationSystem* const NavSys = UNavigationSystem::GetCurrent(GetWorld());
		const FNavPathSharedPtr& NewPath = PFC->GetPath();
		if (NewPath.IsValid())
		{
			// Iterate through the the path points and check for a jump link
			TArray<FNavPathPoint>& PathPoints = NewPath->GetPathPoints();
			for (int32 Idx = NextPathIndex; Idx < PathPoints.Num(); ++Idx)		// note start at next idx -- only return true for upcoming jumps
			{
				FNavPathPoint const& PathPt = PathPoints[Idx];
				INavLinkCustomInterface* const CustomNavLink = NavSys->GetCustomLink(PathPt.CustomLinkId);
				UObject* const NavLinkComp = CustomNavLink ? CustomNavLink->GetLinkOwner() : nullptr;
				AEmpathNavLinkProxy_Jump* const JumpLink = NavLinkComp ? Cast<AEmpathNavLinkProxy_Jump>(NavLinkComp->GetOuter()) : nullptr;
				if (JumpLink)
				{
					// Return the first jump link we find.
					return JumpLink;
				}
			}
		}
	}

	return nullptr;
}


bool AEmpathAIController::GetUpcomingJumpLinkAnimInfo(float& OutPathDistToJumpLink, float& OutEntryAngle, float& OutJumpDistance, float& OutPathDistAfterJumpLink, float& OutExitAngle)
{
	// Track how long it takes to complete this function for the profiler
	SCOPE_CYCLE_COUNTER(STAT_EMPATH_JumpAnim);

	// Reset in variables
	OutPathDistToJumpLink = 0.f;
	OutEntryAngle = 0.f;
	OutJumpDistance = 0.f;
	OutPathDistAfterJumpLink = 0.f;
	OutExitAngle = 0.f;

	UEmpathPathFollowingComponent* const PFC = Cast<UEmpathPathFollowingComponent>(GetPathFollowingComponent());
	if (PFC)
	{
		// Initialize variables
		FVector const CurrentPathLoc = PFC->GetLocationOnPath();
		int32 NextPathIndex = PFC->GetNextPathIndex();
		float PathDistBeforeJumpLink = 0.f;
		float PathDistOnJumpLink = 0.f;
		float PathDistAfterJumpLink = 0.f;

		// Check that the path is valid
		UNavigationSystem* const NavSys = UNavigationSystem::GetCurrent(GetWorld());
		const FNavPathSharedPtr& NewPath = PFC->GetPath();
		if (NewPath.IsValid())
		{
			// Iterate through the upcoming nav points for a jump link
			TArray<FNavPathPoint>& PathPoints = NewPath->GetPathPoints();
			FVector PrevLoc = CurrentPathLoc;
			bool bAfterJumpLink = false;
			bool bPrevWasJumpLink = false;
			int32 JumpLinkIdx = INDEX_NONE;
			for (int32 Idx = NextPathIndex; Idx < PathPoints.Num(); ++Idx)
			{
				FNavPathPoint const& PathPt = PathPoints[Idx];

				// If we found a jump link
				AEmpathNavLinkProxy_Jump* JumpLink = nullptr;
				if (JumpLinkIdx == INDEX_NONE)
				{
					// There could be  multiple jump links on remaining path,  but we only care about the next one
					INavLinkCustomInterface* const CustomNavLink = NavSys->GetCustomLink(PathPt.CustomLinkId);
					UObject* const NavLinkComp = CustomNavLink ? CustomNavLink->GetLinkOwner() : nullptr;
					JumpLink = NavLinkComp ? Cast<AEmpathNavLinkProxy_Jump>(NavLinkComp->GetOuter()) : nullptr;
					if (JumpLink)
					{
						JumpLinkIdx = Idx;
					}
				}

				// Get the distance after the jump link
				float const PathSegmentLength = (PathPt.Location - PrevLoc).Size();
				if (bAfterJumpLink)
				{
					PathDistAfterJumpLink += PathSegmentLength;
				}
				else if (bPrevWasJumpLink)
				{
					bAfterJumpLink = true;
					PathDistOnJumpLink = PathSegmentLength;
				}
				else
				{
					PathDistBeforeJumpLink += PathSegmentLength;
				}

				// Update variables for next loop
				PrevLoc = PathPt.Location;
				bPrevWasJumpLink = JumpLink != nullptr;
			}

			// If we didn't find a jump, return false
			if (JumpLinkIdx == INDEX_NONE)
			{
				return false;
			}

			// Calculation directions pre, during, and post jump
			FVector const PreJumpSegmentDirNorm = (PathPoints[JumpLinkIdx].Location - PathPoints[JumpLinkIdx - 1].Location).GetSafeNormal2D();
			FVector const JumpSegmentDirNorm = (PathPoints[JumpLinkIdx + 1].Location - PathPoints[JumpLinkIdx].Location).GetSafeNormal2D();
			FVector const PostJumpSegmentDirNorm = PathPoints.IsValidIndex(JumpLinkIdx + 2) ?
				(PathPoints[JumpLinkIdx + 2].Location - PathPoints[JumpLinkIdx + 1].Location).GetSafeNormal2D()
				: JumpSegmentDirNorm;

			// Calculation entry angle
			float EntryDot = PreJumpSegmentDirNorm | JumpSegmentDirNorm;
			float ExitDot = JumpSegmentDirNorm | PostJumpSegmentDirNorm;

			// Update out variables
			OutPathDistToJumpLink = PathDistBeforeJumpLink;
			OutEntryAngle = FMath::RadiansToDegrees(FMath::Acos(EntryDot));
			OutJumpDistance = PathDistOnJumpLink;
			OutPathDistAfterJumpLink = PathDistAfterJumpLink;
			OutExitAngle = FMath::RadiansToDegrees(FMath::Acos(ExitDot));

			return true;
		}
	}

	return false;

}

FVector AEmpathAIController::GetNavRecoveryDestination() const
{
	if (Blackboard)
	{
		return Blackboard->GetValueAsVector(FEmpathBBKeys::NavRecoveryDestination);
	}

	return FVector::ZeroVector;
}

void AEmpathAIController::SetNavRecoveryDestination(FVector Destination)
{
	if (Blackboard)
	{
		Blackboard->SetValueAsVector(FEmpathBBKeys::NavRecoveryDestination, Destination);
	}
}

void AEmpathAIController::GetNavRecoverySearchRadii(float& InnerRadius, float& OuterRadius) const
{
	if (InnerRadius && OuterRadius)
	{
		if (Blackboard)
		{
			InnerRadius = Blackboard->GetValueAsFloat(FEmpathBBKeys::NavRecoverySearchInnerRadius);
			OuterRadius = Blackboard->GetValueAsFloat(FEmpathBBKeys::NavRecoverySearchOuterRadius);
		}
		else
		{
			// Return default values
			InnerRadius = 100.f;
			OuterRadius = 300.f;
		}
	}
}

void AEmpathAIController::SetNavRecoverySearchRadii(float InnerRadius, float OuterRadius)
{
	if (Blackboard)
	{
		InnerRadius = FMath::Max(InnerRadius, 0.f);
		OuterRadius = FMath::Max(InnerRadius + 100.f, OuterRadius);

		Blackboard->SetValueAsFloat(FEmpathBBKeys::NavRecoverySearchInnerRadius, InnerRadius);
		Blackboard->SetValueAsFloat(FEmpathBBKeys::NavRecoverySearchOuterRadius, OuterRadius);
	}
}

void AEmpathAIController::ClearNavRecoveryDestination()
{
	if (Blackboard)
	{
		Blackboard->SetValueAsVector(FEmpathBBKeys::NavRecoveryDestination, FVector::ZeroVector);
	}
}
