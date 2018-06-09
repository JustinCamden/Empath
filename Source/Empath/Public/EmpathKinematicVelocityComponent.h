// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "EmpathTypes.h"
#include "EmpathKinematicVelocityComponent.generated.h"

// This class exists to allow us to track the velocity of kinematic objects (like motion controllers),
// which would normally not have a velocity since they are not simulating physics

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class EMPATH_API UEmpathKinematicVelocityComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEmpathKinematicVelocityComponent();

	/** How much time, in seconds, that we sample and average to get our current kinematic velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath|Physics")
		float SampleTime;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void Activate(bool bReset) override;
	virtual void Deactivate() override;

	/** Our location on the last frame, used to calculation our kinematic velocity. Expressed in world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetLastLocation() const { return LastLocation; }

	/** Our rotation on the last frame, used to calculation our kinematic angular velocity. Expressed in world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FRotator GetLastRotation() const { return LastRotation.Rotator(); }

	/** Our change in location since the last frame, used to calculation our kinematic velocity. Expressed in world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetDeltaLocation() const { return DeltaLocation; }

	/** Our change in rotation since the last frame, used to calculation our kinematic angular velocity. Expressed in world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FRotator GetDeltaRotation() const { return DeltaRotation.Rotator(); }

	/** The kinematic velocity of this component, averaged from all the recorded velocities within the same time. Expressed in world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetCurrentKinematicVelocity() const { return CurrentKinematicVelocity; }

	/** The kinematic velocity of this component on the last frame, averaged from all the recorded velocites within the same time. Expressed in world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetLastKinematicVelocity() const { return LastKinematicVelocity; }

	/** The kinematic velocity of this component, calculated with respect to this frame only. Expressed in world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetCurrentFrameVelocity() const { return CurrentFrameVelocity; }

	/** The last kinematic velocity of this component, calculated with respect to the last frame only. Expressed in world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetLastFrameVelocity() const { return LastFrameVelocity; }

	/** The kinematic angular velocity of this component, averaged from all the recorded velocites within the same time. Expressed in Radians and world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetCurrentKinematicAngularVelocityRads() const { return CurrentKinematicAngularVelocity; }

	/** The kinematic angular velocity of this component on the last frame, averaged from all the recorded velocites within the same time. Expressed in Radians and world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetLastKinematicAngularVelocityRads() const { return LastKinematicAngularVelocity; }

	/** The kinematic angular velocity of this component, calculated with respect to this frame only. Expressed in Radians and world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetCurrentFrameAngularVelocityRads() const { return CurrentFrameAngularVelocity; }

	/** The last kinematic angular velocity of this component, calculated with respect to the last frame only. Expressed in Radians and world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetLastFrameAngularVelocityRads() const { return LastFrameAngularVelocity; }

	/** The kinematic angular velocity of this component, averaged from all the recorded velocites within the same time. Expressed in degrees and world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetCurrentKinematicAngularVelocity() const { return FVector::RadiansToDegrees(CurrentKinematicAngularVelocity); }

	/** The kinematic angular velocity of this component on the last frame, averaged from all the recorded velocites within the same time. Expressed in degrees and world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetLastKinematicAngularVelocity() const { return FVector::RadiansToDegrees(LastKinematicAngularVelocity); }

	/** The kinematic angular velocity of this component, calculated with respect to this frame only. Expressed in degrees and world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetCurrentFrameAngularVelocity() const { return FVector::RadiansToDegrees(CurrentFrameAngularVelocity); }

	/** The last kinematic angular velocity of this component, calculated with respect to the last frame only. Expressed in degrees and world space. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Empath|Physics")
		FVector GetLastFrameAngularVelocity() const { return FVector::RadiansToDegrees(LastFrameAngularVelocity); }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:
	/** Our location on the last frame, used to calculation our kinematic velocity. */
	FVector LastLocation;

	/** Our rotation on the last frame, used to calculation our kinematic angular velocity. */
	FQuat LastRotation;

	/** Our change in location since the last frame, used to calculation our kinematic velocity. */
	FVector DeltaLocation;

	/** Our change in rotation since the last frame, used to calculation our kinematic angular velocity. */
	FQuat DeltaRotation;

	/** Per-frame record of kinematic velocity. */
	TArray<FEmpathVelocityFrame> VelocityHistory;

	/** The kinematic velocity of this component, averaged from all the recorded velocities within the same time. */
	FVector CurrentKinematicVelocity;

	/** The kinematic velocity of this component on the last frame, averaged from all the recorded velocites within the same time. */
	FVector LastKinematicVelocity;

	/** The kinematic velocity of this component, calculated with respect to this frame only. */
	FVector CurrentFrameVelocity;

	/** The last kinematic velocity of this component, calculated with respect to last frame only. */
	FVector LastFrameVelocity;

	/** The kinematic angular velocity of this component, averaged from all the recorded velocities within the same time. Expressed in Radians. */
	FVector CurrentKinematicAngularVelocity;

	/** The kinematic angular velocity of this component on the last frame, averaged from all the recorded velocites within the same time. Expressed in Radians. */
	FVector LastKinematicAngularVelocity;

	/** The kinematic angular velocity of this component, calculated with respect to this frame only. Expressed in Radians. */
	FVector CurrentFrameAngularVelocity;

	/** The last angular kinematic velocity of this component, calculated with respect to last frame only. Expressed in Radians. */
	FVector LastFrameAngularVelocity;

	/** Uses our last position to calculate the kinematic velocity for this frame. */
	void CalculateKinematicVelocity();
};
