// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmpathTeleportBeacon.generated.h"

UCLASS()
class EMPATH_API AEmpathTeleportBeacon : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEmpathTeleportBeacon();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/** Called when targeted while tracing teleport. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportation")
	void OnTargetedForTeleport();

	/** Called when un-targeted while tracing teleport. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportation")
	void OnReleasedForTeleport();

	/** Gets the best teleport location for this actor. Returns false if there is no valid location or if teleporting to this actor is disabled. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Empath|Teleportation")
	bool GetBestTeleportLocation(FHitResult TeleportHit,
		FVector TeleportOrigin,
		FVector& OutTeleportLocation,
		ANavigationData* NavData = nullptr,
		TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr) const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	
	
};
