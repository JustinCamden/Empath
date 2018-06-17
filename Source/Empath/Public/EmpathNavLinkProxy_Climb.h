// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EmpathNavLinkProxy.h"
#include "EmpathNavLinkProxy_Climb.generated.h"

class UArrowComponent;

/**
 * 
 */
UCLASS()
class EMPATH_API AEmpathNavLinkProxy_Climb : public AEmpathNavLinkProxy
{
	GENERATED_BODY()
public:
	AEmpathNavLinkProxy_Climb(const FObjectInitializer& ObjectInitializer);

	/** This should be aligned with the ledge, such that it points AWAY from the ledge */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EmpathNavLinkProxy)
	UArrowComponent* LedgeAlignmentComponent;
	
	
	
};
