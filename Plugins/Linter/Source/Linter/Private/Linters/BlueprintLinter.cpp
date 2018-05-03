// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "Linters/BlueprintLinter.h"
#include "Linter.h"
#include "Engine/Blueprint.h"
#include "LinterManager.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintEditorUtils.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Knot.h"
#include "K2Node_VariableGet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Self.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_Tunnel.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_Literal.h"
#include "K2Node_EnumLiteral.h"
#include "K2Node_TemporaryVariable.h"
#include "K2Node_CallFunction.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTService.h"
#include "EdGraphNode_Comment.h"
#include "LintMessages.h"
#include "GameFramework/SaveGame.h"
#include "Resources/Version.h"

bool FBlueprintLinter::Lint(const UObject* InObject)
{
	START_LINT();

	Blueprint = CastChecked<UBlueprint>(Object);

	ClearBlueprintInfo();
	GatherBlueprintInfo();

	bFailedLint |= !PassesBasicAssetChecks();
	bFailedLint |= !IsCompiledCorrectly();
	bFailedLint |= !HasSaneVariables();
	bFailedLint |= !HasEditableVariablesCategorized();

	bFailedLint |= !HasProperReturnNodes();
	bFailedLint |= !IsUnderNodeLimit();
	bFailedLint |= !HasProperFunctionDescriptions();
	
	ClearBlueprintInfo();
	Blueprint = nullptr;
	
	END_LINT();
}

void FBlueprintLinter::ClearBlueprintInfo()
{
	EditableVariables.Reset();
	bIsMacroLibrary = false;
	bIsActor = false;
	CDO = nullptr;
	ACDO = nullptr;

	ComponentCount = 0;
	VariableCount = 0;
}

void FBlueprintLinter::GatherBlueprintInfo()
{
	check(Blueprint);

	bIsMacroLibrary = Blueprint->BlueprintType == EBlueprintType::BPTYPE_MacroLibrary;

	if (bIsMacroLibrary)
	{
		return;
	}

	CDO = Blueprint->GeneratedClass->GetDefaultObject();
	ACDO = Cast<AActor>(CDO);

	VariableCount = Blueprint->NewVariables.Num();

	for (FBPVariableDescription Desc : Blueprint->NewVariables)
	{
		if (FBlueprintEditorUtils::IsVariableComponent(Desc))
		{
			ComponentCount++;
			VariableCount--;
		}
		else
		{
			if ((Desc.PropertyFlags & CPF_DisableEditOnInstance) != CPF_DisableEditOnInstance)
			{
				EditableVariables.Add(Desc);
			}
		}
	}
}

bool FBlueprintLinter::IsNamedCorrectly()
{
	if (!GetDefault<ULinterSettings>()->bDoAssetNamingConventionChecks)
	{
		return true;
	}

	check(Blueprint);

	UClass* ObjectClass = Blueprint->GetClass();
	UClass* NativeBaseClass = nullptr;

	const FAssetTypeNameSetting* NameSetting = nullptr;

	// Macro libraries are a special case as they are always of base class UBlueprint
	if (bIsMacroLibrary)
	{
		NameSetting = &GetDefault<ULinterSettings>()->MacroLibrarySetting;
	}
	

	// If not a special case, see if there exists
	// a rule for the native class this Blueprint extends from...
	if (NameSetting == nullptr)
	{
		// Find the first native class this Blueprint represents in case
		// its native class has a special naming override.
		UClass* GeneratedClass = Blueprint->GeneratedClass;
		while (GeneratedClass != nullptr && !GeneratedClass->IsNative())
		{
			GeneratedClass = GeneratedClass->GetSuperClass();
		}
		NativeBaseClass = GeneratedClass;

		if (NativeBaseClass != nullptr)
		{
			NameSetting = GetDefault<ULinterSettings>()->AssetNameSettings.Find(NativeBaseClass);
		}

		UClass* ParentBaseClass = Blueprint->ParentClass;
		if (NameSetting == nullptr && ParentBaseClass != nullptr)
		{
			if (ParentBaseClass->IsChildOf(UBTDecorator::StaticClass()))
			{
				NameSetting = GetDefault<ULinterSettings>()->AssetNameSettings.Find(UBTDecorator::StaticClass());
			}
			else if (ParentBaseClass->IsChildOf(UBTTaskNode::StaticClass()))
			{
				NameSetting = GetDefault<ULinterSettings>()->AssetNameSettings.Find(UBTTaskNode::StaticClass());
			}
			else if (ParentBaseClass->IsChildOf(UBTService::StaticClass()))
			{
				NameSetting = GetDefault<ULinterSettings>()->AssetNameSettings.Find(UBTService::StaticClass());
			}
		}
	}
	
	// ...otherwise stick to the object's base class
	if (NameSetting == nullptr)
	{
		NameSetting = GetDefault<ULinterSettings>()->AssetNameSettings.Find(ObjectClass);
	}

	// Run prefix and suffix checks using found name settings if they are non-null
	if (NameSetting != nullptr)
	{
		if (!NameSetting->Prefix.IsEmpty())
		{
			if (!Object->GetName().StartsWith(NameSetting->Prefix, ESearchCase::CaseSensitive))
			{
				return ReportError(*Object->GetName(), FText::FormatNamed(Rule_1, TEXT("Correction"), FText::FromString(FString::Printf(TEXT("Should start with '%s'. "), *NameSetting->Prefix))));
			}
		}

		if (!NameSetting->Suffix.IsEmpty())
		{
			if (!Object->GetName().EndsWith(NameSetting->Suffix, ESearchCase::CaseSensitive))
			{
				return ReportError(*Object->GetName(), FText::FormatNamed(Rule_1, TEXT("Correction"), FText::FromString(FString::Printf(TEXT("Should end with '%s'. "), *NameSetting->Suffix))));
			}
		}
	}

	// If we don't have name settings or passed all name checks, simply return true
	return true;
}

