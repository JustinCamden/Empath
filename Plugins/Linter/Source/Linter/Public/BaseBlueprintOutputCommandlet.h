// Copyright 2017 Gamemakin LLC. All Rights Reserved.

#pragma once
#include "Commandlets/Commandlet.h"
#include "AssetRegistryModule.h"
#include "EdGraph/EdGraphPin.h"
#include "BaseBlueprintOutputCommandlet.generated.h"

class UK2Node_FunctionEntry;
class UK2Node_FunctionResult;

UCLASS()
class UBaseBlueprintOutputCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

#if WITH_HOT_RELOAD
	UBaseBlueprintOutputCommandlet(FVTableHelper& Helper);
#endif
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;

	virtual void PrintUsage();

	virtual void CreateCustomEngine(const FString& Params) override;

	virtual void OutputBlueprintClass(const UBlueprint* Blueprint) {};

	TArray<FName> ClassesToFullyLoad;

	FString OutputName;
	FString OutputDir;

	TArray<FString> Paths;
	TArray<FString> Switches;
	bool bPathSearch;

	FAssetRegistryModule& AssetRegistryModule;

	//~ End UCommandlet Interface
};


