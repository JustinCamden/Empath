// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathAIController.h"
#include "EmpathAIManager.h"
#include "EmpathGameModeBase.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EmpathVRCharacter.h"
#include "EmpathCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "EmpathUtility.h"
#include "DrawDebugHelpers.h"


// Stats for UE Profiler
DECLARE_CYCLE_STAT(TEXT("UpdateAttackTarget"), STAT_EMPATH_UpdateAttackTarget, STATGROUP_EMPATH_AICon);
DECLARE_CYCLE_STAT(TEXT("UpdateVision"), STAT_EMPATH_UpdateVision, STATGROUP_EMPATH_AICon);

// Cannot have in class initializer so consts initialize here
static const float MIN_TARGET_SELECTION_SCORE = -9999999.0f;
static const FName AIVisionTraceTag(TEXT("AIVisionTrace"));

AEmpathAIController::AEmpathAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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
	PeripheralVisionAngle = 20.0f;
	PeripheralVisionDistance = 1500.0f;
	AutoSeeDistance = 100.0f;
	bDrawDebugVision = false;
	bDrawDebugLOSBlockingHits = false;

	// Bind events and delegates
	OnLOSTraceCompleteDelegate.BindUObject(this, &AEmpathAIController::OnLOSTraceComplete);


	// Turn off perception component for performance, since we don't use it and don't want it ticking
	UAIPerceptionComponent* const PerceptionComp = GetPerceptionComponent();
	if (PerceptionComp)
	{
		PerceptionComp->PrimaryComponentTick.bCanEverTick = false;
	}
}

void AEmpathAIController::BeginPlay()
{
	// Grab the AI Manager at the start
	AIManager = GetWorld()->GetAuthGameMode<AEmpathGameModeBase>()->GetAIManager();
	if (!AIManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("ERROR: AI Manager not found!"));
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
		SetAttackTarget(GetWorld()->GetFirstPlayerController()->GetPawn());
		return;
	}

	// Initialize internal variables
	AActor* const CurrAttackTarget = GetAttackTarget();
	AActor* BestAttackTarget = CurrAttackTarget;

	// Score potential targets and choose the best one
	if (AIManager)
	{
		// Initialize the best score to very low to ensure that we calculate scores properly
		float BestScore = MIN_TARGET_SELECTION_SCORE;

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
				AEmpathVRCharacter* const PlayerTarget = Cast<AEmpathVRCharacter>(PlayerController->GetPawn());
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
		FVector const TraceEnd = UEmpathUtility::GetAimLocationOnActor(AttackTarget);

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

			// Draw debug shapes if appropriate
			if (bDrawDebugVision)
			{
				// Forward Vision
				DrawDebugCone(World, ViewLoc, EyesForwardNorm, PeripheralVisionDistance + 500.0f, 
					FMath::DegreesToRadians(ForwardVisionAngle), 
					FMath::DegreesToRadians(ForwardVisionAngle), 
					12, FColor::Yellow, false, 1.0f);

				// Peripheral vision
				DrawDebugCone(World, ViewLoc, EyesForwardNorm, PeripheralVisionDistance,
					FMath::DegreesToRadians(PeripheralVisionAngle),
					FMath::DegreesToRadians(PeripheralVisionAngle),
					12, FColor::Green, false, 1.0f);

				// Auto see distance
				DrawDebugSphere(World, ViewLoc, AutoSeeDistance, 12, FColor::Red, false, 1.0f);
			}
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
					}
				}
				UpdateTargetVisiblity(bHasLOS);
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
			// no vision
			UpdateTargetVisiblity(false);
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

	return MIN_TARGET_SELECTION_SCORE;
}

