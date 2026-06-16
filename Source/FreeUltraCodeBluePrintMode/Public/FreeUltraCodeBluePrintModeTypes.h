// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FreeUltraCodeBluePrintModeTypes.generated.h"

/** 上下文加载等级：成本 vs 深度的主旋钮（对齐 Ludus 五级分层）。 */
UENUM(BlueprintType)
enum class EFreeUltraCodeBluePrintModeContextLevel : uint8
{
	/** 仅图/节点/函数/变量数量。最便宜，看规模复杂度。 */
	Overview	UMETA(DisplayName = "Overview"),
	/** 图、节点、变量的名字与类型。理解高层意图。 */
	Header		UMETA(DisplayName = "Header"),
	/** 逻辑流、节点连接、函数、变量（不含属性默认值）。推荐默认。 */
	Balanced	UMETA(DisplayName = "Balanced"),
	/** 全部：逻辑、连接、变量、所有属性值。深度调试用，最贵。 */
	Full		UMETA(DisplayName = "Full"),
	/** 仅蓝图属性与默认值，不加载逻辑图。检查配置用。 */
	Properties	UMETA(DisplayName = "Properties"),
};

/** Agent 推理强度。 */
UENUM(BlueprintType)
enum class EFreeUltraCodeBluePrintModeAgentTier : uint8
{
	/** 快、便宜，适合简单任务。 */
	Lite	UMETA(DisplayName = "Lite"),
	/** 复杂多步任务，深度推理。 */
	Full	UMETA(DisplayName = "Full"),
};

/** 模式生命周期状态。 */
UENUM(BlueprintType)
enum class EFreeUltraCodeBluePrintModeState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Active		UMETA(DisplayName = "Active"),
	Verifying	UMETA(DisplayName = "Verifying"),
};

/** 单个蓝图操作的类型。LLM 编排层产出这些原语，执行层翻成 UE API。 */
UENUM(BlueprintType)
enum class EFreeUltraCodeBluePrintModeOpType : uint8
{
	SpawnNode	UMETA(DisplayName = "SpawnNode"),
	ConnectPins	UMETA(DisplayName = "ConnectPins"),
	SetProperty	UMETA(DisplayName = "SetProperty"),
	AddVariable	UMETA(DisplayName = "AddVariable"),
	AddFunction	UMETA(DisplayName = "AddFunction"),
	DeleteNode	UMETA(DisplayName = "DeleteNode"),
	Disconnect	UMETA(DisplayName = "Disconnect"),
	AutoLayout	UMETA(DisplayName = "AutoLayout"),
};

/** 可被 UI/编排层自动启用或隐藏的编辑能力。 */
UENUM(BlueprintType)
enum class EFreeUltraCodeBluePrintModeCapabilityId : uint8
{
	BlueprintBasicGraph		UMETA(DisplayName = "Blueprint.BasicGraph"),
	BlueprintNodeDiscovery	UMETA(DisplayName = "Blueprint.NodeDiscovery"),
	BlueprintKeySpawn		UMETA(DisplayName = "Blueprint.KeySpawn"),
	BlueprintAutoLayout		UMETA(DisplayName = "Blueprint.AutoLayout"),
	BlueprintDispatcher		UMETA(DisplayName = "Blueprint.Dispatcher"),
	BlueprintTimeline		UMETA(DisplayName = "Blueprint.Timeline"),
	UMGWidgetTree			UMETA(DisplayName = "UMG.WidgetTree"),
	AnimGraphEdit			UMETA(DisplayName = "AnimGraph.Edit"),
	StateTreeEdit			UMETA(DisplayName = "StateTree.Edit"),
	NiagaraStackEdit		UMETA(DisplayName = "Niagara.StackEdit"),
};

