#pragma once

#include "EdGraph/EdGraphPin.h"

class UK2Node_FunctionEntry;
class UK2Node_FunctionResult;

class FBlueprintReflectionHelpers
{
public:

	static TArray<UBlueprint*> LoadAllBlueprints(const TArray<FString> Paths = {});

	static UFunction* GetFunction(const UBlueprint* Blueprint, const FName FunctionName);
	static UFunction* GetSuperFunction(const UBlueprint* Blueprint, const FName FunctionName);

	static void GetFunctionEntryAndResultNodeFromFunctionGraph(const class UEdGraph* FuncGraph, UK2Node_FunctionEntry** Entry, UK2Node_FunctionResult** Result);
	//static UFunction* GetFunctionFromEntryNode(const UK2Node_FunctionEntry* EntryNode);

	// From Epic: BlueprintCompilerCppBackendUtils
	static FString GetCppName(const UField* Field, bool bUInterface = false, bool bForceParameterNameModification = false);
	static FString GetArgumentTypeForPinType(const FEdGraphPinType& FuncPinType);
	static bool HasAllFlags(uint64 Flags, uint64 FlagsToCheck);
	static TArray<FString> GetFunctionTags(const UFunction* Function, bool bIsOverride = false);
	static FString GetFunctionMetadata(const UFunction* Function, bool bIsOverride = false);
};