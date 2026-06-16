// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "FreeUltraCodeBluePrintModeTypes.h"
#include "FreeUltraCodeBluePrintModeSubsystem.generated.h"

class UBlueprint;
class UEdGraph;

/** 一次蓝图模式会话的运行时状态。 */
USTRUCT()
struct FFreeUltraCodeBluePrintModeSession
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UBlueprint> TargetBlueprint;

	UPROPERTY()
	TWeakObjectPtr<UEdGraph> TargetGraph;

	EFreeUltraCodeBluePrintModeContextLevel ContextLevel = EFreeUltraCodeBluePrintModeContextLevel::Balanced;
	EFreeUltraCodeBluePrintModeAgentTier AgentTier = EFreeUltraCodeBluePrintModeAgentTier::Full;
	bool bDryRun = false;

	/** 起始 checkpoint：discard 时回滚到这里。 */
	int32 StartTransactionId = INDEX_NONE;

	/** 缓存的上下文 JSON（按 ContextLevel 加载，编辑循环复用）。 */
	FString CachedContextJson;

	bool IsValid() const { return TargetBlueprint.IsValid(); }
};

/**
 * 蓝图模式核心编排子系统（编辑器内常驻）。
 * 管理 start/end 生命周期、上下文加载、操作计划的执行与回滚，
 * 并在本地无法完成时通过 MCP 桥接补全。
 */
UCLASS()
class FREEULTRACODEBLUEPRINTMODE_API UFreeUltraCodeBluePrintModeSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 进入蓝图模式。处理 targeting、不存在时的创建确认、上下文加载。 */
	UFUNCTION(BlueprintCallable, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FFreeUltraCodeBluePrintModeResult StartMode(const FFreeUltraCodeBluePrintModeStartArgs& Args);

	/** 结束蓝图模式。bCommit=false 表示 discard 回滚。 */
	UFUNCTION(BlueprintCallable, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FFreeUltraCodeBluePrintModeResult EndMode(bool bCommit, bool bVerify, bool bCompile);

	/** 前端确认"创建蓝图"后回调，携带 start 参数继续。 */
	UFUNCTION(BlueprintCallable, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FFreeUltraCodeBluePrintModeResult ConfirmCreateAndStart(const FFreeUltraCodeBluePrintModeStartArgs& Args);

	/** 模式激活期间，提交一条自然语言指令产出的操作计划。 */
	UFUNCTION(BlueprintCallable, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FFreeUltraCodeBluePrintModeResult ApplyPlan(const FFreeUltraCodeBluePrintModeOpPlan& Plan);

	UFUNCTION(BlueprintPure, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	EFreeUltraCodeBluePrintModeState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	bool IsActive() const { return State != EFreeUltraCodeBluePrintModeState::Idle; }

private:
	/** 按名字或当前聚焦的编辑器解析目标蓝图。失败返回 nullptr。 */
	UBlueprint* ResolveTargetBlueprint(const FString& TargetName) const;

	/** 创建一个新蓝图资产（CreateIfMissing 生效时）。 */
	UBlueprint* CreateBlueprintAsset(const FFreeUltraCodeBluePrintModeStartArgs& Args, FString& OutError) const;

	/** 真正进入 active 的内部流程（target 已确定）。 */
	FFreeUltraCodeBluePrintModeResult EnterActive(UBlueprint* Blueprint, const FFreeUltraCodeBluePrintModeStartArgs& Args);

	EFreeUltraCodeBluePrintModeState State = EFreeUltraCodeBluePrintModeState::Idle;

	UPROPERTY()
	FFreeUltraCodeBluePrintModeSession Session;
};
