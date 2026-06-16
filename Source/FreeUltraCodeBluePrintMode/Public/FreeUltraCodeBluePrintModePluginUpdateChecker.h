// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 插件自检：自动判断本插件是否已正确安装、是否有新版本，全程非阻塞。
 * 结果通过编辑器通知（Slate notification）提示用户，发现更新时给出下载入口。
 */
class FREEULTRACODEBLUEPRINTMODE_API FFreeUltraCodeBluePrintModePluginUpdateChecker
{
public:
	/** 模块启动后调用：异步检查安装状态与版本。 */
	static void CheckAsync();

	/** 读取本插件当前版本名（来自 .uplugin）。 */
	static FString GetInstalledVersion();

	/** 本插件是否已被引擎识别并启用。 */
	static bool IsPluginEnabled();

private:
	/** 向版本清单 endpoint 查询最新版本，比较后弹通知。 */
	static void QueryLatestVersion(const FString& CurrentVersion);

	/** 弹出编辑器通知。bHasUpdate=true 时附下载按钮。 */
	static void ShowNotification(const FString& Message, bool bHasUpdate, const FString& DownloadUrl);
};
