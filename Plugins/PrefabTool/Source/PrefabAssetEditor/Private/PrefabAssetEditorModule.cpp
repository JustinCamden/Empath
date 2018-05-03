// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabAssetEditorModule.h"
#include "AssetTypeActions_Prefab.h"
#include "PrefabAssetEditorStyle.h"
#include "PrefabAssetThumbnailRenderer.h"
#include "PrefabComponentDetails.h"
#include "PrefabActorDetails.h"
#include "PrefabToolHelpers.h"
#include "PrefabToolSettings.h"
#include "PrefabAssetEditCommands.h"
#include "PrefabActor.h"
#include "PrefabComponent.h"
#include "PrefabComponentVisualizer.h"
#include "PrefabActorFactory.h"

#include "Editor.h"
#include "EditorModeManager.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "PropertyEditorModule.h"
#include "ISettingsModule.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "ISceneOutliner.h"
#include "ITreeItem.h"
#include "ActorTreeItem.h"
#include "SceneOutlinerVisitorTypes.h"
#include "Layers/ILayers.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Images/SImage.h"

#include "PropertyEditorModule.h"
#include "ISettingsModule.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Engine/Selection.h"
#include "AssetSelection.h"
#include "ClassIconFinder.h"
#include "Styling/SlateIconFinder.h"
#include "GameFramework/Volume.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FPrefabAssetEditorModule"

class SMenuThumbnail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMenuThumbnail)
		: _Width(32)
		, _Height(32)
	{}
	SLATE_ARGUMENT(uint32, Width)
		SLATE_ARGUMENT(uint32, Height)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const FAssetData& InAsset)
	{
		Asset = InAsset;

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		TSharedPtr<FAssetThumbnailPool> ThumbnailPool = LevelEditorModule.GetFirstLevelEditor()->GetThumbnailPool();

		Thumbnail = MakeShareable(new FAssetThumbnail(Asset, InArgs._Width, InArgs._Height, ThumbnailPool));

		ChildSlot
			[
				Thumbnail->MakeThumbnailWidget()
			];
	}

private:

	FAssetData Asset;
	TSharedPtr< FAssetThumbnail > Thumbnail;
};

class SAssetMenuEntry : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SAssetMenuEntry) {}
	SLATE_ARGUMENT(FText, LabelOverride)
		SLATE_END_ARGS()

		/**
		* Construct this widget.  Called by the SNew() Slate macro.
		*
		* @param	InArgs				Declaration used by the SNew() macro to construct this widget
		* @param	InViewModel			The layer the row widget is supposed to represent
		* @param	InOwnerTableView	The owner of the row widget
		*/
		void Construct(const FArguments& InArgs, const FAssetData& Asset, const TArray< FActorFactoryAssetProxy::FMenuItem >& AssetMenuOptions)
	{
		TSharedPtr< SHorizontalBox > ActorType = SNew(SHorizontalBox);

		const bool IsClass = Asset.GetClass() == UClass::StaticClass();
		const bool IsVolume = IsClass ? Cast<UClass>(Asset.GetAsset())->IsChildOf(AVolume::StaticClass()) : false;

		FText AssetDisplayName = FText::FromName(Asset.AssetName);
		if (IsClass)
		{
			AssetDisplayName = FText::FromString(FName::NameToDisplayString(Asset.AssetName.ToString(), false));
		}

		FText ActorTypeDisplayName;
		if (AssetMenuOptions.Num() == 1)
		{
			const FActorFactoryAssetProxy::FMenuItem& MenuItem = AssetMenuOptions[0];

			AActor* DefaultActor = NULL;
			if (IsClass && Cast<UClass>(MenuItem.AssetData.GetAsset())->IsChildOf(AActor::StaticClass()))
			{
				DefaultActor = Cast<AActor>(Cast<UClass>(MenuItem.AssetData.GetAsset())->ClassDefaultObject);
				ActorTypeDisplayName = FText::FromString(FName::NameToDisplayString(DefaultActor->GetClass()->GetName(), false));
			}

			const FSlateBrush* IconBrush = NULL;
			if (MenuItem.FactoryToUse != NULL)
			{
				DefaultActor = MenuItem.FactoryToUse->GetDefaultActor(MenuItem.AssetData);

				// Prefer the class type name set above over the factory's display name
				if (ActorTypeDisplayName.IsEmpty())
				{
					ActorTypeDisplayName = MenuItem.FactoryToUse->GetDisplayName();
				}

				IconBrush = FSlateIconFinder::FindIconBrushForClass(MenuItem.FactoryToUse->GetClass());
			}

			if (DefaultActor != NULL && (MenuItem.FactoryToUse != NULL || !IsClass))
			{
				if (!IconBrush)
				{
					IconBrush = FClassIconFinder::FindIconForActor(DefaultActor);
				}

				if (!IsClass || IsVolume)
				{
					ActorType->AddSlot()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						.Padding(2, 0)
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(ActorTypeDisplayName)
						.Font(FEditorStyle::GetFontStyle("LevelViewportContextMenu.ActorType.Text.Font"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						];

					ActorType->AddSlot()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SImage)
							.Image(IconBrush)
						.ToolTipText(ActorTypeDisplayName)
						];
				}
			}
		}

		if (!InArgs._LabelOverride.IsEmpty())
		{
			AssetDisplayName = InArgs._LabelOverride;
		}

		ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
			.Padding(4, 0, 0, 0)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(35)
			.HeightOverride(35)
			[
				SNew(SMenuThumbnail, Asset)
			]
			]

		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(2, 0, 4, 0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
			.Padding(0, 0, 0, 1)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle("LevelViewportContextMenu.AssetLabel.Text.Font"))
			.Text((IsClass && !IsVolume && !ActorTypeDisplayName.IsEmpty()) ? ActorTypeDisplayName : AssetDisplayName)
			]

		+ SVerticalBox::Slot()
			.Padding(0, 1, 0, 0)
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				ActorType.ToSharedRef()
			]
			]
			];
	}
}; 

