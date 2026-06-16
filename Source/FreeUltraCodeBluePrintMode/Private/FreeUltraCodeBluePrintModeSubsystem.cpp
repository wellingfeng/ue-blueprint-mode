// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#include "FreeUltraCodeBluePrintModeSubsystem.h"
#include "FreeUltraCodeBluePrintModeContextLoader.h"
#include "FreeUltraCodeBluePrintModeOpExecutor.h"
#include "FreeUltraCodeBluePrintModeLog.h"
#include "FreeUltraCodeBluePrintModeMcpBridge.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/BlueprintFactory.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "FreeUltraCodeBluePrintModeSubsystem"

void UFreeUltraCodeBluePrintModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	State = EFreeUltraCodeBluePrintModeState::Idle;
}

void UFreeUltraCodeBluePrintModeSubsystem::Deinitialize()
{
	if (State != EFreeUltraCodeBluePrintModeState::Idle)
	{
		// 安全收尾：未提交的会话默认丢弃，避免悬挂事务。
		EndMode(/*bCommit*/ false, /*bVerify*/ false, /*bCompile*/ false);
	}
	Super::Deinitialize();
}
UBlueprint* UFreeUltraCodeBluePrintModeSubsystem::ResolveTargetBlueprint(const FString& TargetName) const
{
	// 1) 缺省：取当前聚焦/打开的蓝图编辑器。
	if (TargetName.IsEmpty())
	{
		if (UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			TArray<UObject*> Opened = AES->GetAllEditedAssets();
			for (UObject* Obj : Opened)
			{
				if (UBlueprint* BP = Cast<UBlueprint>(Obj))
				{
					return BP; // 取第一个打开的蓝图作为聚焦目标
				}
			}
		}
		return nullptr;
	}

	// 2) 指定了名字：走 AssetRegistry 按名字查蓝图资产。
	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	ARM.Get().GetAssets(Filter, Assets);

	for (const FAssetData& Data : Assets)
	{
		if (Data.AssetName.ToString().Equals(TargetName, ESearchCase::IgnoreCase))
		{
			return Cast<UBlueprint>(Data.GetAsset());
		}
	}
	return nullptr;
}

UBlueprint* UFreeUltraCodeBluePrintModeSubsystem::CreateBlueprintAsset(const FFreeUltraCodeBluePrintModeStartArgs& Args, FString& OutError) const
{
	// 解析父类。先按常见名字映射，找不到再尝试全局查类。
	UClass* ParentClass = AActor::StaticClass();
	if (!Args.CreateParentClass.IsEmpty())
	{
		if (UClass* Found = FFreeUltraCodeBluePrintModeOpExecutor::ResolveClassByName(Args.CreateParentClass))
		{
			ParentClass = Found;
		}
		else
		{
			OutError = FString::Printf(TEXT("无法解析父类 '%s'"), *Args.CreateParentClass);
			return nullptr;
		}
	}

	const FString AssetName = Args.TargetName.IsEmpty() ? TEXT("NewBlueprint") : Args.TargetName;
	const FString PackagePath = Args.CreatePackagePath.IsEmpty() ? TEXT("/Game/Blueprints") : Args.CreatePackagePath;

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
	Factory->ParentClass = ParentClass;

	UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, UBlueprint::StaticClass(), Factory);
	UBlueprint* BP = Cast<UBlueprint>(NewAsset);
	if (!BP)
	{
		OutError = TEXT("创建蓝图资产失败");
		return nullptr;
	}
	UE_LOG(LogFreeUltraCodeBluePrintMode, Log, TEXT("已创建蓝图 %s/%s (父类 %s)"), *PackagePath, *AssetName, *ParentClass->GetName());
	return BP;
}
FFreeUltraCodeBluePrintModeResult UFreeUltraCodeBluePrintModeSubsystem::StartMode(const FFreeUltraCodeBluePrintModeStartArgs& Args)
{
	FFreeUltraCodeBluePrintModeResult Result;

	if (State != EFreeUltraCodeBluePrintModeState::Idle)
	{
		Result.bSuccess = false;
		Result.Message = TEXT("已有一个蓝图模式会话在进行中。请先 FreeUltraCodeBluePrintMode.BlueprintMode.End。");
		return Result;
	}

	// 解析目标蓝图。
	UBlueprint* BP = ResolveTargetBlueprint(Args.TargetName);

	if (!BP)
	{
		// 蓝图不存在 —— 按 CreateIfMissing 策略处理。
		if (Args.CreateIfMissing.Equals(TEXT("Never"), ESearchCase::IgnoreCase))
		{
			Result.bSuccess = false;
			Result.Message = Args.TargetName.IsEmpty()
				? TEXT("没有聚焦的蓝图，且未指定 --target。请先打开或点名一个蓝图。")
				: FString::Printf(TEXT("找不到蓝图 '%s'，且 create-if-missing=Never。"), *Args.TargetName);
			return Result;
		}

		if (Args.CreateIfMissing.Equals(TEXT("Ask"), ESearchCase::IgnoreCase))
		{
			// 默认：不擅自创建，回报需要用户确认，由前端弹可点击确认。
			Result.bSuccess = false;
			Result.bNeedsUserConfirm = true;
			Result.PendingAction = TEXT("CreateBlueprint");
			Result.Message = FString::Printf(
				TEXT("蓝图 '%s' 不存在。是否在 %s 下创建（父类 %s）？确认后将继续进入模式。"),
				*(Args.TargetName.IsEmpty() ? TEXT("NewBlueprint") : Args.TargetName),
				*Args.CreatePackagePath, *Args.CreateParentClass);
			return Result;
		}

		// Always：直接创建。
		FString CreateErr;
		BP = CreateBlueprintAsset(Args, CreateErr);
		if (!BP)
		{
			Result.bSuccess = false;
			Result.Message = CreateErr;
			return Result;
		}
	}

	return EnterActive(BP, Args);
}

