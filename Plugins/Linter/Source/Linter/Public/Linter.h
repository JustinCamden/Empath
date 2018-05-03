// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

class FLinterManager;
class FLinterCommandletManager;

DECLARE_LOG_CATEGORY_EXTERN(LogLinter, Verbose, All);

class FLinterModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	void RegisterSettings();
	void UnregisterSettings();

	FLinterManager* GetLinterManager();
	FLinterCommandletManager* GetLinterCommandletManager();

private:
	FLinterManager* LinterManager;
	FLinterCommandletManager* LinterCommandletManager;
};