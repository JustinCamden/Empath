// Copyright 2017 marynate. All Rights Reserved.

#include "AssetTypeActions_Prefab.h"
#include "PrefabAsset.h"
#include "PrefabAssetEditorToolkit.h"
#include "PrefabToolHelpers.h"
#include "PrefabToolSettings.h"

#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FAssetTypeActions_Prefab::FAssetTypeActions_Prefab(const TSharedRef<ISlateStyle>& InStyle, EAssetTypeCategories::Type InAssetCategory)
	: Style(InStyle)
	, MyAssetCategory(InAssetCategory)
{ }

bool FAssetTypeActions_Prefab::CanFilter()
{
	return true;
}

void FAssetTypeActions_Prefab::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

	TArray<TWeakObjectPtr<UPrefabAsset>> PrefabAssets = GetTypedWeakObjectPtrs<UPrefabAsset>(InObjects);

	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("PrefabAsset_SelectPrefabActors", "Select Prefab Actors Using This Prefab"),
			LOCTEXT("PrefabAsset_SelectPrefabActorsToolTip", "Select Prefab Actors Using This Prefab"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=] {
					FPrefabActorUtil::SelectPrefabActorsInWorld(PrefabAssets);
				}),
				FCanExecuteAction::CreateLambda([=] {
					for (auto& PrefabAsset : PrefabAssets)
					{
						if (PrefabAsset.IsValid())
						{
							return true;
						}
					}
					return false;
				})
			)
		);
	}

#if 0
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("PrefabAsset_AddActorToPrefab", "Add Select Actors to This Prefab"),
			LOCTEXT("PrefabAsset_AddActorToPrefabToolTip", "Add Select Actors to This Prefab"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=] {
					FPrefabToolEditorUtil::AddSelectedActorToPrefabAsset(PrefabAssets[0].Get());
				}),
				FCanExecuteAction::CreateLambda([=] {
					if (PrefabAssets.Num() > 1)
					{
						return false;
					}
					if (!PrefabAssets[0].IsValid())
					{
						return false;
					}
					return false;
				})
			)
		);
	}
#endif

	if (0)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("PrefabAsset_ParseMeta", "Refresh Prefab Meta Data"),
			LOCTEXT("PrefabAsset_ParseMetaToolTip", "Refresh Prefab's Meta Data to update Asset References and Num of Actors"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=] {
					for (auto& PrefabAsset : PrefabAssets)
					{
						if (PrefabAsset.IsValid() && !PrefabAsset->PrefabContent.IsEmpty())
						{
							PrefabAsset->ParsePrefabMeta();
						}
					}
				}),
				FCanExecuteAction::CreateLambda([=] {
					for (auto& PrefabAsset : PrefabAssets)
					{
						if (PrefabAsset.IsValid() && !PrefabAsset->PrefabContent.IsEmpty())
						{
							return true;
						}
					}
					return false;
				})
			)
		);
	}
	
}

uint32 FAssetTypeActions_Prefab::GetCategories()
{
	return EAssetTypeCategories::Basic | EAssetTypeCategories::Misc | MyAssetCategory;
}

FText FAssetTypeActions_Prefab::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PrefabAsset", "Prefab");
}

UClass* FAssetTypeActions_Prefab::GetSupportedClass() const
{
	return UPrefabAsset::StaticClass();
}

FColor FAssetTypeActions_Prefab::GetTypeColor() const
{
	return FColor::Blue;
}

bool FAssetTypeActions_Prefab::HasActions(const TArray<UObject*>& InObjects) const
{
	return true;
}

UThumbnailInfo* FAssetTypeActions_Prefab::GetThumbnailInfo(UObject* Asset) const
{
	UPrefabAsset* Prefab = CastChecked<UPrefabAsset>(Asset);
	UThumbnailInfo* ThumbnailInfo = Prefab->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(Prefab);
		Prefab->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_Prefab::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	const bool bCanOpenAssetEditor = GetDefault<UPrefabToolSettings>()->ShouldEnablePrefabTextEditor();
	if (!bCanOpenAssetEditor)
	{
		return;
	}

	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto PrefabAsset = Cast<UPrefabAsset>(*ObjIt);

		if (PrefabAsset != nullptr)
		{
			TSharedRef<FPrefabAssetEditorToolkit> EditorToolkit = MakeShareable(new FPrefabAssetEditorToolkit(Style));
			EditorToolkit->Initialize(PrefabAsset, Mode, EditWithinLevelEditor);
		}
	}
}

#undef LOCTEXT_NAMESPACE