FFreeUltraCodeBluePrintModeResult UFreeUltraCodeBluePrintModeSubsystem::ConfirmCreateAndStart(const FFreeUltraCodeBluePrintModeStartArgs& Args)
{
	// 前端确认创建后调用：强制创建再进入。
	FFreeUltraCodeBluePrintModeResult Result;
	if (State != EFreeUltraCodeBluePrintModeState::Idle)
	{
		Result.Message = TEXT("已有会话进行中。");
		return Result;
	}

	FString CreateErr;
	UBlueprint* BP = CreateBlueprintAsset(Args, CreateErr);
	if (!BP)
	{
		Result.bSuccess = false;
		Result.Message = CreateErr;
		return Result;
	}
	return EnterActive(BP, Args);
}

FFreeUltraCodeBluePrintModeResult UFreeUltraCodeBluePrintModeSubsystem::EnterActive(UBlueprint* Blueprint, const FFreeUltraCodeBluePrintModeStartArgs& Args)
{
	FFreeUltraCodeBluePrintModeResult Result;

	// 打开编辑器（targeting：保证用户能看到逻辑实时生长）。
	if (UAssetEditorSubsystem* AES = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		AES->OpenEditorForAsset(Blueprint);
	}

	// 选定目标图：优先事件图（Ubergraph），否则第一张函数图。
	UEdGraph* Graph = nullptr;
	if (Blueprint->UbergraphPages.Num() > 0)
	{
		Graph = Blueprint->UbergraphPages[0];
	}
	else if (Blueprint->FunctionGraphs.Num() > 0)
	{
		Graph = Blueprint->FunctionGraphs[0];
	}

	// 建立会话。
	Session = FFreeUltraCodeBluePrintModeSession();
	Session.TargetBlueprint = Blueprint;
	Session.TargetGraph = Graph;
	Session.ContextLevel = Args.ContextLevel;
	Session.AgentTier = Args.AgentTier;
	Session.bDryRun = Args.bDryRun;

	// 起始 checkpoint：开一个事务并标记蓝图，便于 discard 时回滚。
	Session.StartTransactionId = GEditor->BeginTransaction(LOCTEXT("BPModeStart", "FreeUltraCode BluePrint Mode Session"));
	Blueprint->Modify();

	// 按上下文等级加载并缓存。
	Session.CachedContextJson = FFreeUltraCodeBluePrintModeContextLoader::Load(Blueprint, Graph, Args.ContextLevel);

	State = EFreeUltraCodeBluePrintModeState::Active;

	Result.bSuccess = true;
	Result.Message = FString::Printf(
		TEXT("已进入蓝图模式：目标=%s，上下文=%d，agent=%s%s。"),
		*Blueprint->GetName(),
		(int32)Args.ContextLevel,
		Args.AgentTier == EFreeUltraCodeBluePrintModeAgentTier::Lite ? TEXT("lite") : TEXT("full"),
		Args.bDryRun ? TEXT("，dry-run") : TEXT(""));
	return Result;
}
FFreeUltraCodeBluePrintModeResult UFreeUltraCodeBluePrintModeSubsystem::ApplyPlan(const FFreeUltraCodeBluePrintModeOpPlan& Plan)
{
	FFreeUltraCodeBluePrintModeResult Result;
	if (State != EFreeUltraCodeBluePrintModeState::Active || !Session.IsValid())
	{
		Result.bSuccess = false;
		Result.Message = TEXT("未处于激活的蓝图模式，无法应用计划。");
		return Result;
	}

	UBlueprint* BP = Session.TargetBlueprint.Get();
	UEdGraph* Graph = Session.TargetGraph.Get();

	// dry-run：只回报计划摘要与预览，不触碰图。
	if (Session.bDryRun)
	{
		Result.bSuccess = true;
		Result.Message = FString::Printf(TEXT("[dry-run] 计划包含 %d 个操作：%s"),
			Plan.Ops.Num(), *Plan.Summary);
		return Result;
	}

	// 真正执行：交给执行层翻译成 UE API（每个 op 内部包事务）。
	FFreeUltraCodeBluePrintModeOpExecutor Executor(BP, Graph);
	FString ExecError;
	const int32 Applied = Executor.ApplyPlan(Plan, ExecError);

	if (!ExecError.IsEmpty() && Applied < Plan.Ops.Num())
	{
		// 本地无法完成的部分尝试通过 MCP 桥接补全（如放置 actor、改工程设置等执行性动作）。
		const bool bBridged = FFreeUltraCodeBluePrintModeMcpBridge::TryCompletePlanRemainder(Plan, Applied, ExecError);
		Result.bSuccess = bBridged;
		Result.Message = bBridged
			? FString::Printf(TEXT("本地应用 %d 个操作，其余通过 MCP 补全。"), Applied)
			: FString::Printf(TEXT("应用 %d/%d 个操作后失败：%s"), Applied, Plan.Ops.Num(), *ExecError);
		return Result;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("已应用 %d 个操作。"), Applied);
	return Result;
}

