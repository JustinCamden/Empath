// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmpathTypes.h"
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
	
	// ---------------------------------------------------------
	//	Setup

	/** Name of the blocking collision. */
	static FName BlockingCollisionName;

	/** Name of the Kinematic Velocity Component. */
	static FName KinematicVelocityComponentName;

	/** Name of the mesh component. */
	static FName MeshComponentName;

	/** Name of the grip collision component. */
	static FName GripCollisionName;

	/** Name of the root component. */
	static FName MotionControllerRootName;

	/** Call to register this hand with the other hand and the owning player character. */
	UFUNCTION(BlueprintCallable, Category = EmpathHandActor)
	void RegisterHand(AEmpathHandActor* InOtherHand, 
		AEmpathPlayerCharacter* InOwningCharacter,
		USceneComponent* InFollowedComponent,
		EEmpathBinaryHand InOwningHand);

	/** Called after being registered with the owning player character and the other hand. */
	UFUNCTION(BlueprintImplementableEvent, Category = EmpathHandActor)
		void OnHandRegistered();

	// ---------------------------------------------------------
	//	Follow component

	/** The maximum distance from the component we are following before we are officially lost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EmpathHandActor)
	float FollowComponentLostThreshold;

	/** Updates whether we have currently lost the follow component, and fires events if necessary. */
	UFUNCTION(BlueprintCallable, Category = EmpathHandActor)
	void UpdateLostFollowComponent(bool bLost);

	/** Called when we first lose the follow component. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = EmpathHandActor)
	void OnLostFollowComponent();

	/** Called when we regain the follow component after it being lost. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = EmpathHandActor)
	void OnFoundFollowComponent();

	/** Returns whether the follow component is lost. */
	bool IsFollowComponentLost() const { return bLostFollowComponent; }

	/** Called every tick to instruct the hand to sweep its location and rotation to the followed component (usually a motion controller component). */
	UFUNCTION(BlueprintCallable, Category = EmpathHandActor)
	void MoveToFollowComponent();

	// ---------------------------------------------------------
	//	Teleportation

	/** Called by the owning character to determine the origin of the teleportation trace. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintPure, Category = "EmpathHandActor|Teleportation")
	FVector GetTeleportOrigin() const;

	/** Called by the owning character to determine the direction of the teleportation trace. X is forward, Y is right. Input does not need to be normalized */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintPure, Category = "EmpathHandActor|Teleportation")
	FVector GetTeleportDirection(FVector LocalDirection) const;


	// ---------------------------------------------------------
	//	General gripping

	// The current grip state of this hand.
	UPROPERTY(Category = EmpathHandActor, BlueprintReadOnly)
	EEmpathGripType GripState;

	/** The object currently held by the hand. */
	UPROPERTY(BlueprintReadOnly, Category = EmpathHandActor)
		AActor* HeldObject;

	/** Gets the nearest Actor overlapping the grip collision. */
	UFUNCTION(BlueprintCallable, Category = EmpathHandActor)
		void GetBestGripCandidate(AActor*& GripActor, UPrimitiveComponent*& GripComponent, EEmpathGripType& GripResponse);

	/** Called when the grip key is pressed and the Empath Character can grip. */
	UFUNCTION(BlueprintNativeEvent, Category = EmpathHandActor)
		void OnGripPressed();

	/** Called when the grip key is released, or when we are gripping something and grip is disabled. */
	UFUNCTION(BlueprintNativeEvent, Category = EmpathHandActor)
		void OnGripReleased();




	// ---------------------------------------------------------
	//	Casting state


	/** Called to activate spellcasting on this hand. */
	UFUNCTION(BlueprintNativeEvent, Category = EmpathHandActor)
	void ActivateCasting();

	/** Called to deactivate spellcasting on this hand. */
	UFUNCTION(BlueprintNativeEvent, Category = EmpathHandActor)
	void DectivateCasting();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


private:

	// ---------------------------------------------------------
	//	Components

	/** The scene root of this actor, used for offsetting the mesh from the VR motion controller components. */
	UPROPERTY(Category = EmpathHandActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent * MotionControllerRoot;

	/** The Sphere component used for collision. */
	UPROPERTY(Category = EmpathHandActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USphereComponent* BlockingCollision;

	/** The Sphere component used for grip collision. */
	UPROPERTY(Category = EmpathHandActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USphereComponent* GripCollision;

	/** The kinematic velocity component used for movement detection. */
	UPROPERTY(Category = EmpathHandActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UEmpathKinematicVelocityComponent* KinematicVelocityComponent;
	
	/** Reference to the other hand actor. */
	UPROPERTY(Category = EmpathHandActor, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	AEmpathHandActor* OtherHand;

	/** Reference to the owning Empath Player Character hand actor. */
	UPROPERTY(Category = EmpathHandActor, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	AEmpathPlayerCharacter* OwningCharacter;

	/** Reference to the scene component we are following. */
	UPROPERTY(Category = EmpathHandActor, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USceneComponent* FollowedComponent;

	/** The main skeletal mesh associated with this hand. */
	UPROPERTY(Category = EmpathHandActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* MeshComponent;

	/** Whether we are currently separated from the component we are following. */
	UPROPERTY(Category = EmpathHandActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	bool bLostFollowComponent;
	
	/** Which hand this actor represents. Set when registered with the Empath Player Character. */
	UPROPERTY(Category = EmpathHandActor, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	EEmpathBinaryHand OwningHand;

	/** The initial location offset we apply for this hand (automatically inverted for the left hand). */
	UPROPERTY(Category = EmpathHandActor, EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FVector ControllerOffsetLocation;

	/** The initial rotation offset we apply for this hand (automatically inverted for the left hand). */
	UPROPERTY(Category = EmpathHandActor, EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FRotator ControllerOffsetRotation;
	
};
