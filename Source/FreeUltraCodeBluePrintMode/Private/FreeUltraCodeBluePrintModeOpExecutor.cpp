// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#include "FreeUltraCodeBluePrintModeOpExecutor.h"
#include "FreeUltraCodeBluePrintModeLog.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "FreeUltraCodeBluePrintModeOpExecutor"

FFreeUltraCodeBluePrintModeOpExecutor::FFreeUltraCodeBluePrintModeOpExecutor(UBlueprint* InBlueprint, UEdGraph* InGraph)
	: Blueprint(InBlueprint)
	, Graph(InGraph)
{
}

UClass* FFreeUltraCodeBluePrintModeOpExecutor::ResolveClassByName(const FString& Name)
{
	if (Name.IsEmpty()) { return nullptr; }

	// 1) 直接在已加载类里按名字/路径找，避免依赖 UE5 已移除的全局包宏。
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Candidate = *It;
		if (Candidate
			&& (Candidate->GetName().Equals(Name, ESearchCase::IgnoreCase)
				|| Candidate->GetPathName().Equals(Name, ESearchCase::IgnoreCase)))
		{
			return Candidate;
		}
	}
	// 2) 常见简称映射。
	static const TMap<FString, FString> Aliases = {
		{ TEXT("Actor"),      TEXT("/Script/Engine.Actor") },
		{ TEXT("Pawn"),       TEXT("/Script/Engine.Pawn") },
		{ TEXT("Character"),  TEXT("/Script/Engine.Character") },
		{ TEXT("GameMode"),   TEXT("/Script/Engine.GameModeBase") },
		{ TEXT("UserWidget"), TEXT("/Script/UMG.UserWidget") },
		{ TEXT("Object"),     TEXT("/Script/CoreUObject.Object") },
	};
	if (const FString* Path = Aliases.Find(Name))
	{
		return LoadClass<UObject>(nullptr, **Path);
	}
	// 3) 当作完整路径加载。
	return LoadClass<UObject>(nullptr, *Name);
}

int32 FFreeUltraCodeBluePrintModeOpExecutor::ApplyPlan(const FFreeUltraCodeBluePrintModeOpPlan& Plan, FString& OutError)
{
	int32 Applied = 0;
	for (const FFreeUltraCodeBluePrintModeOp& Op : Plan.Ops)
	{
		if (!ApplyOp(Op, OutError))
		{
			UE_LOG(LogFreeUltraCodeBluePrintMode, Warning, TEXT("op 应用失败: %s"), *OutError);
			break;
		}
		++Applied;
	}
	return Applied;
}

bool FFreeUltraCodeBluePrintModeOpExecutor::ApplyOp(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError)
{
	switch (Op.OpType)
	{
	case EFreeUltraCodeBluePrintModeOpType::SpawnNode:   return DoSpawnNode(Op, OutError);
	case EFreeUltraCodeBluePrintModeOpType::ConnectPins: return DoConnectPins(Op, OutError);
	case EFreeUltraCodeBluePrintModeOpType::SetProperty: return DoSetProperty(Op, OutError);
	case EFreeUltraCodeBluePrintModeOpType::AddVariable: return DoAddVariable(Op, OutError);
	case EFreeUltraCodeBluePrintModeOpType::AutoLayout:  return DoAutoLayout(Op, OutError);
	case EFreeUltraCodeBluePrintModeOpType::DeleteNode:  return DoDeleteNode(Op, OutError);
	case EFreeUltraCodeBluePrintModeOpType::Disconnect:  return DoDisconnect(Op, OutError);
	default:
		OutError = TEXT("未知操作类型");
		return false;
	}
}
UEdGraphPin* FFreeUltraCodeBluePrintModeOpExecutor::ResolvePin(const FString& Endpoint, FString& OutError) const
{
	FString NodeId, PinName;
	if (!Endpoint.Split(TEXT("."), &NodeId, &PinName))
	{
		OutError = FString::Printf(TEXT("端点格式应为 NodeId.PinName，收到 '%s'"), *Endpoint);
		return nullptr;
	}

	UEdGraphNode* const* Found = NodeMap.Find(NodeId);
	if (!Found || !*Found)
	{
		OutError = FString::Printf(TEXT("找不到节点 '%s'"), *NodeId);
		return nullptr;
	}

	UEdGraphNode* Node = *Found;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			return Pin;
		}
	}
	OutError = FString::Printf(TEXT("节点 '%s' 上找不到引脚 '%s'"), *NodeId, *PinName);
	return nullptr;
}

