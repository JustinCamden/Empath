// Copyright 2017 marynate. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "PrefabAssetFactory.generated.h"

/**
* PrefabAssetFactory responsible for creating prefab asset
*/
UCLASS(hidecategories=Object)
class UPrefabAssetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UFactory
	//virtual UObject* FactoryCreateBinary(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override;
	//~ End UFactory

private:
	void PostCreateNew(UObject* PrefabAsset, const FTransform& InTransform);
};
