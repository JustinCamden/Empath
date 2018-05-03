// Copyright 2017 Gamemakin LLC. All Rights Reserved.

#pragma once
#include "BaseBlueprintOutputCommandlet.h"
#include "DokuwikiExporter.h"
#include "EdGraph/EdGraphPin.h"
#include "DokuwikiCommandlet.generated.h"

UCLASS()
class UDokuwikiCommandlet : public UBaseBlueprintOutputCommandlet
{
	GENERATED_UCLASS_BODY()

	virtual ~UDokuwikiCommandlet();

	virtual void PrintUsage() override;

	virtual void OutputBlueprintClass(const UBlueprint* Blueprint) override;

	virtual void CreateCustomEngine(const FString& Params) override;

	FDokuwikiExporter* DokuwikiExporter;
};


