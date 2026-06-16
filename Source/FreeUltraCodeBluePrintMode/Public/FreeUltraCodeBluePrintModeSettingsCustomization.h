// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

/**
 * 为「蓝图」设置页注入一键安装按钮。
 * 注册到 PropertyEditor 模块，匹配 UFreeUltraCodeBluePrintModeSettings 类。
 */
class FFreeUltraCodeBluePrintModeSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	/** 按钮点击：执行本地安装并刷新状态。 */
	FReply OnInstallClicked();

	/** 缓存当前正在编辑的设置对象。 */
	TWeakObjectPtr<class UFreeUltraCodeBluePrintModeSettings> SettingsPtr;

	/** 安装结果文本（按钮下方显示）。 */
	FText GetStatusText() const;
	FText StatusText;
};