struct FPrefabToolMenuExtend
{
	static void ExtendMenu(class FMenuBuilder& InputMenuBuilder)
	{
		TArray<AActor*> SelectedActors;
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

		const bool bHasValidActorSelected = SelectedActors.Num() > 0 && SelectedActors.Last() != NULL;
		if (!bHasValidActorSelected)
		{
			return;
		}

		AActor* LastSelectedActor = SelectedActors.Last();

		APrefabActor* LastSelectedPrefabActor = Cast<APrefabActor>(LastSelectedActor);
		const bool bLastSelectedIsPrefabActor = LastSelectedPrefabActor != NULL;
		FText LastSelectedActorLabel = bLastSelectedIsPrefabActor ? FText::FromString(LastSelectedPrefabActor->GetActorLabel()) : FText::GetEmpty();

		APrefabActor* ParentPrefabActor = FPrefabActorUtil::GetFirstAttachedParentPrefabActor(LastSelectedActor);
		const bool bSelectedParentIsPrefabActor = ParentPrefabActor != NULL;
		FText ParentPrefabActorLabel = bSelectedParentIsPrefabActor ? FText::FromString(ParentPrefabActor->GetActorLabel()) : FText::GetEmpty();
		
		InputMenuBuilder.BeginSection("Prefab", LOCTEXT("Prefab", "Prefab"));
		
		//--------------------
		// Select
		//--------------------

		// Select Parent
		if (bSelectedParentIsPrefabActor)
		{
			// - SelectParentPrefabActor
			InputMenuBuilder.AddMenuEntry(
				FPrefabAssetEditCommands::Get().SelectParentPrefabActor,
				NAME_None,
				FText::Format(LOCTEXT("SelectParentPrefabActor", "Select Parent \"{0}\""), ParentPrefabActorLabel),
				LOCTEXT("SelectParentPrefabActorTooltip", "Select Parent Prefab Actor")
			);

			// - CollapseToParentPrefabActor
			InputMenuBuilder.AddMenuEntry(
				FPrefabAssetEditCommands::Get().CollapseToParentPrefabActor,
				NAME_None,
				FText::Format(LOCTEXT("CollapseToParentPrefabActor", "Collapse and Select Parent \"{0}\""), ParentPrefabActorLabel),
				LOCTEXT("CollapseToParentPrefabActorTooltip", "Collapse Scene Outliner to Parent Prefab Actor and Select Parent Prefab Actor")
			);
		}

		// - Select Children
		if (bLastSelectedIsPrefabActor && !LastSelectedPrefabActor->GetLockSelection())
		{
			// - SelectChildrenOfSelectedPrefabActor
			InputMenuBuilder.AddMenuEntry(
				FPrefabAssetEditCommands::Get().SelectChildrenOfSelectedPrefabActor,
				NAME_None,
				FText::Format(LOCTEXT("SelectChildrenOfSelectedPrefabActor", "Select Children of \"{0}\""), LastSelectedActorLabel),
				LOCTEXT("SelectChildrenOfSelectedPrefabActorTooltip", "Select Children of Selected Prefab Actor")
			);
		}
		if (bSelectedParentIsPrefabActor && !ParentPrefabActor->GetLockSelection())
		{
			// - SelectChildrenOfParentPrefabActor
			InputMenuBuilder.AddMenuEntry(
				FPrefabAssetEditCommands::Get().SelectChildrenOfParentPrefabActor,
				NAME_None,
				FText::Format(LOCTEXT("SelectChildrenOfParentPrefabActor", "Select Children of Parent \"{0}\""), ParentPrefabActorLabel),
				LOCTEXT("SelectChildrenOfParentPrefabActorTooltip", "Select Children of Parent Prefab Actor")
			);
		}

		//--------------------
		// Attach
		//--------------------

		// Last selected actor is a prefab actor
		if (bLastSelectedIsPrefabActor)
		{
			// - AttachToPrefabActor
			if (SelectedActors.Num() > 1)
			{
				InputMenuBuilder.AddMenuEntry(
					FPrefabAssetEditCommands::Get().AttachToPrefabActor,
					NAME_None,
					FText::Format(LOCTEXT("AttachToPrefabActor", "Attach to \"{0}\""), LastSelectedActorLabel),
					LOCTEXT("AttachToPrefabActorTooltip", "Attach selected actors to Prefab Actor.")
				);
			}
		}

		//--------------------
		// Hide/Lock/Apply
		//--------------------

		// Last selected actor or its parent is a prefab actor
		if (bLastSelectedIsPrefabActor || bSelectedParentIsPrefabActor)
		{
			APrefabActor* PrefabActor = bLastSelectedIsPrefabActor ? LastSelectedPrefabActor : ParentPrefabActor;

			// Last Selected first
			FText ActorLabel = bLastSelectedIsPrefabActor ? LastSelectedActorLabel : ParentPrefabActorLabel;

			// - ToggleHidePrefabActor
			FText ShowOrHideText = PrefabActor->IsTemporarilyHiddenInEditor() ? FText::FromString(TEXT("Show")) : FText::FromString(TEXT("Hide"));
			InputMenuBuilder.AddMenuEntry(
				FPrefabAssetEditCommands::Get().ToggleHidePrefabActor, 
				NAME_None, 
				FText::Format(LOCTEXT("ToggleHidePrefabActor", "{0} \"{1}\""), ShowOrHideText, ActorLabel), 
				LOCTEXT("ToggleHidePrefabActorTooltip", "Toggle Hide Status of Current Prefab Actor and all Chidren Actors"));

			// - TogglePrefabActorLockSelection
			FText LockOrUnlockText = PrefabActor->GetLockSelection() ? FText::FromString(TEXT("UnLock")) : FText::FromString(TEXT("Lock"));
			InputMenuBuilder.AddMenuEntry(
				FPrefabAssetEditCommands::Get().TogglePrefabActorLockSelection,
				NAME_None,
				FText::Format(LOCTEXT("TogglePrefabActorLockSelection", "{0} \"{1}\""), LockOrUnlockText, ActorLabel),
				LOCTEXT("TogglePrefabActorLockSelectionTooltip", "Toggle Lock Selection of Current Prefab Actor")
			);

			// - ApplyAndLockPrefabActor
			if (bSelectedParentIsPrefabActor || (bLastSelectedIsPrefabActor && !LastSelectedPrefabActor->GetLockSelection()))
			{
				// Parent fist
				FText TargetActorLabel = bSelectedParentIsPrefabActor 
					? FText::FromString(ParentPrefabActor->GetActorLabel())
					: FText::FromString(LastSelectedPrefabActor->GetActorLabel());

				InputMenuBuilder.AddMenuEntry(
					FPrefabAssetEditCommands::Get().ApplyAndLockPrefabActor,
					NAME_None,
					FText::Format(LOCTEXT("ApplyAndLockPrefabActor", "Apply and Lock \"{0}\""), TargetActorLabel),
					LOCTEXT("ApplyAndLockPrefabActorTooltip", "Apply and Lock Prefab Actor Selection")
				);
			}
		}

		InputMenuBuilder.EndSection();

		//--------------------
		// Pivot
		//--------------------

		// Last selected actor or its parent is a prefab actor
		if (bLastSelectedIsPrefabActor || bSelectedParentIsPrefabActor)
		{
			APrefabActor* PrefabActor = bLastSelectedIsPrefabActor ? LastSelectedPrefabActor : ParentPrefabActor;

			if (PrefabActor->GetPrefab())
			{
				// - SetPrefabPivotToCurrentWidgetLocation

				FText TargetPrefabName = FText::FromString(PrefabActor->GetPrefab()->GetName());
				FAssetData PrefabAssetData(PrefabActor->GetPrefab());

				TArray< FActorFactoryAssetProxy::FMenuItem > AssetMenuOptions;
				FActorFactoryAssetProxy::GenerateActorFactoryMenuItems(PrefabAssetData, &AssetMenuOptions, true);

				InputMenuBuilder.BeginSection("SetPrefabPivot", LOCTEXT("SetPrefabPivot", "Set Prefab Pivot"));
				{
// 					FUIAction Action(FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::SetPrefabPivotToCurrentWidgetLocation));
// 					InputMenuBuilder.AddMenuEntry(Action, SNew(SAssetMenuEntry, PrefabAssetData, AssetMenuOptions));
					InputMenuBuilder.AddMenuEntry(
						FPrefabAssetEditCommands::Get().SetPrefabPivotToCurrentWidgetLocation,
						NAME_None,
						LOCTEXT("CurrentWidgetLocation", "Current Widget Location"),
						LOCTEXT("SetPrefabPivotToCurrentWidgetLocationTooltip", "Set Prefab Pivot to Current Widget Location and Apply Changes to All Referenced Prefab Actors")
					);
				}
				InputMenuBuilder.EndSection();
			}
		}

		//--------------------
		// Stamp/Change/Replace
		//--------------------

		TArray<FAssetData> SelectedAssets;
		AssetSelectionUtils::GetSelectedAssets(SelectedAssets);
		const bool bHasPrefabAssetSelected = SelectedAssets.Num() > 0 && SelectedAssets.Top().AssetClass == UPrefabAsset::StaticClass()->GetFName();

		if (bHasPrefabAssetSelected)
		{
			FAssetData& PrefabAssetData = SelectedAssets.Top();

			TArray< FActorFactoryAssetProxy::FMenuItem > AssetMenuOptions;
			FActorFactoryAssetProxy::GenerateActorFactoryMenuItems(PrefabAssetData, &AssetMenuOptions, true);

			FName PrefabAssetName = PrefabAssetData.AssetName;		
			FText PrefabAssetNameText = FText::FromName(PrefabAssetName);

			// - StampPrefab
			InputMenuBuilder.BeginSection("StampPrefab", LOCTEXT("StampPrefabHeading", "Stamp Prefab"));
			{
				FUIAction Action(FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::StampPrefabActorsWithPrefab));
				InputMenuBuilder.AddMenuEntry(Action, SNew(SAssetMenuEntry, PrefabAssetData, AssetMenuOptions));
			}
			InputMenuBuilder.EndSection();


			bool bSamePrefab = false;
			if (bLastSelectedIsPrefabActor || bLastSelectedIsPrefabActor)
			{
				bSamePrefab = bLastSelectedIsPrefabActor
					? Cast<UPrefabAsset>(PrefabAssetData.GetAsset()) == LastSelectedPrefabActor->GetPrefab()
					: Cast<UPrefabAsset>(PrefabAssetData.GetAsset()) == ParentPrefabActor->GetPrefab();
			}

			// - ChangePrefab
			if (bSelectedParentIsPrefabActor || bLastSelectedIsPrefabActor)
			{
				APrefabActor* PrefabActor = bLastSelectedIsPrefabActor ? LastSelectedPrefabActor : ParentPrefabActor;

				if (!bSamePrefab)
				{
					InputMenuBuilder.BeginSection("ChangePrefab", LOCTEXT("ChangePrefabHeading", "Change Prefab to"));
					{
						FUIAction Action(FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::ChangePrefab));
						InputMenuBuilder.AddMenuEntry(Action, SNew(SAssetMenuEntry, PrefabAssetData, AssetMenuOptions));
					}
					InputMenuBuilder.EndSection();
				}
			}