bool FBlueprintLinter::IsCompiledCorrectly()
{
	if (!GetDefault<ULinterSettings>()->bBPCompiles)
	{
		return true;
	}

	check(Blueprint);

	switch (Blueprint->Status)
	{
		case EBlueprintStatus::BS_BeingCreated:
		case EBlueprintStatus::BS_Dirty:
		case EBlueprintStatus::BS_Unknown:
		case EBlueprintStatus::BS_UpToDate:
return true;
		case EBlueprintStatus::BS_Error:
			return ReportError(*Blueprint->GetName(), Rule_3_1_Errors);
		case EBlueprintStatus::BS_UpToDateWithWarnings:
			return ReportError(*Blueprint->GetName(), Rule_3_1_Warnings);
		default:
			return true;
	}
}

bool FBlueprintLinter::HasSaneVariables()
{
	const ULinterSettings* DefaultSettings = GetDefault<ULinterSettings>();
	if (!DefaultSettings->bBPVariableChecks)
	{
		return true;
	}

	check(Blueprint);
	check(!Blueprint->bHasBeenRegenerated || Blueprint->bIsRegeneratingOnLoad || (Blueprint->SkeletonGeneratedClass != NULL));

	bool bHadErrors = false;

	for (FBPVariableDescription Desc : Blueprint->NewVariables)
	{
		FString PropName = Desc.VarName.ToString();
		FText TypeName = UEdGraphSchema_K2::TypeToText(Desc.VarType);
		bool bIsBool = Desc.VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean;

		if (DefaultSettings->bBPVarPascalCase)
		{
			// PascalCase check
			FRegexMatcher Matcher(bIsBool ? PascalBooleanCase : PascalCase, PropName);
			if (!Matcher.FindNext() || Matcher.GetMatchBeginning() != 0 || Matcher.GetMatchEnding() != PropName.Len())
			{
				ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_2_1_2, TEXT("Type"), TypeName, TEXT("Name"), FText::FromString(PropName)));
				bHadErrors = true;
			}
		}

		// Boolean specific checks
		if (bIsBool)
		{
			if (DefaultSettings->bBPVarBoolPrefix)
			{
				if (!PropName.StartsWith(TEXT("b"), ESearchCase::CaseSensitive))
				{
					ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_2_1_3, TEXT("Type"), TypeName, TEXT("Name"), FText::FromString(PropName)));
					bHadErrors = true;
				}
			}

			if (DefaultSettings->bBPVarBoolIsQuestion)
			{
				if (PropName.StartsWith(TEXT("bIs"), ESearchCase::CaseSensitive))
				{
					ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_2_1_4_1, TEXT("Type"), TypeName, TEXT("Name"), FText::FromString(PropName)));
					bHadErrors = true;
				}
			}
		}

		if (DefaultSettings->bBPVarAtomicInclusion)
		{
			// Atomic name self reference check
			if (IsVariableAtomic(Desc) && PropName.Contains(TypeName.ToString()))
			{
				ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_2_1_6, TEXT("Type"), TypeName, TEXT("Name"), FText::FromString(PropName)));
				bHadErrors = true;
			}
		}

		// Array checks
#if ENGINE_MINOR_VERSION < 17
		if (Desc.VarType.bIsArray)
#else
		if (Desc.VarType.IsArray())
