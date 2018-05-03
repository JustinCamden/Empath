// Copyright 2017 marynate. All Rights Reserved.

#include "PrefabAsset.h"
#include "PrefabToolHelpers.h"

#include "Misc/SecureHash.h"

struct FPrefabCustomVersion
{
	enum Type
	{
		// Before any version changes were made
		BeforeCustomVersionWasAdded = 0,
		// -----<new versions can be added above this line>-----
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
private:
	FPrefabCustomVersion() {}
};

const FString UPrefabAsset::PREFAG_TAG_PREFIX = FString(TEXT("PREFAB-"));

UPrefabAsset::UPrefabAsset(const FObjectInitializer& ObjectInitializer)
{
	PrefabId = NewPrefabId();
	PrefabPivot = FVector::ZeroVector;
	NumActors = 0;
}

void UPrefabAsset::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	OutTags.Add(FAssetRegistryTag("Actors", FString::FromInt(NumActors), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Asset References", FString::FromInt(AssetReferences.Num()), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Prefab ID", PrefabId.ToString(EGuidFormats::Digits), FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag("Prefab Hash", PrefabHash, FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag("Generated Blueprint", GeneratedBlueprintAssetReference.ToString(), FAssetRegistryTag::TT_Alphabetical));

	Super::GetAssetRegistryTags(OutTags);
}

FText UPrefabAsset::GetPrefabContent() const
{
	return PrefabContent;
}

void UPrefabAsset::SetPrefabContent(const FText& InText)
{
	PrefabContent = InText;

	UpdatePrefabHash();

#if WITH_EDITOR
	ParsePrefabMeta();
#endif
}

void UPrefabAsset::SetGeneratedBlueprintAssetReference(const FSoftObjectPath& InAssetReference)
{
	GeneratedBlueprintAssetReference = InAssetReference;

	UpdatePrefabHash();

#if WITH_EDITOR
	ParsePrefabMeta();
#endif
}

FString UPrefabAsset::GetPrefabTagPrefix() const
{
	FString MyPrefabTagPrefix = PREFAG_TAG_PREFIX;
	return MyPrefabTagPrefix.Append(PrefabId.ToString(EGuidFormats::Digits)).Append(TEXT(":"));
}

#if WITH_EDITOR
void UPrefabAsset::ParsePrefabMeta()
{
	const TCHAR* Content = *PrefabContent.ToString();
	FPrefabMetaData PrefabMeta;

	FPrefabToolEditorUtil::ParsePrefabText(Content, GWarn, PrefabMeta);

	AssetReferences = PrefabMeta.AssetReferences;
	NumActors = PrefabMeta.Actors.Num();
}
#endif

void UPrefabAsset::SetPrefabPivot(const FVector& InPivot)
{
	PrefabPivot = InPivot;
}

bool UPrefabAsset::IsEmpty() const
{
	return PrefabContent.IsEmptyOrWhitespace();
}

void UPrefabAsset::UpdatePrefabHash()
{
	PrefabHash = GetStringHash(PrefabContent.ToString()) + TEXT("-") + GetStringHash(PrefabPivot.ToString()) + TEXT("-") + GetStringHash(GeneratedBlueprintAssetReference.ToString());
}

FGuid UPrefabAsset::NewPrefabId()
{
	return FGuid::NewGuid();
}

FString UPrefabAsset::GetStringHash(const FString& InContentString)
{
	FSHA1 Hash;
	Hash.UpdateWithString(*InContentString, InContentString.Len());
	Hash.Final();
	FSHAHash SHAHash;
	Hash.GetHash(&SHAHash.Hash[0]);
	return SHAHash.ToString();
}

FString UPrefabAsset::NewPrefabInstanceId(const FString& PrefabTagPrefix)
{
	return PrefabTagPrefix + FGuid::NewGuid().ToString(EGuidFormats::Digits);
}
