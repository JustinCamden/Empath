// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EmpathTeamAgentInterface.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EmpathFunctionLibrary.generated.h"

class AEmpathAIManager;

/**
 * 
 */
UCLASS()
class EMPATH_API UEmpathFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Gets the center mass location of an actor, or the overriden location on a VR character. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Utility")
	static const FVector GetAimLocationOnActor(AActor const* Actor);

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
	static const ETeamAttitude::Type GetTeamAttitudeTowards(const AActor* A, const AActor* B);

	/** Add distributed impulse function, (borrowed from Robo Recall so if it causes problems, lets just remove it. */
	UFUNCTION(BlueprintCallable, Category = "Empath|Physics")
	static void AddDistributedImpulseAtLocation(USkeletalMeshComponent* SkelMesh, FVector Impulse, FVector Location, FName BoneName, float DistributionPct = 0.5f);
};
