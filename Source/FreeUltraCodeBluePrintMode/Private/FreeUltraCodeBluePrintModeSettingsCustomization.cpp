// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#include "FreeUltraCodeBluePrintModeSettingsCustomization.h"
#include "FreeUltraCodeBluePrintModeSettings.h"
#include "FreeUltraCodeBluePrintModeInstaller.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "FreeUltraCodeBluePrintModeSettingsCustomization"

TSharedRef<IDetailCustomization> FFreeUltraCodeBluePrintModeSettingsCustomization::MakeInstance()
{
	return MakeShared<FFreeUltraCodeBluePrintModeSettingsCustomization>();
}

void FFreeUltraCodeBluePrintModeSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// 拿到正在编辑的设置对象。
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	for (const TWeakObjectPtr<UObject>& Obj : Objects)
	{
		if (UFreeUltraCodeBluePrintModeSettings* S = Cast<UFreeUltraCodeBluePrintModeSettings>(Obj.Get()))
		{
			SettingsPtr = S;
			StatusText = FText::FromString(S->LastInstallResult);
			break;
		}
	}

	// 在「安装」分类里加一行自定义按钮。
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
		TEXT("安装"), FText::GetEmpty(), ECategoryPriority::Important);

	Category.AddCustomRow(LOCTEXT("InstallRow", "一键安装"))
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SBox)
				.MaxDesiredWidth(220.f)
				.HAlign(HAlign_Left)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.OnClicked(this, &FFreeUltraCodeBluePrintModeSettingsCustomization::OnInstallClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("InstallButton", "一键安装（从 GitHub 下载）"))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(this, &FFreeUltraCodeBluePrintModeSettingsCustomization::GetStatusText)
			]
		];
}

FText FFreeUltraCodeBluePrintModeSettingsCustomization::GetStatusText() const
{
	return StatusText;
}

FReply FFreeUltraCodeBluePrintModeSettingsCustomization::OnInstallClicked()
{
	UFreeUltraCodeBluePrintModeSettings* Settings = SettingsPtr.Get();
	if (!Settings)
	{
		return FReply::Handled();
	}

	FFreeUltraCodeBluePrintModeInstallResult Result = FFreeUltraCodeBluePrintModeInstaller::InstallFromGithub(
		Settings->GithubRepoUrl,
		Settings->TargetInstallDir,
		Settings->bOverwriteExisting);

	if (!Result.bSuccess && !Settings->LocalSourceDir.IsEmpty())
	{
		Result.Message += TEXT("\n正在尝试从本地源目录安装...");
		const FFreeUltraCodeBluePrintModeInstallResult LocalResult = FFreeUltraCodeBluePrintModeInstaller::InstallFromLocal(
			Settings->LocalSourceDir,
			Settings->TargetInstallDir,
			Settings->bOverwriteExisting);
		Result.bSuccess = LocalResult.bSuccess;
		Result.FilesCopied = LocalResult.FilesCopied;
		Result.Message += TEXT("\n") + LocalResult.Message;
	}

	// 更新设置页内的状态文本与持久字段。
	Settings->LastInstallResult = Result.Message;
	StatusText = FText::FromString(Result.Message);

	// 同时弹一个编辑器通知。
	FNotificationInfo Info(FText::FromString(Result.Message));
	Info.ExpireDuration = Result.bSuccess ? 6.0f : 10.0f;
	Info.bUseSuccessFailIcons = true;
	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(Result.bSuccess
			? SNotificationItem::CS_Success
			: SNotificationItem::CS_Fail);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
