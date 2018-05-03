// Copyright 2017 Gamemakin LLC. All Rights Reserved.

#include "DokuwikiExporter.h"
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
#include "SourceCodeNavigation.h"
#include "KismetEditorUtilities.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "SGraphPanel.h"
#include "SlateApplication.h"
#include "StandaloneRenderer.h"
#include "TooltipStringHelper.h"
#include "BlueprintFunctionNodeSpawner.h"
#include "WidgetRenderer.h"
#include "HighResScreenshot.h"
#include "Engine/TextureRenderTarget2D.h"

#if SUPPORTS_GENERATING_NODE_IMAGES
#include "NodeFactory.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(DokuwikiExporter, Log, All);


FDokuwikiExporter::FDokuwikiExporter(const FString _APINamespace, const FString _OutputDir /*= FPaths::GameSavedDir() / TEXT("DokuwikiExport") / FDateTime::Now().ToString()*/)
    : APINamespace(_APINamespace)
	, OutputDir(_OutputDir)
	, EmptyBlueprint(nullptr)
	, TempGraph(nullptr)
{
	APIPath = _APINamespace.Replace(TEXT(":"), TEXT("/"));
	SetUpBlueprintNodeRenderer();
}

FDokuwikiExporter::~FDokuwikiExporter()
{
#if PLATFORM_WINDOWS
	if (!IsRunningCommandlet())
	{
		FString TargetDir = FPaths::ConvertRelativePathToFull(OutputDir);
		FPaths::MakePlatformFilename(TargetDir);
		FPlatformProcess::CreateProc(TEXT("explorer"), *TargetDir, true, false, false, nullptr, 0, nullptr, nullptr);
	}
#endif

	if (EmptyBlueprint != nullptr)
	{
		if (TempGraph != nullptr)
		{
			TempGraph->RemoveFromRoot();
			TempGraph = nullptr;
		}

		EmptyBlueprint->RemoveFromRoot();
		EmptyBlueprint = nullptr;
	}
}

void FDokuwikiExporter::SetUpBlueprintNodeRenderer()
{
	if (EmptyBlueprint != nullptr)
	{
		return;
	}

	EmptyBlueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), GetTransientPackage(), NAME_None, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	TempGraph = FBlueprintEditorUtils::CreateNewGraph(EmptyBlueprint, TEXT("ScratchPad"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());

	// Prevent GC
	EmptyBlueprint->AddToRoot();
	TempGraph->AddToRoot();

	SAssignNew(GraphPanel, SGraphPanel).GraphObj(TempGraph);
	GraphPanel->RestoreViewSettings(FVector2D(0.0f, 0.0f), 10.0f); // Zoom in for higher LOD, from KantanDocGen

	FHighResScreenshotConfig& HighResScreenshotConfig = GetHighResScreenshotConfig();
	HighResScreenshotConfig.Init(20);

	check(EmptyBlueprint && TempGraph && GraphPanel.IsValid());
}

