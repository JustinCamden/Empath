// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "CoreUObject.h"
#include "EngineMinimal.h"
#include "PrefabComponent.generated.h"

class APrefabActor;

/**
* PrefabComponent is used to create an instance of a Prefab.
* @see UPrefabAsset
*/
UCLASS(ClassGroup = Common, hidecategories = (Object, Activation, "Components|Activation"), ShowCategories = (Mobility))
class PREFABASSET_API UPrefabComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:

	/** If receive update from prefab asset. */
	UPROPERTY(EditAnywhere, Category = Prefab)
	uint32 bConnected : 1;

	/** If lock selection for all children instance actors */
	UPROPERTY(EditAnywhere, Category = Prefab)
	uint32 bLockSelection : 1;

	/** Generated blueprint from prefab actor */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, AdvancedDisplay, Category = Prefab)
	UBlueprint* GeneratedBlueprint;

	/** The prefab asset this component is using. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, AdvancedDisplay, Category = Prefab, meta = (DisplayThumbnail = "true", AllowPrivateAccess = "true"))
	class UPrefabAsset* Prefab; 
		
	/** Mapping from prefab asset to spawned prefab actor instances */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, AdvancedDisplay, Category = Prefab, meta = (AllowPrivateAccess = "true"))
	TMap<FName, AActor*> PrefabInstancesMap;

	/** If true, will not be saved in current map. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, AdvancedDisplay, Category = Prefab)
	uint32 bTransient : 1;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	FString CachedPrefabHash;

	/** Optional actor use as prefab pivot when applying changes to prefab asset. */
// 	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, AdvancedDisplay, Category = Prefab, meta = (AllowPrivateAccess = "true"))
// 	AActor* PrefabPivot;

	/** Sprite for connected prefab in the editor. */
	UPROPERTY(transient)
	UTexture2D* PrefabConnectedEditorTexture;

	/** Sprite scaling for connected prefab in the editor. */
	UPROPERTY(transient)
	float PrefabConnectedEditorTextureScale;

	/** Sprite for dis-connected prefab in the editor. */
	UPROPERTY(transient)
	UTexture2D* PrefabDisConnectedEditorTexture;

	/** Sprite scaling for dis-connected prefab in the editor. */
	UPROPERTY(transient)
	float PrefabDisConnectedEditorTextureScale;

#endif

public:

	//~ Begin USceneComponent Interface
// 	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
// 	virtual bool ShouldCollideWhenPlacing() const override { return true; }
	//~ End USceneComponent Interface

	virtual ~UPrefabComponent();

	const void GetAllAttachedChildren(AActor* Parent, TArray<AActor*>& OutChildren) const;
	const void GetAllPotentialChildrenInstanceActors(TArray<AActor*>& OutChildren) const;
	const void GetDirectAttachedInstanceActors(TArray<AActor*>& OutInstanceActors) const;
	const void GetDirectAttachedPrefabActors(TArray<APrefabActor*>& OutPrefabActors) const;
	const void GetAllAttachedPrefabActors(TArray<APrefabActor*>& OutChildrenActors) const;

	void DeleteInvalidAttachedChildren(bool bRecursive);

	const TMap<FName, AActor*>& GetPrefabInstancesMap() const;
	TMap<FName, AActor*>& GetPrefabInstancesMap();
	void ValidatePrefabInstancesMap(bool bRecursive = false, TArray<AActor*>* OutInValidActorsPtr = nullptr);

	void SetConnected(bool bInConnected, bool bRecursive /*=false*/);
	FORCEINLINE bool GetConnected() const { return bConnected; }

	void SetLockSelection(bool bInLocked, bool bCheckParent = false, bool bRecursive = false);
	FORCEINLINE bool GetLockSelection() const { return bLockSelection; }

	FORCEINLINE UPrefabAsset* GetPrefab() const { return Prefab; }

#if WITH_EDITOR
	bool SetPrefab(class UPrefabAsset* NewPrefab);
	bool IsPrefabContentChanged(bool bRecursive = false) const;
	FVector GetPrefabPivotOffset(bool bWorldSpace) const;
#endif

protected:

	//~ Begin USceneComponent Interface
	//virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//virtual bool ShouldCollideWhenPlacing() const override { return false;  }
	//~ End USceneComponent Interface

#if WITH_EDITOR
	//~ Begin UObject Interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface.

	//~ Begin ActorComponent Interface
	virtual void OnRegister() override;
 	virtual void OnUnregister() override;
	//~ End ActorComponent Interface

	UTexture2D* GetEditorSprite() const;
	float GetEditorSpriteScale() const;
	void UpdatePrefabSpriteTexture();
#endif

};
