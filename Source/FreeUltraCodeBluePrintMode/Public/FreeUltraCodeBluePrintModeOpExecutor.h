// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FreeUltraCodeBluePrintModeTypes.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/**
 * 把结构化操作计划翻译成 UE 图表 API 调用。
 * 维护 逻辑NodeId -> UEdGraphNode* 映射，供 Connect/SetProperty 引用。
 * 每个 op 包在事务里，保证 Ctrl+Z 可逐步撤销。
 */
class FREEULTRACODEBLUEPRINTMODE_API FFreeUltraCodeBluePrintModeOpExecutor
{
public:
	FFreeUltraCodeBluePrintModeOpExecutor(UBlueprint* InBlueprint, UEdGraph* InGraph);

	/** 应用整份计划。返回成功应用的 op 数；失败时通过 OutError 回报。 */
	int32 ApplyPlan(const FFreeUltraCodeBluePrintModeOpPlan& Plan, FString& OutError);

	/** 工具：按常见名字解析 UClass（父类/变量类型用）。 */
	static UClass* ResolveClassByName(const FString& Name);

private:
	bool ApplyOp(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError);

	bool DoSpawnNode(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError);
	bool DoConnectPins(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError);
	bool DoSetProperty(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError);
	bool DoAddVariable(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError);
	bool DoDeleteNode(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError);
	bool DoDisconnect(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError);

	/** 解析 "NodeId.PinName" 端点。 */
	UEdGraphPin* ResolvePin(const FString& Endpoint, FString& OutError) const;

	UBlueprint* Blueprint = nullptr;
	UEdGraph* Graph = nullptr;

	/** 逻辑 Id -> 实际节点。 */
	TMap<FString, UEdGraphNode*> NodeMap;
};
