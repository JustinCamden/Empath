// Copyright 2017 Gamemakin LLC. All Rights Reserved.

#pragma once
#include "BaseBlueprintOutputCommandlet.h"
#include "EdGraph/EdGraphPin.h"
#include "DoxygenCommandlet.generated.h"

class UK2Node_FunctionEntry;
class UK2Node_FunctionResult;

UCLASS()
class UDoxygenCommandlet : public UBaseBlueprintOutputCommandlet
{
	GENERATED_UCLASS_BODY()
	//~ Begin UCommandlet Interface

	virtual void PrintUsage() override;

	virtual void OutputBlueprintClass(const UBlueprint* Blueprint) override;

	static FString ConvertStringToCPPComment(const FString& InString, const FString LinePrefix = TEXT("\t"));
	static FString GetArgStringFromFunction(UFunction* Function, FString& OutReturnType);
	static FString CleanUpFunctionName(const FString& FunctionName);

	static bool ShouldOutputFunction(const UFunction* Function);
	
	static bool OutputFunctionGraphs(const UBlueprint* Blueprint, const TArray<UEdGraph*> FunctionGraphs, FString& PublicFunctions, FString& ProtectedFunctions, FString& PrivateFunctions, const FString InterfaceName = TEXT(""));

	//~ End UCommandlet Interface
};


