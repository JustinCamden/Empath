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
	static AEmpathAIManager* GetAIManager(const UObject* WorldContextObject);

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
	static void CalculateJumpTimings(const UObject* WorldContextObject, FVector LaunchVelocity, FVector StartLocation, FVector EndLocation, float& OutAscendingTime, float& OutDescendingTime);

	/** Custom navigation projection that uses the agent, rather than the world, as a fallback for if no nav data or filter class is provided. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI", meta = (WorldContext = "WorldContextObject"))
	static bool EmpathProjectPointToNavigation(const UObject* WorldContextObject, FVector& ProjectedPoint, FVector Point, ANavigationData* NavData = nullptr, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr, const FVector QueryExtent = FVector::ZeroVector);

	/** Custom query that uses the agent, rather than the world, as a fallback for if no nav data or filter class is provided. */
	UFUNCTION(BlueprintCallable, Category = "Empath|AI")
	static bool EmpathHasPathToLocation(AEmpathCharacter* EmpathCharacter, FVector Point, ANavigationData* NavData = nullptr, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	/** Gets delta time in real seconds, unaffected by global time dilation. Does NOT account for game paused. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Utility", meta = (WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static float GetUndilatedDeltaTime(const UObject* WorldContextObject);


	// Wrappers to BreakHitResult that just does individual fields, not copying all members of the struct just to read 1 at a time (which is done in blueprints).
	// Also doesn't litter nativized builds with lots of extra variables.

	/** Indicates if this hit was a result of blocking collision. If false, there was no hit or it was an overlap/touch instead. */
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_BlockingHit(const struct FHitResult& Hit, bool& bBlockingHit);

	/**
	* Whether the trace started in penetration, i.e. with an initial blocking overlap.
	* In the case of penetration, if PenetrationDepth > 0.f, then it will represent the distance along the Normal vector that will result in
	* minimal contact between the swept shape and the object that was hit. In this case, ImpactNormal will be the normal opposed to movement at that location
	* (ie, Normal may not equal ImpactNormal). ImpactPoint will be the same as Location, since there is no single impact point to report.
	*/
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_InitialOverlap(const struct FHitResult& Hit, bool& bInitialOverlap);

	/**
	* 'Time' of impact along trace direction (ranging from 0.0 to 1.0) if there is a hit, indicating time between TraceStart and TraceEnd.
	* For swept movement (but not queries) this may be pulled back slightly from the actual time of impact, to prevent precision problems with adjacent geometry.
	*/
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_Time(const struct FHitResult& Hit, float& Time);

	/** The distance from the TraceStart to the Location in world space. This value is 0 if there was an initial overlap (trace started inside another colliding object). */
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_Distance(const struct FHitResult& Hit, float& Distance);

	/**
	* The location in world space where the moving shape would end up against the impacted object, if there is a hit. Equal to the point of impact for line tests.
	* Example: for a sphere trace test, this is the point where the center of the sphere would be located when it touched the other object.
	* For swept movement (but not queries) this may not equal the final location of the shape since hits are pulled back slightly to prevent precision issues from overlapping another surface.
	*/
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_Location(const struct FHitResult& Hit, FVector& Location);

	/**
	* Location in world space of the actual contact of the trace shape (box, sphere, ray, etc) with the impacted object.
	* Example: for a sphere trace test, this is the point where the surface of the sphere touches the other object.
	* @note: In the case of initial overlap (bStartPenetrating=true), ImpactPoint will be the same as Location because there is no meaningful single impact point to report.
	*/
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_ImpactPoint(const struct FHitResult& Hit, FVector& ImpactPoint);

	/**
	* Normal of the hit in world space, for the object that was swept. Equal to ImpactNormal for line tests.
	* This is computed for capsules and spheres, otherwise it will be the same as ImpactNormal.
	* Example: for a sphere trace test, this is a normalized vector pointing in towards the center of the sphere at the point of impact.
	*/
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_Normal(const struct FHitResult& Hit, FVector& Normal);

	/**
	* Normal of the hit in world space, for the object that was hit by the sweep, if any.
	* For example if a box hits a flat plane, this is a normalized vector pointing out from the plane.
	* In the case of impact with a corner or edge of a surface, usually the "most opposing" normal (opposed to the query direction) is chosen.
	*/
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_ImpactNormal(const struct FHitResult& Hit, FVector& ImpactNormal);

	/**
	* Physical material that was hit.
	* @note Must set bReturnPhysicalMaterial on the swept PrimitiveComponent or in the query params for this to be returned.
	*/
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_PhysMat(const struct FHitResult& Hit, class UPhysicalMaterial*& PhysMat);

	/** Actor hit by the trace. */
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_Actor(const struct FHitResult& Hit, class AActor*& HitActor);

	/** PrimitiveComponent hit by the trace. */
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_Component(const struct FHitResult& Hit, class UPrimitiveComponent*& HitComponent);

	/** Name of the _my_ bone which took part in hit event (in case of two skeletal meshes colliding). */
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_MyBoneName(const struct FHitResult& Hit, FName& MyHitBoneName);

	/** Name of bone we hit (for skeletal meshes). */
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_BoneName(const struct FHitResult& Hit, FName& HitBoneName);

	/** Extra data about item that was hit (hit primitive specific). */
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_Item(const struct FHitResult& Hit, int32& HitItem);

	/** Face index we hit (for complex hits with triangle meshes). */
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_FaceIndex(const struct FHitResult& Hit, int32& FaceIndex);

	/**
	* Start location of the trace.
	* For example if a sphere is swept against the world, this is the starting location of the center of the sphere.
	*/
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_TraceStart(const struct FHitResult& Hit, FVector& TraceStart);

	/**
	* End location of the trace; this is NOT where the impact occurred (if any), but the furthest point in the attempted sweep.
	* For example if a sphere is swept against the world, this would be the center of the sphere if there was no blocking hit.
	*/
	UFUNCTION(BlueprintPure, Category = "Empath|Utility")
	static void BreakHit_TraceEnd(const struct FHitResult& Hit, FVector& TraceEnd);
};