// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "CoreUObject.h"
#include "EngineMinimal.h"
#include "CoreMinimal.h"

#define ENABLE_PREFABTOOL_LOG 0

DECLARE_LOG_CATEGORY_EXTERN(LogPrefabTool, Verbose, All);

#if ENABLE_PREFABTOOL_LOG
#define PREFABTOOL_LOG(Verbosity, Format, ...) \
	UE_LOG(LogPrefabTool, Verbosity, Format, ##__VA_ARGS__);
#else
#define PREFABTOOL_LOG(Verbosity, Format, ...)
#endif

class AActor;
class AGroupActor;

class APrefabActor;
class UPrefabAsset;

#if WITH_EDITOR

/**
* Provide own implementation of vary editor methods
*/
struct PREFABASSET_API FPrefabGEditorAdapter
{
	static bool EditorDestroyActor(AActor* Actor, bool bShouldModifyLevel);
	
	static bool GEditor_CanParentActors(const AActor* ParentActor, const AActor* ChildActor, FText* ReasonText);

	static void edactPasteSelected(UWorld* InWorld, bool bDuplicate, bool bOffsetLocations, bool bWarnIfHidden, FString* SourceData, const TMap<FString, FSoftObjectPath>& AssetRemapper, TArray<AActor*>& OutSpawnActors, TArray<AGroupActor*>* OutGroupActorsPtr, EObjectFlags InObjectFlags);

	//@ULevelFactory
	static UObject* LevelFactory_FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, const TCHAR* InType, const TCHAR*& InBuffer, const TCHAR* InBufferEnd, FFeedbackContext* InWarn, const TMap<FString, FSoftObjectPath>& InAssetRemapper, TArray<AActor*>& OutSpawnActors, TArray<AGroupActor*>* OutGroupActorsPtr);
};

struct PREFABASSET_API FPrefabParser
{
	/**
	* Converts the supplied export text path to an object path and class name.
	*
	* @param InExportTextPath The export text path for an object. Takes on the form: ClassName'ObjectPath'
	* @param OutClassName The name of the class at the start of the path.
	* @param OutObjectPath The path to the object.
	* @return True if the supplied export text path could be parsed
	*/
	static bool ParseExportTextPath(const FString& InExportTextPath, FString* OutClassName, FString* OutObjectPath);
};

/**
* Helpers for dealing with prefab actor
*/
struct PREFABASSET_API FPrefabActorUtil
{
	static APrefabActor* GetFirstAttachedParentPrefabActor(AActor* InActor);

	static APrefabActor* GetFirstLockedParentPrefabActor(AActor* InActor);

	static bool IsAttachToConnectedChildPrefabActor(const TArray<APrefabActor*>& AllAttachedChildPrefabActors, AActor* InActor);

	static void GetAllParentPrefabActors(AActor* InActor, TArray<APrefabActor*>& OutParentPrefabActors, const APrefabActor* RootPrefabActor = nullptr);

	static void GetAllParentPrefabAssets(AActor* InActor, TArray<UPrefabAsset*>& OutParentPrefabAsset, const APrefabActor* RootPrefabActor = nullptr);

	static void GetAllAttachedChildren(AActor* Parent, TArray<AActor*>& OutChildren);

	static void ShowHidePrefabActor(class APrefabActor* PrefabActor, bool bVisible);

	static void SelectPrefabActorsInWorld(const TArray<TWeakObjectPtr<class UPrefabAsset>>& Prefabs);

	static void AttachSelectedActorsToPrefabActor();

	static AActor* SpawnEmptyActor(ULevel* InLevel, const FTransform& Transform, const FName Name = NAME_None, EObjectFlags InObjectFlags = RF_Transactional, EComponentMobility::Type InComponentMobility = EComponentMobility::Static);

	static bool IsActorSupported(AActor* Actor, const class UPrefabToolSettings* PrefabToolSettings);

	static void SortActorsHierarchy(TArray<AActor*>& Actors, bool bFromParentToChild);

	static void FilterParentActors(const TArray<AActor*>& InActors, TArray<AActor*>& OutParentActors);

	static FBox GetAllComponentsBoundingBox(AActor* InActor, bool bRecursive = true);

#if 0
	static void AddSelectedActorToPrefabAsset(class UPrefabAsset* InPrefabAsset);
#endif
};

/**
* Main prefab editing logic
*/
struct PREFABASSET_API FPrefabToolEditorUtil
{
	static void SpawnPrefabInstances(class UPrefabAsset* Asset, UWorld* InWorld, TArray<AActor*>& OutSpawnPrefabInstances, TArray<class AGroupActor*>* OutSpawnGroupActorsPtr, EObjectFlags InObjectFlags = RF_Transactional);

	static void PostSpawnPrefabInstances(class UPrefabAsset* Asset, TArray<AActor*>& InSpawnPrefabInstances, APrefabActor* InParentPrefabActor, TArray<class AGroupActor*>* InSpawnGroupActorsPtr);

	static void AttachSpawnInstances(class UPrefabAsset* Asset, TArray<AActor*>& InSpawnActors, AActor* InParentActor, bool bKeepRelativetransform = true);

	static void edactCopySelectedForNewPrefab(UWorld* InWorld, FString* DestinationData, const FString& InPrefabTagPrefix, const FVector& LastSelectedPivot);

	static void RebuildAlteredBSP(const TArray<AActor*> InActors);

	static void ReplacePrefabInstances(class UPrefabAsset* Asset, const TMap<FName, AActor*> &OldPrefabInstancesMap, TMap<FName, AActor*>& NewPrefabInstancesMap, TArray<AActor*>* ActorsToDelete = nullptr);

	static void RevertPrefabActor(class APrefabActor* PrefabActor, bool bRevertIfPrefabChanged = false);

	static void RevertPrefabActorEvenDisconnected(class APrefabActor* PrefabActor, bool bRevertOnIfPrefabChanged = false);

	static void ApplyPrefabActor(class APrefabActor* PefabActor, FVector InNewPrefabPivot = FVector::ZeroVector);

	static void RevertAllPrefabActorsInCurrentLevel(bool bRevertIfPrefabChanged = true);

	static void RevertAllReferencedActorsInWorld(UWorld* World, UPrefabAsset* PrefabAssetToFind, APrefabActor* ExcludeActor);

	static void DestroyPrefabActor(class APrefabActor* PrefabActor, bool bDestroyInstanceActors = true);

	static void DeletePrefabInstances(class APrefabActor* PrefabActor);

	static void DeleteAllTransientPrefabInstancesInCurrentLevel(class UWorld* InWorld, uint32 SaveFlags);

	static void RestoreAllTransientPrefabInstancesInCurrentLevel(class UWorld* InWorld, uint32 SaveFlags);

	static void ConvertPrefabActorToNormalActor(class APrefabActor* PrefabActor);

	static void ParsePrefabText(const TCHAR*& Buffer, FFeedbackContext* Warn, struct FPrefabMetaData& OutPrefabMeta);

	static void ReplaceSelectedActorsAfterPrefabCreated(UPrefabAsset* PrefabAsset, const FTransform& NewActorTransform, AActor* AttachParent = NULL, FName AttachSocketName = NAME_None);

	static void NotifyMessage(const FText& Message, float InDuration = 1.0f);

	static bool bSkipSelectionMonitor;
	static void BeginSkipSelectionMonitor() { bSkipSelectionMonitor = true; };
	static void EndSkipSelectionMonitor() { bSkipSelectionMonitor = false; };

private:
	static void DoRevertPrefabActor(class APrefabActor* PrefabActor, bool bRevertingDisconnectedPrefab = false, bool bRevertIfPrefabChanged = false, int32 Depth = 0);

	static void ValidatePrefabInstancesTag(const TArray<AActor*>& InActors, const APrefabActor* RootPrefabActor = nullptr);

	static void EnsureActorsHavePrefabInstanceTag(const TArray<AActor*>& InActors, const FString& InPrefabTagPrefix);

	static void DeletePrefabInstancesTag(class APrefabActor* PrefabActor);

	static void GatherPrefabInstancesMap(class UPrefabAsset* Prefab, APrefabActor* RootPrefabActor, const TArray<AActor*>& InPrefabInstances, TMap<FName, AActor*>* OutPrefabInstancesMapPtr, bool bDeleteInvalid = true);

	static void CenterGroupActors(TArray<AGroupActor*>& InGroupActors);

	static void ReplaceActor(AActor* OldActor, AActor* NewActor);
};

/**
* Helpers for dealing with prefab tags
*/
struct PREFABASSET_API FPrefabTagUtil
{
	static void BackupActorTags(const TArray<AActor*>& InActors, TMap<AActor*, TArray<FName>>& OutActorTags);

	static void RestoreActorTags(TArray<AActor*>& InActors, const TMap < AActor*, TArray<FName>>& InActorTags);

	static int32 GetTagCount(const TArray<FName>& InTags, const FName& InTag);

	static int32 GetTagCountInActors(const TArray<AActor*>& InActors, const FName& InTag);

	/** Get count of tag is valid in given prefix array */
	static int32 GetPrefixCount(const FString& InTagString, const TArray<FString>& InValidPrefixs);

	/** Get count of tags with specific prefix */
	static int32 GetTagPrefixCount(const TArray<FName>& InTags, const FString& InPrefabPrefix);

	/** Get prefab prefix part from an instance tag ("PREFAB-PrefabId:") */
	static FString GetPrefabPrefixFromTag(const FString& TagString);

	/** Get first found instance tag from tags */
	static FName GetPrefabInstanceTag(const TArray<FName>& InTags, const FString& InPrefabPrefix);

	/** Merge instance tags from old actor into new actor */
	static void MergeActorInstanceTags(const TArray<FName>& OldActorTags, TArray<FName>& NewActorTags, const FString& KeepPrefabTagPrefix);

	static void DeleteActorTagByPrefix(AActor* Actor, const FString& InTagPrefix);
};

/**
* Generate blueprint from a prefab
*/
class PREFABASSET_API  FCreateBlueprintFromPrefabActor
{
public:
	FCreateBlueprintFromPrefabActor()
		: Blueprint(nullptr)
		//, SCS(nullptr)
	{
	}

	UBlueprint* CreateBlueprint(FString Path, APrefabActor* PrefabActor, bool bReplaceInWorld);

	UBlueprint* UpdateBlueprint(UBlueprint* ExistingBlueprint, APrefabActor* PrefabActor, bool bReplaceInWorld);

private:
	UBlueprint* NewBlueprint(APrefabActor* PrefabActor, UPackage* Package, FName BlueprintName);
	void AddChildNodes(APrefabActor* PrefabActor, USimpleConstructionScript* SCS, TMap<class USceneComponent*, class USCS_Node*>& NodeMap);

protected:
	UBlueprint* Blueprint;
	//USimpleConstructionScript* SCS;
};
#endif
