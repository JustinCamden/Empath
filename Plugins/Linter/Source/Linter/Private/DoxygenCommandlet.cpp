// Copyright 2017 Gamemakin LLC. All Rights Reserved.

#include "DoxygenCommandlet.h"
#include "AssetRegistryModule.h"
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

DEFINE_LOG_CATEGORY_STATIC(DoxygenCommandlet, Log, All);

UDoxygenCommandlet::UDoxygenCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsClient = false;
	IsServer = false;
	OutputName = TEXT("DoxygenOutput");
}

void UDoxygenCommandlet::PrintUsage()
{
	UE_LOG(DoxygenCommandlet, Display, TEXT("Doxygen Usage: {Editor}.exe Project.uproject -run=Doxygen \"/Game/\""));
	UE_LOG(DoxygenCommandlet, Display, TEXT(""));
	UE_LOG(DoxygenCommandlet, Display, TEXT("This will run the Doxygen Generator on the provided project and will scan the supplied directory, example being the project's full Content/Game tree. Can add multiple paths as additional arguments."));
}

void UDoxygenCommandlet::OutputBlueprintClass(const UBlueprint* Blueprint)
{
	check(Blueprint);

	switch (Blueprint->BlueprintType)
	{
	case BPTYPE_MacroLibrary:
		UE_LOG(DoxygenCommandlet, Warning, TEXT("Output for MacroLibraies not supported. (%s)"), *Blueprint->GetFName().ToString());
		return;
	case BPTYPE_Const:
		UE_LOG(DoxygenCommandlet, Warning, TEXT("Output for Const Blueprints not supported. (%s)"), *Blueprint->GetFName().ToString());
		return;
	case BPTYPE_Interface:
		UE_LOG(DoxygenCommandlet, Warning, TEXT("Output for Blueprint Interfaces not supported. (%s)"), *Blueprint->GetFName().ToString());
		return;
	case BPTYPE_LevelScript:
		UE_LOG(DoxygenCommandlet, Warning, TEXT("Output for Blueprint Level Script not supported. (%s)"), *Blueprint->GetFName().ToString());
		return;
	default:
		break;
	};

	FString Output;
	FString ParentClassName = FString(Blueprint->GeneratedClass->GetSuperClass()->GetPrefixCPP()) + Blueprint->GeneratedClass->GetSuperClass()->GetName();
	ParentClassName.RemoveFromEnd(TEXT("_C"));		

	TArray<FString> Interfaces;
	FString InterfaceInheritString;
	for (FBPInterfaceDescription InterfaceDesc : Blueprint->ImplementedInterfaces)
	{
		FString InterfaceName = InterfaceDesc.Interface->GetName();
		if (!InterfaceName.StartsWith(TEXT("I")))
		{
			InterfaceName.InsertAt(0, TEXT("I"));
		}
		InterfaceName.RemoveFromEnd(TEXT("_C"));
		Interfaces.Push(InterfaceName);
		InterfaceInheritString.Append(TEXT("public ") + InterfaceName + TEXT(", "));
	}
	check(Interfaces.Num() == Blueprint->ImplementedInterfaces.Num());

	if (!InterfaceInheritString.IsEmpty())
	{
		InterfaceInheritString.RemoveFromEnd(TEXT(", "));
		InterfaceInheritString.InsertAt(0, TEXT(", "));
	}

	Output.Append(TEXT("#pragma once\r\n"));
	Output.Append(TEXT("\r\n"));
	Output.Append(TEXT("UCLASS(BlueprintType, Blueprintable)\r\n"));
	Output.Append(FString::Printf(TEXT("class %s%s : public %s%s\r\n"), Blueprint->GeneratedClass->GetPrefixCPP(), *Blueprint->GetFName().ToString(), *ParentClassName, *InterfaceInheritString));
	Output.Append(TEXT("{\r\n"));
	Output.Append(TEXT("\tGENERATED_UCLASS_BODY()\r\n"));
	Output.Append(TEXT("\r\n"));

	FString PublicFunctions;
	FString ProtectedFunctions;
	FString PrivateFunctions;

	FString FuncName;

	if (Blueprint->GetFName().ToString() == TEXT("BP_PlayerCharacter"))
	{
		UE_LOG(DoxygenCommandlet, Warning, TEXT("WHAT AM I (%s)"), TEXT("ASS"));
	}

	OutputFunctionGraphs(Blueprint, Blueprint->FunctionGraphs, PublicFunctions, ProtectedFunctions, PrivateFunctions);
	OutputFunctionGraphs(Blueprint, Blueprint->EventGraphs, PublicFunctions, ProtectedFunctions, PrivateFunctions);

	for (int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); ++i)
	{
		OutputFunctionGraphs(Blueprint, Blueprint->ImplementedInterfaces[i].Graphs, PublicFunctions, ProtectedFunctions, PrivateFunctions, Interfaces[i]);
	}

	if (!PublicFunctions.IsEmpty())
	{
		Output.Append(TEXT("public:")); Output.Append(TEXT("\r\n"));
		Output.Append(TEXT("\r\n"));
		Output.Append(PublicFunctions);
		Output.Append(TEXT("\r\n"));
	}

	if (!ProtectedFunctions.IsEmpty())
	{
		Output.Append(TEXT("protected:")); Output.Append(TEXT("\r\n"));
		Output.Append(TEXT("\r\n"));
		Output.Append(ProtectedFunctions);
		Output.Append(TEXT("\r\n"));
	}

	if (!PrivateFunctions.IsEmpty())
	{
		Output.Append(TEXT("private:")); Output.Append(TEXT("\r\n"));
		Output.Append(TEXT("\r\n"));
		Output.Append(PrivateFunctions);
		Output.Append(TEXT("\r\n"));
	}

	Output.Append(TEXT("\r\n"));;
	Output.Append(TEXT("};"));

	FString FullPath = FPaths::ConvertRelativePathToFull(OutputDir / Blueprint->GetFName().ToString() + TEXT(".h"));

	FFileHelper::SaveStringToFile(Output, *(OutputDir / Blueprint->GetFName().ToString() + TEXT(".h")));

	//Quick testing code
	if (Blueprint->GetFName().ToString() == TEXT("BP_PlayerCharacter"))
	{
		FPlatformProcess::CreateProc(TEXT("C:\\Program Files (x86)\\Notepad++\\Notepad++.exe"), *FullPath, true, false, false, nullptr, 0, nullptr, nullptr);
	}

}

