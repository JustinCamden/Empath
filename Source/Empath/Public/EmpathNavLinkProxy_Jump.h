// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "EmpathNavLinkProxy.h"
#include "EmpathNavLinkProxy_Jump.generated.h"

/**
 * 
 */
UCLASS()
class EMPATH_API AEmpathNavLinkProxy_Jump : public AEmpathNavLinkProxy
{
	GENERATED_BODY()

public:

	AEmpathNavLinkProxy_Jump(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;

	/** Determines the arc of the jump. 1 is directly at the target, 0 is perfectly vertical, 0.5 is midway (e.g. 45 deg from level ground) */
	UPROPERTY(EditAnywhere, Category = Jump, meta = (UIMin = 0.01f, UIMax = 0.99f, ClampMin = 0.01f, ClampMax = 0.99f))
	float JumpArc;

#if WITH_EDITORONLY_DATA
	// Whether to draw the jump arc. Data on the arc is always computed.
	UPROPERTY(EditAnywhere, Category = JumpTrace)
	bool bTraceJumpArc;

	UPROPERTY(EditAnywhere, Category = JumpTrace)
	float TraceRadius;

	// Whether to use collision the the jump arc trace.
	UPROPERTY(EditAnywhere, Category = JumpTrace)
	bool bTraceArcWithCollision;

	UPROPERTY(EditAnywhere, Category = JumpTrace)
	TEnumAsByte<ECollisionChannel> TraceCollisionChannel;

	// Quality of arc trace (steps per second).
	UPROPERTY(EditAnywhere, Category = JumpTrace, meta = (DisplayName = "Trace Arc Sim Frequency", UIMin = 5.f, UIMax = 30.f, ClampMin = 5.f, ClampMax = 30.f))
	float DrawArcSimFrequency;

	UPROPERTY(EditAnywhere, Category = JumpTrace, AdvancedDisplay, meta = (EditCondition = bTraceJumpArc))
	bool bDrawApex;

	UPROPERTY(EditAnywhere, Category = JumpTrace, AdvancedDisplay)
	TArray< TSubclassOf<AActor> > ActorTypesToIgnore;

	uint64 LastVisualizerUpdateFrame;

	UPROPERTY()
	class ULineBatchComponent* LineBatcher;
#endif // WITH_EDITORONLY_DATA


	/**
	* Data about the trace result.
	*/

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = JumpTraceResult)
	bool bTracedPath;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = JumpTraceResult)
	bool bTraceHit;

	// Position, Velocity, and time from start to the start point.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = JumpTraceResult)
	FPredictProjectilePathPointData TraceStart;

	// Position, Velocity, and time from start to the apex.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = JumpTraceResult)
	FPredictProjectilePathPointData TraceApex;

	// Position, Velocity, and time from start to the end point.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = JumpTraceResult)
	FPredictProjectilePathPointData TraceEndPoint;


	/**
	* Hooks for updating the path visualizer
	*/

	bool bWasLoaded;
	bool bRegisteredCallbacks;

#if WITH_EDITOR
	virtual void PostEditImport() override;


	/** Predicts the path of the jump. */
	bool PredictJumpPath(FPredictProjectilePathResult& OutPathResult, bool& OutHit) const;
	void SetRefreshTimer();

	virtual void SyncLinkDataToComponents() override;
#endif // WITH_EDITOR

	UFUNCTION()
	void RefreshPathVisualizer();

	virtual void PostRegisterAllComponents() override;
};