bool FFreeUltraCodeBluePrintModeOpExecutor::DoSpawnNode(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("没有可用的目标图");
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("SpawnNode", "Spawn Blueprint Node"));
	Graph->Modify();

	// 这里以 K2Node_CallFunction 为示例骨架。完整实现按 NodeType 前缀分派到不同 UK2Node_* 子类。
	FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
	UK2Node_CallFunction* NewNode = Creator.CreateNode();
	NewNode->NodePosX = (int32)Op.GraphPosition.X;
	NewNode->NodePosY = (int32)Op.GraphPosition.Y;

	// NodeType 形如 "K2Node_CallFunction:Class.Function"，解析出函数引用。
	FString Kind, FuncRef;
	if (Op.NodeType.Split(TEXT(":"), &Kind, &FuncRef))
	{
		FString ClassName, FuncName;
		if (FuncRef.Split(TEXT("."), &ClassName, &FuncName))
		{
			if (UClass* OwnerClass = ResolveClassByName(ClassName))
			{
				if (UFunction* Func = OwnerClass->FindFunctionByName(*FuncName))
				{
					NewNode->SetFromFunction(Func);
				}
			}
		}
	}
	Creator.Finalize();

	// 记录逻辑 Id 映射；同时把节点的真实 Guid 同步到 NodeMap，供后续引用。
	if (!Op.NodeId.IsEmpty())
	{
		NodeMap.Add(Op.NodeId, NewNode);
	}
	NodeMap.Add(NewNode->NodeGuid.ToString(), NewNode);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

bool FFreeUltraCodeBluePrintModeOpExecutor::DoConnectPins(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError)
{
	UEdGraphPin* From = ResolvePin(Op.FromPin, OutError);
	if (!From) { return false; }
	UEdGraphPin* To = ResolvePin(Op.ToPin, OutError);
	if (!To) { return false; }

	const FScopedTransaction Transaction(LOCTEXT("ConnectPins", "Connect Blueprint Pins"));
	From->GetOwningNode()->Modify();
	To->GetOwningNode()->Modify();

	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
	if (!Schema)
	{
		OutError = TEXT("图的 schema 非 K2");
		return false;
	}

	// schema 自动做 pin 类型校验。
	if (!Schema->TryCreateConnection(From, To))
	{
		OutError = FString::Printf(TEXT("无法连接 %s -> %s（pin 类型不兼容）"), *Op.FromPin, *Op.ToPin);
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

bool FFreeUltraCodeBluePrintModeOpExecutor::DoSetProperty(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError)
{
	// 设置某节点某引脚的默认值。Endpoint 用 NodeId，Key 是 PinName，Value 是字符串值。
	UEdGraphNode* const* Found = NodeMap.Find(Op.NodeId);
	if (!Found || !*Found)
	{
		OutError = FString::Printf(TEXT("SetProperty: 找不到节点 '%s'"), *Op.NodeId);
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetProperty", "Set Blueprint Node Property"));
	UEdGraphNode* Node = *Found;
	Node->Modify();

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(Op.Key, ESearchCase::IgnoreCase))
		{
			const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
			if (Schema)
			{
				Schema->TrySetDefaultValue(*Pin, Op.Value);
			}
			else
			{
				Pin->DefaultValue = Op.Value;
			}
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			return true;
		}
	}
	OutError = FString::Printf(TEXT("SetProperty: 节点上找不到引脚 '%s'"), *Op.Key);
	return false;
}

bool FFreeUltraCodeBluePrintModeOpExecutor::DoAddVariable(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError)
{
	FEdGraphPinType PinType;
	// 简单类型映射；复杂类型（对象/结构）按 TypeId 扩展。
	const FString& T = Op.TypeId;
	if (T == TEXT("bool"))        { PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean; }
	else if (T == TEXT("int"))    { PinType.PinCategory = UEdGraphSchema_K2::PC_Int; }
	else if (T == TEXT("float") || T == TEXT("double"))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
	}
	else if (T == TEXT("string")) { PinType.PinCategory = UEdGraphSchema_K2::PC_String; }
	else if (T == TEXT("name"))   { PinType.PinCategory = UEdGraphSchema_K2::PC_Name; }
	else
	{
		// 当作对象类型
		if (UClass* Cls = ResolveClassByName(T))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			PinType.PinSubCategoryObject = Cls;
		}
		else
		{
			OutError = FString::Printf(TEXT("AddVariable: 无法解析类型 '%s'"), *T);
			return false;
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("AddVariable", "Add Blueprint Variable"));
	const bool bOk = FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Op.Key), PinType, Op.Value);
	if (!bOk)
	{
		OutError = FString::Printf(TEXT("AddVariable: 添加变量 '%s' 失败"), *Op.Key);
		return false;
	}
	return true;
}