void FDokuwikiExporter::OutputBlueprintClass(const UBlueprint* Blueprint)
{
	check(Blueprint);

	bool bIsInterface = false;
	bool bIsConst = false;

	switch (Blueprint->BlueprintType)
	{
	case BPTYPE_MacroLibrary:
		UE_LOG(DokuwikiExporter, Warning, TEXT("Output for MacroLibraies not supported. (%s)"), *Blueprint->GetFName().ToString());
		return;
	case BPTYPE_Const:
		bIsConst = true;
		break;
	case BPTYPE_Interface:
		bIsInterface = true;
		break;
	case BPTYPE_LevelScript:
		UE_LOG(DokuwikiExporter, Warning, TEXT("Output for Blueprint Level Script not supported. (%s)"), *Blueprint->GetFName().ToString());
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
		InterfaceName.RemoveFromEnd(TEXT("_C"));
		Interfaces.Push(InterfaceName);
		InterfaceInheritString.Append(TEXT("[[") + InterfaceName + TEXT("]], "));
	}
	InterfaceInheritString.RemoveFromEnd(TEXT(", "));


	Output.Append(ConvertToHeading(Blueprint->GetFName().ToString(), 1));
	Output.Append(TEXT("\r\n"));
	Output.Append(ConvertToFolded(TEXT("Inheritance Hierarchy"), BuildInheritanceHierarchy(Blueprint)));
	Output.Append(TEXT("\r\n"));
	if (!InterfaceInheritString.IsEmpty())
	{
		Output.Append(TEXT("Implements interface") + (Interfaces.Num() > 1 ? FString(TEXT("s ")) : FString(TEXT(" "))) + InterfaceInheritString + TEXT(".\r\n"));
		Output.Append(TEXT("\r\n"));
	}
	Output.Append(ConvertToFolded(TEXT("Description"), Blueprint->BlueprintDescription.Len() > 0 ? Blueprint->BlueprintDescription : TEXT("No description provided.")));
	Output.Append(TEXT("\r\n"));

	FString PublicFunctions;
	FString ProtectedFunctions;
	FString PrivateFunctions;
	FString FuncName;

	FString FunctionTableHeader = TEXT("^ ^ Name ^ Category ^ Description ^");

	OutputFunctionGraphs(Blueprint, Blueprint->FunctionGraphs, PublicFunctions, ProtectedFunctions, PrivateFunctions);
	OutputFunctionGraphs(Blueprint, Blueprint->EventGraphs, PublicFunctions, ProtectedFunctions, PrivateFunctions, TEXT(""), true);

	FString FunctionList;

	if (!PublicFunctions.IsEmpty())
	{
		FunctionList.Append(ConvertToFolded(TEXT("Public Functions"), FunctionTableHeader + PublicFunctions));
	}

	if (!ProtectedFunctions.IsEmpty())
	{
		FunctionList.Append(ConvertToFolded(TEXT("Protected Functions"), FunctionTableHeader + ProtectedFunctions));
	}

	if (!PrivateFunctions.IsEmpty())
	{
		FunctionList.Append(ConvertToFolded(TEXT("Private Functions"), FunctionTableHeader + PrivateFunctions));
	}

	if (!FunctionList.IsEmpty())
	{
		FunctionList.Append(TEXT("\r\n"));
	}

	for (int32 i = 0; i < Blueprint->ImplementedInterfaces.Num(); ++i)
	{
		PublicFunctions.Empty();
		ProtectedFunctions.Empty();
		PrivateFunctions.Empty();

		OutputFunctionGraphs(Blueprint, Blueprint->ImplementedInterfaces[i].Graphs, PublicFunctions, ProtectedFunctions, PrivateFunctions, Interfaces[i]);
	
		FunctionList.Append(ConvertToFolded(FString::Printf(TEXT("[[%s]] Interface Functions"), *Interfaces[i]), FunctionTableHeader + PublicFunctions + ProtectedFunctions + PrivateFunctions));
	}

	if (Blueprint->ImplementedInterfaces.Num() > 0)
	{
		FunctionList.Append(TEXT("\r\n"));
	}

	FString ComponentHeader = TEXT("^ Name ^ Type ^ Description ^");
	FString Components;
	FString VarHeader = TEXT("^ Name ^ Type ^ Category ^ Description ^");
	FString EditableVars;
	FString NoneditableVars;

	for (FBPVariableDescription VarDesc : Blueprint->NewVariables)
	{
		FString VarName = VarDesc.VarName.ToString();
		FString VarType = *UEdGraphSchema_K2::TypeToText(VarDesc.VarType).ToString();
		FString Desc = VarDesc.HasMetaData(FBlueprintMetadata::MD_Tooltip) ? VarDesc.GetMetaData(FBlueprintMetadata::MD_Tooltip).Replace(TEXT("|"), TEXT("%%|%%")).Replace(TEXT("\r"), TEXT("")).Replace(TEXT("\n"), TEXT(" ")) : TEXT("");
		FString Category = VarDesc.Category.ToString().Replace(TEXT("|"), TEXT("%%|%%"));

		if (FBlueprintEditorUtils::IsVariableComponent(VarDesc))
		{
			Components.Append(FString::Printf(TEXT("\r\n| %s | %s | %s |"), *VarName, *VarType, *Desc));
		}
		else if ((VarDesc.PropertyFlags & CPF_DisableEditOnInstance) != CPF_DisableEditOnInstance)
		{
			EditableVars.Append(FString::Printf(TEXT("\r\n| %s | %s | %s | %s |"), *VarName, *VarType, *Category, *Desc));
		}
		else
		{
			NoneditableVars.Append(FString::Printf(TEXT("\r\n| %s | %s | %s | %s |"), *VarName, *VarType, *Category, *Desc));
		}
	}

	if (!Components.IsEmpty())
	{
		Output.Append(ConvertToFolded(TEXT("Components"), ComponentHeader + Components));
		Output.Append(TEXT("\r\n"));
	}

	Output.Append(FunctionList);

	if (!EditableVars.IsEmpty())
	{
		Output.Append(ConvertToFolded(TEXT("Editable Variables"), VarHeader + EditableVars));
		Output.Append(TEXT("\r\n"));
	}
	if (!NoneditableVars.IsEmpty())
	{
		Output.Append(ConvertToFolded(TEXT("Non-Editable Variables"), VarHeader + NoneditableVars));
		Output.Append(TEXT("\r\n"));
	}
	

	FString FullPath = FPaths::ConvertRelativePathToFull(OutputDir / TEXT("pages") / APIPath / Blueprint->GetFName().ToString() + TEXT(".txt")).ToLower();

	FFileHelper::SaveStringToFile(Output, *FullPath);

}

