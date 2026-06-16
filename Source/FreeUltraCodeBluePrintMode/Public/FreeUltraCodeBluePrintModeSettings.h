// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FreeUltraCodeBluePrintModeSettings.generated.h"

/**
 * 项目设置中的「蓝图」分类页。
 * 继承 UDeveloperSettings 后会自动出现在 项目设置 → FreeUltraCode BluePrint Mode 分组下，
 * 按钮通过 IDetailCustomization 注入（见 BlueprintModeSettingsCustomization）。
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "FreeUltraCode 蓝图模式"))
class FREEULTRACODEBLUEPRINTMODE_API UFreeUltraCodeBluePrintModeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFreeUltraCodeBluePrintModeSettings();

	// 让设置页出现在 "Project" 类别下，分组名 "FreeUltraCode BluePrint Mode"。
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
	virtual FName GetSectionName() const override { return FName(TEXT("FreeUltraCodeBluePrintMode.BlueprintMode")); }

	/**
	 * 一键安装的源目录。
	 * GitHub 不可用时，可从该本地目录拷贝插件到 UE 工程的 Plugins 下。
	 */
	UPROPERTY(config, EditAnywhere, Category = "安装", meta = (DisplayName = "本地源目录"))
	FString LocalSourceDir = TEXT("E:/ue-blueprint-mode");

	/**
	 * 目标安装路径。空则默认装到当前工程的 Plugins/FreeUltraCodeBluePrintMode。
	 */
	UPROPERTY(config, EditAnywhere, Category = "安装", meta = (DisplayName = "目标安装目录（留空=工程 Plugins）"))
	FString TargetInstallDir;

	/** 安装时若目标已存在是否覆盖。 */
	UPROPERTY(config, EditAnywhere, Category = "安装", meta = (DisplayName = "覆盖已存在文件"))
	bool bOverwriteExisting = true;

	/** GitHub 仓库地址。一键安装默认从这里下载公开仓库。 */
	UPROPERTY(config, EditAnywhere, Category = "安装", meta = (DisplayName = "GitHub 仓库"))
	FString GithubRepoUrl = TEXT("https://github.com/wellingfeng/ue-blueprint-mode.git");

	/** 安装状态展示（只读，安装后更新）。 */
	UPROPERTY(VisibleAnywhere, Transient, Category = "状态", meta = (DisplayName = "上次安装结果"))
	FString LastInstallResult;
};
