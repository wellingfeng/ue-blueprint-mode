// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#include "FreeUltraCodeBluePrintModePluginUpdateChecker.h"
#include "FreeUltraCodeBluePrintModeLog.h"
#include "Interfaces/IPluginManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/EngineVersionComparison.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "FreeUltraCodeBluePrintModePluginUpdateChecker"

namespace FreeUltraCodeBluePrintMode
{
namespace Private
{
	const TCHAR* GPluginName = TEXT("FreeUltraCodeBluePrintMode");
	// 版本清单：返回 { "version": "x.y.z", "downloadUrl": "..." }
	const TCHAR* GManifestUrl = TEXT("https://raw.githubusercontent.com/wellingfeng/ue-blueprint-mode/main/latest.json");
	const TCHAR* GDownloadPage = TEXT("https://github.com/wellingfeng/ue-blueprint-mode");
}
}

using namespace FreeUltraCodeBluePrintMode::Private;

bool FFreeUltraCodeBluePrintModePluginUpdateChecker::IsPluginEnabled()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(GPluginName);
	return Plugin.IsValid() && Plugin->IsEnabled();
}

FString FFreeUltraCodeBluePrintModePluginUpdateChecker::GetInstalledVersion()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(GPluginName);
	if (Plugin.IsValid())
	{
		return Plugin->GetDescriptor().VersionName;
	}
	return TEXT("0.0.0");
}

void FFreeUltraCodeBluePrintModePluginUpdateChecker::CheckAsync()
{
	// 1) 安装/启用自检。
	if (!IsPluginEnabled())
	{
		UE_LOG(LogFreeUltraCodeBluePrintMode, Warning, TEXT("FreeUltraCode BluePrint Mode 插件未被识别为已启用。"));
		ShowNotification(
			LOCTEXT("NotEnabled", "FreeUltraCode BluePrint Mode 插件未正确启用，请在 编辑 → 插件 中开启并重启编辑器。").ToString(),
			/*bHasUpdate*/ false, FString());
		return;
	}

	const FString Current = GetInstalledVersion();
	UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("FreeUltraCode BluePrint Mode 已启用，版本 %s。开始检查更新。"), *Current);

	// 2) 异步查询最新版本。
	QueryLatestVersion(Current);
}

void FFreeUltraCodeBluePrintModePluginUpdateChecker::QueryLatestVersion(const FString& CurrentVersion)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GManifestUrl);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

	Request->OnProcessRequestComplete().BindLambda(
		[CurrentVersion](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk)
		{
			if (!bOk || !Resp.IsValid() || Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
			{
				// 查询失败不打扰用户，仅记日志（自动化、静默降级）。
				UE_LOG(LogFreeUltraCodeBluePrintMode, Verbose, TEXT("版本检查请求失败，跳过。"));
				return;
			}

			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
			{
				return;
			}

			const FString Latest = Json->GetStringField(TEXT("version"));
			const FString DownloadUrl = Json->HasField(TEXT("downloadUrl"))
				? Json->GetStringField(TEXT("downloadUrl")) : FString(GDownloadPage);

			// 简单语义版本比较。
			auto VersionLess = [](const FString& A, const FString& B)
			{
				TArray<FString> PA, PB;
				A.ParseIntoArray(PA, TEXT("."));
				B.ParseIntoArray(PB, TEXT("."));
				const int32 N = FMath::Max(PA.Num(), PB.Num());
				for (int32 i = 0; i < N; ++i)
				{
					const int32 VA = PA.IsValidIndex(i) ? FCString::Atoi(*PA[i]) : 0;
					const int32 VB = PB.IsValidIndex(i) ? FCString::Atoi(*PB[i]) : 0;
					if (VA != VB) { return VA < VB; }
				}
				return false;
			};

			const bool bHasUpdate = !Latest.IsEmpty() && VersionLess(CurrentVersion, Latest);

			// 回到 game thread 弹通知。
			AsyncTask(ENamedThreads::GameThread, [bHasUpdate, CurrentVersion, Latest, DownloadUrl]()
			{
				if (bHasUpdate)
				{
					FFreeUltraCodeBluePrintModePluginUpdateChecker::ShowNotification(
						FString::Printf(TEXT("FreeUltraCode BluePrint Mode 有新版本 %s（当前 %s），点击下载更新。"), *Latest, *CurrentVersion),
						/*bHasUpdate*/ true, DownloadUrl);
				}
				else
				{
					UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("FreeUltraCode BluePrint Mode 已是最新版本 %s。"), *CurrentVersion);
				}
			});
		});

	Request->ProcessRequest();
}

void FFreeUltraCodeBluePrintModePluginUpdateChecker::ShowNotification(const FString& Message, bool bHasUpdate, const FString& DownloadUrl)
{
	FNotificationInfo Info(FText::FromString(Message));
	Info.bFireAndForget = !bHasUpdate;       // 有更新时常驻，便于点击
	Info.FadeOutDuration = 3.0f;
	Info.ExpireDuration = bHasUpdate ? 0.0f : 6.0f;

	if (bHasUpdate && !DownloadUrl.IsEmpty())
	{
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			LOCTEXT("Download", "下载更新"),
			LOCTEXT("DownloadTip", "打开下载页面"),
			FSimpleDelegate::CreateLambda([DownloadUrl]()
			{
				FPlatformProcess::LaunchURL(*DownloadUrl, nullptr, nullptr);
			}),
			SNotificationItem::CS_None));
	}

	FSlateNotificationManager::Get().AddNotification(Info);
}

#undef LOCTEXT_NAMESPACE
