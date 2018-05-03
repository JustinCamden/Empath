// Copyright 2016 Gamemakin LLC. All Rights Reserved.

#pragma once

#include "TickableEditorObject.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Notifications/SNotificationList.h"

struct FProcHandle;

class FLinterCommandletProcess : public TSharedFromThis<FLinterCommandletProcess>
{
	friend class FLinterCommandletManager;
protected:
	static TSharedPtr<FLinterCommandletProcess> Execute(const TArray<FString>& Paths);

private:
	FLinterCommandletProcess(FProcHandle& InProcessHandle, const FString& InProcessArguments)
		: ProcessHandle(InProcessHandle)
		, ProcessArguments(InProcessArguments)
	{
	}

public:
	~FLinterCommandletProcess();

	FProcHandle& GetHandle()
	{
		return ProcessHandle;
	}

	const FString& GetProcessArguments() const
	{
		return ProcessArguments;
	}

private:
	FProcHandle& ProcessHandle;
	FString ProcessArguments;
};

class FLinterCommandletManager : public FTickableEditorObject
{
	friend class FLinterCommandletProcess;
public:
	FLinterCommandletManager() : CurrentLinterProcess(nullptr) {}

	bool RunLinter(const TArray<FString>& Paths, const bool bBlock = false);
	bool IsLinting() const;
	void Update();
	void BlockUntilFinished();

	/** FTickableEditorObject interface */
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

	~FLinterCommandletManager();

private:
	TSharedPtr<FLinterCommandletProcess> CurrentLinterProcess;

	/** The actual item being shown */
	TWeakPtr<SNotificationItem> NotificationPtr;
};