FString FDokuwikiExporter::GetChildClasses(UClass* Class, int32 Level, FString Base)
{
	check(Class);

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* TestClass = *ClassIt;
		if (!TestClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			if (TestClass->GetSuperClass() == Class)
			{
				FString TestClassName = TestClass->GetName();
				TestClassName.RemoveFromEnd(TEXT("_C"));
				Base.Append(FString::ChrN((Level + 1) * 2 + 2, TEXT(' ')) + TEXT("* [[") + TestClassName + TEXT("]]\r\n"));
				Base = GetChildClasses(TestClass, Level + 1, Base);
			}
		}
	}

	return Base;
}

FString FDokuwikiExporter::BuildInheritanceHierarchy(const UBlueprint* Blueprint)
{
	TArray<FString> Hierarchy;

	TSubclassOf<UObject> CurrentClass = Blueprint->GeneratedClass;
	while (CurrentClass != nullptr)
	{
		FString SourcePath;
		FSourceCodeNavigation::FindClassHeaderPath(CurrentClass, SourcePath);

		if (SourcePath.StartsWith(TEXT("../../../Engine/Source/")))
		{
			SourcePath = FPaths::GetPath(SourcePath);
			SourcePath = SourcePath.RightChop(FString(TEXT("../../../Engine/Source/")).Len());
			SourcePath.ReplaceInline(TEXT("/Public/"), TEXT("/"));
			SourcePath.ReplaceInline(TEXT("/Private/"), TEXT("/"));
			SourcePath.ReplaceInline(TEXT("/Classes/"), TEXT("/"));
			SourcePath /= CurrentClass->GetPrefixCPP() + CurrentClass->GetName() + TEXT("/");
			SourcePath = TEXT("https://docs.unrealengine.com/latest/INT/API/") + SourcePath;

			Hierarchy.Insert(TEXT("[[") + SourcePath + TEXT("|") + CurrentClass->GetPrefixCPP() + CurrentClass->GetName() + TEXT("]]"), 0);
		}
		else if (!CurrentClass->IsNative())
		{
			FString ClassName = CurrentClass->GetName();
			ClassName.RemoveFromEnd(TEXT("_C"));
			if (CurrentClass != Blueprint->GeneratedClass)
			{
				Hierarchy.Insert(TEXT("[[") + ClassName + TEXT("]]"), 0);
			}
			else
			{
				Hierarchy.Insert(ClassName, 0);
			}
			
		}

		CurrentClass = CurrentClass->GetSuperClass();
	}

	FString Output;
	for (int32 i = 0; i < Hierarchy.Num(); ++i)
	{
		Output.Append(FString::ChrN(i * 2 + 2, TEXT(' ')) + TEXT("* ") + Hierarchy[i] + TEXT("\r\n"));
	}

	Output = GetChildClasses(Blueprint->GeneratedClass, Hierarchy.Num(), Output);

	return Output;
}

FString FDokuwikiExporter::ConvertToHeading(const FString& Text, const uint32& HeadingLevel)
{
	if (HeadingLevel > 0 && HeadingLevel <= 5)
	{
		FString HeadingMarkup = FString::ChrN(7 - HeadingLevel, TEXT('='));
		return HeadingMarkup + TEXT(" ") + Text + TEXT(" ") + HeadingMarkup + TEXT("\r\n");
	}
	return Text;
}

