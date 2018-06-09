// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathFunctionLibrary.h"
#include "EmpathPlayerCharacter.h"
#include "EmpathGameModeBase.h"
#include "EmpathAimLocationInterface.h"
#include "AIController.h"
#include "EmpathCharacter.h"

DECLARE_CYCLE_STAT(TEXT("PredictProjectilePath"), STAT_EMPATH_PredictProjectilePath, STATGROUP_EMPATH_FunctionLibrary);
DECLARE_CYCLE_STAT(TEXT("SuggestProjectileVelocity_CustomArc"), STAT_EMPATH_SuggestProjectileVelocity, STATGROUP_EMPATH_FunctionLibrary);

// Static consts
static const float MaxImpulsePerMass = 5000.f;
static const float MaxPointImpulseMag = 120000.f;

const FVector UEmpathFunctionLibrary::GetAimLocationOnActor(const AActor* Actor, FVector LookOrigin, FVector LookDirection)
{
	// First, check for the aim location interface
	if (Actor->GetClass()->ImplementsInterface(UEmpathAimLocationInterface::StaticClass()))
	{
		return IEmpathAimLocationInterface::Execute_GetCustomAimLocationOnActor(Actor, LookOrigin, LookDirection);
	}

	// Nect, check if the object as a controller. If so, call this function in its pawn instead.
	const AController* ControllerTarget = Cast<AController>(Actor);
	if (ControllerTarget && ControllerTarget->GetPawn())
	{
		return GetAimLocationOnActor(ControllerTarget->GetPawn(), LookDirection);
	}
	
	return GetCenterMassLocationOnActor(Actor);
}

const FVector UEmpathFunctionLibrary::GetCenterMassLocationOnActor(const AActor* Actor)
{
	// Check if the object is the vr player
	const AEmpathPlayerCharacter* VRChar = Cast<AEmpathPlayerCharacter>(Actor);
	if (VRChar)
	{
		// Aim at the defined center mass of the vr character
		return VRChar->GetVRLocation();
	}

	// Else, get the center of the bounding box
	if (Actor)
	{
		// Aim at bounds center
		FBox BoundingBox = Actor->GetComponentsBoundingBox();

		// Add Child Actor components to bounding box (since we use them)
		// Workaround for GetActorBounds not considering child actor components
		for (const UActorComponent* ActorComponent : Actor->GetComponents())
		{
			UChildActorComponent const* const CAComp = Cast<const UChildActorComponent>(ActorComponent);
			AActor const* const CA = CAComp ? CAComp->GetChildActor() : nullptr;
			if (CA)
				{
					BoundingBox += CA->GetComponentsBoundingBox();
				}
			}

		if (BoundingBox.IsValid)
		{
			FVector BoundsOrigin = BoundingBox.GetCenter();
			return BoundsOrigin;
		}
	}

	// If all else fails, aim at the target's location
	return Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
}

const float UEmpathFunctionLibrary::AngleBetweenVectors(FVector A, FVector B)
{
	return FMath::RadiansToDegrees(FMath::Acos(A.GetSafeNormal() | B.GetSafeNormal()));
}

void UEmpathFunctionLibrary::AngleAndAxisBetweenVectors(FVector A, FVector B, float &Angle, FVector &Axis)
{
	FVector ANormalized = A.GetSafeNormal();
	FVector BNormalized = B.GetSafeNormal();
	Angle = FMath::RadiansToDegrees(FMath::Acos(ANormalized | BNormalized));
	Axis = FVector::CrossProduct(ANormalized, BNormalized);
	return;
}

AEmpathAIManager* UEmpathFunctionLibrary::GetAIManager(const UObject* WorldContextObject)
{
	AEmpathGameModeBase* EmpathGMD = WorldContextObject->GetWorld()->GetAuthGameMode<AEmpathGameModeBase>();
	if (EmpathGMD)
	{
		return EmpathGMD->GetAIManager();
	}
	return nullptr;
}

const bool UEmpathFunctionLibrary::IsPlayer(AActor* Actor)
{
	if (Actor)
	{
		AController* PlayerCon = Actor->GetWorld()->GetFirstPlayerController();
		if (PlayerCon)
		{
			if (PlayerCon == Actor)
			{
				return true;
			}
			if (PlayerCon->GetPawn() == Actor)
			{
				return true;
			}
		}
	}
	return false;
}

