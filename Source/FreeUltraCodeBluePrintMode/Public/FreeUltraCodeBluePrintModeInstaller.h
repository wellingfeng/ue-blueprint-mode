// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 安装结果。 */
struct FFreeUltraCodeBluePrintModeInstallResult
{
	bool bSuccess = false;
	FString Message;
	int32 FilesCopied = 0;
};

/**
 * 一键安装逻辑。
 * 支持从 GitHub 公开仓库下载，也支持从本地源目录拷贝插件到 UE 工程的 Plugins 下。
 */
class FREEULTRACODEBLUEPRINTMODE_API FFreeUltraCodeBluePrintModeInstaller
{
public:
	/**
	 * 从本地目录安装。
	 * @param SourceDir       源目录（含 .uplugin 的插件根）
	 * @param TargetDir       目标目录；空则取 工程根/Plugins/FreeUltraCodeBluePrintMode
	 * @param bOverwrite      目标已存在时是否覆盖
	 */
	static FFreeUltraCodeBluePrintModeInstallResult InstallFromLocal(const FString& SourceDir, const FString& TargetDir, bool bOverwrite);

	/** 从 GitHub 下载并安装。需要系统 PATH 中可执行 git。 */
	static FFreeUltraCodeBluePrintModeInstallResult InstallFromGithub(const FString& RepoUrl, const FString& TargetDir, bool bOverwrite);

	/** 解析默认目标目录：工程根/Plugins/FreeUltraCodeBluePrintMode。 */
	static FString ResolveDefaultTargetDir(const FString& SourceDir);

private:
	/** 递归拷贝目录。返回拷贝的文件数，失败时 OutError 非空。 */
	static int32 CopyDirectoryRecursive(const FString& From, const FString& To, bool bOverwrite, FString& OutError);
};
