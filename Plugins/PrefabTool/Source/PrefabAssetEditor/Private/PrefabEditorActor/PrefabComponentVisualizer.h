// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "SceneManagement.h"

class FPrefabComponentVisualizer : public FComponentVisualizer
{
public:
	//~ Begin FComponentVisualizer Interface
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	//~ End FComponentVisualizer Interface

private:
	virtual bool IsVisualizerEnabled(const FEngineShowFlags& ShowFlags) const;
};