FString UDoxygenCommandlet::ConvertStringToCPPComment(const FString& InString, const FString LinePrefix /* = TEXT("\t")*/)
{
	FString Output = LinePrefix + TEXT("/**\r\n");

	TArray<FString> Lines;
	InString.ParseIntoArrayLines(Lines, false);
	for (FString Line : Lines)
	{
		Output.Append(LinePrefix + ("* ") + Line + TEXT("\r\n"));
	}
	
	Output.Append(LinePrefix + TEXT("*/"));
	return Output;
}

FString UDoxygenCommandlet::GetArgStringFromFunction(UFunction* Function, FString& OutReturnType)
{
	check(Function);
	FStringOutputDevice FunctionInputs;
	FStringOutputDevice FunctionOutputs;

#if ENGINE_MINOR_VERSION < 18
#define OUTPUTS_TRIM_WHITESPACE FunctionOutputs.TrimTrailing();
#else
#define OUTPUTS_TRIM_WHITESPACE FunctionOutputs.TrimEndInline();
#endif

	bool bTreatFunctionOutputsAsReturnType = false;

	TArray<UProperty*> Properties;
	for (UProperty* Property : TFieldRange<UProperty>(Function))
	{
		if (!Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
		Properties.Push(Property);
	}

	for (int32 i = 0; i < Properties.Num(); ++i)
	{
		FString TypeOverride;

		const UStructProperty* PotentialUDSProperty = Cast<const UStructProperty>(Properties[i]);
		const UUserDefinedStruct* UDS = PotentialUDSProperty ? Cast<const UUserDefinedStruct>(PotentialUDSProperty->Struct) : nullptr;
		if (UDS != nullptr)
		{
			TypeOverride = UDS->GetName();
			if (!TypeOverride.StartsWith(TEXT("F")))
			{
				TypeOverride.InsertAt(0, TEXT("F"));
			}
		}

		bool bIsOutput = Properties[i]->HasAnyPropertyFlags(CPF_OutParm);
		if (bIsOutput && i + 1 == Properties.Num() && FunctionOutputs.IsEmpty()) // Only one output
		{
			bTreatFunctionOutputsAsReturnType = true;
			Properties[i]->ExportCppDeclaration(FunctionOutputs, EExportedDeclaration::Parameter, NULL, CPPF_NoRef, true, TypeOverride.IsEmpty() ? nullptr : &TypeOverride);
			if (Properties[i]->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				FunctionOutputs.Append(("&"));
			}
			OUTPUTS_TRIM_WHITESPACE
			continue;
		}
		FStringOutputDevice& OutputDevice = bIsOutput ? FunctionOutputs : FunctionInputs;
		Properties[i]->ExportCppDeclaration(OutputDevice, EExportedDeclaration::Parameter, NULL, 0, false, TypeOverride.IsEmpty() ? nullptr : &TypeOverride);
		OutputDevice.Append(TEXT(", "));
	}

	if (FunctionOutputs.Len() == 0 && Function->FunctionFlags & FUNC_Native)
	{
		if (UProperty* ReturnProp = Function->GetReturnProperty())
		{
			bTreatFunctionOutputsAsReturnType = true;
			ReturnProp->ExportCppDeclaration(FunctionOutputs, EExportedDeclaration::Parameter, NULL, CPPF_NoRef, true);
			if (ReturnProp->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				FunctionOutputs.Append(("&"));
			}
			OUTPUTS_TRIM_WHITESPACE
		}
	}

	FunctionInputs.RemoveFromEnd(TEXT(", "));
	FunctionOutputs.RemoveFromEnd(TEXT(", "));

	FString ArgString = FunctionInputs;
	if (FunctionInputs.Len() > 0 && (!bTreatFunctionOutputsAsReturnType && FunctionOutputs.Len() > 0))
	{
		ArgString += TEXT(", ");
	}

	if (!bTreatFunctionOutputsAsReturnType)
	{
		ArgString += FunctionOutputs;
	}
	else
	{
		OutReturnType = FunctionOutputs;
	}

	ArgString.ReplaceInline(TEXT("_C*"), TEXT("*"), ESearchCase::CaseSensitive);

	return ArgString;

#undef OUTPUTS_TRIM_WHITESPACE
}

FString UDoxygenCommandlet::CleanUpFunctionName(const FString& FunctionName)
{
	if (FunctionName.StartsWith(TEXT("InpActEvt_")) || FunctionName.StartsWith(TEXT("InpAxisEvt_")))
	{
		int32 Index;
		FunctionName.FindChar(TEXT('_'), Index);
		FString Output = FunctionName.RightChop(Index+1);
		Output.FindChar(TEXT('_'), Index);
		return TEXT("Input") + Output.Left(Index);
	}
	return FunctionName;
}

bool UDoxygenCommandlet::ShouldOutputFunction(const UFunction* Function)
{
	check(Function);

	// Skip latent functions
	if (Function->HasMetaData(FBlueprintMetadata::MD_Latent))
	{
		return false;
	}

	if (Function->GetName().StartsWith(TEXT("OnLoaded_")))
	{
		return false;
	}

	return true;
}

bool UDoxygenCommandlet::OutputFunctionGraphs(const UBlueprint* Blueprint, const TArray<UEdGraph*> FunctionGraphs, FString& PublicFunctions, FString& ProtectedFunctions, FString& PrivateFunctions, const FString InterfaceName /*= TEXT("") */)
{
	check(Blueprint);

	if (!InterfaceName.IsEmpty())
	{
		PublicFunctions.Append(FString::Printf(TEXT("\t//~ Begin %s Interface.\r\n\r\n"), *InterfaceName));
	}

	for (UEdGraph* FuncGraph : FunctionGraphs)
	{
		if (FuncGraph->GetFName() == UEdGraphSchema_K2::FN_UserConstructionScript)
		{
			continue;
		}

		FString FuncName = FuncGraph->GetFName().ToString();
		UFunction* Function = nullptr;
		bool bFuncIsOverride = true;

		Function = FBlueprintReflectionHelpers::GetSuperFunction(Blueprint, FuncGraph->GetFName());

		if (Function == nullptr)
		{
			bFuncIsOverride = !InterfaceName.IsEmpty();
			Function = FBlueprintReflectionHelpers::GetFunction(Blueprint, FuncGraph->GetFName());
		}

		if (Function == nullptr || !ShouldOutputFunction(Function))
		{
			continue;
		}

		// Set output based on access specifier
		FString& FuncOutput = ((Function->FunctionFlags & FUNC_Public) || ((Function->FunctionFlags & (FUNC_Public | FUNC_Protected | FUNC_Private)) == 0) || !InterfaceName.IsEmpty()) ? PublicFunctions : ((Function->FunctionFlags & FUNC_Protected) ? ProtectedFunctions : PrivateFunctions);

		FString Category = Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory);
		Category = Category.IsEmpty() ? TEXT("Default") : Category;

		TArray<FString> FuncFlags = FBlueprintReflectionHelpers::GetFunctionTags(Function, bFuncIsOverride);
		FString FuncMacro = FString::Join(FuncFlags, TEXT(", "));
		if (!FuncMacro.IsEmpty())
		{
			FuncMacro.Append(TEXT(", "));
		}

		FString ReturnType = TEXT("void");
		FString ArgString = GetArgStringFromFunction(Function, ReturnType);

		FuncOutput.Append(ConvertStringToCPPComment(Function->GetToolTipText().ToString()));
		FuncOutput.Append(TEXT("\r\n"));
		FuncOutput.Append(FString::Printf(TEXT("\tUFUNCTION(%sCategory = \"%s\"%s)"), *FuncMacro, *Category, *FBlueprintReflectionHelpers::GetFunctionMetadata(Function, bFuncIsOverride))); FuncOutput.Append(TEXT("\r\n"));;
		FuncOutput.Append(FString::Printf(TEXT("\tvirtual %s %s(%s)%s%s;"), *ReturnType, *CleanUpFunctionName(FuncName), *ArgString, Function->FunctionFlags & FUNC_Const ? TEXT(" const") : TEXT(""), bFuncIsOverride ? TEXT(" override") : TEXT(""))); FuncOutput.Append(TEXT("\r\n"));;
		FuncOutput.Append(TEXT("\r\n"));
	}

	if (!InterfaceName.IsEmpty())
	{
		PublicFunctions.Append(FString::Printf(TEXT("\t//~ End %s Interface.\r\n\r\n"), *InterfaceName));
	}

	return true;
}