FString FDokuwikiExporter::ConvertToFolded(const FString& Title, const FString& Content)
{
	return TEXT("++++ ") + Title + TEXT(" |\r\n") + Content + TEXT("\r\n++++\r\n");
}

FString FDokuwikiExporter::CleanUpFunctionName(const FString& FunctionName)
{
	if (FunctionName.StartsWith(TEXT("InpActEvt_")) || FunctionName.StartsWith(TEXT("InpAxisEvt_")))
	{
		int32 Index;
		FunctionName.FindChar(TEXT('_'), Index);
		FString Output = FunctionName.RightChop(Index + 1);
		Output.FindChar(TEXT('_'), Index);
		return TEXT("Input") + Output.Left(Index);
	}
	return FunctionName;
}

bool FDokuwikiExporter::ShouldOutputFunction(const UFunction* Function)
{
	check(Function);

	// Skip latent functions
	if (Function->HasMetaData(FBlueprintMetadata::MD_Latent))
	{
		return false;
	}

	// Skip BP uncallable functions
	if (!Function->HasAllFunctionFlags(FUNC_BlueprintCallable))
	{
		return false;
	}

	// Skip some event graph generated functions
	if (Function->GetName().StartsWith(TEXT("OnLoaded_")))
	{
		return false;
	}

	if (Function->GetName().StartsWith(TEXT("OnSuccess_")))
	{
		return false;
	}

	if (Function->GetName().StartsWith(TEXT("OnFailure_")))
	{
		return false;
	}

	if (Function->GetName().StartsWith(TEXT("BndEvt")))
	{
		return false;
	}

	return true;
}

void FDokuwikiExporter::GetFunctionInputsAndOutputs(const UFunction* Function,  TArray<FBPFunctionArgumentDescription>& Inputs, TArray<FBPFunctionArgumentDescription>& Outputs)
{
	check(Function);
	FStringOutputDevice FunctionInputs;
	FStringOutputDevice FunctionOutputs;

	FText FunctionDescription;
	TArray<TSharedPtr<FBPFunctionArgumentDescription>> TooltipInputs;
	TArray<TSharedPtr<FBPFunctionArgumentDescription>> TooltipOutputs;
	FText ReturnText;

	FTooltipStringHelper::ParseFunctionRawTooltip(Function->GetToolTipText().ToString(), FunctionDescription, TooltipInputs, TooltipOutputs, ReturnText);

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
		bool bIsSingleReturn = bIsOutput && i + 1 == Properties.Num();

		FString ArgType = TypeOverride;
		if (ArgType.IsEmpty())
		{
			ArgType = Properties[i]->GetCPPType(nullptr, bIsSingleReturn ? CPPF_NoRef : 0);
		}
		if (!Properties[i]->IsNative())
		{
			ArgType.RemoveFromEnd(TEXT("*"));
			ArgType.RemoveFromEnd(TEXT("_C"));
			if (UStruct* Struct = Cast<UStruct>(Properties[i]))
			{
				ArgType.RemoveFromStart(Struct->GetPrefixCPP());
			}
		}
		
		TArray<TSharedPtr<FBPFunctionArgumentDescription>>& Lookup = bIsOutput ? TooltipOutputs : TooltipInputs;
		TSharedPtr<FBPFunctionArgumentDescription>* ExistingInfo = Lookup.FindByPredicate([&](TSharedPtr<FBPFunctionArgumentDescription> Item){ return Item->ArgumentName.EqualTo(FText::FromString(Properties[i]->GetName()));});
		if (ExistingInfo != nullptr) // Tooltip has data for this already
		{
			bIsOutput ? Outputs.Push(**ExistingInfo) : Inputs.Push(**ExistingInfo);
		}
		else if (bIsSingleReturn) // If return, we either have ReturnText or no description at all
		{
			Outputs.Push(FBPFunctionArgumentDescription(FText::FromString(Properties[i]->GetName()), ReturnText, FText::FromString(ArgType)));
		}
		else // Push an empty description
		{
			FBPFunctionArgumentDescription ArgDesc(FText::FromString(Properties[i]->GetName()), FText::GetEmpty(), FText::FromString(ArgType));
			bIsOutput ? Outputs.Push(ArgDesc) : Inputs.Push(ArgDesc);
		}
	}

}

