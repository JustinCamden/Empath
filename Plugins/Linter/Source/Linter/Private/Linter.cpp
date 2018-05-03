// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "Linter.h"
#include "LinterCommandletExecution.h"
#include "LinterStyle.h"
#include "ISettingsModule.h"
#include "LinterSettings.h"
#include "ContentBrowserExtensions.h"
#include "LinterManager.h"

#define LOCTEXT_NAMESPACE "FLinterModule"

void FLinterModule::StartupModule()
{
	LinterManager = nullptr;

	// Integrate Linter actions into existing editor context menus
	if (!IsRunningCommandlet())
	{
		// Register slate style overrides
		FLinterStyle::Initialize();
		FLinterContentBrowserExtensions::InstallHooks();
		LinterCommandletManager = new FLinterCommandletManager();
	}
	else
	{
		LinterManager = new FLinterManager();
	}

	RegisterSettings();
}

void FLinterModule::ShutdownModule()
{
	if (LinterManager != nullptr)
	{
		delete LinterManager;
		LinterManager = nullptr;
	}

	if (UObjectInitialized())
	{
		FLinterContentBrowserExtensions::RemoveHooks();

		// Unregister slate style overrides
		FLinterStyle::Shutdown();
	}

	UnregisterSettings();
}
void FLinterModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "Plugins",
			LOCTEXT("RuntimeSettingsName", "Linter"),
			LOCTEXT("RuntimeSettingsDescription", "Configure the Linter plugin"),
			GetMutableDefault<ULinterSettings>());
	}
}

void FLinterModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Plugins");
	}
}

FLinterManager* FLinterModule::GetLinterManager()
{
	if (ensure(IsRunningCommandlet() && LinterManager != nullptr))
	{
		return LinterManager;
	}
	else
	{
		return nullptr;
	}
	
}

FLinterCommandletManager* FLinterModule::GetLinterCommandletManager()
{
	if (ensure(!IsRunningCommandlet() && LinterCommandletManager != nullptr))
	{
		return LinterCommandletManager;
	}
	else
	{
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLinterModule, Linter)
DEFINE_LOG_CATEGORY(LogLinter);