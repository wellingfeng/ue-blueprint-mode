// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#include "FreeUltraCodeBluePrintModeModule.h"
#include "FreeUltraCodeBluePrintModeTypes.h"
#include "FreeUltraCodeBluePrintModeSubsystem.h"
#include "FreeUltraCodeBluePrintModeSettings.h"
#include "FreeUltraCodeBluePrintModeSettingsCustomization.h"
#include "FreeUltraCodeBluePrintModeLog.h"
#include "FreeUltraCodeBluePrintModePluginUpdateChecker.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FFreeUltraCodeBluePrintModeModule"

DEFINE_LOG_CATEGORY(LogFreeUltraCodeBluePrintMode);

namespace
{
	/** 从 "--key value" / "--flag" 形式的 token 列表里取值。 */
	FString GetArgValue(const TArray<FString>& Args, const FString& Key, const FString& Default)
	{
		for (int32 i = 0; i < Args.Num(); ++i)
		{
			if (Args[i].Equals(Key, ESearchCase::IgnoreCase) && Args.IsValidIndex(i + 1))
			{
				return Args[i + 1];
			}
		}
		return Default;
	}

	bool HasFlag(const TArray<FString>& Args, const FString& Flag)
	{
		return Args.ContainsByPredicate([&Flag](const FString& A) { return A.Equals(Flag, ESearchCase::IgnoreCase); });
	}

	EFreeUltraCodeBluePrintModeContextLevel ParseContext(const FString& In)
	{
		if (In.Equals(TEXT("overview"), ESearchCase::IgnoreCase))   return EFreeUltraCodeBluePrintModeContextLevel::Overview;
		if (In.Equals(TEXT("header"), ESearchCase::IgnoreCase))     return EFreeUltraCodeBluePrintModeContextLevel::Header;
		if (In.Equals(TEXT("full"), ESearchCase::IgnoreCase))       return EFreeUltraCodeBluePrintModeContextLevel::Full;
		if (In.Equals(TEXT("properties"), ESearchCase::IgnoreCase)) return EFreeUltraCodeBluePrintModeContextLevel::Properties;
		return EFreeUltraCodeBluePrintModeContextLevel::Balanced; // 默认
	}
}

FFreeUltraCodeBluePrintModeStartArgs FFreeUltraCodeBluePrintModeModule::ParseStartArgs(const TArray<FString>& Args)
{
	FFreeUltraCodeBluePrintModeStartArgs Out;
	Out.TargetName        = GetArgValue(Args, TEXT("--target"), TEXT(""));
	Out.ContextLevel      = ParseContext(GetArgValue(Args, TEXT("--context"), TEXT("balanced")));
	Out.AgentTier         = GetArgValue(Args, TEXT("--agent"), TEXT("full")).Equals(TEXT("lite"), ESearchCase::IgnoreCase)
								? EFreeUltraCodeBluePrintModeAgentTier::Lite : EFreeUltraCodeBluePrintModeAgentTier::Full;
	Out.bDryRun           = HasFlag(Args, TEXT("--dry-run"));
	Out.CreateIfMissing   = GetArgValue(Args, TEXT("--create-if-missing"), TEXT("Ask"));
	Out.CreateParentClass = GetArgValue(Args, TEXT("--create-parent"), TEXT("Actor"));
	Out.CreatePackagePath = GetArgValue(Args, TEXT("--create-path"), TEXT("/Game/Blueprints"));
	// 简写：--create 等价于 --create-if-missing Always
	if (HasFlag(Args, TEXT("--create")))
	{
		Out.CreateIfMissing = TEXT("Always");
	}
	return Out;
}

void FFreeUltraCodeBluePrintModeModule::StartupModule()
{
	UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("FreeUltraCode BluePrint Mode 模块启动"));
	RegisterConsoleCommands();

	// 注册「蓝图」设置页的详情定制（注入一键安装按钮）。
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		UFreeUltraCodeBluePrintModeSettings::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FFreeUltraCodeBluePrintModeSettingsCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	// 引擎初始化后触发插件自检：是否安装、是否有更新。全程自动化、非阻塞。
	FFreeUltraCodeBluePrintModePluginUpdateChecker::CheckAsync();
}

void FFreeUltraCodeBluePrintModeModule::ShutdownModule()
{
	UnregisterConsoleCommands();

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(UFreeUltraCodeBluePrintModeSettings::StaticClass()->GetFName());
	}

	UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("FreeUltraCode BluePrint Mode 模块关闭"));
}

void FFreeUltraCodeBluePrintModeModule::RegisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("FreeUltraCodeBluePrintMode.BlueprintMode.Start"),
		TEXT("进入蓝图编排模式。用法: FreeUltraCodeBluePrintMode.BlueprintMode.Start [--target <名字>] [--context overview|header|balanced|full|properties] [--agent lite|full] [--dry-run] [--create] [--create-parent <类>] [--create-path <路径>]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (!GEditor) { return; }
			UFreeUltraCodeBluePrintModeSubsystem* Sub = GEditor->GetEditorSubsystem<UFreeUltraCodeBluePrintModeSubsystem>();
			if (!Sub) { return; }

			const FFreeUltraCodeBluePrintModeStartArgs Parsed = FFreeUltraCodeBluePrintModeModule::ParseStartArgs(Args);
			const FFreeUltraCodeBluePrintModeResult Result = Sub->StartMode(Parsed);
			UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("[start] %s"), *Result.Message);
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("FreeUltraCodeBluePrintMode.BlueprintMode.End"),
		TEXT("结束蓝图编排模式。用法: FreeUltraCodeBluePrintMode.BlueprintMode.End [--commit|--discard] [--verify] [--compile]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (!GEditor) { return; }
			UFreeUltraCodeBluePrintModeSubsystem* Sub = GEditor->GetEditorSubsystem<UFreeUltraCodeBluePrintModeSubsystem>();
			if (!Sub) { return; }

			const bool bDiscard = HasFlag(Args, TEXT("--discard"));
			const bool bVerify  = HasFlag(Args, TEXT("--verify"));
			const bool bCompile = HasFlag(Args, TEXT("--compile"));
			const FFreeUltraCodeBluePrintModeResult Result = Sub->EndMode(!bDiscard, bVerify, bCompile);
			UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("[end] %s"), *Result.Message);
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("FreeUltraCodeBluePrintMode.BlueprintMode.Capabilities"),
		TEXT("查询当前 UE 环境可用的蓝图/UMG/AnimGraph/StateTree/Niagara 编辑能力。"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (!GEditor) { return; }
			UFreeUltraCodeBluePrintModeSubsystem* Sub = GEditor->GetEditorSubsystem<UFreeUltraCodeBluePrintModeSubsystem>();
			if (!Sub) { return; }

			const FFreeUltraCodeBluePrintModeResult Result = Sub->QueryCapabilities();
			UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("[capabilities] %s"), *Result.CapabilitiesJson);
		}),
		ECVF_Default));
}

void FFreeUltraCodeBluePrintModeModule::UnregisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();
	for (IConsoleObject* Cmd : ConsoleCommands)
	{
		if (Cmd)
		{
			CM.UnregisterConsoleObject(Cmd);
		}
	}
	ConsoleCommands.Empty();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FFreeUltraCodeBluePrintModeModule, FreeUltraCodeBluePrintMode)
