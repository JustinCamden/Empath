// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "LinterCommandlet.h"
#include "AssetRegistryModule.h"
#include "Engine/ObjectLibrary.h"
#include "Linter.h"
#include "LinterManager.h"
#include "IPluginManager.h"
#include "FileHelper.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LinterCommandlet, Log, All);

ULinterCommandlet::ULinterCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsClient = false;
	IsServer = false;
}

static void PrintUsage()
{
	UE_LOG(LinterCommandlet, Display, TEXT("Linter Usage: {Editor}.exe Project.uproject -run=Linter \"/Game/\""));
	UE_LOG(LinterCommandlet, Display, TEXT(""));
	UE_LOG(LinterCommandlet, Display, TEXT("This will run the Linter on the provided project and will scan the supplied directory, example being the project's full Content/Game tree. Can add multiple paths as additional arguments."));
}

int32 ULinterCommandlet::Main(const FString& InParams)
{
	FString Params = InParams;
	// Parse command line.
	TArray<FString> Paths;
	TArray<FString> Switches;
	UCommandlet::ParseCommandLine(*Params, Paths, Switches);

	UE_LOG(LinterCommandlet, Display, TEXT("Linter is indeed running!"));

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	UE_LOG(LinterCommandlet, Display, TEXT("Loading the asset registry..."));
	AssetRegistryModule.Get().SearchAllAssets(/*bSynchronousSearch =*/true);
	UE_LOG(LinterCommandlet, Display, TEXT("Finished loading the asset registry. Loading assets..."));

	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAllAssets(AssetList, true);

	bool bPathSearch = (Paths.Num() > 0 && !Paths[0].IsEmpty());

	FLinterManager* LinterManager = FModuleManager::LoadModuleChecked<FLinterModule>(TEXT("Linter")).GetLinterManager();
	LinterManager->StartReport();

	for (FAssetData const& Asset : AssetList)
	{
		FString const AssetPath = Asset.ObjectPath.ToString();
		UE_LOG(LinterCommandlet, Verbose, TEXT("Considering '%s'..."), *AssetPath);

		// If commandlet was passed in paths, only consider assets belong to those paths
		if (bPathSearch)
		{
			bool bRelevant = false;
			for (FString const& Path : Paths)
			{
				if (AssetPath.StartsWith(Path))
				{
					bRelevant = true;
					break;
				}
			}

			if (!bRelevant)
			{
				continue;
			}
		}

		if (Asset.IsRedirector())
		{
			// @TODO: Report Redirectors
			UE_LOG(LinterCommandlet, Warning, TEXT("'%s' is a redirector, skipping."), *AssetPath);
			continue;
		}

		UObject* LoadedObject = Asset.GetAsset();
		if (LoadedObject != nullptr)
		{
			UE_LOG(LinterCommandlet, Display, TEXT("Loaded '%s'..."), *AssetPath);
			LinterManager->Lint(LoadedObject);
		}
		else
		{
			UE_LOG(LinterCommandlet, Error, TEXT("Failed to load: '%s'."), *AssetPath);
		}
	}	

	LinterManager->EndReport();

	FString TemplatePath = FPaths::Combine(*IPluginManager::Get().FindPlugin(TEXT("Linter"))->GetBaseDir(), TEXT("Resources"), TEXT("LintReportTemplate.html"));

	FString ReportHTML;
	if (FFileHelper::LoadFileToString(ReportHTML, *TemplatePath))
	{
		ReportHTML.ReplaceInline(TEXT("{% TITLE %}"), *FPaths::GetBaseFilename(FPaths::GetProjectFilePath()));
		ReportHTML.ReplaceInline(TEXT("{% ERRORS %}"), *FString::FromInt(LinterManager->GetErrorCount()));
		ReportHTML.ReplaceInline(TEXT("{% LINT_REPORT %}"), *LinterManager->GetAndClearReport());

#if ENGINE_MINOR_VERSION < 18
		FString ReportPath = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("LintReports"), *(FDateTime::Now().ToString() + TEXT(".html")));
#else
		FString ReportPath = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("LintReports"), *(FDateTime::Now().ToString() + TEXT(".html")));
#endif

		FFileHelper::SaveStringToFile(ReportHTML, *ReportPath);
		FPlatformProcess::LaunchURL(*ReportPath, TEXT(""), nullptr);
		UE_LOG(LinterCommandlet, Display, TEXT("Saved Lint Report to '%s'"), *ReportPath);
	}
	else
	{
		UE_LOG(LinterCommandlet, Error, TEXT("Could not load lint report template."));
	}

	return 0;
}