EEmpathTeam UEmpathFunctionLibrary::GetActorTeam(const AActor* Actor)
{
	if (Actor)
	{
		// First check for the team agent interface on the actor
		{
			if (Actor->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()))
			{
				return IEmpathTeamAgentInterface::Execute_GetTeamNum(Actor);
			}
		}

		// Next, check on the controller
		{
			APawn const* const TargetPawn = Cast<APawn>(Actor);
			if (TargetPawn)
			{
				AController const* const TargetController = Cast<AController>(TargetPawn);
				if (TargetController && TargetController->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()))
				{
					return IEmpathTeamAgentInterface::Execute_GetTeamNum(TargetController);
				}
			}
		}

		// Next, check the instigator
		{
			AActor const* TargetInstigator = Actor->Instigator;
			if (TargetInstigator && TargetInstigator->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()))
			{
				return IEmpathTeamAgentInterface::Execute_GetTeamNum(Actor->Instigator);
			}
		}

		// Finally, check if the instigator controller has a team
		{
			AController* const InstigatorController = Actor->GetInstigatorController();
			if (InstigatorController && InstigatorController->GetClass()->ImplementsInterface(UEmpathTeamAgentInterface::StaticClass()))
			{
				return IEmpathTeamAgentInterface::Execute_GetTeamNum(InstigatorController);
			}
		}
	}

	// By default, return neutral
	return EEmpathTeam::Neutral;
}

const ETeamAttitude::Type UEmpathFunctionLibrary::GetTeamAttitudeBetween(const AActor* A, const AActor* B)
{
	// Get the teams
	EEmpathTeam TeamA = GetActorTeam(A);
	EEmpathTeam TeamB = GetActorTeam(B);

	// If either of them are neutral, we don't care about them
	if (TeamA == EEmpathTeam::Neutral || TeamB == EEmpathTeam::Neutral)
	{
		return ETeamAttitude::Neutral;
	}

	// If they're the same, return frendly
	if (TeamA == TeamB)
	{
		return ETeamAttitude::Friendly;
	}

	// Otherwise, they're hostile
	return ETeamAttitude::Hostile;
}

void UEmpathFunctionLibrary::AddDistributedImpulseAtLocation(USkeletalMeshComponent* SkelMesh, FVector Impulse, FVector Location, FName BoneName, float DistributionPct)
{
	if (Impulse.IsNearlyZero() || (SkelMesh == nullptr))
	{
		return;
	}

	// 
	// overall strategy here: 
	// add some portion of the overall impulse distributed across all bodies in a radius, with linear falloff, but preserving impulse direction
	// the remaining portion is applied as a straight impulse to the given body at the given location
	// Desired outcome is similar output to a simple hard whack at the loc/bone, but more stable.
	//

	// compute total mass, per-bone distance info
	TArray<float> BodyDistances;
	BodyDistances.AddZeroed(SkelMesh->Bodies.Num());
	float MaxDistance = 0.f;

	float TotalMass = 0.0f;
	for (int32 i = 0; i < SkelMesh->Bodies.Num(); ++i)
	{
		FBodyInstance* const BI = SkelMesh->Bodies[i];
		if (BI->IsValidBodyInstance())
		{
			TotalMass += BI->GetBodyMass();

			FVector const BodyLoc = BI->GetUnrealWorldTransform().GetLocation();
			float Dist = (Location - BodyLoc).Size();
			BodyDistances[i] = Dist;
			MaxDistance = FMath::Max(MaxDistance, Dist);
		}
	}

	// sanity, since we divide with these
	TotalMass = FMath::Max(TotalMass, KINDA_SMALL_NUMBER);
	MaxDistance = FMath::Max(MaxDistance, KINDA_SMALL_NUMBER);

	float const OriginalImpulseMag = Impulse.Size();
	FVector const ImpulseDir = Impulse / OriginalImpulseMag;
	float const DistributedImpulseMag = OriginalImpulseMag * DistributionPct;
	float const PointImpulseMag = FMath::Min((OriginalImpulseMag - DistributedImpulseMag), MaxPointImpulseMag);

	const float DistributedImpulseMagPerMass = FMath::Min((DistributedImpulseMag / TotalMass), MaxImpulsePerMass);
	for (int32 i = 0; i < SkelMesh->Bodies.Num(); ++i)
	{
		FBodyInstance* const BI = SkelMesh->Bodies[i];

		if (BI->IsValidBodyInstance())
		{
			// linear falloff
			float const ImpulseFalloffScale = FMath::Max(0.f, 1.f - (BodyDistances[i] / MaxDistance));
			const float ImpulseMagForThisBody = DistributedImpulseMagPerMass * BI->GetBodyMass() * ImpulseFalloffScale;
			if (ImpulseMagForThisBody > 0.f)
			{
				BI->AddImpulse(ImpulseDir*ImpulseMagForThisBody, false);
			}
		}
	}

	// add the rest as a point impulse on the loc
	SkelMesh->AddImpulseAtLocation(ImpulseDir * PointImpulseMag, Location, BoneName);
}

