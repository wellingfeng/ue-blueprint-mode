// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#include "FreeUltraCodeBluePrintModeContextLoader.h"
#include "FreeUltraCodeBluePrintModeCapabilityRegistry.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void FFreeUltraCodeBluePrintModeContextLoader::AppendOverview(UBlueprint* Blueprint, const TSharedRef<FJsonObject>& Out)
{
	int32 NodeCount = 0;
	for (UEdGraph* G : Blueprint->UbergraphPages) { NodeCount += G ? G->Nodes.Num() : 0; }
	for (UEdGraph* G : Blueprint->FunctionGraphs) { NodeCount += G ? G->Nodes.Num() : 0; }

	TArray<FBPVariableDescription> Vars = Blueprint->NewVariables;

	Out->SetNumberField(TEXT("ubergraphCount"), Blueprint->UbergraphPages.Num());
	Out->SetNumberField(TEXT("functionGraphCount"), Blueprint->FunctionGraphs.Num());
	Out->SetNumberField(TEXT("macroGraphCount"), Blueprint->MacroGraphs.Num());
	Out->SetNumberField(TEXT("nodeCount"), NodeCount);
	Out->SetNumberField(TEXT("variableCount"), Vars.Num());
}

void FFreeUltraCodeBluePrintModeContextLoader::AppendHeader(UBlueprint* Blueprint, const TSharedRef<FJsonObject>& Out)
{
	// 变量名 + 类型
	TArray<TSharedPtr<FJsonValue>> VarArr;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedRef<FJsonObject> V = MakeShared<FJsonObject>();
		V->SetStringField(TEXT("name"), Var.VarName.ToString());
		V->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
		VarArr.Add(MakeShared<FJsonValueObject>(V));
	}
	Out->SetArrayField(TEXT("variables"), VarArr);

	// 函数图签名（名字）
	TArray<TSharedPtr<FJsonValue>> FnArr;
	for (UEdGraph* G : Blueprint->FunctionGraphs)
	{
		if (G) { FnArr.Add(MakeShared<FJsonValueString>(G->GetName())); }
	}
	Out->SetArrayField(TEXT("functions"), FnArr);

	// 节点类名（去重计数）
	TMap<FString, int32> NodeKinds;
	auto Collect = [&NodeKinds](UEdGraph* G)
	{
		if (!G) { return; }
		for (UEdGraphNode* N : G->Nodes)
		{
			if (N) { NodeKinds.FindOrAdd(N->GetClass()->GetName())++; }
		}
	};
	for (UEdGraph* G : Blueprint->UbergraphPages) { Collect(G); }
	for (UEdGraph* G : Blueprint->FunctionGraphs) { Collect(G); }

	TSharedRef<FJsonObject> Kinds = MakeShared<FJsonObject>();
	for (const TPair<FString, int32>& Pair : NodeKinds)
	{
		Kinds->SetNumberField(Pair.Key, Pair.Value);
	}
	Out->SetObjectField(TEXT("nodeKinds"), Kinds);
}

void FFreeUltraCodeBluePrintModeContextLoader::AppendGraphLogic(UEdGraph* Graph, const TSharedRef<FJsonObject>& Out, bool bIncludeDefaults)
{
	if (!Graph) { return; }

	TArray<TSharedPtr<FJsonValue>> NodeArr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) { continue; }

		TSharedRef<FJsonObject> N = MakeShared<FJsonObject>();
		N->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
		N->SetStringField(TEXT("class"), Node->GetClass()->GetName());
		N->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());

		TArray<TSharedPtr<FJsonValue>> PinArr;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin) { continue; }
			TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetStringField(TEXT("name"), Pin->PinName.ToString());
			P->SetStringField(TEXT("dir"), Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"));
			P->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());

			// 连接关系
			TArray<TSharedPtr<FJsonValue>> Links;
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNode())
				{
					Links.Add(MakeShared<FJsonValueString>(
						FString::Printf(TEXT("%s.%s"),
							*Linked->GetOwningNode()->NodeGuid.ToString(),
							*Linked->PinName.ToString())));
				}
			}
			P->SetArrayField(TEXT("linkedTo"), Links);

			// Full：额外含默认值
			if (bIncludeDefaults && !Pin->DefaultValue.IsEmpty())
			{
				P->SetStringField(TEXT("default"), Pin->DefaultValue);
			}
			PinArr.Add(MakeShared<FJsonValueObject>(P));
		}
		N->SetArrayField(TEXT("pins"), PinArr);
		NodeArr.Add(MakeShared<FJsonValueObject>(N));
	}
	Out->SetArrayField(TEXT("nodes"), NodeArr);
}

void FFreeUltraCodeBluePrintModeContextLoader::AppendProperties(UBlueprint* Blueprint, const TSharedRef<FJsonObject>& Out)
{
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		TSharedRef<FJsonObject> V = MakeShared<FJsonObject>();
		V->SetStringField(TEXT("name"), Var.VarName.ToString());
		V->SetStringField(TEXT("type"), Var.VarType.PinCategory.ToString());
		V->SetStringField(TEXT("default"), Var.DefaultValue);
		Arr.Add(MakeShared<FJsonValueObject>(V));
	}
	Out->SetArrayField(TEXT("properties"), Arr);
}

FString FFreeUltraCodeBluePrintModeContextLoader::Load(UBlueprint* Blueprint, UEdGraph* FocusGraph, EFreeUltraCodeBluePrintModeContextLevel Level)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	if (!Blueprint)
	{
		return TEXT("{}");
	}

	Root->SetStringField(TEXT("blueprint"), Blueprint->GetName());
	Root->SetStringField(TEXT("level"), FString::FromInt((int32)Level));
	FFreeUltraCodeBluePrintModeCapabilityRegistry::AppendCapabilitiesToJson(Root);

	switch (Level)
	{
	case EFreeUltraCodeBluePrintModeContextLevel::Overview:
		AppendOverview(Blueprint, Root);
		break;
	case EFreeUltraCodeBluePrintModeContextLevel::Header:
		AppendOverview(Blueprint, Root);
		AppendHeader(Blueprint, Root);
		break;
	case EFreeUltraCodeBluePrintModeContextLevel::Balanced:
		AppendHeader(Blueprint, Root);
		AppendGraphLogic(FocusGraph, Root, /*bIncludeDefaults*/ false);
		break;
	case EFreeUltraCodeBluePrintModeContextLevel::Full:
		AppendHeader(Blueprint, Root);
		AppendGraphLogic(FocusGraph, Root, /*bIncludeDefaults*/ true);
		AppendProperties(Blueprint, Root);
		break;
	case EFreeUltraCodeBluePrintModeContextLevel::Properties:
		AppendProperties(Blueprint, Root);
		break;
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}
