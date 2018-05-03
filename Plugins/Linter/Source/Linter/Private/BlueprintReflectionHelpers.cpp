#include "BlueprintReflectionHelpers.h"
#include "Engine/ObjectLibrary.h"
#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/UserDefinedStruct.h"
#include "Resources/Version.h"
#include "IPluginManager.h"
#include "AssetRegistryModule.h"

DEFINE_LOG_CATEGORY_STATIC(BlueprintReflectionHelpers, Log, All);


TArray<UBlueprint*> FBlueprintReflectionHelpers::LoadAllBlueprints(const TArray<FString> Paths)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	bool bPathSearch = (Paths.Num() > 0 && !Paths[0].IsEmpty());

	UE_LOG(BlueprintReflectionHelpers, Display, TEXT("Loading the asset registry..."));
	AssetRegistryModule.Get().SearchAllAssets(/*bSynchronousSearch =*/true);
	UE_LOG(BlueprintReflectionHelpers, Display, TEXT("Finished loading the asset registry. Loading assets..."));

	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAllAssets(AssetList, true);

	TArray<UBlueprint*> Blueprints;

	for (FAssetData const& Asset : AssetList)
	{
		FString const AssetPath = Asset.ObjectPath.ToString();

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
			UE_LOG(BlueprintReflectionHelpers, Warning, TEXT("'%s' is a redirector, skipping."), *AssetPath);
			continue;
		}

		if (Asset.AssetClass.IsEqual(TEXT("Blueprint")))
		{
			UBlueprint* LoadedObject = Cast<UBlueprint>(Asset.GetAsset());
			if (LoadedObject != nullptr)
			{
				UE_LOG(BlueprintReflectionHelpers, Display, TEXT("Pre-Loaded '%s'..."), *AssetPath);
				Blueprints.Push(LoadedObject);
			}
		}
	}

	return Blueprints;
}

UFunction* FBlueprintReflectionHelpers::GetFunction(const UBlueprint* Blueprint, const FName FunctionName)
{
	check(Blueprint);
	return Blueprint->GeneratedClass->FindFunctionByName(FunctionName, EIncludeSuperFlag::ExcludeSuper);
}

UFunction* FBlueprintReflectionHelpers::GetSuperFunction(const UBlueprint* Blueprint, const FName FunctionName)
{
	check(Blueprint);
	UClass* SuperClass = Blueprint->GeneratedClass->GetSuperClass();
	if (SuperClass != nullptr)
	{
		return SuperClass->FindFunctionByName(FunctionName, EIncludeSuperFlag::IncludeSuper);
	}

	return nullptr;
}

void FBlueprintReflectionHelpers::GetFunctionEntryAndResultNodeFromFunctionGraph(const class UEdGraph* FuncGraph, class UK2Node_FunctionEntry** Entry, class UK2Node_FunctionResult** Result)
{
	check(FuncGraph && Entry && Result);

	TArray<UK2Node_FunctionEntry*> EntryNodes;
	FuncGraph->GetNodesOfClass(EntryNodes);
	if (EntryNodes.Num() > 0)
	{
		*Entry = Cast<UK2Node_FunctionEntry>(EntryNodes[0]);
	}

	TArray<UK2Node_FunctionResult*> ResultNodes;
	FuncGraph->GetNodesOfClass(ResultNodes);
	if (ResultNodes.Num() > 0)
	{
		*Result = Cast<UK2Node_FunctionResult>(ResultNodes[0]);
	}
}

//UFunction* FBlueprintReflectionHelpers::GetFunctionFromEntryNode(const class UK2Node_FunctionEntry* EntryNode)
//{
//	UClass* ClassToLookup = EntryNode->SignatureClass;
//
//	if (EntryNode->SignatureClass && EntryNode->SignatureClass->ClassGeneratedBy)
//	{
//		UBlueprint* GeneratingBP = CastChecked<UBlueprint>(EntryNode->SignatureClass->ClassGeneratedBy);
//		ClassToLookup = GeneratingBP->SkeletonGeneratedClass;
//	}
//
//	return FindField<UFunction>(ClassToLookup, EntryNode->SignatureName);
//}