bool FDokuwikiExporter::OutputFunctionGraphs(const UBlueprint* Blueprint, const TArray<UEdGraph*> FunctionGraphs, FString& PublicFunctions, FString& ProtectedFunctions, FString& PrivateFunctions, const FString InterfaceName /*= TEXT("") */, const bool bAreEvents/* = false*/)
{
	check(Blueprint);

	if (!InterfaceName.IsEmpty())
	{
		//PublicFunctions.Append(FString::Printf(TEXT("\t//~ Begin %s Interface.\r\n\r\n"), *InterfaceName));
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

		FString Description = FTooltipStringHelper::ParseFunctionRawTooltipGetDescription(Function->GetToolTipText().ToString(), true).ToString();
		if (Description.Equals(Function->GetDisplayNameText().ToString()))
		{
			Description = TEXT("");
		}

		Category.ReplaceInline(TEXT("|"), TEXT("%%|%%"));
		Description.ReplaceInline(TEXT("|"), TEXT("%%|%%"));

		FString ClassName = Blueprint->GetFName().ToString();

		FString ImgPath = FPaths::ConvertRelativePathToFull(OutputDir / TEXT("media") / APIPath / Blueprint->GetFName().ToString().ToLower() / FuncName.ToLower() + TEXT("_k2node.png"));
		FString ImgText;
		FString FuncNamespace = FString::Printf(TEXT("%s:%s:%s"), *APINamespace, *ClassName.ToLower(), *FuncName.ToLower());
		if (GenerateNodeImage(Function, ImgPath))
		{
			ImgText = FString::Printf(TEXT("[[%s|{{%s_k2node.png}}]]"), *FuncNamespace, *FuncNamespace);
		}

		FuncOutput.Append(FString::Printf(TEXT("\r\n|  %s | [[%s|%s]] | %s | %s |"), *ImgText, *FuncNamespace, *Function->GetDisplayNameText().ToString(), *Category, *Description));
	
		// Function Page
		FString DescriptionWithWhitespace = FTooltipStringHelper::ParseFunctionRawTooltipGetDescription(Function->GetToolTipText().ToString()).ToString();
		DescriptionWithWhitespace.ReplaceInline(TEXT("|"), TEXT("%%|%%"));
#if ENGINE_MINOR_VERSION < 18
		DescriptionWithWhitespace.TrimTrailing();
#else
		DescriptionWithWhitespace.TrimEndInline();
#endif

		if (!DescriptionWithWhitespace.EndsWith(TEXT("."))) // Try to add a period to tooltips that don't end in one.
		{
			DescriptionWithWhitespace.Append(TEXT("."));
		}

		FString PageOutput = ConvertToHeading(Function->GetDisplayNameText().ToString(), 1);
		PageOutput.Append(TEXT("\r\n"));
		PageOutput.Append(FString::Printf(TEXT("Category: %s\r\n"), *Category));
		PageOutput.Append(FString::Printf(TEXT("Defined in class [[%s:%s|%s]].\r\n"), *APINamespace, *ClassName.ToLower(), *ClassName));
		PageOutput.Append(TEXT("\r\n"));
		PageOutput.Append(DescriptionWithWhitespace);
		PageOutput.Append(TEXT("\r\n\r\n"));
		PageOutput.Append(FString::Printf(TEXT("{{%s_k2node.png}}\r\n"), *FuncNamespace));
		PageOutput.Append(TEXT("\r\n"));

		FString ArgumentsHeader = TEXT("^ Name ^ Type ^ Description ^");

		TArray<FBPFunctionArgumentDescription> Inputs;
		TArray<FBPFunctionArgumentDescription> Outputs;

		GetFunctionInputsAndOutputs(Function, Inputs, Outputs);

		if (Inputs.Num() > 0)
		{
			FString InputString;
			InputString = ArgumentsHeader;
			for (FBPFunctionArgumentDescription ArgDesc : Inputs)
			{
				InputString.Append(FString::Printf(TEXT("\r\n| %s | %s | %s |"), *ArgDesc.ArgumentName.ToString(), *ArgDesc.ArgumentType.ToString(), *ArgDesc.Tooltip.ToString().Replace(TEXT("|"), TEXT("%%|%%"))));
			}
			PageOutput.Append(ConvertToFolded(TEXT("Function Inputs"), InputString));
			PageOutput.Append(TEXT("\r\n"));
		}

		if (Outputs.Num() > 0)
		{
			FString OutputString;
			OutputString = ArgumentsHeader;
			for (FBPFunctionArgumentDescription ArgDesc : Outputs)
			{
				OutputString.Append(FString::Printf(TEXT("\r\n| %s | %s | %s |"), *ArgDesc.ArgumentName.ToString(), *ArgDesc.ArgumentType.ToString(), *ArgDesc.Tooltip.ToString().Replace(TEXT("|"), TEXT("%%|%%"))));
			}
			PageOutput.Append(ConvertToFolded(TEXT("Function Outputs"), OutputString));
		}

		FFileHelper::SaveStringToFile(PageOutput, *FPaths::ConvertRelativePathToFull(OutputDir / TEXT("pages") / APIPath / Blueprint->GetFName().ToString() / *FuncName + TEXT(".txt")).ToLower());

	}

	if (!InterfaceName.IsEmpty())
	{
		//PublicFunctions.Append(FString::Printf(TEXT("\t//~ End %s Interface.\r\n\r\n"), *InterfaceName));
	}

	return true;
}