void AEmpathAIController::SetAttackTarget(AActor* NewTarget)
{
	// Ensure we have a blackboard
	if (Blackboard)
	{
		// Ensure that this is a new target
		UObject* const OldTarget = Cast<AActor>(Blackboard->GetValueAsObject(FEmpathBBKeys::AttackTarget));
		if (NewTarget != OldTarget)
		{
			// TODO: Unregister delegates on old target

			// Do the set in the blackboard
			Blackboard->SetValueAsObject(FEmpathBBKeys::AttackTarget, NewTarget);

			// Update the target radius
			if (AIManager)
			{
				CurrentAttackTargetRadius = AIManager->GetAttackTargetRadius(NewTarget);
			}

			// TODO: Register delegates on new target

			// If the new target is null, then we can no longer see the target
			if (NewTarget == nullptr)
			{
				Blackboard->SetValueAsBool(FEmpathBBKeys::bCanSeeTarget, false);
			}		
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

bool AEmpathAIController::CanSeeTarget() const
{
	if (Blackboard)
	{
		return Blackboard->GetValueAsBool(FEmpathBBKeys::bCanSeeTarget);
	}

	return false;
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

void AEmpathAIController::GetActorEyesViewPoint(FVector& out_Location, FRotator& out_Rotation) const
{
	if (VisionBoneName != NAME_None)
	{
		// Check if we have a skeletal mesh
		AEmpathCharacter* const MyChar = Cast<AEmpathCharacter>(GetPawn());
		USkeletalMeshComponent* const MySkelMesh = MyChar ? MyChar->GetMesh() : nullptr;
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

void AEmpathAIController::UpdateTargetVisiblity(bool bNewVisibility)
{
	if (Blackboard)
	{
		// Update the blackboard
		Blackboard->SetValueAsBool(FEmpathBBKeys::bCanSeeTarget, bNewVisibility);

		// If we can see the target, update shared knowledge of the target's location
		if (bNewVisibility)
		{
			// update shared knowledge of target location
			AActor* const Target = GetAttackTarget();
			if (Target)
			{
				SetKnownTargetLocation(Target);
			}
		}
	}
}

void AEmpathAIController::SetKnownTargetLocation(AActor const* AITarget)
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
		AIManager->SetKnownTargetLocation(AITarget);

		if (bNotifySpotted)
		{
			// We want to update vision immediately because AI doesn't necessarily need to reposition, 
			// thinking it can't see the target for a few frames after hearing it.
			UpdateVision(true);

			// We didn't know where the player was before, so this AI triggers its "spotted" event
			OnTargetSpotted();
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
	FHitResult const* const bHit = FHitResult::GetFirstBlockingHit(TraceDatum.OutHits);

	// If we hit an object that isn't us or the attack target, then we do not have line of sight
	if (bHit)
	{
		if (bHit && bHit->Actor != GetAttackTarget())
		{
			bHasLOS = false;

			// Draw debug blocking hits if appropriate
			if (bDrawDebugLOSBlockingHits)
			{
				UE_LOG(LogTemp, Log, TEXT("Visual trace hit! %s"), *GetNameSafe(bHit->GetActor()));

				DrawDebugBox(GetWorld(), TraceDatum.Start, FVector(8.f), FColor::White, false, 5.f);
				DrawDebugBox(GetWorld(), TraceDatum.End, FVector(8.f), FColor::Yellow, false, 5.f);
				DrawDebugSphere(GetWorld(), bHit->ImpactPoint, 8.f, 10, FColor::Red, false, 5.f);
				DrawDebugLine(GetWorld(), TraceDatum.Start, TraceDatum.End, FColor::Yellow, false, 5.f, 0, 3.f);
			}
		}
	}

	// If we see the target and are drawing debug hits, draw a line to the target to show we hit it
	else
	{
		if (bDrawDebugLOSBlockingHits)
		{
			DrawDebugLine(GetWorld(), TraceDatum.Start, TraceDatum.End, FColor::Green, false, 5.f, 0, 3.f);
		}
	}

	// Update the visibility
	UpdateTargetVisiblity(bHasLOS);
}