			// - ReplaceSelectedActorsWithPrefab

			if (!bSamePrefab)
			{
				InputMenuBuilder.BeginSection("ReplaceActor", LOCTEXT("ReplaceActorHeading", "Replace Selected Actors with Prefab"));
				{
					FUIAction Action(FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::ReplaceSelectedActorsWithPrefab));
					InputMenuBuilder.AddMenuEntry(Action, SNew(SAssetMenuEntry, PrefabAssetData, AssetMenuOptions));
				}
				InputMenuBuilder.EndSection();
			}
		}
	}
};

TSharedRef<FExtender> ExtendLevelViewportContextMenuForPrefab(const TSharedRef<FUICommandList> CommandList, TArray<AActor*> SelectedActors)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedRef<FUICommandList> LevelEditorCommandBindings = LevelEditor.GetGlobalLevelEditorActions();

	const bool bHasValidActorSelected = SelectedActors.Num() > 0 && SelectedActors.Last() != NULL;
	if (bHasValidActorSelected)
	{
		Extender->AddMenuExtension(
			"ActorControl", EExtensionHook::After, LevelEditorCommandBindings,
			FMenuExtensionDelegate::CreateStatic(&FPrefabToolMenuExtend::ExtendMenu));
	}

	return Extender;
}

FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors LevelViewportContextMenuPrefabExtender;

/**
 * Implements the PrefabAssetEditor module.
 */
TSharedPtr<FExtensibilityManager> FPrefabAssetEditorModule::GetMenuExtensibilityManager()
{
	return MenuExtensibilityManager;
}

TSharedPtr<FExtensibilityManager> FPrefabAssetEditorModule::GetToolBarExtensibilityManager()
{
	return ToolBarExtensibilityManager;
}

void FPrefabAssetEditorModule::StartupModule()
{
	FPrefabAssetEditorStyle::Initialize();

	RegisterActorFactories();
	RegisterAssetTools();
	RegisterMenuExtensions();
	RegisterThumbnailRenderer();
	RegisterEditorDelegates();
	RegisterSettings();
	RegisterCommands();
	RegisterComponentVisualizer();
}

