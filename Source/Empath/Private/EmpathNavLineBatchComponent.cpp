// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathNavLineBatchComponent.h"


UEmpathNavLineBatchComponent::UEmpathNavLineBatchComponent()
{

}

FPrimitiveSceneProxy* UEmpathNavLineBatchComponent::CreateSceneProxy()
{
	return new FEmpathNavLineBatcherSceneProxy(this);
}

FEmpathNavLineBatcherSceneProxy::FEmpathNavLineBatcherSceneProxy(const UEmpathNavLineBatchComponent* InComponent)
	: Super(InComponent)
{

}

FPrimitiveViewRelevance FEmpathNavLineBatcherSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance = Super::GetViewRelevance(View);
	Relevance.bDrawRelevance &= (IsSelected() || (View && View->Family && View->Family->EngineShowFlags.Navigation));
	return Relevance;
}

