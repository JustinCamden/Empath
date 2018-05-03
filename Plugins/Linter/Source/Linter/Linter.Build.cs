// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Linter : ModuleRules
{
#if WITH_FORWARDED_MODULE_RULES_CTOR
    public Linter(ReadOnlyTargetRules Target) : base(Target)
#else
    public Linter(TargetInfo Target)
#endif
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

#if UE_4_17_OR_LATER
        Definitions.Add("SUPPORTS_GENERATING_NODE_IMAGES=1");
#else
        Definitions.Add("SUPPORTS_GENERATING_NODE_IMAGES=0");
#endif

        PublicIncludePaths.AddRange(
			new string[] {
				"Linter/Public"
			}
		);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"Linter/Private",
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
            {
                "CoreUObject",
                "Engine",
                "IntroTutorials",
                "Slate",
                "SlateCore",
                "RenderCore",
                "UnrealEd",
                "UMG",
                "UMGEditor",
                "GraphEditor",
                "ContentBrowser",
                "AssetTools",
                "EditorStyle",
                "Projects",
                "MediaAssets",
                "Landscape",
                "AIModule",
                "NetworkReplayStreaming",
                "BlueprintGraph",
                "InputCore",
                "StandaloneRenderer"
				// ... add private dependencies that you statically link with here ...	
			}
		);

        PublicIncludePathModuleNames.Add("Launch");

    }
}
