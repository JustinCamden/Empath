// Copyright 2017 marynate. All Rights Reserved.

#include "AssetEditorToolkit.h"
#include "ModuleInterface.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetData.h"

class FPrefabAssetEditorModule
	: public IHasMenuExtensibility
	, public IHasToolBarExtensibility
	, public IModuleInterface
{
public:

	// IHasMenuExtensibility interface
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override;

	// IHasToolBarExtensibility interface
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override;

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	static void SelectParentPrefabActor();
	static void ToggleHidePrefabActor();
	static void TogglePrefabActorLockSelection();
	static void CollapseToParentPrefabActor();
	static void SelectChildrenOfSelectedPrefabActor();
	static void SelectChildrenOfParentPrefabActor();

	static void ApplyAndLockPrefabActor();

	static void SetPrefabPivotToCurrentWidgetLocation();

	static void StampPrefabActorsWithPrefab(); 
	static void ReplaceSelectedActorsWithPrefab();
	static void ChangePrefab();

protected:
	void RegisterActorFactories();
	void RegisterAssetTools();
	void UnregisterAssetTools();

	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);

	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();

	void RegisterEditorDelegates();
	void UnregisterEditorDelegates();

	void RegisterThumbnailRenderer();

	void RegisterSettings();
	void UnregisterSettings(); 
	
	void RegisterCommands();

	void RegisterComponentVisualizer();

	void OnMapOpened(const FString& Filename, bool bLoadAsTemplate);
	void OnPreSaveWorld(uint32 SaveFlags, class UWorld* World);
	void OnPostSaveWorld(uint32 SaveFlags, class UWorld* World, bool bSuccess);

	void OnLevelSelectionChanged(UObject* Obj);

private:
	static AActor* AddPrefabActor(const FAssetData& AssetData, bool bStampMode = false, const FTransform* ActorTransform = NULL);

private:

	/** Holds the menu extensibility manager. */
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;

	/** The collection of registered asset type actions. */
	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

	/** Holds the tool bar extensibility manager. */
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	TSharedPtr<FUICommandList> PrefabAssetCommands;

	/** Handle to a registered LevelViewportContextMenuPrefabExtender delegate */
	FDelegateHandle LevelViewportContextMenuPrefabExtenderDelegateHandle;
};
