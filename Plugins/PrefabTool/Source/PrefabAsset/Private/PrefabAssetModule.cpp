// Copyright 2017 marynate. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * Implements the PrefabAsset module.
 */
class FPrefabAssetModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override 
	{
	}

	virtual void ShutdownModule() override 
	{
	}

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}
};


IMPLEMENT_MODULE(FPrefabAssetModule, PrefabAsset);
