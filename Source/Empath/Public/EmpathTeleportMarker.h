// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmpathTeleportMarker.generated.h"

class AEmpathCharacter;
class AEmpathTeleportBeacon;
class AEmpathPlayerCharacter;

UCLASS()
class EMPATH_API AEmpathTeleportMarker : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEmpathTeleportMarker();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/** Called when tracing teleport to update the location of the teleport marker. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Teleportation")
	void UpdateMarkerLocation(FVector TeleportLocation, 
		bool bLocationValid, 
		AEmpathCharacter* TargetedTeleportCharacter, 
		AEmpathTeleportBeacon* TargetedTeleportBeacon);

	/** Called by the controlling Character when the marker is made visible. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportion")
	void OnShowTeleportMarker();

	/** Called by the controlling Character when the marker is made invisible. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportion")
	void OnHideTeleportMarker();

	/** Called by the controlling Character when teleport location becomes valid. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportion")
	void OnTeleportLocationFound();

	/** Called by the controlling Character when teleport location becomes invalid. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Teleportion")
	void OnTeleportLocationLost();

	/** Reference to the player character controlling this teleport marker. */
	UPROPERTY(BlueprintReadOnly, Category = "Empath|TeleportMarker")
	AEmpathPlayerCharacter * OwningCharacter;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

};
