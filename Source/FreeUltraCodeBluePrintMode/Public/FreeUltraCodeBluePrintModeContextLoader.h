// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FreeUltraCodeBluePrintModeTypes.h"

class UBlueprint;
class UEdGraph;

/**
 * 按上下文等级从蓝图提取上下文，序列化成 JSON 供 LLM 编排层使用。
 * 等级越高加载越深、token/成本越高（对齐 Ludus 五级分层）。
 */
class FREEULTRACODEBLUEPRINTMODE_API FFreeUltraCodeBluePrintModeContextLoader
{
public:
	/** 主入口：按等级加载，返回 JSON 字符串。 */
	static FString Load(UBlueprint* Blueprint, UEdGraph* FocusGraph, EFreeUltraCodeBluePrintModeContextLevel Level);

private:
	/** Overview：仅计数。 */
	static void AppendOverview(UBlueprint* Blueprint, const TSharedRef<class FJsonObject>& Out);
	/** Header：名字与类型。 */
	static void AppendHeader(UBlueprint* Blueprint, const TSharedRef<class FJsonObject>& Out);
	/** Balanced/Full：逻辑流与连接，bIncludeDefaults 控制是否含属性默认值。 */
	static void AppendGraphLogic(UEdGraph* Graph, const TSharedRef<class FJsonObject>& Out, bool bIncludeDefaults);
	/** Properties：仅变量/属性默认值。 */
	static void AppendProperties(UBlueprint* Blueprint, const TSharedRef<class FJsonObject>& Out);
};