FString FBlueprintReflectionHelpers::GetArgumentTypeForPinType(const FEdGraphPinType& FuncPinType)
{
	auto PinTypeToNativeTypeInner = [](const FEdGraphPinType& InType) -> FString
	{
		auto Schema = GetDefault<UEdGraphSchema_K2>();
		if (UEdGraphSchema_K2::PC_String == InType.PinCategory)
		{
			return TEXT("FString");
		}
		else if (UEdGraphSchema_K2::PC_Boolean == InType.PinCategory)
		{
			return TEXT("bool");
		}
		else if ((UEdGraphSchema_K2::PC_Byte == InType.PinCategory) || (UEdGraphSchema_K2::PC_Enum == InType.PinCategory))
		{
			if (UEnum* Enum = Cast<UEnum>(InType.PinSubCategoryObject.Get()))
			{
				const bool bEnumClassForm = Enum->GetCppForm() == UEnum::ECppForm::EnumClass;
				const bool bNonNativeEnum = Enum->GetClass() != UEnum::StaticClass();
				ensure(!bNonNativeEnum || Enum->CppType.IsEmpty());
				const FString FullyQualifiedEnumName = (!Enum->CppType.IsEmpty()) ? Enum->CppType : GetCppName(Enum);
				// TODO: sometimes we need unwrapped type for enums without size specified. For example when native function has a raw ref param.
				return (bEnumClassForm || bNonNativeEnum) ? FullyQualifiedEnumName : FString::Printf(TEXT("TEnumAsByte<%s>"), *FullyQualifiedEnumName);
			}
			return TEXT("uint8");
		}
		else if (UEdGraphSchema_K2::PC_Int == InType.PinCategory)
		{
			return TEXT("int32");
		}
		else if (UEdGraphSchema_K2::PC_Float == InType.PinCategory)
		{
			return TEXT("float");
		}
		else if (UEdGraphSchema_K2::PC_Float == InType.PinCategory)
		{
			return TEXT("float");
		}
		else if (UEdGraphSchema_K2::PC_Name == InType.PinCategory)
		{
			return TEXT("FName");
		}
		else if (UEdGraphSchema_K2::PC_Text == InType.PinCategory)
		{
			return TEXT("FText");
		}
		else if (UEdGraphSchema_K2::PC_Struct == InType.PinCategory)
		{
			if (UScriptStruct* Struct = Cast<UScriptStruct>(InType.PinSubCategoryObject.Get()))
			{
				return GetCppName(Struct);
			}
		}
		else if (UEdGraphSchema_K2::PC_Class == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSubclassOf<%s>"), *GetCppName(Class));
			}
		}
#if ENGINE_MINOR_VERSION < 18
		else if (UEdGraphSchema_K2::PC_AssetClass == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TAssetSubclassOf<%s>"), *GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_Asset == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TAssetPtr<%s>"), *GetCppName(Class));
			}
		}
#else
		else if (UEdGraphSchema_K2::PC_SoftClass == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSoftClassPtr<%s>"), *GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_SoftObject == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *GetCppName(Class));
			}
		}
#endif
		else if (UEdGraphSchema_K2::PC_Interface == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("TScriptInterface<%s>"), *GetCppName(Class));
			}
		}
		else if (UEdGraphSchema_K2::PC_Object == InType.PinCategory)
		{
			if (UClass* Class = Cast<UClass>(InType.PinSubCategoryObject.Get()))
			{
				return FString::Printf(TEXT("%s*"), *GetCppName(Class));
			}
		}
		UE_LOG(BlueprintReflectionHelpers, Error, TEXT("Blueprint Reflection Helper could not determine type name for pin."));
		return FString{};
	};

	FString InnerTypeName = PinTypeToNativeTypeInner(FuncPinType);

#if ENGINE_MINOR_VERSION < 17
	return FuncPinType.bIsArray ? FString::Printf(TEXT("TArray<%s>"), *InnerTypeName) : InnerTypeName;
#else
	return FuncPinType.IsArray() ? FString::Printf(TEXT("TArray<%s>"), *InnerTypeName) : InnerTypeName;
#endif

}

bool FBlueprintReflectionHelpers::HasAllFlags(uint64 Flags, uint64 FlagsToCheck)
{
	return FlagsToCheck == (Flags & FlagsToCheck);
}

