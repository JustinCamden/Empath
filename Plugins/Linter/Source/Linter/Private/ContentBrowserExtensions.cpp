// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "ContentBrowserExtensions.h"
#include "Linter.h"
#include "LinterSettings.h"
#include "ContentBrowserModule.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "IAssetTools.h"
#include "LinterStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "DokuwikiExporter.h"
#include "LinterCommandletExecution.h"
#include "BatchRenameTool.h"
#include "TooltipTool.h"

#define LOCTEXT_NAMESPACE "Linter"

DEFINE_LOG_CATEGORY_STATIC(LinterCBExtensions, Log, All);

//////////////////////////////////////////////////////////////////////////

// DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<FExtender>, FContentBrowserMenuExtender_SelectedPaths, const TArray<FString>& /*SelectedPaths*/);
FContentBrowserMenuExtender_SelectedPaths LinterContentBrowserPathsExtenderDelegate;
FDelegateHandle LinterContentBrowserPathsExtenderDelegateHandle;
// DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<FExtender>, FContentBrowserMenuExtender_SelectedAssets, const TArray<FAssetData>& /*SelectedAssets*/);
FContentBrowserMenuExtender_SelectedAssets LinterContentBrowserAssetsExtenderDelegate;
FDelegateHandle LinterContentBrowserAssetsExtenderDelegateHandle;


//////////////////////////////////////////////////////////////////////////
// FLinterContentBrowserSelectedPathsExtensionBase

struct FLinterContentBrowserSelectedPathsExtensionBase
{
public:
	TArray<FString> SelectedPaths;

public:
	virtual void Execute() {}
	virtual ~FLinterContentBrowserSelectedPathsExtensionBase() {}
};

//////////////////////////////////////////////////////////////////////////
// FRunLinterForPathExtension