#endif
		{
			if (DefaultSettings->bBPVarArrayPlurals) // Partial support
			{
				if (PropName.Contains(TEXT("Array"), ESearchCase::CaseSensitive))
				{
					ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_2_1_8, TEXT("Type"), TypeName, TEXT("Name"), FText::FromString(PropName)));
					bHadErrors = true;
				}
			}
		}

		if (DefaultSettings->bBPVarExposeOnSpawnEditable)
		{
			// Expose on Spawn but not Editable check
			if ((Desc.PropertyFlags & CPF_ExposeOnSpawn) == CPF_ExposeOnSpawn && (Desc.PropertyFlags & CPF_DisableEditOnInstance) == CPF_DisableEditOnInstance)
			{
				ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_2_2, TEXT("Type"), TypeName, TEXT("Name"), FText::FromString(PropName)));
				bHadErrors = true;
			}
		}

		if (DefaultSettings->bBPVarEditableMustHaveTooltip)
		{
			// Editable?
			if ((Desc.PropertyFlags & CPF_DisableEditOnInstance) != CPF_DisableEditOnInstance)
			{
				if (!Desc.HasMetaData(FBlueprintMetadata::MD_Tooltip) || Desc.GetMetaData(FBlueprintMetadata::MD_Tooltip).Len() <= 0)
				{
					ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_2_2_1, TEXT("Type"), TypeName, TEXT("Name"), FText::FromString(PropName)));
					bHadErrors = true;
				}
			}
		}
		
		
		if (DefaultSettings->bBPVarSaveNoConfig)
		{
			// Config var check
			if ((Desc.PropertyFlags & CPF_Config) == CPF_Config)
			{
				ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_2_7, TEXT("Type"), TypeName, TEXT("Name"), FText::FromString(PropName)));
				bHadErrors = true;
			}
		}
	}	

	return !bHadErrors;
}

bool FBlueprintLinter::HasEditableVariablesCategorized()
{
	if (!GetDefault<ULinterSettings>()->bBPVariableChecks || !GetDefault<ULinterSettings>()->bBPVarEditableCategories)
	{
		return true;
	}

	check(Blueprint);

	bool bHadErrors = false;

	if (VariableCount >= 5 && EditableVariables.Num() > 0)
	{
		for (FBPVariableDescription Desc : EditableVariables)
		{
			FString PropName = Desc.VarName.ToString();
			FText TypeName = UEdGraphSchema_K2::TypeToText(Desc.VarType);

			if (Desc.Category.IsEmptyOrWhitespace())
			{
				ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_2_3_NoCategory, TEXT("Type"), TypeName, TEXT("Name"), FText::FromString(PropName)));
				bHadErrors = true;
			}

			if (!Desc.Category.ToString().StartsWith(TEXT("Config")))
			{
				ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_2_3_BadCategory, TEXT("Type"), TypeName, TEXT("Name"), FText::FromString(PropName), TEXT("Category"), Desc.Category));
				bHadErrors = true;
			}
		}
	}

	return !bHadErrors;
}

bool FBlueprintLinter::HasProperReturnNodes()
{
	if (!GetDefault<ULinterSettings>()->bBPFunctionChecks || !GetDefault<ULinterSettings>()->bBPFunctionsShouldHaveReturnNodes)
	{
		return true;
	}

	check(Blueprint);
	bool bHadErrors = false;

	// Early return out if blueprint type shouldn't be checked for return nodes
	switch (Blueprint->BlueprintType)
	{
	case BPTYPE_Normal:
	case BPTYPE_Const:
	case BPTYPE_LevelScript:
	case BPTYPE_FunctionLibrary:
		break;
	case BPTYPE_MacroLibrary:
	case BPTYPE_Interface:
	default:
		return !bHadErrors;
	}

	for (auto FunctionGraph : Blueprint->FunctionGraphs)
	{
		if (FunctionGraph->GetFName() != UEdGraphSchema_K2::FN_UserConstructionScript)
		{
			TArray<UK2Node_FunctionResult*> AllResultNodes;
			FunctionGraph->GetNodesOfClass(AllResultNodes);
			if (AllResultNodes.Num() <= 0)
			{
				ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_3_2, TEXT("Name"), FText::FromName(FunctionGraph->GetFName())));
				bHadErrors = true;
			}
		}
	}

	return !bHadErrors;
}

