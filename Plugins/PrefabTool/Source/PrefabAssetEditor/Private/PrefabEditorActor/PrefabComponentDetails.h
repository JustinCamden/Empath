// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"

/**
* Wraps a details panel customized for viewing prefab component
*/
class FPrefabComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	~FPrefabComponentDetails();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	
	FReply OnSelectPrefabClicked();
	FReply OnRevertPrefabClicked();
	FReply OnApplyPrefabClicked();

	FReply OnConvertPrefabToNormalActorClicked();
	FReply OnDestroyPrefabActorClicked();
	FReply OnDestroyPrefabActorHierarchyClicked();

	FReply OnGenerateBlueprintClicked();
	FReply OnUpdateBlueprintClicked();
	FReply OnUpdateBlueprintAndApplyClicked();

	enum ESetPivotOption
	{
		CurrentWidgetLoc,
		Top,
		Center,
		Bottom
	};

	FReply OnSetPrefabPivotClicked(ESetPivotOption PivotOption);

	void DoDestroySelectedPrefabActor(bool bDeleteInstanceActors);

	bool HasConnectedPrefab() const;
	bool HasConnectedBlueprint() const;
	bool HasOnlyOnePrefabSelected() const;
	bool CanApplyPrefab() const;
	bool CanUpdateAndApplyPrefab() const;

//	const FTextBlockStyle& GetConnectedTextStyle() const;

	TArray<TWeakObjectPtr<class APrefabActor>> SelectedPrefabActors;
};