struct FRunLinterForPathExtension : public FLinterContentBrowserSelectedPathsExtensionBase
{
	virtual void Execute() override
	{
		UE_LOG(LinterCBExtensions, Display, TEXT("Starting Linter."));

		FLinterCommandletManager* LinterCommandletManager = FModuleManager::LoadModuleChecked<FLinterModule>(TEXT("Linter")).GetLinterCommandletManager();
		if (LinterCommandletManager && LinterCommandletManager->RunLinter(SelectedPaths))
		{
			UE_LOG(LinterCBExtensions, Display, TEXT("Linter started."));
		}
		else
		{
			UE_LOG(LinterCBExtensions, Error, TEXT("Failed to start Linter. Already Linting?"));
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// FRunDokuwikiExportForPathExtension

struct FRunDokuwikiExportForPathExtension : public FLinterContentBrowserSelectedPathsExtensionBase
{
	virtual void Execute() override
	{
		UE_LOG(LinterCBExtensions, Display, TEXT("Starting Dokuwiki export."));

		TArray<UBlueprint*> Blueprints;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		for (FString Path : SelectedPaths)
		{
			TArray<FAssetData> AssetData;
			AssetRegistryModule.Get().GetAssetsByPath(*Path, AssetData, true, true);

			for (FAssetData Asset : AssetData)
			{
				if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset()))
				{
					Blueprints.AddUnique(Blueprint);
				}
			}
		}

		FDokuwikiExporter DokuwikiExporter(GetDefault<ULinterSettings>()->DokuwikiExportNamespace);
		for (UBlueprint* Blueprint : Blueprints)
		{
			DokuwikiExporter.OutputBlueprintClass(Blueprint);
		}		
	}
};

//////////////////////////////////////////////////////////////////////////
// FLinterContentBrowserSelectedAssetsExtensionBase

struct FLinterContentBrowserSelectedAssetsExtensionBase
{
public:
	TArray<FAssetData> SelectedAssets;

public:
	virtual void Execute() {}
	virtual ~FLinterContentBrowserSelectedAssetsExtensionBase() {}
};

//////////////////////////////////////////////////////////////////////////
// FTooltipToolExtension

struct FTooltipToolExtension : public FLinterContentBrowserSelectedAssetsExtensionBase
{
	virtual void Execute() override
	{
		UE_LOG(LinterCBExtensions, Display, TEXT("Opening Tooltip Tool window."));
		FTooltipTool AssetDlg(SelectedAssets);
		if (AssetDlg.ShowModal() == FTooltipTool::Confirm)
		{
			UE_LOG(LinterCBExtensions, Display, TEXT("Tooltip Tool did the thing."));
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// FBatchRenameAssetsExtension

struct FBatchRenameAssetsExtension : public FLinterContentBrowserSelectedAssetsExtensionBase
{
	virtual void Execute() override
	{
		UE_LOG(LinterCBExtensions, Display, TEXT("Starting batch rename."));
		FDlgBatchRenameTool AssetDlg;
		if (AssetDlg.ShowModal() == FDlgBatchRenameTool::Confirm)
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			TArray<FAssetRenameData> AssetsAndNames;

			for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
			{
				const FAssetData& Asset = *AssetIt;

				// Early out on assets that can not be renamed
				if (!(!Asset.IsRedirector() && Asset.AssetClass != NAME_Class && !(Asset.PackageFlags & PKG_FilterEditorOnly)))
				{
					continue;
				}

				// Work on a copy of the asset name and see if after name operations
				// if the copy is different than the original before creating rename data
				FString AssetNewName = Asset.AssetName.ToString();

				if (!AssetDlg.Find.IsEmpty())
				{
					AssetNewName.ReplaceInline(*AssetDlg.Find, *AssetDlg.Replace);
				}

				if (!AssetDlg.Prefix.IsEmpty())
				{
					if (AssetDlg.bRemovePrefix)
					{
						AssetNewName.RemoveFromStart(AssetDlg.Prefix, ESearchCase::CaseSensitive);
					}
					else
					{
						if (!AssetNewName.StartsWith(AssetDlg.Prefix, ESearchCase::CaseSensitive))
						{
							AssetNewName.InsertAt(0, AssetDlg.Prefix);
						}
					}
				}

				if (!AssetDlg.Suffix.IsEmpty())
				{
					if (AssetDlg.bRemoveSuffix)
					{
						AssetNewName.RemoveFromEnd(AssetDlg.Suffix, ESearchCase::CaseSensitive);
					}
					else
					{
						if (!AssetNewName.EndsWith(AssetDlg.Suffix, ESearchCase::CaseSensitive))
						{
							AssetNewName = AssetNewName.Append(AssetDlg.Suffix);
						}
					}
				}

				if (AssetNewName != Asset.AssetName.ToString())
				{
					AssetsAndNames.Push(FAssetRenameData(Asset.GetAsset(), Asset.PackagePath.ToString(), AssetNewName));
				}
			}

			AssetToolsModule.Get().RenameAssets(AssetsAndNames);
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// FPaperContentBrowserExtensions_Impl

class FLinterContentBrowserExtensions_Impl
{
public:

	/////////////////////////////////////////////////////////////////////
	// Selected Path extension

	static void ExecuteSelectedPathFunctor(TSharedPtr<FLinterContentBrowserSelectedPathsExtensionBase> SelectedPathsFunctor)
	{
		SelectedPathsFunctor->Execute();
	}

	static void CreateLinterPathActionsSubMenu(FMenuBuilder& MenuBuilder, TArray<FString> SelectedPaths)
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("LinterPathActionsSubMenuLabel", "Linter Actions"),
			LOCTEXT("LinterPathActionsSubMenuToolTip", "Linter-related actions for this path."),
			FNewMenuDelegate::CreateStatic(&FLinterContentBrowserExtensions_Impl::PopulatePathLinterActionsMenu, SelectedPaths),
			false,
			FSlateIcon()
			);
	}

	static void PopulatePathLinterActionsMenu(FMenuBuilder& MenuBuilder, TArray<FString> SelectedPaths)
	{
		// Create sprites
		TSharedPtr<FRunLinterForPathExtension> RunLinterFunctor = MakeShareable(new FRunLinterForPathExtension());
		RunLinterFunctor->SelectedPaths = SelectedPaths;

		FUIAction Action_RunLinter(
			FExecuteAction::CreateStatic(&FLinterContentBrowserExtensions_Impl::ExecuteSelectedPathFunctor, StaticCastSharedPtr<FLinterContentBrowserSelectedPathsExtensionBase>(RunLinterFunctor)));

		const FName LinterStyleSetName = FLinterStyle::Get()->GetStyleSetName();

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CB_Extension_RunLinter", "Run Linter"),
			LOCTEXT("CB_Extension_RunLinter_Tooltip", "Run Linter on selected paths."),
			FSlateIcon(LinterStyleSetName, "AssetActions.RunLinter"),
			Action_RunLinter,
			NAME_None,
			EUserInterfaceActionType::Button);


		bool bAnyBlueprintsSelected = false;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		for (FString Path : SelectedPaths)
		{
			TArray<FAssetData> AssetData;
			AssetRegistryModule.Get().GetAssetsByPath(*Path, AssetData, true, true);

			for (auto AssetIt = AssetData.CreateConstIterator(); AssetIt; ++AssetIt)
			{
				const FAssetData& Asset = *AssetIt;
				// Cannot rename redirectors or classes or cooked packages
				if (!Asset.IsRedirector() && Asset.AssetClass != NAME_Class && !(Asset.PackageFlags & PKG_FilterEditorOnly))
				{
					if (Asset.GetClass()->IsChildOf(UBlueprint::StaticClass()))
					{
						bAnyBlueprintsSelected = true;
						break;
					}
				}
			}

			if (bAnyBlueprintsSelected)
			{
				break;
			}
		}

		if (bAnyBlueprintsSelected)
		{
			TSharedPtr<FRunDokuwikiExportForPathExtension> RunDokuwikiExporterFunctor = MakeShareable(new FRunDokuwikiExportForPathExtension());
			RunDokuwikiExporterFunctor->SelectedPaths = SelectedPaths;

			FUIAction Action_RunDokuwikiExport(
				FExecuteAction::CreateStatic(&FLinterContentBrowserExtensions_Impl::ExecuteSelectedPathFunctor, StaticCastSharedPtr<FLinterContentBrowserSelectedPathsExtensionBase>(RunDokuwikiExporterFunctor)));

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CB_Extension_RunDokuwikiExport", "Run Dokuwiki Export"),
				LOCTEXT("CB_Extension_RunDokuwikiExport_Tooltip", "Run Dokuwiki Export on selected paths."),
				FSlateIcon(LinterStyleSetName, "AssetActions.RunDokuwiki"),
				Action_RunDokuwikiExport,
				NAME_None,
				EUserInterfaceActionType::Button);
		}
	}

	static TSharedRef<FExtender> OnExtendContentBrowserPathSelectionMenu(const TArray<FString>& SelectedPaths)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		// Run through the paths to determine if any meet our criteria
		bool bAnyContentPaths = false;
		for (auto PathIt = SelectedPaths.CreateConstIterator(); PathIt; ++PathIt)
		{
			const FString& Asset = *PathIt;
			bAnyContentPaths = bAnyContentPaths || Asset.StartsWith(TEXT("/Game"));
		}

		if (bAnyContentPaths)
		{
			// Add the sprite actions sub-menu extender
			Extender->AddMenuExtension(
				"PathViewFolderOptions",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateStatic(&FLinterContentBrowserExtensions_Impl::CreateLinterPathActionsSubMenu, SelectedPaths));
		}

		return Extender;
	}