// Core taken from KantanDocGen - https://github.com/kamrann/KantanDocGenPlugin
bool FDokuwikiExporter::GenerateNodeImage(UField* Field, const FString ImagePath)
{
#if !SUPPORTS_GENERATING_NODE_IMAGES
	UE_LOG(DokuwikiExporter, Warning, TEXT("Generating function node images currently not supported on engine versions older than 4.17"));
	return false;
#else
	check(TempGraph);
	UBlueprintNodeSpawner* Spawner = nullptr;

	UFunction* Function = Cast<UFunction>(Field);
	if (Function != nullptr)
	{
		Spawner = UBlueprintFunctionNodeSpawner::Create(Function);
	}

	if (Spawner != nullptr)
	{
		UK2Node* Node = Cast<UK2Node>(Spawner->Invoke(TempGraph, {}, FVector2D(0.0f, 0.0f)));
		if (Node != nullptr)
		{
			// Hide default value box containing 'self' for Target pin - From KantanDocGen
			if (const UEdGraphSchema_K2* K2_Schema = Cast<UEdGraphSchema_K2>(Node->GetSchema()))
			{
				if (UEdGraphPin* TargetPin = Node->FindPin(K2_Schema->PN_Self))
				{
					TargetPin->bDefaultValueIsIgnored = true;
				}
			}

			TSharedPtr<SGraphNode> GraphNode = FNodeFactory::CreateNodeWidget(Node);
			GraphNode->SetOwner(GraphPanel.ToSharedRef());

			FWidgetRenderer Renderer(true, true);
			Renderer.SetIsPrepassNeeded(true);

			// Have to manually create render target to set our clear color
			UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
			RenderTarget->Filter = TF_Bilinear;
			RenderTarget->ClearColor = FLinearColor(0.15f, 0.15f, 0.15f, 1.0f);
			RenderTarget->SRGB = true;
			RenderTarget->TargetGamma = 1;
			RenderTarget->InitCustomFormat(1024.0f, 1024.0f, PF_B8G8R8A8, true);
			RenderTarget->UpdateResourceImmediate(true);

			Renderer.DrawWidget(RenderTarget, GraphNode.ToSharedRef(), FVector2D(1024.0f, 1024.0f), 0);
			const FIntPoint Size = GraphNode->GetDesiredSize().IntPoint();

			FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
			FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
			ReadPixelFlags.SetLinearToGamma(true);

			TArray<FColor> PixelData;
			if (!RTResource->ReadPixels(PixelData, ReadPixelFlags, FIntRect(0, 0, Size.X, Size.Y)))
			{
				UE_LOG(DokuwikiExporter, Error, TEXT("Could not read pixels from blueprint node render target for field %s."), *Field->GetName());
				FBlueprintEditorUtils::RemoveNode(EmptyBlueprint, Node, true);
				return false;
			}

			FHighResScreenshotConfig& HighResScreenshotConfig = GetHighResScreenshotConfig();
			HighResScreenshotConfig.SetHDRCapture(false);

			if (HighResScreenshotConfig.SaveImage(ImagePath.ToLower(), PixelData, Size))
			{
				FBlueprintEditorUtils::RemoveNode(EmptyBlueprint, Node, true);
				return true;
			}
		}
	}

	UE_LOG(DokuwikiExporter, Error, TEXT("Could not render blueprint node for field %s."), *Field->GetName());
	return false;
#endif
}
