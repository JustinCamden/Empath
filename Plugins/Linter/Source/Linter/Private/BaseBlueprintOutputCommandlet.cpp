// Copyright 2017 Gamemakin LLC. All Rights Reserved.

#include "BaseBlueprintOutputCommandlet.h"
#include "Engine/ObjectLibrary.h"
#include "Linter.h"
#include "LinterManager.h"
#include "IPluginManager.h"
#include "FileHelper.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/UserDefinedStruct.h"
#include "Resources/Version.h"
#include "BlueprintReflectionHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(BaseBlueprintOutputCommandlet, Log, All);

UBaseBlueprintOutputCommandlet::UBaseBlueprintOutputCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, OutputName(TEXT("BaseOutput"))
	, OutputDir(FPaths::GameSavedDir() / OutputName / FDateTime::Now().ToString())
	, bPathSearch(false)
	, AssetRegistryModule(FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")))
{
	IsClient = false;
	IsServer = false;
}

#if WITH_HOT_RELOAD
UBaseBlueprintOutputCommandlet::UBaseBlueprintOutputCommandlet(FVTableHelper& Helper)
	: Super(Helper)
	, OutputName(TEXT("BaseOutput"))
	, OutputDir(FPaths::GameSavedDir() / OutputName / FDateTime::Now().ToString())
	, bPathSearch(false)
	, AssetRegistryModule(FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")))
{
	IsClient = false;
	IsServer = false;
}
#endif

void UBaseBlueprintOutputCommandlet::PrintUsage()
{
	UE_LOG(BaseBlueprintOutputCommandlet, Display, TEXT("This should not be ran directly."));
}

int32 UBaseBlueprintOutputCommandlet::Main(const FString& InParams)
{
	FString Params = InParams;
	// Parse command line.
	
	UCommandlet::ParseCommandLine(*Params, Paths, Switches);

	UE_LOG(BaseBlueprintOutputCommandlet, Display, TEXT("%s is indeed running! Outputting to %s"), *OutputName, *OutputDir);

	TArray<UBlueprint*> Blueprints = FBlueprintReflectionHelpers::LoadAllBlueprints(Paths);

	for (UBlueprint* Blueprint : Blueprints)
	{
		OutputBlueprintClass(Blueprint);
	}
#if !IS_MONOLITHIC
	if (PRIVATE_GAllowCommandletRendering)
	{
		GIsRequestingExit = true;
		GIsRunning = false;
		GIsCriticalError = true; // Prevents crash on exit due to Slate font cleanup?
	}
#endif

	return 0;
}

void UBaseBlueprintOutputCommandlet::CreateCustomEngine(const FString& Params)
{
	GEngine = GEditor = nullptr;
}
