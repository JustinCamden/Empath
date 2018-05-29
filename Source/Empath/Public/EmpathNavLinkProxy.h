// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "AI/Navigation/NavLinkProxy.h"
#include "EmpathNavLinkProxy.generated.h"

/**
 * 
 */

class AEmpathAIController;
class UBoxComponent;

UCLASS()
class EMPATH_API AEmpathNavLinkProxy : public ANavLinkProxy
{
	GENERATED_BODY()

public:
	AEmpathNavLinkProxy(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostRegisterAllComponents() override;

	virtual void SyncLinkDataToComponents();
	virtual void OnChildTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType TeleportType);
	bool bRegisteredCallbacks;
#endif // WITH_EDITOR

	bool IsClaimed() const { return ClaimedBy != nullptr; };
	void Claim(AEmpathAIController* ClaimHolder);
	void BeginReleaseClaim(bool bImmediate = false);

protected:

#if WITH_EDITORONLY_DATA
	/** Editor-only component to visualize the link start point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath")
	UBoxComponent* StartEditorComp;

	/** Editor-only component to visualize the link end point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empath")
	UBoxComponent* EndEditorComp;
#endif

	UPROPERTY(transient)
	AEmpathAIController* ClaimedBy;

	void ReleaseClaim();
	FTimerHandle ReleaseClaimTimerHandle;
	float ClaimReleaseDelay;
	
	
};
