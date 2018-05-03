// Copyright 2017 marynate. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PrefabAsset : ModuleRules
	{
		public PrefabAsset(ReadOnlyTargetRules Target) : base(Target)
        {
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
			
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"Projects",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"PrefabAsset/Private",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[] {
						//"BlueprintGraph",
						"AssetRegistry",
						"AssetTools",
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
						"Landscape",
						"Projects",
						"PrefabAsset",
						"PropertyEditor",
						"RenderCore",
						"Slate",
						"SlateCore",
						"UnrealEd",
					}
				);
			}
		}
	}
}
