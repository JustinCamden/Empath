// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EmpathTeamAgentInterface.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EmpathFunctionLibrary.generated.h"

// Stat groups for UE Profiler
DECLARE_STATS_GROUP(TEXT("Empath Function Library"), STATGROUP_EMPATH_FunctionLibrary, STATCAT_Advanced);

class AEmpathAIManager;
class AEmpathCharacter;

/**
 * 
 */
UCLASS()
class EMPATH_API UEmpathFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Gets the aim location of an actor (or its pawn if it is a controller). 
	First checks for Aim Location interface, then calls GetCenterMassLocation. 
	Look direction is used with the interface when multiple aim locations are possible. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const FVector GetAimLocationOnActor(const AActor* Actor, FVector LookOrigin = FVector::ZeroVector, FVector LookDirection = FVector::ZeroVector);

	/** Gets the center mass location of an Actor, or the VR Location if it is a VR Character. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const FVector GetCenterMassLocationOnActor(const AActor* Actor);

	/** Gets the angle between 2 vectors. (Input does not need to be normalized.) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const float AngleBetweenVectors(FVector A, FVector B);

	/** Gets the angle and axis between 2 vectors. (Input does not need to be normalized.) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static void AngleAndAxisBetweenVectors(FVector A, FVector B, float &Angle, FVector &Axis);

	/** Gets the world's AI Manager. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static AEmpathAIManager* GetAIManager(UObject* WorldContextObject);

	/** Returns whether an actor is the player. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|AI")
	static const bool IsPlayer(AActor* Actor);

	/** Gets the team of the target actor. Includes fallbacks to check the targets instigator and controller if no team is found. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Teams")
	static EEmpathTeam GetActorTeam(const AActor* Actor);

	/** Gets the attitude of two actors towards each other. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Teams")
	static const ETeamAttitude::Type GetTeamAttitudeBetween(const AActor* A, const AActor* B);

	/** Add distributed impulse function, (borrowed from Robo Recall so if it causes problems, lets just remove it. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Physics")
	static void AddDistributedImpulseAtLocation(USkeletalMeshComponent* SkelMesh, FVector Impulse, FVector Location, FName BoneName, float DistributionPct = 0.5f);

	/** Transforms a world direction (such as velocity) to be expressed in a component's relative space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const FVector ConvertWorldDirectionToComponentSpace(const USceneComponent* SceneComponent, const FVector Direction);

	/** Transforms a world direction (such as velocity) to be expressed in an actors's relative space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const FVector ConvertWorldDirectionToActorSpace(const AActor* Actor, const FVector Direction);

	/** Transforms a component direction (such as forward) to be expressed in world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const FVector ConvertComponentDirectionToWorldSpace(const USceneComponent* SceneComponent, const FVector Direction);

	/** Transforms a component direction (such as forward) to be expressed in an actors's relative space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const FVector ConvertComponentDirectionToActorSpace(const USceneComponent* SceneComponent, const AActor* Actor, const FVector Direction);

	/** Transforms an actor direction (such as forward) to be expressed in world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const FVector ConvertActorDirectionToWorldSpace(const AActor* Actor, const FVector Direction);

	/** Transforms an actor direction (such as forward) to be expressed in a component's relative space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const FVector ConvertActorDirectionToComponentSpace(const AActor* Actor, const USceneComponent* SceneComponent, const FVector Direction);

	/** Gets how far one vector (such as velocity) travels along a unit direction vector (such as world up). Automatically normalizes the directional vector. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const float GetMagnitudeInDirection(const FVector Vector, const FVector Direction);

	/**
	* Returns true if it hit something.
	* @param OutPathPositions			Predicted projectile path. Ordered series of positions from StartPos to the end. Includes location at point of impact if it hit something.
	* @param OutHit						Predicted hit result, if the projectile will hit something
	* @param OutLastTraceDestination	Goal position of the final trace it did. Will not be in the path if there is a hit.
	*/
	UFUNCTION(BlueprintCallable, Category = "Empath|Utility", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore", AdvancedDisplay = "DrawDebugTime, DrawDebugType", TraceChannel = ECC_WorldDynamic, bTracePath = true))
	static bool PredictProjectilePath(const UObject* WorldContextObject, FHitResult& OutHit, TArray<FVector>& OutPathPositions, FVector& OutLastTraceDestination, FVector StartPos, FVector LaunchVelocity, bool bTracePath, float ProjectileRadius, const TArray<TEnumAsByte<EObjectTypeQuery> >& ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, float DrawDebugTime, float SimFrequency = 30.f, float MaxSimTime = 2.f);

	/**
	* Returns the launch velocity needed for a projectile at rest at StartPos to land on EndPos.
	* @param Arc	In range (0..1). 0 is straight up, 1 is straight at target, linear in between. 0.5 would give 45 deg arc if Start and End were at same height.
	* Projectile launch speed is variable and unconstrained.
	* Does no tracing.
	*/
	UFUNCTION(BlueprintCallable, Category = "Empath|Utility", meta = (WorldContext = "WorldContextObject"))
	static bool SuggestProjectileVelocity(const UObject* WorldContextObject, FVector& OutLaunchVelocity, FVector StartPos, FVector EndPos, float Arc = 0.5f);

	/** Determines the ascent and descent time of a jump. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void CalculateJumpTimings(UObject* WorldContextObject, FVector LaunchVelocity, FVector StartLocation, FVector EndLocation, float& OutAscendingTime, float& OutDescendingTime);

	/** Custom navigation projection that uses the agent, rather than the world, as a fallback for if no nav data or filter class is provided. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI", meta = (WorldContext = "WorldContextObject"))
	static bool EmpathProjectPointToNavigation(UObject* WorldContextObject, FVector& ProjectedPoint, FVector Point, ANavigationData* NavData = nullptr, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr, const FVector QueryExtent = FVector::ZeroVector);

	/** Custom query that uses the agent, rather than the world, as a fallback for if no nav data or filter class is provided. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	static bool EmpathHasPathToLocation(AEmpathCharacter* EmpathCharacter, FVector Point, ANavigationData* NavData = nullptr, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);
};
