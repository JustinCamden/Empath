// Copyright 2018 Team Empath All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "EmpathNavLineBatchComponent.generated.h"

/**
 * Line batcher component for drawing debug lines in editor
 */
UCLASS()
class EMPATH_API UEmpathNavLineBatchComponent : public ULineBatchComponent
{
	GENERATED_BODY()

public:

	UEmpathNavLineBatchComponent();

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;	
};

class EMPATH_API FEmpathNavLineBatcherSceneProxy : public FLineBatcherSceneProxy
{
	typedef FLineBatcherSceneProxy Super;

public:
	FEmpathNavLineBatcherSceneProxy(const UEmpathNavLineBatchComponent* InComponent);

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
};