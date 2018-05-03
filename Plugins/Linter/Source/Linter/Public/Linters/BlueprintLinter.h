// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#pragma once

#include "Engine/Blueprint.h"
#include "Linters/LinterBase.h"

class FBlueprintLinter : public FLinterBase 
{ 
	virtual bool Lint(const UObject* InObject) override;

	virtual void ClearBlueprintInfo();

	virtual void GatherBlueprintInfo();

	virtual bool IsNamedCorrectly() override;
	virtual bool IsCompiledCorrectly();
	
	virtual bool HasSaneVariables();
	virtual bool HasEditableVariablesCategorized();

	virtual bool HasProperReturnNodes();
	virtual bool IsNodeTrivial(const UEdGraphNode* Node);
	virtual bool IsUnderNodeLimit();
	virtual bool HasProperFunctionDescriptions();
	
	static bool IsVariableAtomic(FBPVariableDescription& VarDesc);

protected:
	const UBlueprint* Blueprint;
	bool bIsMacroLibrary;

	bool bIsActor;
	UObject* CDO;
	AActor* ACDO;

	int32 ComponentCount;
	int32 VariableCount;

	TArray<FBPVariableDescription> EditableVariables;
};