	static TArray<FContentBrowserMenuExtender_SelectedPaths>& GetPathExtenderDelegates()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		return ContentBrowserModule.GetAllPathViewContextMenuExtenders();
	}

	/////////////////////////////////////////////////////////////////////
	// Selected Assets extension

	static void ExecuteSelectedAssetsFunctor(TSharedPtr<FLinterContentBrowserSelectedAssetsExtensionBase> SelectedAssetsFunctor)
	{
		SelectedAssetsFunctor->Execute();
	}

	static void CreateLinterAssetActionsSubMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("LinterAssetActionsSubMenuLabel", "Linter Actions"),
			LOCTEXT("LinterAssetActionsSubMenuToolTip", "Linter-related actions for selected assets."),
			FNewMenuDelegate::CreateStatic(&FLinterContentBrowserExtensions_Impl::PopulateAssetLinterActionsMenu, SelectedAssets),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions")
			);
	}

	static void PopulateAssetLinterActionsMenu(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
	{
		const FName LinterStyleSetName = FLinterStyle::Get()->GetStyleSetName();

		// Create batch rename tool option
		// Create sprites
		TSharedPtr<FBatchRenameAssetsExtension> BatchRenameFunctor = MakeShareable(new FBatchRenameAssetsExtension());
		BatchRenameFunctor->SelectedAssets = SelectedAssets;

		FUIAction Action_BatchRename(
			FExecuteAction::CreateStatic(&FLinterContentBrowserExtensions_Impl::ExecuteSelectedAssetsFunctor, StaticCastSharedPtr<FLinterContentBrowserSelectedAssetsExtensionBase>(BatchRenameFunctor)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("CB_Extension_BatchRename", "Batch Rename (Dangerous)..."),
			LOCTEXT("CB_Extension_BatchRename_Tooltip", "Batch rename selected assets. This is a potentially dangerous operation. Please back up your project first."),
			FSlateIcon(LinterStyleSetName, "AssetActions.BatchRename"),
			Action_BatchRename,
			NAME_None,
			EUserInterfaceActionType::Button);

		// Create Tooltip Tool option
		// Run through the assets to determine if any meet our criteria
		bool bAnyBlueprintsSelected = false;
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& Asset = *AssetIt;
			// Cannot rename redirectors or classes or cooked packages
			if (!Asset.IsRedirector() && Asset.AssetClass != NAME_Class && !(Asset.PackageFlags & PKG_FilterEditorOnly))
			{
				if (Asset.GetClass()->IsChildOf(UBlueprint::StaticClass()))
				{
					bAnyBlueprintsSelected = true;
					break;
				}			
			}
		}

		if (bAnyBlueprintsSelected)
		{
			// Create sprites
			TSharedPtr<FTooltipToolExtension> TooltipToolFunctor = MakeShareable(new FTooltipToolExtension());
			TooltipToolFunctor->SelectedAssets = SelectedAssets;

			FUIAction Action_TooltipTool(
				FExecuteAction::CreateStatic(&FLinterContentBrowserExtensions_Impl::ExecuteSelectedAssetsFunctor, StaticCastSharedPtr<FLinterContentBrowserSelectedAssetsExtensionBase>(TooltipToolFunctor)));

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CB_Extension_TooltipTool", "Tooltip Tool (Dangerous)..."),
				LOCTEXT("CB_Extension_TooltipTool_Tooltip", "Allows quicker editing of Blueprint member tooltips. This is a potentially dangerous operation. Please back up your project first."),
				FSlateIcon(LinterStyleSetName, "AssetActions.TooltipTool"),
				Action_TooltipTool,
				NAME_None,
				EUserInterfaceActionType::Button);
		}
	}

	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		// Run through the assets to determine if any meet our criteria
		bool bAnyAssetCanBeRenamed = false;
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& Asset = *AssetIt;
			// Cannot rename redirectors or classes or cooked packages
			if (!Asset.IsRedirector() && Asset.AssetClass != NAME_Class && !(Asset.PackageFlags & PKG_FilterEditorOnly))
			{
				bAnyAssetCanBeRenamed = true;
				break;
			}
		}

		if (bAnyAssetCanBeRenamed)
		{
			// Add the sprite actions sub-menu extender
			Extender->AddMenuExtension(
				"CommonAssetActions",
				EExtensionHook::After,
				nullptr,
				FMenuExtensionDelegate::CreateStatic(&FLinterContentBrowserExtensions_Impl::CreateLinterAssetActionsSubMenu, SelectedAssets));
		}

		return Extender;
	}

	static TArray<FContentBrowserMenuExtender_SelectedAssets>& GetAssetExtenderDelegates()
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		return ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	}
};

