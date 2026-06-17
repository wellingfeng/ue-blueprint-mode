// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FreeUltraCodeBluePrintModeTypes.h"

class FJsonObject;

/**
 * 运行时能力探测表。
 *
 * 不通过 UE 大版本硬编码直接放行，而是组合判断：
 * 引擎版本、模块存在、插件启用、关键类/API 是否可加载，以及当前执行后端是否已实现。
 */
class FREEULTRACODEBLUEPRINTMODE_API FFreeUltraCodeBluePrintModeCapabilityRegistry
{
public:
	static TArray<FFreeUltraCodeBluePrintModeCapability> DetectAll();
	static FString DetectAllAsJson();
	static void AppendCapabilitiesToJson(const TSharedRef<FJsonObject>& Out);
	static FString CapabilityIdToName(EFreeUltraCodeBluePrintModeCapabilityId CapabilityId);

private:
	static FFreeUltraCodeBluePrintModeCapability MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId CapabilityId,
		bool bSupported,
		bool bImplemented,
		const FString& Reason,
		const FString& MinVersion,
		const FString& Backend,
		bool bExperimental = false);

	static bool IsEngineAtLeast(int32 Major, int32 Minor);
	static bool ModuleExists(const TCHAR* ModuleName);
	static bool PluginEnabled(const FString& PluginName);
	static bool ClassExists(const FString& ClassPath, const FString& ShortClassName = FString());
	static FString EngineVersionString();
};
