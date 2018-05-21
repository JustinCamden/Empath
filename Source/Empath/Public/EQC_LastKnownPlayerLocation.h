// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EQC_LastKnownPlayerLocation.generated.h"

/**
 * 
 */
UCLASS()
class EMPATH_API UEQC_LastKnownPlayerLocation : public UEnvQueryContext
{
	GENERATED_BODY()
public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;

	
	
	
	
};