const FVector UEmpathFunctionLibrary::ConvertWorldDirectionToComponentSpace(const USceneComponent* SceneComponent, const FVector Direction)
{
	if (SceneComponent)
	{
		return SceneComponent->GetComponentTransform().InverseTransformVectorNoScale(Direction);
	}
	return FVector::ZeroVector;
}

const FVector UEmpathFunctionLibrary::ConvertWorldDirectionToActorSpace(const AActor* Actor, const FVector Direction)
{
	if (Actor)
	{
		return Actor->GetActorTransform().InverseTransformVectorNoScale(Direction);
	}
	return FVector::ZeroVector;
}

const FVector UEmpathFunctionLibrary::ConvertComponentDirectionToWorldSpace(const USceneComponent* SceneComponent, const FVector Direction)
{
	if (SceneComponent)
	{
		return SceneComponent->GetComponentTransform().TransformVectorNoScale(Direction);
	}
	return FVector::ZeroVector;
}

const FVector UEmpathFunctionLibrary::ConvertComponentDirectionToActorSpace(const USceneComponent* SceneComponent, const AActor* Actor, const FVector Direction)
{
	if (Actor && SceneComponent)
	{
		return Actor->GetActorTransform().InverseTransformVectorNoScale(SceneComponent->GetComponentTransform().TransformVectorNoScale(Direction));
	}
	return FVector::ZeroVector;
}

const FVector UEmpathFunctionLibrary::ConvertActorDirectionToWorldSpace(const AActor* Actor, const FVector Direction)
{
	if (Actor)
	{
		return Actor->GetActorTransform().TransformVectorNoScale(Direction);
	}
	return FVector::ZeroVector;
}


const FVector UEmpathFunctionLibrary::ConvertActorDirectionToComponentSpace(const AActor* Actor, const USceneComponent* SceneComponent, const FVector Direction)
{
	if (Actor && SceneComponent)
	{
		return SceneComponent->GetComponentTransform().InverseTransformVectorNoScale(Actor->GetActorTransform().TransformVectorNoScale(Direction));
	}
	return FVector::ZeroVector;
}

const float UEmpathFunctionLibrary::GetMagnitudeInDirection(const FVector Vector, const FVector Direction)
{
	return FVector::DotProduct(Vector, Direction.GetSafeNormal());
}

bool UEmpathFunctionLibrary::PredictProjectilePath(const UObject* WorldContextObject, FHitResult& OutHit, TArray<FVector>& OutPathPositions, FVector& OutLastTraceDestination, FVector StartPos, FVector LaunchVelocity, bool bTracePath, float ProjectileRadius, const TArray<TEnumAsByte<EObjectTypeQuery> >& ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, float DrawDebugTime, float SimFrequency, float MaxSimTime)
{
	SCOPE_CYCLE_COUNTER(STAT_EMPATH_PredictProjectilePath);
	return UGameplayStatics::Blueprint_PredictProjectilePath_ByObjectType(WorldContextObject, OutHit, OutPathPositions, OutLastTraceDestination, StartPos, LaunchVelocity, bTracePath, ProjectileRadius, ObjectTypes, bTraceComplex, ActorsToIgnore, DrawDebugType, DrawDebugTime, SimFrequency, MaxSimTime);
}