void FPrefabAssetEditorModule::ShutdownModule()
{
	UnregisterAssetTools();
	UnregisterMenuExtensions();
	//UThumbnailManager::Get().UnregisterCustomRenderer(UPrefabAsset::StaticClass());
	UnregisterEditorDelegates();
	UnregisterSettings();

	FPrefabAssetEditorStyle::Shutdown();
}

void FPrefabAssetEditorModule::RegisterActorFactories()
{
	// add actor factories
	UPrefabActorFactory* PrefabActorFactory = NewObject<UPrefabActorFactory>();
	GEditor->ActorFactories.Add(PrefabActorFactory);
}

void FPrefabAssetEditorModule::RegisterAssetTools()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	EAssetTypeCategories::Type PrefabAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Prefab")), LOCTEXT("PrefabAssetCategory", "Prefab"));

	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_Prefab(FPrefabAssetEditorStyle::StyleInstance.ToSharedRef(), PrefabAssetCategoryBit)));
}

void FPrefabAssetEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	RegisteredAssetTypeActions.Add(Action);
}

void FPrefabAssetEditorModule::UnregisterAssetTools()
{
	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

	if (AssetToolsModule != nullptr)
	{
		IAssetTools& AssetTools = AssetToolsModule->Get();

		for (auto Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action);
		}
	}
}

void FPrefabAssetEditorModule::RegisterMenuExtensions()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();

	LevelViewportContextMenuPrefabExtender = FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(&ExtendLevelViewportContextMenuForPrefab);
	MenuExtenders.Add(LevelViewportContextMenuPrefabExtender);
	LevelViewportContextMenuPrefabExtenderDelegateHandle = MenuExtenders.Last().GetHandle();
}

void FPrefabAssetEditorModule::UnregisterMenuExtensions()
{
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	if (LevelViewportContextMenuPrefabExtenderDelegateHandle.IsValid())
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule)
		{
			typedef FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors DelegateType;
			LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll([=](const DelegateType& In) { return In.GetHandle() == LevelViewportContextMenuPrefabExtenderDelegateHandle; });
		}
	}
}

void FPrefabAssetEditorModule::RegisterEditorDelegates()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("PrefabComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FPrefabComponentDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("PrefabActor", FOnGetDetailCustomizationInstance::CreateStatic(&FPrefabActorDetails::MakeInstance));

	FEditorDelegates::OnMapOpened.AddRaw(this, &FPrefabAssetEditorModule::OnMapOpened);
	FEditorDelegates::PreSaveWorld.AddRaw(this, &FPrefabAssetEditorModule::OnPreSaveWorld);
	FEditorDelegates::PostSaveWorld.AddRaw(this, &FPrefabAssetEditorModule::OnPostSaveWorld);

	// Selection change
	USelection::SelectionChangedEvent.AddRaw(this, &FPrefabAssetEditorModule::OnLevelSelectionChanged);
	USelection::SelectObjectEvent.AddRaw(this, &FPrefabAssetEditorModule::OnLevelSelectionChanged);
}

void FPrefabAssetEditorModule::RegisterThumbnailRenderer()
{
	UThumbnailManager::Get().RegisterCustomRenderer(UPrefabAsset::StaticClass(), UPrefabAssetThumbnailRenderer::StaticClass());
}

void FPrefabAssetEditorModule::UnregisterEditorDelegates()
{
	FEditorDelegates::OnMapOpened.RemoveAll(this);
	FEditorDelegates::PreSaveWorld.RemoveAll(this);
	FEditorDelegates::PostSaveWorld.RemoveAll(this);

	USelection::SelectionChangedEvent.RemoveAll(this);
	USelection::SelectObjectEvent.RemoveAll(this);
}

void FPrefabAssetEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Editor", "ContentEditors", "PrefabTool",
			LOCTEXT("PrefabToolEditorSettingsName", "Prefab Tool"),
			LOCTEXT("PrefabToolEditorSettingsDescription", "Configure the options of the Prefab Tool."),
			GetMutableDefault<UPrefabToolSettings>());
	}
}

void FPrefabAssetEditorModule::RegisterCommands()
{
	FPrefabAssetEditCommands::Register();
	PrefabAssetCommands = MakeShareable(new FUICommandList);

	const FPrefabAssetEditCommands& Commands = FPrefabAssetEditCommands::Get();
	PrefabAssetCommands->MapAction(Commands.AttachToPrefabActor, FExecuteAction::CreateStatic(&FPrefabActorUtil::AttachSelectedActorsToPrefabActor));
	PrefabAssetCommands->MapAction(Commands.ToggleHidePrefabActor, FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::ToggleHidePrefabActor));
	PrefabAssetCommands->MapAction(Commands.TogglePrefabActorLockSelection, FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::TogglePrefabActorLockSelection));
	PrefabAssetCommands->MapAction(Commands.ApplyAndLockPrefabActor, FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::ApplyAndLockPrefabActor));
	PrefabAssetCommands->MapAction(Commands.SelectParentPrefabActor, FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::SelectParentPrefabActor));
	PrefabAssetCommands->MapAction(Commands.CollapseToParentPrefabActor, FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::CollapseToParentPrefabActor));
	PrefabAssetCommands->MapAction(Commands.SelectChildrenOfSelectedPrefabActor, FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::SelectChildrenOfSelectedPrefabActor));
	PrefabAssetCommands->MapAction(Commands.SelectChildrenOfParentPrefabActor, FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::SelectChildrenOfParentPrefabActor));
	PrefabAssetCommands->MapAction(Commands.SetPrefabPivotToCurrentWidgetLocation, FExecuteAction::CreateStatic(&FPrefabAssetEditorModule::SetPrefabPivotToCurrentWidgetLocation));

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedRef<FUICommandList> CommandBindings = LevelEditor.GetGlobalLevelEditorActions();
	CommandBindings->Append(PrefabAssetCommands.ToSharedRef());
}

void FPrefabAssetEditorModule::RegisterComponentVisualizer()
{
	if (GUnrealEd)
	{
		GUnrealEd->RegisterComponentVisualizer(UPrefabComponent::StaticClass()->GetFName(), MakeShareable(new FPrefabComponentVisualizer()));
	}
}

void FPrefabAssetEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "ContentEditors", "PrefabTool");
	}
}

