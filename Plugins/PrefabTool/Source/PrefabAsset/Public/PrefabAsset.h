// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPath.h"
#include "PrefabAsset.generated.h"

// Actor Record stored in Prefab Meta to describe base actor info
struct PREFABASSET_API FPrefabMetaActorRecord
{
	FName Name;
	FName Class;
	FString Archetype;
	FName ParentActor;
	FName GroupActor;

	TArray<FName> Tags;

	FPrefabMetaActorRecord() {};
	FPrefabMetaActorRecord(const FName& InName, const FName& InClass)
		: Name(InName), Class(InClass), ParentActor(NAME_None)
	{}
};

// Prefab Meta: contains stored actor records and asset references in a prefab
struct PREFABASSET_API FPrefabMetaData
{
	TArray<FPrefabMetaActorRecord> Actors;
	TMap<FString, FSoftObjectPath> AssetReferences;
	void Reset() 
	{
		Actors.Empty();
		AssetReferences.Empty();
	}
};

/**
* A Prefab is an asset stored collection of configured actors.
* Prefab can be dragged into level to restore saved actors.
* Prefab can be nested to support complex scene hierarchy.
*/
UCLASS(BlueprintType, hidecategories=(Object))
class PREFABASSET_API UPrefabAsset : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	
	UPROPERTY(VisibleAnywhere, Category = Prefab)
	FGuid PrefabId;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category= Prefab)
	FText PrefabContent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Prefab)
	FVector PrefabPivot;

	UPROPERTY(VisibleAnywhere, Category = Prefab)
	FString PrefabHash;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Prefab)
	TMap<FString, FSoftObjectPath> AssetReferences;

	UPROPERTY()
	int32 NumActors;

	UPROPERTY(VisibleAnywhere, Category = Prefab)
	FSoftObjectPath GeneratedBlueprintAssetReference;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;
#endif

public:
	//~ Begin UObject Interface.
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//~ End UObject Interface.

	bool IsEmpty() const;
	FText GetPrefabContent() const;
	void SetPrefabContent(const FText& InText);
	void SetGeneratedBlueprintAssetReference(const FSoftObjectPath& InAssetReference);

	FString GetPrefabTagPrefix() const;

	void SetPrefabPivot(const FVector& InPivot);

	static FString NewPrefabInstanceId(const FString& PrefabTagPrefix);

#if WITH_EDITOR
	void ParsePrefabMeta();
#endif

public:
	static const FString PREFAG_TAG_PREFIX;

private:
	void UpdatePrefabHash();
	FGuid NewPrefabId();
	FString GetStringHash(const FString& InContentString);
};