bool UEmpathFunctionLibrary::SuggestProjectileVelocity(const UObject* WorldContextObject, FVector& OutLaunchVelocity, FVector StartPos, FVector EndPos, float Arc)
{
	SCOPE_CYCLE_COUNTER(STAT_EMPATH_SuggestProjectileVelocity);
	return UGameplayStatics::SuggestProjectileVelocity_CustomArc(WorldContextObject, OutLaunchVelocity, StartPos, EndPos, /*OverrideCustomGravityZ=*/ 0.f, Arc);
}


void UEmpathFunctionLibrary::CalculateJumpTimings(const UObject* WorldContextObject, FVector LaunchVelocity, FVector StartLocation, FVector EndLocation, float& OutAscendingTime, float& OutDescendingTime)
{
	// Cache variables
	UWorld* World = WorldContextObject->GetWorld();
	float const Gravity = World->GetGravityZ();
	float const XYSpeed = LaunchVelocity.Size2D();
	float const XYDistance = FVector::DistXY(StartLocation, EndLocation);
	float const TotalTime = XYDistance / XYSpeed;

	// If we are ascending for part of the jump
	if (LaunchVelocity.Z > 0.0f)
	{
		// Calculate how long until we hit the apex of the jump
		float TimeToApex = -LaunchVelocity.Z / Gravity;

		// We hit the destination while still ascending
		if (TimeToApex > TotalTime)
		{
			OutAscendingTime = TotalTime;
			OutDescendingTime = 0.0f;
		}

		// Descent time is the remaining time after ascending
		else
		{
			OutAscendingTime = TimeToApex;
			OutDescendingTime = TotalTime - OutAscendingTime;
		}
	}

	// We are descending for the entire jump
	else
	{
		OutAscendingTime = 0.0f;
		OutDescendingTime = TotalTime;
	}
}

bool UEmpathFunctionLibrary::EmpathProjectPointToNavigation(const UObject* WorldContextObject, FVector& ProjectedPoint, FVector Point, ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass, const FVector QueryExtent)
{
	UWorld* const World = WorldContextObject->GetWorld();
	UNavigationSystem* const NavSys = UNavigationSystem::GetCurrent(World);
	if (NavSys)
	{
		// If no nav data is provided, try and find one from the world context object.
		// Should work if it is a Character or AController.
		if (NavData == nullptr)
		{
			const FNavAgentProperties* AgentProps = nullptr;
			if (const INavAgentInterface* AgentContext = Cast<INavAgentInterface>(WorldContextObject))
			{
				AgentProps = &AgentContext->GetNavAgentPropertiesRef();
			}

			NavData = (AgentProps ? NavSys->GetNavDataForProps(*AgentProps) : NavSys->GetMainNavData(FNavigationSystem::DontCreate));
		}

		// If no nav filter is provided, try and find one from the world context object.
		// Should work if it is an AController.
		if (FilterClass == nullptr)
		{
			const AAIController* AI = Cast<AAIController>(WorldContextObject);
			if (AI == nullptr)
			{
				if (const APawn* Pawn = Cast<APawn>(WorldContextObject))
				{
					AI = Cast<AAIController>(Pawn->GetController());
				}
			}

			if (AI)
			{
				FilterClass = AI->GetDefaultNavigationFilterClass();
			}
		}

		// Finally, do the actually projection
		if (NavData)
		{
			FNavLocation ProjectedNavLoc(Point);
			bool const bRet = NavSys->ProjectPointToNavigation(Point, ProjectedNavLoc, (QueryExtent.IsNearlyZero() ? INVALID_NAVEXTENT : QueryExtent), NavData,
				UNavigationQueryFilter::GetQueryFilter(*NavData, WorldContextObject, FilterClass));
			ProjectedPoint = ProjectedNavLoc.Location;
			return bRet;
		}
	}

	return false;
}