FString FBlueprintReflectionHelpers::GetFunctionMetadata(const UFunction* Function, bool bIsOverride /*= false*/)
{
	TArray<FString> MetaTags;
	if (Function->HasMetaData(FBlueprintMetadata::MD_CallInEditor))
	{
		MetaTags.Push(TEXT("CallInEditor"));
	}

	FString MetaString = FString::Join(MetaTags, TEXT(", "));
	if (!MetaString.IsEmpty())
	{
		return TEXT(", meta=(") + MetaString + TEXT(")");
	}
	else
	{
		return TEXT("");
	}
}

// From BlueprintCompilerCppBackendUtils.h
FString FBlueprintReflectionHelpers::GetCppName(const UField* Field, bool bUInterface /*= false*/, bool bForceParameterNameModification /*= false*/)
{
	check(Field);
	auto AsClass = Cast<UClass>(Field);
	auto AsScriptStruct = Cast<UScriptStruct>(Field);
	FString ClassName;
	if (AsClass || AsScriptStruct)
	{

		if (AsClass && AsClass->HasAnyClassFlags(CLASS_Interface))
		{
			ensure(AsClass->IsChildOf<UInterface>());
			ClassName = FString::Printf(TEXT("%s%s")
				, bUInterface ? TEXT("U") : TEXT("I")
				, *AsClass->GetName());
			ClassName.RemoveFromEnd(TEXT("_C"));
			return ClassName;
		}
		auto AsStruct = CastChecked<UStruct>(Field);
		if (AsStruct->IsNative())
		{
			ClassName = FString::Printf(TEXT("%s%s"), AsStruct->GetPrefixCPP(), *AsStruct->GetName());
		}
		else
		{
			ClassName = FString(AsStruct->GetPrefixCPP()) + AsStruct->GetName();
		}
		ClassName.RemoveFromEnd(TEXT("_C"));
		return ClassName;
	}
	else if (auto AsProperty = Cast<UProperty>(Field))
	{
		ClassName = AsProperty->GetNameCPP();
		ClassName.RemoveFromEnd(TEXT("_C"));
		return ClassName;
	}
	return Field->GetName();
}

#ifdef HANDLE_CPF_TAG
static_assert(false, "Macro HANDLE_CPF_TAG redefinition.");
#endif
#define HANDLE_CPF_TAG(TagName, CheckedFlags) if (HasAllFlags(Flags, (CheckedFlags))) { Tags.Emplace(TagName); }

TArray<FString> FBlueprintReflectionHelpers::GetFunctionTags(const UFunction* Function, bool bIsOverride)
{
	check(Function);
	uint64 Flags = Function->FunctionFlags;

	TArray<FString> Tags;

	//Pointless: CustomThunk
	//Pointless: ServiceRequest, ServiceResponse - only useful for native UFunctions, they're for serializing to json
	//Pointless: SealedEvent
	HANDLE_CPF_TAG(TEXT("Exec"), FUNC_Exec)
		HANDLE_CPF_TAG(TEXT("Server"), FUNC_Net | FUNC_NetServer)
		HANDLE_CPF_TAG(TEXT("Client"), FUNC_Net | FUNC_NetClient)
		HANDLE_CPF_TAG(TEXT("NetMulticast"), FUNC_Net | FUNC_NetMulticast)
		HANDLE_CPF_TAG(TEXT("Reliable"), FUNC_NetReliable)
		HANDLE_CPF_TAG(TEXT("BlueprintCallable"), FUNC_BlueprintCallable)
		HANDLE_CPF_TAG(TEXT("BlueprintPure"), FUNC_BlueprintCallable | FUNC_BlueprintPure)
		HANDLE_CPF_TAG(TEXT("BlueprintAuthorityOnly"), FUNC_BlueprintAuthorityOnly)
		HANDLE_CPF_TAG(TEXT("BlueprintCosmetic"), FUNC_BlueprintCosmetic)
		HANDLE_CPF_TAG(TEXT("WithValidation"), FUNC_NetValidate)

		if (HasAllFlags(Flags, FUNC_Net) && !HasAllFlags(Flags, FUNC_NetReliable))
		{
			Tags.Emplace(TEXT("Unreliable"));
		}

	if (bIsOverride)
	{
		if (HasAllFlags(Flags, FUNC_BlueprintEvent | FUNC_Native))
		{
			Tags.Emplace(TEXT("BlueprintNativeEvent"));
		}
	}

	//if (Function->HasMetaData(FBlueprintMetadata::MD_CallInEditor))
	//{
	//	Tags.Emplace(TEXT("CallInEditor"));
	//}

	return Tags;
}

#undef HANDLE_CPF_TAG