void FPrefabAssetEditorModule::OnMapOpened(const FString& Filename, bool bLoadAsTemplate)
{
	//UE_LOG(LogTemp, Display, TEXT("[PrefabAssetEditor] OnMapOpened: %s"), *Filename);

	const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>();
	const bool  bUpdateAllPrefabActorsWhenOpenMap = PrefabToolSettings->ShouldUpdateAllPrefabActorsWhenOpenMap();
	const bool bRevertIfPrefabChanged = PrefabToolSettings->ShouldCheckPrefabChangeBeforeUpdateAllPrefabActorsWhenOpenMap();
		
	if (bUpdateAllPrefabActorsWhenOpenMap)
	{
		FPrefabToolEditorUtil::RevertAllPrefabActorsInCurrentLevel(bRevertIfPrefabChanged);
	}
}

void FPrefabAssetEditorModule::OnPreSaveWorld(uint32 SaveFlags, class UWorld* World)
{
	//UE_LOG(LogTemp, Display, TEXT("[PrefabAssetEditor] OnPreSaveWorld: %d"), SaveFlags);
	FPrefabToolEditorUtil::DeleteAllTransientPrefabInstancesInCurrentLevel(World, SaveFlags);
}

void FPrefabAssetEditorModule::OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess)
{
	//UE_LOG(LogTemp, Display, TEXT("[PrefabAssetEditor] OnPostSaveWorld: %d, %d"), SaveFlags, bSuccess);
	//if (bSuccess)
	{
		FPrefabToolEditorUtil::RestoreAllTransientPrefabInstancesInCurrentLevel(World, SaveFlags);
	}
}

void FPrefabAssetEditorModule::OnLevelSelectionChanged(UObject* Obj)
{
	struct Local
	{
		static void ValidateSelectionLock(AActor* InputActor)
		{
			if (!InputActor->IsSelected())
			{
				return;
			}

			//PREFABTOOL_LOG(Display, TEXT("  Checking actor %s, parent prefab locked? %d"), *InputActor->GetActorLabel(), bParentLocked);

			if (APrefabActor* FirstLockedParentPrefabActor = FPrefabActorUtil::GetFirstLockedParentPrefabActor(InputActor))
			{
				PREFABTOOL_LOG(Display, TEXT("[OnLevelSelectionChanged.Locked]  %s => %s"), *InputActor->GetActorLabel(), *FirstLockedParentPrefabActor->GetActorLabel());
				GEditor->SelectActor(InputActor, false, /*bNotify*/ false, true);
				GEditor->SelectActor(FirstLockedParentPrefabActor, true, /*bNotify*/ false, true);
			}
		}
	};

	if (FPrefabToolEditorUtil::bSkipSelectionMonitor)
	{
		return;
	}

	const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>();
	if (PrefabToolSettings->ShouldDisableLockPrefabSelectionFeature())
	{
		return;
	}

	USelection* Selection = Cast<USelection>(Obj);
	AActor* SelectedActor = Cast<AActor>(Obj);

	if (Selection)
	{
		const int32 NumSelected = Selection->Num();

		//PREFABTOOL_LOG(Display, TEXT("[OnLevelSelectionChanged.Selection] %d"), NumSelected);

		for (int32 SelectionIndex = NumSelected - 1; SelectionIndex >= 0; --SelectionIndex)
		{
			if (AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(SelectionIndex)))
			{
				Local::ValidateSelectionLock(Actor);
			}
		}
	}
	else if (SelectedActor)
	{
		//PREFABTOOL_LOG(Display, TEXT("[OnLevelSelectionChanged.SelectedActor] %s, selected? %d"), *SelectedActor->GetActorLabel(), GEditor->GetSelectedActors()->IsSelected(SelectedActor));
		Local::ValidateSelectionLock(SelectedActor);
	}
}

AActor* FPrefabAssetEditorModule::AddPrefabActor(const FAssetData& AssetData, bool bStampMode /*= false*/, const FTransform* ActorTransform /*= NULL*/)
{
	AActor* NewActor = NULL;
	if (UPrefabActorFactory* ActorFactory = Cast<UPrefabActorFactory>(GEditor->FindActorFactoryForActorClass(APrefabActor::StaticClass())))
	{
		FText ErrorMessage;
		if (ActorFactory->CanCreateActorFrom(AssetData, ErrorMessage))
		{
			if (bStampMode)
			{
				ActorFactory->bStampMode = true;
			}

			NewActor = GEditor->UseActorFactory(ActorFactory, AssetData, ActorTransform);

			ActorFactory->bStampMode = false;
		}
	}

	return NewActor;
}

void FPrefabAssetEditorModule::SelectParentPrefabActor()
{
	if (!GEditor)
	{
		return;
	}

	if (GEditor->GetSelectedActorCount() < 1)
	{
		return;
	}

	AActor* SelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>();
	if (!SelectedActor || SelectedActor->IsPendingKillPending())
	{
		return;
	}

	if (APrefabActor* PrefabActor = FPrefabActorUtil::GetFirstAttachedParentPrefabActor(SelectedActor))
	{
		GEditor->SelectNone(/*bNoteSelectionChanged*/false, true);
		GEditor->SelectActor(PrefabActor, true, /*bNotify=*/true, true);
	}
}

void FPrefabAssetEditorModule::ToggleHidePrefabActor()
{
	if (!GEditor)
	{
		return;
	}

	if (GEditor->GetSelectedActorCount() < 1)
	{
		return;
	}

	AActor* SelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>();
	if (!SelectedActor || SelectedActor->IsPendingKillPending())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ToggleShowHidePrefabActor", "Show/Hide Prefab Actor"));

	if (APrefabActor* PrefabActor = Cast<APrefabActor>(SelectedActor))
	{
		FPrefabActorUtil::ShowHidePrefabActor(PrefabActor, PrefabActor->IsTemporarilyHiddenInEditor());
	}
	else if (APrefabActor* ParentPrefabActor = FPrefabActorUtil::GetFirstAttachedParentPrefabActor(SelectedActor))
	{
		FPrefabActorUtil::ShowHidePrefabActor(ParentPrefabActor, ParentPrefabActor->IsTemporarilyHiddenInEditor());
	}
}

void FPrefabAssetEditorModule::TogglePrefabActorLockSelection()
{
	if (!GEditor)
	{
		return;
	}

	if (GEditor->GetSelectedActorCount() < 1)
	{
		return;
	}

	AActor* SelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>();
	if (!SelectedActor || SelectedActor->IsPendingKillPending())
	{
		return;
	}
	
	const FScopedTransaction Transaction(LOCTEXT("ToggleShowPrefabActorLockSelection", "Lock/Unlock Prefab Actor"));

	APrefabActor* PrefabActor = Cast<APrefabActor>(SelectedActor) != NULL
		? Cast<APrefabActor>(SelectedActor)
		: FPrefabActorUtil::GetFirstAttachedParentPrefabActor(SelectedActor);

	if (PrefabActor)
	{
		PrefabActor->SetAndModifyLockSelection(!PrefabActor->GetLockSelection());
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(PrefabActor, true, /*bNotify*/true, true);
	}
}

void FPrefabAssetEditorModule::CollapseToParentPrefabActor()
{
	if (!GEditor)
	{
		return;
	}

	if (GEditor->GetSelectedActorCount() < 1)
	{
		return;
	}

	AActor* SelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>();
	if (!SelectedActor || SelectedActor->IsPendingKillPending())
	{
		return;
	}

	struct FToggleExpansionVisitor : SceneOutliner::IMutableTreeItemVisitor
	{
		const bool bSetExpansion;

		FToggleExpansionVisitor(bool bInSetExpansion) : bSetExpansion(bInSetExpansion) {}

		virtual void Visit(SceneOutliner::FActorTreeItem& ActorItem) const override
		{
			AActor* Actor = ActorItem.Actor.Get();
			if (Actor)
			{
				ActorItem.Flags.bIsExpanded = bSetExpansion;
			}
		}
	};

	bool bCollapseSuccess = false;
	if (APrefabActor* PrefabActor = FPrefabActorUtil::GetFirstAttachedParentPrefabActor(SelectedActor))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
		TSharedPtr<SDockTab> SceneOutlinerTab = LevelEditorTabManager->InvokeTab(FTabId("LevelEditorSceneOutliner"));
		if (SceneOutlinerTab.IsValid())
		{
			auto BorderWidget = StaticCastSharedRef<SBorder>(SceneOutlinerTab->GetContent());
			auto SceneOutlinerWidget = StaticCastSharedRef<ISceneOutliner>(BorderWidget->GetContent());
			const auto& TreeView = SceneOutlinerWidget->GetTree();
			TSet<SceneOutliner::FTreeItemPtr> ExpandedItems;
			TreeView.GetExpandedItems(ExpandedItems);

			for (SceneOutliner::FTreeItemPtr& ExpandedItem : ExpandedItems)
			{
				TSharedPtr<SceneOutliner::FActorTreeItem> ActorTreeItem = StaticCastSharedPtr<SceneOutliner::FActorTreeItem>(ExpandedItem);
				if (ActorTreeItem.IsValid() && !ActorTreeItem->Actor->IsPendingKillPending() && ActorTreeItem->Actor == PrefabActor)
				{
					const TSharedPtr<SceneOutliner::FActorTreeItem> ParentActorTreeItem = StaticCastSharedPtr<SceneOutliner::FActorTreeItem>(ActorTreeItem);
					if (ParentActorTreeItem.IsValid())
					{
						FToggleExpansionVisitor ExpansionVisitor(!TreeView.IsItemExpanded(ActorTreeItem));
						ParentActorTreeItem->Visit(ExpansionVisitor);
						bCollapseSuccess = true;
					}
					break;
				}
			}

			if (bCollapseSuccess)
			{
				GEditor->SelectNone(false, true);
				GEditor->SelectActor(PrefabActor, true, /*bNotify=*/true, true);
			}
			SceneOutlinerWidget->Refresh();
			GEditor->RedrawLevelEditingViewports();
		}
	}
}

void FPrefabAssetEditorModule::SelectChildrenOfSelectedPrefabActor()
{
	if (!GEditor)
	{
		return;
	}

	if (GEditor->GetSelectedActorCount() < 1)
	{
		return;
	}

	AActor* SelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>();
	if (!SelectedActor || SelectedActor->IsPendingKillPending())
	{
		return;
	}

	if (APrefabActor* SelectedPrefabActor = Cast<APrefabActor>(SelectedActor))
	{
		if (!SelectedPrefabActor->GetLockSelection())
		{
			const FScopedTransaction Transaction(LOCTEXT("SelectChildrenActors", "Select Children Actors"));

			//FPrefabToolEditorUtil::BeginSkipSelectionMonitor();
			{
				TArray<AActor*> AllChildrenActors;
				FPrefabActorUtil::GetAllAttachedChildren(SelectedPrefabActor, AllChildrenActors);

				GEditor->SelectNone(false, true);
				for (AActor* Child : AllChildrenActors)
				{
					GEditor->SelectActor(Child, /*InSelected*/ true, /*bNotify*/ false, /*bSelectEvenIfHidden*/ true);
				}
				GEditor->NoteSelectionChange();
			}
			//FPrefabToolEditorUtil::EndSkipSelectionMonitor();
		}
	}
}

void FPrefabAssetEditorModule::SelectChildrenOfParentPrefabActor()
{
	if (!GEditor)
	{
		return;
	}

	if (GEditor->GetSelectedActorCount() < 1)
	{
		return;
	}

	AActor* SelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>();
	if (!SelectedActor || SelectedActor->IsPendingKillPending())
	{
		return;
	}

	if (APrefabActor* ParentPrefabActor = FPrefabActorUtil::GetFirstAttachedParentPrefabActor(SelectedActor))
	{
		if (!ParentPrefabActor->GetLockSelection())
		{
			const FScopedTransaction Transaction(LOCTEXT("SelectChildrenActors", "Select Children Actors"));

			//FPrefabToolEditorUtil::BeginSkipSelectionMonitor();
			{
				TArray<AActor*> AllChildrenActors;
				FPrefabActorUtil::GetAllAttachedChildren(ParentPrefabActor, AllChildrenActors);

				GEditor->SelectNone(false, true);
				for (AActor* Child : AllChildrenActors)
				{
					GEditor->SelectActor(Child, /*InSelected*/ true, /*bNotify*/ false, /*bSelectEvenIfHidden*/ true);
				}
				GEditor->NoteSelectionChange();
			}
			//FPrefabToolEditorUtil::EndSkipSelectionMonitor();
		}
	}
}

void FPrefabAssetEditorModule::ApplyAndLockPrefabActor()
{
	if (!GEditor)
	{
		return;
	}

	if (GEditor->GetSelectedActorCount() < 1)
	{
		return;
	}

	AActor* SelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>();
	if (!SelectedActor || SelectedActor->IsPendingKillPending())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ApplyAndLockPrefabActor", "Apply and Lock Prefab Actor"));

	APrefabActor* ParentPrefabActor = FPrefabActorUtil::GetFirstAttachedParentPrefabActor(SelectedActor);

	APrefabActor* PrefabActor = ParentPrefabActor != NULL
		? ParentPrefabActor
		: Cast<APrefabActor>(SelectedActor);

	if (PrefabActor)
	{
		// Apply
		FPrefabToolEditorUtil::ApplyPrefabActor(PrefabActor);

		// Lock
		PrefabActor->SetAndModifyLockSelection(/*bInLocked*/true);
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(PrefabActor, true, /*bNotify*/true, true);
	}
}

void FPrefabAssetEditorModule::SetPrefabPivotToCurrentWidgetLocation()
{
	if (!GEditor)
	{
		return;
	}

	if (GEditor->GetSelectedActorCount() < 1)
	{
		return;
	}

	AActor* SelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>();
	if (!SelectedActor || SelectedActor->IsPendingKillPending())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetPrefabPivot", "Set Prefab Pivot"));

	APrefabActor* PrefabActor = Cast<APrefabActor>(SelectedActor) != NULL
		? Cast<APrefabActor>(SelectedActor)
		: FPrefabActorUtil::GetFirstAttachedParentPrefabActor(SelectedActor);

	if (PrefabActor && PrefabActor->GetPrefab())
	{
		FEditorModeTools& EditorModeTools = GLevelEditorModeTools();
		const FVector ActorLocation = PrefabActor->GetActorLocation();
		FVector PrefabPivotOffset = EditorModeTools.PivotLocation - ActorLocation;

		FPrefabToolEditorUtil::ApplyPrefabActor(PrefabActor, PrefabPivotOffset);

		// Clear Actor Pivot Offset to avoid confusion
		PrefabActor->SetPivotOffset(FVector::ZeroVector);

		// Re-Select parent prefab since selected actor might get destroyed
		if (!PrefabActor->IsSelected())
		{
			FPrefabToolEditorUtil::BeginSkipSelectionMonitor();
			{
				GEditor->SelectActor(PrefabActor, true, /*bNotify*/ true);
			}
			FPrefabToolEditorUtil::EndSkipSelectionMonitor();
		}

		// Re-fresh current widget location
		EditorModeTools.SetPivotLocation(PrefabActor->GetActorLocation(), /*bIncGridBase*/false);
	}
}

void FPrefabAssetEditorModule::StampPrefabActorsWithPrefab()
{
	TArray<FAssetData> SelectedAssets;
	AssetSelectionUtils::GetSelectedAssets(SelectedAssets);

	const bool bHasPrefabAssetSelected = SelectedAssets.Num() > 0 && SelectedAssets.Top().AssetClass == UPrefabAsset::StaticClass()->GetFName();

	if (bHasPrefabAssetSelected)
	{
		FAssetData AssetData = SelectedAssets.Top();
		AddPrefabActor(AssetData, /*bStampMode*/ true);
	}
}

void FPrefabAssetEditorModule::ReplaceSelectedActorsWithPrefab()
{
	TArray<FAssetData> SelectedAssets;
	AssetSelectionUtils::GetSelectedAssets(SelectedAssets);

	const bool bHasPrefabAssetSelected = SelectedAssets.Num() > 0 && SelectedAssets.Top().AssetClass == UPrefabAsset::StaticClass()->GetFName();
	if (!bHasPrefabAssetSelected)
	{
		return;
	}

	FAssetData& PrefabAssetData = SelectedAssets.Top();
	UPrefabAsset* PrefabAsset = Cast<UPrefabAsset>(PrefabAssetData.GetAsset());
	if (!PrefabAsset)
	{
		return;
	}

	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);
	if (SelectedActors.Num() < 1)
	{
		return;
	}

	if (UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass(APrefabActor::StaticClass()))
	{
		FText ErrorMessage;
		if (ActorFactory->CanCreateActorFrom(PrefabAssetData, ErrorMessage))
		{
			TArray<AActor*> FilteredActors;
			FPrefabActorUtil::FilterParentActors(SelectedActors, FilteredActors);

			// Nested prefab check
			{
				const float MessageDuration = 1.8f;
				const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>();
				const bool bNestedPrefabSupport = PrefabToolSettings->ShouldEnableNestedPrefabSupport();
				bool bAttempToNestingPrefabButNotSupported = false;
				bool bCircularNestedPrefabFound = false;
				for (int32 ActorIdx = FilteredActors.Num() - 1; ActorIdx >= 0; --ActorIdx)
				{
					AActor* OldActor = FilteredActors[ActorIdx];
					TArray<UPrefabAsset*> AllParentPrefabAssets;
					FPrefabActorUtil::GetAllParentPrefabAssets(OldActor, AllParentPrefabAssets);

					bAttempToNestingPrefabButNotSupported = !bNestedPrefabSupport && AllParentPrefabAssets.Num() > 0;
					if (bAttempToNestingPrefabButNotSupported)
					{
						break;
					}

					bCircularNestedPrefabFound = AllParentPrefabAssets.Contains(PrefabAsset);
					if (bCircularNestedPrefabFound)
					{
						break;
					}
				}

				if (bAttempToNestingPrefabButNotSupported)
				{
					FPrefabToolEditorUtil::NotifyMessage(FText::FromString(TEXT("Nested prefab support is disabled! Aborting!")), MessageDuration);
					return;
				}
				if (bCircularNestedPrefabFound)
				{
					FPrefabToolEditorUtil::NotifyMessage(FText::FromString(TEXT("Circular nested prefab detected! Aborting!")), MessageDuration);
					return;
				}
			}

			// Replace with prefab
			if (FilteredActors.Num() > 0)
			{
				const FScopedTransaction Transaction(LOCTEXT("ReplaceSelectedActorsWithPrefab", "Replace Selected Actors With Prefab"));

				struct FAttachInfo
				{
					FTransform ChildActorTransform;
					AActor* ParentActor;
					FName AttachSocket;

					FAttachInfo(const FTransform& InChildActorTransform, AActor* InParentActor, FName InAttachSocket)
						: ChildActorTransform(InChildActorTransform), ParentActor(InParentActor), AttachSocket(InAttachSocket)
					{}
				};
				TArray<FAttachInfo> AttachInfos;

				// Gather Attach Info
				for (int32 ActorIdx = 0; ActorIdx < FilteredActors.Num(); ++ActorIdx)
				{
					AActor* OldActor = FilteredActors[ActorIdx];
					AttachInfos.Emplace(OldActor->GetActorTransform(), OldActor->GetAttachParentActor(), OldActor->GetAttachParentSocketName());
				}

				// Destroy selected actors plus instance children actors
				TArray<AActor*> PrefabInstanceChildrenActors;
				for (int32 ActorIdx = 0; ActorIdx < SelectedActors.Num(); ++ActorIdx)
				{
					AActor* OldActor = SelectedActors[ActorIdx];
					if (APrefabActor* PrefabActor = Cast<APrefabActor>(OldActor))
					{
						TArray<AActor*> ChildrenActors;
						FPrefabActorUtil::GetAllAttachedChildren(PrefabActor, ChildrenActors);
						for (AActor* ChildActor : ChildrenActors)
						{
							if (ChildActor && !ChildActor->IsPendingKillPending()
								&& !SelectedActors.Contains(ChildActor)
								&& !PrefabInstanceChildrenActors.Contains(ChildActor))
							{
								PrefabInstanceChildrenActors.Add(ChildActor);
							}
						}
					}
				}
				for (int32 ActorIdx = 0; ActorIdx < SelectedActors.Num(); ++ActorIdx)
				{
					AActor* OldActor = SelectedActors[ActorIdx];
					GEditor->SelectActor(OldActor, /*bInSelected*/false, /*bNotify=*/false);
					GEditor->Layers->DisassociateActorFromLayers(OldActor);
					OldActor->GetWorld()->EditorDestroyActor(OldActor, true);
				}
				for (int32 ActorIdx = 0; ActorIdx < PrefabInstanceChildrenActors.Num(); ++ActorIdx)
				{
					AActor* OldActor = PrefabInstanceChildrenActors[ActorIdx];
					GEditor->Layers->DisassociateActorFromLayers(OldActor);
					OldActor->GetWorld()->EditorDestroyActor(OldActor, true);
				}

				// Create preafab actor and restore attachment
				for (int32 Index = 0; Index < AttachInfos.Num(); ++Index)
				{
					const FAttachInfo& AttachInfo = AttachInfos[Index];
// 					FTransform NewActorTransform = FTransform::Identity;
// 					NewActorTransform.SetLocation(AttachInfo.ChildActorTransform.GetLocation());
					const FTransform& NewActorTransform = AttachInfo.ChildActorTransform;
					AActor* NewActor = GEditor->UseActorFactory(ActorFactory, PrefabAssetData, &NewActorTransform);
					PREFABTOOL_LOG(Display, TEXT("ReplaceSelectedActorsWithPrefab at Location: %s"), *NewActorTransform.GetLocation().ToString());

					FText ReasonText;
					if (AttachInfo.ParentActor && FPrefabGEditorAdapter::GEditor_CanParentActors(AttachInfo.ParentActor, NewActor, &ReasonText))
					{
						NewActor->AttachToActor(AttachInfo.ParentActor, FAttachmentTransformRules::KeepWorldTransform, AttachInfo.AttachSocket);
					}
				}
			}
		}
	}
}

void FPrefabAssetEditorModule::ChangePrefab()
{
	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

	const bool bHasValidActorSelected = SelectedActors.Num() > 0 && SelectedActors.Last() != NULL;
	if (!bHasValidActorSelected)
	{
		return;
	}

	TArray<FAssetData> SelectedAssets;
	AssetSelectionUtils::GetSelectedAssets(SelectedAssets);
	const bool bHasPrefabAssetSelected = SelectedAssets.Num() > 0 && SelectedAssets.Top().AssetClass == UPrefabAsset::StaticClass()->GetFName();
	if (!bHasPrefabAssetSelected)
	{
		return;
	}

	FAssetData& PrefabAssetData = SelectedAssets.Top();
	UPrefabAsset* PrefabAsset = Cast<UPrefabAsset>(PrefabAssetData.GetAsset());
	if (!PrefabAsset)
	{
		return;
	}

	AActor* LastSelectedActor = SelectedActors.Last();

	APrefabActor* PrefabActorToChange = NULL;

	if (APrefabActor* PrefabActor = Cast<APrefabActor>(LastSelectedActor))
	{
		PrefabActorToChange = PrefabActor;
	}
	else if (APrefabActor* ParentPrefabActor = FPrefabActorUtil::GetFirstAttachedParentPrefabActor(LastSelectedActor))
	{
		PrefabActorToChange = ParentPrefabActor;
	}

	if (PrefabActorToChange != NULL && PrefabActorToChange->GetPrefab() != PrefabAsset)
	{
		// Nested prefab check
		{
			const float MessageDuration = 1.8f;
			const UPrefabToolSettings* PrefabToolSettings = GetDefault<UPrefabToolSettings>();
			const bool bNestedPrefabSupport = PrefabToolSettings->ShouldEnableNestedPrefabSupport();
			bool bAttempToNestingPrefabButNotSupported = false;
			bool bCircularNestedPrefabFound = false;

			TArray<UPrefabAsset*> AllParentPrefabAssets;
			FPrefabActorUtil::GetAllParentPrefabAssets(PrefabActorToChange, AllParentPrefabAssets);

			bAttempToNestingPrefabButNotSupported = !bNestedPrefabSupport && AllParentPrefabAssets.Num() > 0;
			bCircularNestedPrefabFound = AllParentPrefabAssets.Contains(PrefabAsset);

			if (bAttempToNestingPrefabButNotSupported)
			{
				FPrefabToolEditorUtil::NotifyMessage(FText::FromString(TEXT("Nested prefab support is disabled! Aborting!")), MessageDuration);
				return;
			}
			if (bCircularNestedPrefabFound)
			{
				FPrefabToolEditorUtil::NotifyMessage(FText::FromString(TEXT("Circular nested prefab detected! Aborting!")), MessageDuration);
				return;
			}
		}

		// Change Prefab
		{
			const FScopedTransaction Transaction(LOCTEXT("ChangePrefab", "Change Prefab"));
			PrefabActorToChange->SetPrefab(PrefabAsset, /*bForceRevertEvenDisconnected*/ true);

			GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDselectBSPSurfs*/ true);
			GEditor->SelectActor(PrefabActorToChange, /*bInSelected*/ true, /*bNotify*/ true);
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPrefabAssetEditorModule, PrefabAssetEditor);

