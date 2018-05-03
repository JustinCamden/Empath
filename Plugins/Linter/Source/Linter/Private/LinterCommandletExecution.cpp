// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#include "LinterCommandletExecution.h"
#include "CommandletHelpers.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "ScopedSlowTask.h"
#include "Editor.h"

const FText LinterInProgressText = NSLOCTEXT("Linter", "LinterInProgress", "Running Linter...");

TSharedPtr<FLinterCommandletProcess> FLinterCommandletProcess::Execute(const TArray<FString>& Paths)
{
	// Create process.
	FString CommandletArguments;

	for (auto PathIt = Paths.CreateConstIterator(); PathIt; ++PathIt)
	{
		if (!(*PathIt).IsEmpty())
		{
			CommandletArguments += FString::Printf(TEXT("\"%s\""), **PathIt);
		}
	}

	const FString ProjectFilePath = FString::Printf(TEXT("\"%s\""), *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()));
	const FString ProcessArguments = CommandletHelpers::BuildCommandletProcessArguments(TEXT("Linter"), *ProjectFilePath, *CommandletArguments);
	FProcHandle CommandletProcessHandle = FPlatformProcess::CreateProc(TEXT("UE4Editor.exe"), *ProcessArguments, false, false, false, nullptr, 0, nullptr, nullptr);
	
	if (!CommandletProcessHandle.IsValid())
	{
		return nullptr;
	}

	return MakeShareable(new FLinterCommandletProcess(CommandletProcessHandle, ProcessArguments));
}

FLinterCommandletProcess::~FLinterCommandletProcess()
{
	if (ProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(ProcessHandle))
	{
		FPlatformProcess::TerminateProc(ProcessHandle);
	}
}

bool FLinterCommandletManager::RunLinter(const TArray<FString>& Paths, const bool bBlock /*= false */)
{
	// Only run one linter process at a time
	if (CurrentLinterProcess.IsValid())
	{
		return false;
	}

	FNotificationInfo Info(LinterInProgressText);
	Info.bFireAndForget = bBlock;

	// Setting fade out and expire time to 0 as the expire message is currently very obnoxious
	Info.FadeOutDuration = 3.0f;
	Info.ExpireDuration = 0.0f;

	TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr = NotificationItem;

	if (NotificationItem.IsValid())
	{
		NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
		NotificationItem->SetVisibility(EVisibility::HitTestInvisible);
	}

	CurrentLinterProcess = FLinterCommandletProcess::Execute(Paths);

	if (bBlock)
	{
		BlockUntilFinished();
		return true;
	}

	return CurrentLinterProcess.IsValid();
}

bool FLinterCommandletManager::IsLinting() const
{
	return CurrentLinterProcess.IsValid() ? FPlatformProcess::IsProcRunning(CurrentLinterProcess->GetHandle()) : false;
}


void FLinterCommandletManager::Update()
{
	if (CurrentLinterProcess.IsValid())
	{
		TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();
		if (IsLinting())
		{
			if (NotificationItem.IsValid())
			{
				NotificationItem->SetText(LinterInProgressText);
				NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
			}
		}
		else
		{
			if (NotificationItem.IsValid())
			{
				NotificationItem->SetText(NSLOCTEXT("Linter", "LinterComplete", "Finished Linting."));
				NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
				NotificationItem->ExpireAndFadeout();

				NotificationPtr.Reset();
			}

			CurrentLinterProcess.Reset();
		}
	}
	
}

void FLinterCommandletManager::BlockUntilFinished()
{
	if (IsLinting())
	{
		FScopedSlowTask SlowTask(0, LinterInProgressText, GIsEditor && !IsRunningCommandlet());
		FPlatformProcess::WaitForProc(CurrentLinterProcess->GetHandle());
		Update();
	}
}


void FLinterCommandletManager::Tick(float DeltaTime)
{
	Update();
}


bool FLinterCommandletManager::IsTickable() const
{
	return true;
}


TStatId FLinterCommandletManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FLinterCommandletManager, STATGROUP_Tickables);
}

FLinterCommandletManager::~FLinterCommandletManager()
{
	TSharedPtr<SNotificationItem> NotificationItem = NotificationPtr.Pin();

	if (NotificationItem.IsValid())
	{
		NotificationItem->SetText(FText::GetEmpty());
		NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		NotificationItem->ExpireAndFadeout();

		NotificationPtr.Reset();
	}

	CurrentLinterProcess.Reset();
}