bool FFreeUltraCodeBluePrintModeOpExecutor::DoAutoLayout(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("AutoLayout: 没有可用的目标图");
		return false;
	}

	TArray<UEdGraphNode*> Nodes;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node)
		{
			Nodes.Add(Node);
		}
	}

	if (Nodes.Num() == 0)
	{
		return true;
	}

	const bool bUseDefaultOrigin = FMath::IsNearlyZero(Op.GraphPosition.X) && FMath::IsNearlyZero(Op.GraphPosition.Y);
	const int32 StartX = bUseDefaultOrigin ? 0 : (int32)Op.GraphPosition.X;
	const int32 StartY = bUseDefaultOrigin ? 0 : (int32)Op.GraphPosition.Y;
	const int32 ColumnWidth = 420;
	const int32 RowHeight = 180;

	TMap<UEdGraphNode*, int32> Depths;
	TMap<UEdGraphNode*, int32> IncomingCounts;

	for (UEdGraphNode* Node : Nodes)
	{
		Depths.Add(Node, 0);
		IncomingCounts.Add(Node, 0);
	}

	for (UEdGraphNode* Node : Nodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (LinkedNode && LinkedNode != Node && IncomingCounts.Contains(LinkedNode))
				{
					IncomingCounts.FindOrAdd(LinkedNode)++;
				}
			}
		}
	}

	TArray<UEdGraphNode*> Queue;
	for (UEdGraphNode* Node : Nodes)
	{
		if (IncomingCounts.FindRef(Node) == 0)
		{
			Queue.Add(Node);
		}
	}
	if (Queue.Num() == 0)
	{
		Queue = Nodes;
	}

	TSet<UEdGraphNode*> Queued;
	for (UEdGraphNode* Node : Queue)
	{
		Queued.Add(Node);
	}
	TSet<UEdGraphNode*> Processed;
	for (int32 Index = 0; Index < Queue.Num(); ++Index)
	{
		UEdGraphNode* Node = Queue[Index];
		if (!Node || Processed.Contains(Node))
		{
			continue;
		}

		Processed.Add(Node);
		const int32 CurrentDepth = Depths.FindRef(Node);
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				UEdGraphNode* LinkedNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
				if (!LinkedNode || LinkedNode == Node || !Depths.Contains(LinkedNode))
				{
					continue;
				}

				const int32 DesiredDepth = CurrentDepth + 1;
				if (Depths.FindRef(LinkedNode) < DesiredDepth)
				{
					Depths.Add(LinkedNode, DesiredDepth);
				}
				if (!Queued.Contains(LinkedNode))
				{
					Queue.Add(LinkedNode);
					Queued.Add(LinkedNode);
				}
			}
		}
	}

	TMap<int32, int32> RowsByDepth;
	const FScopedTransaction Transaction(LOCTEXT("AutoLayout", "Auto Layout Blueprint Graph"));
	Graph->Modify();

	Nodes.Sort([](const UEdGraphNode& A, const UEdGraphNode& B)
	{
		if (A.NodePosX != B.NodePosX)
		{
			return A.NodePosX < B.NodePosX;
		}
		return A.NodePosY < B.NodePosY;
	});

	for (UEdGraphNode* Node : Nodes)
	{
		const int32 Depth = Depths.FindRef(Node);
		int32& Row = RowsByDepth.FindOrAdd(Depth);

		Node->Modify();
		Node->NodePosX = StartX + Depth * ColumnWidth;
		Node->NodePosY = StartY + Row * RowHeight;
		++Row;
	}

	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}

bool FFreeUltraCodeBluePrintModeOpExecutor::DoDeleteNode(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError)
{
	UEdGraphNode* const* Found = NodeMap.Find(Op.NodeId);
	if (!Found || !*Found)
	{
		OutError = FString::Printf(TEXT("DeleteNode: 找不到节点 '%s'"), *Op.NodeId);
		return false;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteNode", "Delete Blueprint Node"));
	UEdGraphNode* Node = *Found;
	FBlueprintEditorUtils::RemoveNode(Blueprint, Node, /*bDontRecompile*/ true);
	NodeMap.Remove(Op.NodeId);
	return true;
}

bool FFreeUltraCodeBluePrintModeOpExecutor::DoDisconnect(const FFreeUltraCodeBluePrintModeOp& Op, FString& OutError)
{
	UEdGraphPin* From = ResolvePin(Op.FromPin, OutError);
	if (!From) { return false; }
	UEdGraphPin* To = ResolvePin(Op.ToPin, OutError);
	if (!To) { return false; }

	const FScopedTransaction Transaction(LOCTEXT("Disconnect", "Disconnect Blueprint Pins"));
	From->GetOwningNode()->Modify();
	To->GetOwningNode()->Modify();
	From->BreakLinkTo(To);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	return true;
}
#undef LOCTEXT_NAMESPACE
