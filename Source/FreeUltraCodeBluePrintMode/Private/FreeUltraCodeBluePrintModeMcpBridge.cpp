// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#include "FreeUltraCodeBluePrintModeMcpBridge.h"
#include "FreeUltraCodeBluePrintModeLog.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/ConfigCacheIni.h"

namespace FreeUltraCodeBluePrintMode
{
namespace Private
{
	const TCHAR* GDefaultEndpoint = TEXT("https://mcp.ludusengine.com/mcp");
	const TCHAR* GSettingsSection = TEXT("FreeUltraCodeBluePrintMode.BlueprintMode");
	const TCHAR* GLegacySettingsSection = TEXT("BlueprintMode");
}
}

using namespace FreeUltraCodeBluePrintMode::Private;

FString FFreeUltraCodeBluePrintModeMcpBridge::GetEndpoint()
{
	// 允许通过 EditorPerProjectUserSettings 覆盖。
	FString Endpoint;
	if (GConfig)
	{
		if (GConfig->GetString(GSettingsSection, TEXT("McpEndpoint"), Endpoint, GEditorPerProjectIni)
			&& !Endpoint.IsEmpty())
		{
			return Endpoint;
		}

		if (GConfig->GetString(GLegacySettingsSection, TEXT("McpEndpoint"), Endpoint, GEditorPerProjectIni)
			&& !Endpoint.IsEmpty())
		{
			return Endpoint;
		}
	}
	return GDefaultEndpoint;
}

bool FFreeUltraCodeBluePrintModeMcpBridge::IsAvailable()
{
	// 简化判断：有 endpoint 即视为可用。真实实现可加一次轻量 ping/握手。
	return !GetEndpoint().IsEmpty();
}

bool FFreeUltraCodeBluePrintModeMcpBridge::SendIntent(const FString& Intent, FString& OutResponse)
{
	if (!IsAvailable())
	{
		OutResponse = TEXT("MCP 未配置");
		return false;
	}

	// 构造 JSON-RPC 风格请求体。
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("method"), TEXT("blueprint.intent"));
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("intent"), Intent);
	Body->SetObjectField(TEXT("params"), Params);

	FString Payload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
	FJsonSerializer::Serialize(Body, Writer);

	// 同步等待（带超时），用于"必要时自动补全"的兜底路径。
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetEndpoint());
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(Payload);

	bool bDone = false;
	bool bSuccess = false;
	FString Captured;

	Request->OnProcessRequestComplete().BindLambda(
		[&bDone, &bSuccess, &Captured](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnectedOk)
		{
			bDone = true;
			if (bConnectedOk && Resp.IsValid() && Resp->GetResponseCode() >= 200 && Resp->GetResponseCode() < 300)
			{
				bSuccess = true;
				Captured = Resp->GetContentAsString();
			}
			else
			{
				Captured = Resp.IsValid()
					? FString::Printf(TEXT("MCP HTTP %d"), Resp->GetResponseCode())
					: TEXT("MCP 连接失败");
			}
		});

	Request->ProcessRequest();

	// 阻塞等待，最多 30s。
	const double StartTime = FPlatformTime::Seconds();
	const double Timeout = 30.0;
	while (!bDone && (FPlatformTime::Seconds() - StartTime) < Timeout)
	{
		FHttpModule::Get().GetHttpManager().Tick(0.1f);
		FPlatformProcess::Sleep(0.05f);
	}

	OutResponse = bDone ? Captured : TEXT("MCP 请求超时");
	return bSuccess;
}

bool FFreeUltraCodeBluePrintModeMcpBridge::TryCompletePlanRemainder(const FFreeUltraCodeBluePrintModeOpPlan& Plan, int32 AppliedCount, const FString& LocalError)
{
	if (!IsAvailable())
	{
		UE_LOG(LogFreeUltraCodeBluePrintMode, Warning, TEXT("MCP 不可用，无法补全剩余操作。本地错误：%s"), *LocalError);
		return false;
	}

	// 把剩余 op 概述成自然语言意图交给 MCP。
	const int32 Remaining = Plan.Ops.Num() - AppliedCount;
	const FString Intent = FString::Printf(
		TEXT("本地已应用 %d 个蓝图操作后遇到限制（%s）。请通过 Python Execution 完成剩余 %d 个操作：%s"),
		AppliedCount, *LocalError, Remaining, *Plan.Summary);

	FString Response;
	const bool bOk = SendIntent(Intent, Response);
	UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("MCP 补全 %s：%s"), bOk ? TEXT("成功") : TEXT("失败"), *Response);
	return bOk;
}