bool UEmpathFunctionLibrary::EmpathHasPathToLocation(AEmpathCharacter* EmpathCharacter, FVector Destination, ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	UWorld* const World = EmpathCharacter->GetWorld();
	UNavigationSystem* const NavSys = UNavigationSystem::GetCurrent(World);
	if (NavSys)
	{
		// Use correct nav mesh
		if (NavData == nullptr)
		{
			// If WorldContextObject is a Character or AIController, get agent properties from them.
			const FNavAgentProperties* AgentProps = &EmpathCharacter->GetNavAgentPropertiesRef();
			NavData = (AgentProps ? NavSys->GetNavDataForProps(*AgentProps) : NavSys->GetMainNavData(FNavigationSystem::DontCreate));
		}

		// Use correct filter class
		if (FilterClass == nullptr)
		{
			if (AAIController* AI = Cast<AAIController>(EmpathCharacter->GetController()))
			{
				FilterClass = AI->GetDefaultNavigationFilterClass();
			}
		}

		// Now do the path query
		if (NavData)
		{
			const FVector SourceLocation = EmpathCharacter->GetPathingSourceLocation();
			FSharedConstNavQueryFilter NavFilter = UNavigationQueryFilter::GetQueryFilter(*NavData, EmpathCharacter, FilterClass);
			FPathFindingQuery Query(*EmpathCharacter, *NavData, SourceLocation, Destination, NavFilter);
			Query.SetAllowPartialPaths(false);

			const bool bPathExists = NavSys->TestPathSync(Query, EPathFindingMode::Regular);
			return bPathExists;
		}
	}

	return false;
}

float UEmpathFunctionLibrary::GetUndilatedDeltaTime(const UObject* WorldContextObject)
{
	float CurrTimeDilation = WorldContextObject->GetWorld()->GetWorldSettings()->TimeDilation;
	return WorldContextObject->GetWorld()->GetTimeSeconds() / (CurrTimeDilation > 0.0f ? CurrTimeDilation : 1.0f);
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_BlockingHit(const struct FHitResult& Hit, bool& bBlockingHit)
{
	bBlockingHit = Hit.bBlockingHit;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_InitialOverlap(const struct FHitResult& Hit, bool& bInitialOverlap)
{
	bInitialOverlap = Hit.bStartPenetrating;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_Time(const struct FHitResult& Hit, float& Time)
{
	Time = Hit.Time;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_Distance(const struct FHitResult& Hit, float& Distance)
{
	Distance = Hit.Distance;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_Location(const struct FHitResult& Hit, FVector& Location)
{
	Location = Hit.Location;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_ImpactPoint(const struct FHitResult& Hit, FVector& ImpactPoint)
{
	ImpactPoint = Hit.ImpactPoint;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_Normal(const struct FHitResult& Hit, FVector& Normal)
{
	Normal = Hit.Normal;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_ImpactNormal(const struct FHitResult& Hit, FVector& ImpactNormal)
{
	ImpactNormal = Hit.ImpactNormal;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_PhysMat(const struct FHitResult& Hit, class UPhysicalMaterial*& PhysMat)
{
	PhysMat = Hit.PhysMaterial.Get();
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_Actor(const struct FHitResult& Hit, class AActor*& HitActor)
{
	HitActor = Hit.Actor.Get();
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_Component(const struct FHitResult& Hit, class UPrimitiveComponent*& HitComponent)
{
	HitComponent = Hit.Component.Get();
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_BoneName(const struct FHitResult& Hit, FName& HitBoneName)
{
	HitBoneName = Hit.BoneName;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_MyBoneName(const struct FHitResult& Hit, FName& MyHitBoneName)
{
	MyHitBoneName = Hit.MyBoneName;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_Item(const struct FHitResult& Hit, int32& HitItem)
{
	HitItem = Hit.Item;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_FaceIndex(const struct FHitResult& Hit, int32& FaceIndex)
{
	FaceIndex = Hit.FaceIndex;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_TraceStart(const struct FHitResult& Hit, FVector& TraceStart)
{
	TraceStart = Hit.TraceStart;
}

FORCEINLINE_DEBUGGABLE void UEmpathFunctionLibrary::BreakHit_TraceEnd(const struct FHitResult& Hit, FVector& TraceEnd)
{
	TraceEnd = Hit.TraceEnd;
}