bool FBlueprintLinter::IsNodeTrivial(const UEdGraphNode* Node)
{
	if (Node->IsA(UK2Node_Knot::StaticClass())
		|| Node->IsA(UK2Node_FunctionEntry::StaticClass())
		|| Node->IsA(UK2Node_Self::StaticClass())
		|| Node->IsA(UK2Node_DynamicCast::StaticClass())
		|| Node->IsA(UK2Node_BreakStruct::StaticClass())
		|| Node->IsA(UEdGraphNode_Comment::StaticClass())
		|| Node->IsA(UK2Node_VariableGet::StaticClass())
		|| Node->IsA(UK2Node_StructMemberGet::StaticClass())
		|| Node->IsA(UK2Node_Tunnel::StaticClass())
		|| Node->IsA(UK2Node_TemporaryVariable::StaticClass())
		|| Node->IsA(UK2Node_FunctionResult::StaticClass())
		|| Node->IsA(UK2Node_FunctionTerminator::StaticClass())
		|| Node->IsA(UK2Node_CastByteToEnum::StaticClass())
		)
	{
		return true;
	}

	if (const UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(Node))
	{
		FName FuncName = CallFuncNode->FunctionReference.GetMemberName();
		if (FuncName == TEXT("Conv_InterfaceToObject")
			|| FuncName.ToString().Contains(TEXT("MakeLiteral"))
			|| FuncName.ToString().Contains(TEXT("ToString"))
			|| FuncName.ToString().StartsWith(TEXT("Make"))
			|| FuncName.ToString().StartsWith(TEXT("Break"))
			)
		{
			return true;
		}
	}

	return false;

}

bool FBlueprintLinter::IsUnderNodeLimit()
{
	if (!GetDefault<ULinterSettings>()->bBPFunctionChecks)
	{
		return true;
	}

	check(Blueprint);
	bool bHadErrors = false;

	int32 FunctionNodeLimit = GetDefault<ULinterSettings>()->FunctionNodeLimit;

	// Early out if node limit is disabled
	if (FunctionNodeLimit <= 0)
	{
		return !bHadErrors;
	}

	for (auto FunctionGraph : Blueprint->FunctionGraphs)
	{
		if (FunctionGraph->GetFName() != UEdGraphSchema_K2::FN_UserConstructionScript)
		{
			// If initial graph check exceeds node limit, filter out nodes that do not contribute to complexity
			if (FunctionGraph->Nodes.Num() > FunctionNodeLimit)
			{
				auto NodesCopy = FunctionGraph->Nodes;
				NodesCopy.RemoveAll([this](UEdGraphNode* Val) { return IsNodeTrivial(Val); });

				// If removing knots and comments still exceeds node limit, report error
				if (NodesCopy.Num() > FunctionNodeLimit)
				{
					ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_3_3, TEXT("Name"), FText::FromName(FunctionGraph->GetFName()), TEXT("Limit"), FText::AsNumber(FunctionNodeLimit), TEXT("Count"), FText::AsNumber(NodesCopy.Num())));
					bHadErrors = true;
				}
			}
		}
	}

	return !bHadErrors;
}

bool FBlueprintLinter::HasProperFunctionDescriptions()
{
	if (!GetDefault<ULinterSettings>()->bBPFunctionChecks || !GetDefault<ULinterSettings>()->bBPFunctionsShouldHaveReturnNodes)
	{
		return true;
	}

	check(Blueprint);
	bool bHadErrors = false;

	// Early return out if blueprint type shouldn't be checked for function descriptions
	switch (Blueprint->BlueprintType)
	{
	case BPTYPE_Normal:
	case BPTYPE_Const:
	case BPTYPE_LevelScript:
	case BPTYPE_FunctionLibrary:
		break;
	case BPTYPE_MacroLibrary:
	case BPTYPE_Interface:
	default:
		return !bHadErrors;
	}

	for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
	{
		if (FunctionGraph->GetFName() != UEdGraphSchema_K2::FN_UserConstructionScript)
		{
			UK2Node_FunctionEntry* FunctionEntryNode = nullptr;
			TArray<UK2Node_FunctionEntry*> EntryNodes;

			FunctionGraph->GetNodesOfClass(EntryNodes);

			if ((EntryNodes.Num() > 0) && EntryNodes[0]->IsEditable())
			{
				FunctionEntryNode = Cast<UK2Node_FunctionEntry>(EntryNodes[0]);
			}

			if (FunctionEntryNode != nullptr)
			{
				if (FUNC_AccessSpecifiers & FunctionEntryNode->GetFunctionFlags() & FUNC_Public)
				{
					if (FunctionEntryNode->MetaData.ToolTip.IsEmpty())
					{
						ReportError(*Blueprint->GetName(), FText::FormatNamed(Rule_3_3_4, TEXT("Name"), FText::FromName(FunctionGraph->GetFName())));
						bHadErrors = true;
					}
				}
			}
		}
	}

	return !bHadErrors;	
}

bool FBlueprintLinter::IsVariableAtomic(FBPVariableDescription& VarDesc)
{
	return (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean
		|| VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Byte
		|| VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Int
		|| VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Float
		|| VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_String
		|| VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Enum
		);
}