FFreeUltraCodeBluePrintModeResult UFreeUltraCodeBluePrintModeSubsystem::EndMode(bool bCommit, bool bVerify, bool bCompile)
{
	FFreeUltraCodeBluePrintModeResult Result;
	if (State == EFreeUltraCodeBluePrintModeState::Idle)
	{
		Result.bSuccess = false;
		Result.Message = TEXT("当前不在蓝图模式中。");
		return Result;
	}

	UBlueprint* BP = Session.TargetBlueprint.Get();

	if (!bCommit)
	{
		// discard：回滚到起始 checkpoint。
		GEditor->CancelTransaction(Session.StartTransactionId);
		Session = FFreeUltraCodeBluePrintModeSession();
		State = EFreeUltraCodeBluePrintModeState::Idle;
		Result.bSuccess = true;
		Result.Message = TEXT("已丢弃本次会话的全部改动并退出模式。");
		return Result;
	}

	// commit：结束事务，保留改动。
	GEditor->EndTransaction();

	if (BP)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);

		if (bVerify || bCompile)
		{
			State = EFreeUltraCodeBluePrintModeState::Verifying;
			FKismetEditorUtilities::CompileBlueprint(BP);

			const bool bHasErrors = (BP->Status == EBlueprintStatus::BS_Error);
			if (bHasErrors)
			{
				// 校验/编译失败：不退出，留在 Active 让用户继续修。
				State = EFreeUltraCodeBluePrintModeState::Active;
				Result.bSuccess = false;
				Result.Message = TEXT("编译存在错误，已保留在蓝图模式中，请继续修正。");
				// 重新开一个事务承接后续修正。
				Session.StartTransactionId = GEditor->BeginTransaction(LOCTEXT("BPModeFix", "FreeUltraCode BluePrint Mode Fix"));
				BP->Modify();
				return Result;
			}
		}
	}

	Session = FFreeUltraCodeBluePrintModeSession();
	State = EFreeUltraCodeBluePrintModeState::Idle;
	Result.bSuccess = true;
	Result.Message = TEXT("已提交改动并退出蓝图模式。");
	return Result;
}

#undef LOCTEXT_NAMESPACE

