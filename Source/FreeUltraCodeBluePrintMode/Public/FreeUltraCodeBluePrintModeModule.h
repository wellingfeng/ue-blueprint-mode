// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FreeUltraCode BluePrint Mode 编辑器模块。
 * 负责注册控制台命令 /FreeUltraCodeBluePrintMode.BlueprintMode.Start 与 /FreeUltraCodeBluePrintMode.BlueprintMode.End，
 * 并在引擎初始化后触发插件自身的更新检测。
 */
class FFreeUltraCodeBluePrintModeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** 注册 start/end 控制台命令。 */
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();

	/** 解析命令行参数字符串为 start 参数结构。 */
	static struct FFreeUltraCodeBluePrintModeStartArgs ParseStartArgs(const TArray<FString>& Args);

	TArray<IConsoleObject*> ConsoleCommands;
};