//////////////////////////////////////////////////////////////////////////
// FLinterContentBrowserExtensions

void FLinterContentBrowserExtensions::InstallHooks()
{
	LinterContentBrowserPathsExtenderDelegate = FContentBrowserMenuExtender_SelectedPaths::CreateStatic(&FLinterContentBrowserExtensions_Impl::OnExtendContentBrowserPathSelectionMenu);
	TArray<FContentBrowserMenuExtender_SelectedPaths>& CBMenuPathExtenderDelegates = FLinterContentBrowserExtensions_Impl::GetPathExtenderDelegates();
	CBMenuPathExtenderDelegates.Add(LinterContentBrowserPathsExtenderDelegate);
	LinterContentBrowserPathsExtenderDelegateHandle = CBMenuPathExtenderDelegates.Last().GetHandle();

	LinterContentBrowserAssetsExtenderDelegate = FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FLinterContentBrowserExtensions_Impl::OnExtendContentBrowserAssetSelectionMenu);
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuAssetExtenderDelegates = FLinterContentBrowserExtensions_Impl::GetAssetExtenderDelegates();
	CBMenuAssetExtenderDelegates.Add(LinterContentBrowserAssetsExtenderDelegate);
	LinterContentBrowserAssetsExtenderDelegateHandle = CBMenuAssetExtenderDelegates.Last().GetHandle();
}

void FLinterContentBrowserExtensions::RemoveHooks()
{
	TArray<FContentBrowserMenuExtender_SelectedPaths>& CBMenuPathExtenderDelegates = FLinterContentBrowserExtensions_Impl::GetPathExtenderDelegates();
	CBMenuPathExtenderDelegates.RemoveAll([](const FContentBrowserMenuExtender_SelectedPaths& Delegate) { return Delegate.GetHandle() == LinterContentBrowserPathsExtenderDelegateHandle; });

	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuAssetExtenderDelegates = FLinterContentBrowserExtensions_Impl::GetAssetExtenderDelegates();
	CBMenuAssetExtenderDelegates.RemoveAll([](const FContentBrowserMenuExtender_SelectedAssets& Delegate) { return Delegate.GetHandle() == LinterContentBrowserAssetsExtenderDelegateHandle; });
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE