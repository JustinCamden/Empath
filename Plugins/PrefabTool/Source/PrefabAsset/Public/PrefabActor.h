// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "PrefabActor.generated.h"

/**
* PrefabActor act as a host of prefab instance. 
* PrefabAsset dragged into the level from the Content Browser are automatically converted into PrefabActor
* Main Prefab related logic will be delegated to PrefabComponent
* @see UPrefabComponent
*/
UCLASS(hidecategories=(Input), ConversionRoot, ComponentWrapperClass, NotBlueprintable) // NotBlueprintable hides components tree
class PREFABASSET_API APrefabActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category=Prefab, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Prefab,Rendering,Physics,Components|Prefab", AllowPrivateAccess = "true"))
	class UPrefabComponent* PrefabComponent;

public:

	/**
	 * Set Mobility of owned PrefabComponent
	 */
	UFUNCTION(BlueprintCallable, Category = Actor)
	void SetMobility(EComponentMobility::Type InMobility); 
	
	/**
	 * Set prefab asset, will call RevertPrefabActor on current actor if inside editor
	 * @param NewPrefab							The new prefab asset to assign
	 * @param bForceRevertEvenDisconnected		Revert prefab even it's disconnected
	 */
	UFUNCTION(BlueprintCallable, Category = Prefab)
	void SetPrefab(class UPrefabAsset* NewPrefab, bool bForceRevertEvenDisconnected = false);

	/**
	* Get prefab asset from PrefabComponent
	*/
	UFUNCTION(BlueprintCallable, Category = Prefab)
	class UPrefabAsset* GetPrefab();

	/**
	 * Destroy current actor in editor with option to destroy all attached children actors
	 * If current actor is attached to parent actor, will try re-attached children actors to the attached parent actor
	 * No effect in runtime
	 * @param bDestroyAttachedChildren	If true all attached children actors will be destroyed too
	 */
	UFUNCTION(BlueprintCallable, Category = Prefab)
	void DestroyPrefabActor(bool bDestroyAttachedChildren);

	class UPrefabComponent* GetPrefabComponent() const;

	void SetAndModifyLockSelection(bool bInLocked);
	void SetLockSelection(bool bInLocked);
	bool GetLockSelection();

	bool IsConnected() const;

	bool IsTransient() const;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface
	
	//~ Begin AActor Interface
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	virtual void PostEditMove(bool bFinished) override;
	//~ End AActor Interface

	static void SetSuppressPostDuplicate(bool bSuppress) { bSuppressPostDuplicate = bSuppress; }
#endif // WITH_EDITOR

private:
	static bool bSuppressPostDuplicate;
};
