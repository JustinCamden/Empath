// Copyright 2017 Gamemakin LLC. All Rights Reserved.

#include "DokuwikiCommandlet.h"
#include "AssetRegistryModule.h"
#include "Engine/ObjectLibrary.h"
#include "Linter.h"
#include "LinterManager.h"
#include "IPluginManager.h"
#include "FileHelper.h"

#if !IS_MONOLITHIC
#include "SlateApplication.h"
#include "StandaloneRenderer.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(DokuwikiCommandlet, Log, All);

UDokuwikiCommandlet::UDokuwikiCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DokuwikiExporter(nullptr)
{
	IsClient = false;
	IsServer = false;
	OutputName = TEXT("DokuwikiOutput");

	ClassesToFullyLoad.Push(TEXT("Blueprint"));

	// Required for rendering
#if !IS_MONOLITHIC
	PRIVATE_GAllowCommandletRendering = true;
#endif
}

UDokuwikiCommandlet::~UDokuwikiCommandlet()
{
	if (DokuwikiExporter)
	{
		delete DokuwikiExporter;
		DokuwikiExporter = nullptr;
	}
}

void UDokuwikiCommandlet::PrintUsage()
{
	UE_LOG(DokuwikiCommandlet, Display, TEXT("Dokuwiki Usage: {Editor}.exe Project.uproject -run=Dokuwiki \"/Game/\""));
	UE_LOG(DokuwikiCommandlet, Display, TEXT(""));
	UE_LOG(DokuwikiCommandlet, Display, TEXT("This will run the Dokuwiki Generator on the provided project and will scan the supplied directory, example being the project's full Content/Game tree. Can add multiple paths as additional arguments."));
}

void UDokuwikiCommandlet::OutputBlueprintClass(const UBlueprint* Blueprint)
{
	check(Blueprint);
	if (DokuwikiExporter == nullptr)
	{
		DokuwikiExporter = new FDokuwikiExporter(GetDefault<ULinterSettings>()->DokuwikiExportNamespace);
	}

	check(DokuwikiExporter);
	DokuwikiExporter->OutputBlueprintClass(Blueprint);
}

void UDokuwikiCommandlet::CreateCustomEngine(const FString& Params)
{
#if !IS_MONOLITHIC
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());
#endif
}

