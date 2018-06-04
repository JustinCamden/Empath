// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmpathHandActor.generated.h"

class USphereComponent;
class AEmpathPlayerCharacter;

UCLASS()
class EMPATH_API AEmpathHandActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEmpathHandActor();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/** Name of the Sphere Component. */
	static FName SphereComponentName;

	/** Call to register this hand with the other hand and the owning player character. */
	void RegisterHand(AEmpathHandActor* InOtherHand, 
		AEmpathPlayerCharacter* InOwnCharacter,
		USceneComponent* InFollowedComponent);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** Called after being registered with the owning player character and the other hand. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Empath|Hand")
	void OnHandRegistered();

private:
	/** The Sphere component used for collision. */
	UPROPERTY(Category = "Empath|Hand", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USphereComponent* SphereComponent;
	
	/** Reference to the other hand actor. */
	UPROPERTY(Category = "Empath|Hand", BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	AEmpathHandActor* OtherHand;

	/** Reference to the owning Empath Player Character hand actor. */
	UPROPERTY(Category = "Empath|Hand", BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	AEmpathPlayerCharacter* OwningCharacter;

	/** Reference to the scene component we are following. */
	UPROPERTY(Category = "Empath|Hand", BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent* FollowedComponent;
	
};
