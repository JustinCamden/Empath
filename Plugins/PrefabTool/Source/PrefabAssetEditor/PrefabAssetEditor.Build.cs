// Copyright 2017 marynate. All Rights Reserved.

using UnrealBuildTool;

public class PrefabAssetEditor : ModuleRules
{
	public PrefabAssetEditor(ReadOnlyTargetRules Target) : base(Target)
    {
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"MainFrame",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"PrefabAssetEditor/Private",
				"PrefabAssetEditor/Private/AssetTools",
				"PrefabAssetEditor/Private/Factories",
				"PrefabAssetEditor/Private/PrefabEditorActor",
				"PrefabAssetEditor/Private/Styles",
				"PrefabAssetEditor/Private/ThumbnailRendering",
				"PrefabAssetEditor/Private/Utils",
				"PrefabAssetEditor/Private/Widgets",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"BlueprintGraph",
				"ContentBrowser",
				"Core",
				"CoreUObject",
                "DesktopWidgets",
				"EditorStyle",
				"Engine",
				"Foliage",
				"InputCore",
				"Kismet",
				"LevelEditor",
				"Projects",
				"PrefabAsset",
				"PropertyEditor",
				"RenderCore",
				"SceneOutliner",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"UnrealEd",
			}
		);
	}
}
