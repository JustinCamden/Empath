// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathKinematicVelocityComponent.h"
#include "Runtime/Engine/Public/EngineUtils.h"


// Sets default values for this component's properties
UEmpathKinematicVelocityComponent::UEmpathKinematicVelocityComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// We don't necessarily want to be active at the start of the game. Most likely we will activate on a delay.
	bAutoActivate = false;

	SampleTime = 0.1f;

}


// Called when the game starts
void UEmpathKinematicVelocityComponent::BeginPlay()
{
	Super::BeginPlay();

	// Ensure we are not ticking when we don't want to.
	if (!bIsActive)
	{
		SetComponentTickEnabled(false);
	}

}


// Called every frame
void UEmpathKinematicVelocityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	CalculateKinematicVelocity();
}

void UEmpathKinematicVelocityComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	// Initialize last position and rotation to this position and rotation,
	// so that our calculations don't think we suddenly jumped places.
	if (bIsActive)
	{
		LastLocation = GetComponentLocation();
		LastRotation = GetComponentQuat();
		VelocityHistory.Empty();
	}
}

void UEmpathKinematicVelocityComponent::Deactivate()
{
	Super::Deactivate();

	// Clear out old variables
	if (!bIsActive)
	{
		CurrentKinematicVelocity = FVector::ZeroVector;
		LastKinematicVelocity = FVector::ZeroVector;
		CurrentKinematicAngularVelocity = FVector::ZeroVector;
		LastKinematicAngularVelocity = FVector::ZeroVector;
		DeltaLocation = FVector::ZeroVector;
		LastRotation = FQuat::Identity;
	}
}

void UEmpathKinematicVelocityComponent::CalculateKinematicVelocity()
{
	// Archive old kinematic velocity
	LastKinematicVelocity = CurrentKinematicVelocity;
	LastFrameVelocity = CurrentFrameVelocity;
	LastKinematicAngularVelocity = CurrentKinematicAngularVelocity;

	// If for some reason no seconds have passed, don't do anything
	UWorld* World = GetWorld();
	float DeltaSeconds = World->GetDeltaSeconds();
	if (DeltaSeconds <= SMALL_NUMBER)
	{
		return;
	}

	// Get the velocity this frame by the change in location / change in time.
	FVector CurrentLocation = GetComponentLocation();
	DeltaLocation = CurrentLocation - LastLocation;
	CurrentFrameVelocity = (DeltaLocation / DeltaSeconds);

	// Next get the current angular velocity by the delta rotation
	FQuat CurrentRotation = GetComponentQuat();
	DeltaRotation = LastRotation.Inverse() * CurrentRotation;
	FVector Axis;
	float Angle;
	DeltaRotation.ToAxisAndAngle(Axis, Angle);
	CurrentFrameAngularVelocity = CurrentRotation.RotateVector((Axis * Angle) / DeltaSeconds);

	// Next get the velocity over the sample time if appropriate.
	if (SampleTime > 0.0f)
	{

		// Log the velocity and the timestamp
		VelocityHistory.Add(FEmpathVelocityFrame(CurrentFrameVelocity, CurrentFrameAngularVelocity, World->GetTimeSeconds()));

		// Loop through and average the array to get our new kinematic velocity.
		// Clean up while we're at it. This should take at most one iteration through the array.
		int32 NumToRemove = 0;
		for (int32 Idx = 0; Idx < VelocityHistory.Num(); ++Idx)
		{
			FEmpathVelocityFrame& VF = VelocityHistory[Idx];
			if (World->TimeSince(VF.FrameTimeStamp) > SampleTime)
			{
				NumToRemove++;
			}
			else
			{
				break;
			}
		}

		// Remove expired frames
		if (NumToRemove > 0)
		{
			VelocityHistory.RemoveAt(0, NumToRemove);
		}


		// Remaining history array is now guaranteed to be inside the time threshold.
		// Add it all up and average by the remaining frames to get the current kinematic velocity.
		FVector TotalVelocity = FVector::ZeroVector;
		FVector TotalAngularVelocity = FVector::ZeroVector;
		for (FEmpathVelocityFrame& VF : VelocityHistory)
		{
			TotalVelocity += VF.Velocity;
			TotalAngularVelocity += VF.AngularVelocity;
		}
		CurrentKinematicVelocity = TotalVelocity / (float)VelocityHistory.Num();
		CurrentKinematicAngularVelocity = TotalAngularVelocity / (float)VelocityHistory.Num();
	}

	// If our sample time is <= 0 we just use the frame velocity.
	else
	{
		CurrentKinematicVelocity = CurrentFrameVelocity;
		CurrentKinematicAngularVelocity = CurrentFrameAngularVelocity;
	}

	// Log our current location for the next time we check our velocity.
	LastLocation = CurrentLocation;
	LastRotation = CurrentRotation;
}