/** 单项能力的运行时探测结果。 */
USTRUCT(BlueprintType)
struct FFreeUltraCodeBluePrintModeCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|Capabilities")
	EFreeUltraCodeBluePrintModeCapabilityId CapabilityId = EFreeUltraCodeBluePrintModeCapabilityId::BlueprintBasicGraph;

	/** 稳定机器名，如 "Blueprint.AutoLayout"。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|Capabilities")
	FString Name;

	/** 当前 UE 版本、插件、模块与 API 都满足时为 true。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|Capabilities")
	bool bSupported = false;

	/** 当前插件已有执行器时为 true；false 表示需要可选后端或后续实现。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|Capabilities")
	bool bImplemented = false;

	/** 可直接向用户暴露并执行；通常等于 bSupported && bImplemented。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|Capabilities")
	bool bEnabled = false;

	/** ok / partial / engine_too_old / plugin_missing / api_missing / not_implemented。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|Capabilities")
	FString Reason;

	/** 该能力的最低 UE 版本策略说明。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|Capabilities")
	FString MinVersion;

	/** python / editor_plugin / optional_editor_plugin / unavailable。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|Capabilities")
	FString Backend;

	/** 高风险或小版本差异较大的能力标记为实验性。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|Capabilities")
	bool bExperimental = false;
};

/**
 * 结构化蓝图操作（中间表示）。
 * 字段语义随 OpType 变化，未用到的字段留空即可。
 */
USTRUCT(BlueprintType)
struct FFreeUltraCodeBluePrintModeOp
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	EFreeUltraCodeBluePrintModeOpType OpType = EFreeUltraCodeBluePrintModeOpType::SpawnNode;

	/** LLM 给的逻辑节点 Id（执行层维护 逻辑Id -> UEdGraphNode* 映射）。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString NodeId;

	/** SpawnNode：节点类型标识，如 "K2Node_CallFunction:KismetMathLibrary.Add_IntInt"。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString NodeType;

	/** ConnectPins/Disconnect：源 端点，格式 "NodeId.PinName"。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString FromPin;

	/** ConnectPins/Disconnect：目标端点，格式 "NodeId.PinName"。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString ToPin;

	/** SetProperty/AddVariable：键名（属性名 / 变量名）。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString Key;

	/** SetProperty/AddVariable：值（字符串化，执行层按 pin 类型解析）。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString Value;

	/** AddVariable/AddFunction：类型标识，如 "int"、"bool"、"Actor"。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString TypeId;

	/** SpawnNode：图上坐标。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FVector2D GraphPosition = FVector2D::ZeroVector;
};

/** 一份操作计划（一条自然语言指令产出一份）。 */
USTRUCT(BlueprintType)
struct FFreeUltraCodeBluePrintModeOpPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	TArray<FFreeUltraCodeBluePrintModeOp> Ops;

	/** 人类可读的计划摘要，用于 dry-run 预览。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString Summary;
};

/** start 命令参数。 */
USTRUCT(BlueprintType)
struct FFreeUltraCodeBluePrintModeStartArgs
{
	GENERATED_BODY()

	/** 目标蓝图名；空则取当前聚焦的蓝图编辑器。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString TargetName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	EFreeUltraCodeBluePrintModeContextLevel ContextLevel = EFreeUltraCodeBluePrintModeContextLevel::Balanced;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	EFreeUltraCodeBluePrintModeAgentTier AgentTier = EFreeUltraCodeBluePrintModeAgentTier::Full;

	/** 只产计划与预览，不落盘。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	bool bDryRun = false;

	/**
	 * 蓝图不存在时是否允许创建。
	 * Ask：弹确认让用户选；Always：自动建；Never：报错不建。
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString CreateIfMissing = TEXT("Ask");

	/** 创建时的父类（CreateIfMissing 生效时），如 "Actor"、"Pawn"、"UserWidget"。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString CreateParentClass = TEXT("Actor");

	/** 创建路径，缺省 /Game/Blueprints。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString CreatePackagePath = TEXT("/Game/Blueprints");
};

/** 操作结果。 */
USTRUCT(BlueprintType)
struct FFreeUltraCodeBluePrintModeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString Message;

	/** 是否需要前端向用户弹确认（如"蓝图不存在，是否创建？"）。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	bool bNeedsUserConfirm = false;

	/** 待确认动作的标识，前端确认后回传。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString PendingAction;

	/** 当前环境的能力探测 JSON；start 成功或查询能力时返回。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "FreeUltraCodeBluePrintMode|BlueprintMode")
	FString CapabilitiesJson;
};
