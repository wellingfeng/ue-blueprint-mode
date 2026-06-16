// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FreeUltraCodeBluePrintModeTypes.h"

/**
 * MCP 桥接：本地 UE API 无法完成的操作（如放置 actor、改工程设置等执行性动作，
 * 或 Python Execution 范畴的动作），转发给 Ludus 风格的 MCP server 自动完成。
 *
 * 设计为非阻塞、可降级：MCP 不可用时返回 false，由上层决定如何处理。
 */
class FREEULTRACODEBLUEPRINTMODE_API FFreeUltraCodeBluePrintModeMcpBridge
{
public:
	/** MCP 是否已配置（从插件设置读取 endpoint）。 */
	static bool IsAvailable();

	/** MCP server endpoint。默认 https://mcp.ludusengine.com/mcp，可被设置覆盖。 */
	static FString GetEndpoint();

	/**
	 * 计划在本地应用到第 AppliedCount 个 op 后失败时，
	 * 把剩余 op 转交 MCP 远程完成。成功返回 true。
	 */
	static bool TryCompletePlanRemainder(const FFreeUltraCodeBluePrintModeOpPlan& Plan, int32 AppliedCount, const FString& LocalError);

	/** 发送一段自然语言意图给 MCP（同步等待，带超时）。 */
	static bool SendIntent(const FString& Intent, FString& OutResponse);
};
