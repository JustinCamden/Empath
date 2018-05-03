// Copyright 2017 Gamemakin LLC. All Rights Reserved.

#pragma once
#include "BaseBlueprintOutputCommandlet.h"
#include "Misc/Paths.h"
#include "EdGraph/EdGraphPin.h"
#include "TooltipStringHelper.h"


class FDokuwikiExporter
{
public:

	FDokuwikiExporter(const FString _APINamespace, const FString _OutputDir = FPaths::GameSavedDir() / TEXT("DokuwikiExport") / FDateTime::Now().ToString());
	virtual ~FDokuwikiExporter();

	void SetUpBlueprintNodeRenderer();

	virtual void OutputBlueprintClass(const UBlueprint* Blueprint);

	FString GetChildClasses(UClass* Class, int32 Level, FString Base);
	FString BuildInheritanceHierarchy(const UBlueprint* Blueprint);

	static FString ConvertToHeading(const FString& Text, const uint32& HeadingLevel);
	static FString ConvertToFolded(const FString& Title, const FString& Content);

	static FString CleanUpFunctionName(const FString& FunctionName);
	static bool ShouldOutputFunction(const UFunction* Function);

	static void GetFunctionInputsAndOutputs(const UFunction* Function, TArray<FBPFunctionArgumentDescription>& Inputs, TArray<FBPFunctionArgumentDescription>& Outputs);

	bool OutputFunctionGraphs(const UBlueprint* Blueprint, const TArray<UEdGraph*> FunctionGraphs, FString& PublicFunctions, FString& ProtectedFunctions, FString& PrivateFunctions, const FString InterfaceName = TEXT(""), const bool bAreEvents = false);
	bool GenerateNodeImage(UField* Field, const FString ImagePath);

	FString OutputDir;
	FString APIPath;
	FString APINamespace;

	UBlueprint* EmptyBlueprint;
	UEdGraph* TempGraph;
	TSharedPtr<class SGraphPanel> GraphPanel;
};


