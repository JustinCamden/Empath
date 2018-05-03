// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabActorDetails.h"
#include "PrefabActor.h"

#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "Editor/LevelEditor/Public/LevelEditorActions.h"
#include "ActorEditorUtils.h"
#include "AssetSelection.h"
#include "ScopedTransaction.h"
#include "DetailLayoutBuilder.h"
#include "IDetailsView.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyCustomizationHelpers.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "PrefabActorDetails"


TSharedRef<IDetailCustomization> FPrefabActorDetails::MakeInstance()
{
	return MakeShareable(new FPrefabActorDetails);
}

FPrefabActorDetails::~FPrefabActorDetails()
{
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FPrefabActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{

	IDetailCategoryBuilder& PrefabCategory = DetailLayout.EditCategory( "Prefab", FText::GetEmpty(), ECategoryPriority::Important);

	TSharedPtr< SHorizontalBox > PrefabHorizontalBox;

	PrefabCategory.AddCustomRow(FText::GetEmpty(), false)
	[
		SAssignNew(PrefabHorizontalBox, SHorizontalBox)
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE

