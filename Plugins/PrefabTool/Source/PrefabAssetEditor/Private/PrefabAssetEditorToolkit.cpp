// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabAssetEditorToolkit.h"
#include "SPrefabAssetEditor.h"
#include "PrefabAsset.h"

#include "Editor.h"
#include "EditorReimportHandler.h"
#include "Widgets/Docking/SDockTab.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "FPrefabAssetEditorToolkit"

DEFINE_LOG_CATEGORY_STATIC(LogPrefabAssetEditor, Log, All);


/* Local constants
 *****************************************************************************/

static const FName PrefabAssetEditorAppIdentifier("PrefabAssetEditorApp");
static const FName PrefabTextEditorTabId("TextEditor");


/* FPrefabAssetEditorToolkit structors
 *****************************************************************************/

FPrefabAssetEditorToolkit::FPrefabAssetEditorToolkit(const TSharedRef<ISlateStyle>& InStyle)
	: PrefabAsset(nullptr)
	, Style(InStyle)
{ }


FPrefabAssetEditorToolkit::~FPrefabAssetEditorToolkit()
{
	FReimportManager::Instance()->OnPreReimport().RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);

	GEditor->UnregisterForUndo(this);
}


/* FPrefabAssetEditorToolkit interface
 *****************************************************************************/

void FPrefabAssetEditorToolkit::Initialize(UPrefabAsset* InPrefabAsset, const EToolkitMode::Type InMode, const TSharedPtr<class IToolkitHost>& InToolkitHost)
{
	PrefabAsset = InPrefabAsset;

	// Support undo/redo
	PrefabAsset->SetFlags(RF_Transactional);
	GEditor->RegisterForUndo(this);

	// create tab layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("Standalone_PrefabAssetEditor")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()
						->SetOrientation(Orient_Vertical)
						->SetSizeCoefficient(0.66f)
						->Split
						(
							FTabManager::NewStack()
								->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
								->SetHideTabWell(true)
								->SetSizeCoefficient(0.1f)
								
						)
						->Split
						(
							FTabManager::NewStack()
								->AddTab(PrefabTextEditorTabId, ETabState::OpenedTab)
								->SetHideTabWell(true)
								->SetSizeCoefficient(0.9f)
						)
				)
		);

	FAssetEditorToolkit::InitAssetEditor(
		InMode,
		InToolkitHost,
		PrefabAssetEditorAppIdentifier,
		Layout,
		true /*bCreateDefaultStandaloneMenu*/,
		true /*bCreateDefaultToolbar*/,
		InPrefabAsset
	);

	RegenerateMenusAndToolbars();
}


/* FAssetEditorToolkit interface
 *****************************************************************************/

FString FPrefabAssetEditorToolkit::GetDocumentationLink() const
{
	return FString(TEXT(""));
}


void FPrefabAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_PrefabAssetEditor", "Prefab Asset Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(PrefabTextEditorTabId, FOnSpawnTab::CreateSP(this, &FPrefabAssetEditorToolkit::HandleTabManagerSpawnTab, PrefabTextEditorTabId))
		.SetDisplayName(LOCTEXT("PrefabTextEditorTabName", "Prefab Text Editor"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));
}


void FPrefabAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(PrefabTextEditorTabId);
}


/* IToolkit interface
 *****************************************************************************/

FText FPrefabAssetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Prefab Asset Editor");
}


FName FPrefabAssetEditorToolkit::GetToolkitFName() const
{
	return FName("PrefabAssetEditor");
}


FLinearColor FPrefabAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}


FString FPrefabAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "PrefabAsset ").ToString();
}


/* FGCObject interface
 *****************************************************************************/

void FPrefabAssetEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PrefabAsset);
}


/* FEditorUndoClient interface
*****************************************************************************/

void FPrefabAssetEditorToolkit::PostUndo(bool bSuccess)
{ }


void FPrefabAssetEditorToolkit::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}


/* FPrefabAssetEditorToolkit callbacks
 *****************************************************************************/

TSharedRef<SDockTab> FPrefabAssetEditorToolkit::HandleTabManagerSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier)
{
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;

	if (TabIdentifier == PrefabTextEditorTabId)
	{
		TabWidget = SNew(SPrefabAssetEditor, PrefabAsset, Style);
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			TabWidget.ToSharedRef()
		];
}


#undef LOCTEXT_NAMESPACE
