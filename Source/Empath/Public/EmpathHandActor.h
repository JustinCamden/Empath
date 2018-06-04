// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmpathHandActor.generated.h"

class USphereComponent;
class AEmpathPlayerCharacter;
class UEmpathKinematicVelocityComponent;

UCLASS()
class EMPATH_API AEmpathHandActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEmpathHandActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/** Name of the Sphere Component. */
	static FName SphereComponentName;

	/** Name of the Kinematic Velocity Component. */
	static FName KinematicVelocityComponentName;

	/** Name of the mesh component. */
	static FName MeshComponentName;

	/** Call to register this hand with the other hand and the owning player character. */
	void RegisterHand(AEmpathHandActor* InOtherHand, 
		AEmpathPlayerCharacter* InOwnCharacter,
		USceneComponent* InFollowedComponent);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** Called after being registered with the owning player character and the other hand. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Hand")
	void OnHandRegistered();

	/** Called to activate spellcasting on this hand. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Hand")
	void ActivateCasting();

	/** Called to deactivate spellcasting on this hand. */
	UFUNCTION(BlueprintNativeEvent, Category = "Empath|Hand")
	void DectivateCasting();

private:
	/** The Sphere component used for collision. */
	UPROPERTY(Category = "Empath|Hand", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USphereComponent* SphereComponent;

	/** The kinematic velocity component used for movement detection. */
	UPROPERTY(Category = "Empath|Hand", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UEmpathKinematicVelocityComponent* KinematicVelocityComponent;
	
	/** Reference to the other hand actor. */
	UPROPERTY(Category = "Empath|Hand", BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	AEmpathHandActor* OtherHand;

	/** Reference to the owning Empath Player Character hand actor. */
	UPROPERTY(Category = "Empath|Hand", BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	AEmpathPlayerCharacter* OwningCharacter;

	/** Reference to the scene component we are following. */
	UPROPERTY(Category = "Empath|Hand", BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent* FollowedComponent;

	/** The main skeletal mesh associated with this hand. */
	UPROPERTY(Category = "Empath|Hand", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* MeshComponent;
	
};
