// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "EmpathPathFollowingComponent.generated.h"

/**
 * 
 */
UCLASS()
class EMPATH_API UEmpathPathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()

public:

	// Overrides
	UEmpathPathFollowingComponent();
	virtual void BeginPlay() override;

	// Exposes SetNextMoveSegment
	void AdvanceToNextMoveSegment();

	/** Whether we should currently be paused in following our path. */
	UPROPERTY(transient, BlueprintReadWrite)
	bool bPausePathfollowingVelocity;

	FVector GetLocationOnPath() const;
	void OnDecelerationPossiblyChanged();

	virtual void FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink) override;

protected:

	// Overrides
	virtual void StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint) override;
	virtual bool HasReachedCurrentTarget(const FVector& CurrentLocation) const override;
	virtual void FollowPathSegment(float DeltaTime) override;
	virtual void Reset() override;

	void AdjustMove(UPathFollowingComponent* PathFollowComp, FVector& Velocity);

	/** When using accelearation for pathfollowing, controls the magnitude of the velocity-based adjustment component of the final acceleration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float VelocityCompensationAccelAdjustmentScale;

};
