// Copyright FreeUltraCode BluePrint Mode. All Rights Reserved.

#include "FreeUltraCodeBluePrintModeCapabilityRegistry.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonValue.h"
#include "UObject/UObjectIterator.h"

namespace
{
	TSharedRef<FJsonObject> CapabilityToJson(const FFreeUltraCodeBluePrintModeCapability& Capability)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), FFreeUltraCodeBluePrintModeCapabilityRegistry::CapabilityIdToName(Capability.CapabilityId));
		Obj->SetStringField(TEXT("name"), Capability.Name);
		Obj->SetBoolField(TEXT("supported"), Capability.bSupported);
		Obj->SetBoolField(TEXT("implemented"), Capability.bImplemented);
		Obj->SetBoolField(TEXT("enabled"), Capability.bEnabled);
		Obj->SetStringField(TEXT("reason"), Capability.Reason);
		Obj->SetStringField(TEXT("minVersion"), Capability.MinVersion);
		Obj->SetStringField(TEXT("backend"), Capability.Backend);
		Obj->SetBoolField(TEXT("experimental"), Capability.bExperimental);
		return Obj;
	}

	FString ChooseReason(bool bEngineOk, bool bPluginOk, bool bModuleOk, bool bClassOk, bool bImplemented)
	{
		if (!bEngineOk) { return TEXT("engine_too_old"); }
		if (!bPluginOk) { return TEXT("plugin_missing"); }
		if (!bModuleOk) { return TEXT("api_missing"); }
		if (!bClassOk) { return TEXT("api_missing"); }
		if (!bImplemented) { return TEXT("not_implemented"); }
		return TEXT("ok");
	}
}

TArray<FFreeUltraCodeBluePrintModeCapability> FFreeUltraCodeBluePrintModeCapabilityRegistry::DetectAll()
{
	TArray<FFreeUltraCodeBluePrintModeCapability> Capabilities;

	const bool bBlueprintGraph = ModuleExists(TEXT("BlueprintGraph"));
	const bool bKismet = ModuleExists(TEXT("Kismet"));
	const bool bK2Schema = ClassExists(TEXT("/Script/BlueprintGraph.EdGraphSchema_K2"), TEXT("EdGraphSchema_K2"));
	const bool bBlueprintBasic = bBlueprintGraph && bKismet && bK2Schema;

	Capabilities.Add(MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId::BlueprintBasicGraph,
		bBlueprintBasic,
		true,
		bBlueprintBasic ? TEXT("ok") : TEXT("api_missing"),
		TEXT("4.25"),
		bBlueprintBasic ? TEXT("editor_plugin") : TEXT("unavailable")));

	const bool bNodeSpawner = ClassExists(TEXT("/Script/BlueprintGraph.BlueprintNodeSpawner"), TEXT("BlueprintNodeSpawner"));
	const bool bNodeDiscovery = bBlueprintGraph && bNodeSpawner;
	Capabilities.Add(MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId::BlueprintNodeDiscovery,
		bNodeDiscovery,
		false,
		bNodeDiscovery ? TEXT("not_implemented") : TEXT("api_missing"),
		TEXT("4.25"),
		bNodeDiscovery ? TEXT("editor_plugin") : TEXT("unavailable"),
		true));

	Capabilities.Add(MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId::BlueprintKeySpawn,
		bBlueprintBasic,
		true,
		bBlueprintBasic ? TEXT("partial_k2_call_function") : TEXT("api_missing"),
		TEXT("4.25"),
		bBlueprintBasic ? TEXT("editor_plugin") : TEXT("unavailable"),
		true));

	Capabilities.Add(MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId::BlueprintAutoLayout,
		bBlueprintBasic,
		true,
		bBlueprintBasic ? TEXT("ok") : TEXT("api_missing"),
		TEXT("4.25"),
		bBlueprintBasic ? TEXT("editor_plugin") : TEXT("unavailable")));

	const bool bDispatcherApi = bBlueprintGraph && ClassExists(TEXT("/Script/BlueprintGraph.K2Node_Event"), TEXT("K2Node_Event"));
	Capabilities.Add(MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId::BlueprintDispatcher,
		bDispatcherApi,
		false,
		bDispatcherApi ? TEXT("not_implemented") : TEXT("api_missing"),
		TEXT("4.25"),
		bDispatcherApi ? TEXT("editor_plugin") : TEXT("unavailable"),
		true));

	const bool bTimelineApi = bBlueprintGraph && ClassExists(TEXT("/Script/BlueprintGraph.K2Node_Timeline"), TEXT("K2Node_Timeline"));
	Capabilities.Add(MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId::BlueprintTimeline,
		bTimelineApi,
		false,
		bTimelineApi ? TEXT("not_implemented") : TEXT("api_missing"),
		TEXT("4.25"),
		bTimelineApi ? TEXT("editor_plugin") : TEXT("unavailable"),
		true));

	const bool bUMGPlugin = PluginEnabled(TEXT("UMG")) || ModuleExists(TEXT("UMGEditor"));
	const bool bUMGWidgetBlueprint = ClassExists(TEXT("/Script/UMGEditor.WidgetBlueprint"), TEXT("WidgetBlueprint"));
	const bool bUMGAvailable = bUMGPlugin && bUMGWidgetBlueprint;
	Capabilities.Add(MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId::UMGWidgetTree,
		bUMGAvailable,
		false,
		ChooseReason(true, bUMGPlugin, ModuleExists(TEXT("UMGEditor")), bUMGWidgetBlueprint, false),
		TEXT("4.25"),
		bUMGAvailable ? TEXT("optional_editor_plugin") : TEXT("unavailable"),
		true));

	const bool bAnimGraphModule = ModuleExists(TEXT("AnimGraph")) || ModuleExists(TEXT("AnimGraphRuntime")) || ModuleExists(TEXT("AnimGraphEditor"));
	const bool bAnimGraphNode = ClassExists(TEXT("/Script/AnimGraph.AnimGraphNode_Base"), TEXT("AnimGraphNode_Base"));
	const bool bAnimGraphAvailable = bAnimGraphModule && bAnimGraphNode;
	Capabilities.Add(MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId::AnimGraphEdit,
		bAnimGraphAvailable,
		false,
		ChooseReason(true, true, bAnimGraphModule, bAnimGraphNode, false),
		TEXT("4.25"),
		bAnimGraphAvailable ? TEXT("optional_editor_plugin") : TEXT("unavailable"),
		true));

	const bool bStateTreeVersion = IsEngineAtLeast(5, 0);
	const bool bStateTreePlugin = PluginEnabled(TEXT("StateTree"));
	const bool bStateTreeModule = ModuleExists(TEXT("StateTreeModule")) || ModuleExists(TEXT("StateTreeEditorModule"));
	const bool bStateTreeClass = ClassExists(TEXT("/Script/StateTreeModule.StateTree"), TEXT("StateTree"));
	const bool bStateTreeAvailable = bStateTreeVersion && bStateTreePlugin && bStateTreeModule && bStateTreeClass;
	Capabilities.Add(MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId::StateTreeEdit,
		bStateTreeAvailable,
		false,
		ChooseReason(bStateTreeVersion, bStateTreePlugin, bStateTreeModule, bStateTreeClass, false),
		TEXT("5.0"),
		bStateTreeAvailable ? TEXT("optional_editor_plugin") : TEXT("unavailable"),
		true));

	const bool bNiagaraPlugin = PluginEnabled(TEXT("Niagara"));
	const bool bNiagaraModule = ModuleExists(TEXT("Niagara")) || ModuleExists(TEXT("NiagaraEditor"));
	const bool bNiagaraClass = ClassExists(TEXT("/Script/Niagara.NiagaraSystem"), TEXT("NiagaraSystem"));
	const bool bNiagaraAvailable = bNiagaraPlugin && bNiagaraModule && bNiagaraClass;
	Capabilities.Add(MakeCapability(
		EFreeUltraCodeBluePrintModeCapabilityId::NiagaraStackEdit,
		bNiagaraAvailable,
		false,
		ChooseReason(true, bNiagaraPlugin, bNiagaraModule, bNiagaraClass, false),
		TEXT("4.25"),
		bNiagaraAvailable ? TEXT("optional_editor_plugin") : TEXT("unavailable"),
		true));

	return Capabilities;
}

FString FFreeUltraCodeBluePrintModeCapabilityRegistry::DetectAllAsJson()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("FreeUltraCodeBluePrintMode.Capabilities.v1"));
	Root->SetStringField(TEXT("engineVersion"), EngineVersionString());
	AppendCapabilitiesToJson(Root);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

void FFreeUltraCodeBluePrintModeCapabilityRegistry::AppendCapabilitiesToJson(const TSharedRef<FJsonObject>& Out)
{
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FFreeUltraCodeBluePrintModeCapability& Capability : DetectAll())
	{
		Items.Add(MakeShared<FJsonValueObject>(CapabilityToJson(Capability)));
	}
	Out->SetStringField(TEXT("capabilitySchema"), TEXT("FreeUltraCodeBluePrintMode.Capabilities.v1"));
	Out->SetStringField(TEXT("engineVersion"), EngineVersionString());
	Out->SetArrayField(TEXT("capabilities"), Items);
}

FString FFreeUltraCodeBluePrintModeCapabilityRegistry::CapabilityIdToName(EFreeUltraCodeBluePrintModeCapabilityId CapabilityId)
{
	switch (CapabilityId)
	{
	case EFreeUltraCodeBluePrintModeCapabilityId::BlueprintBasicGraph: return TEXT("Blueprint.BasicGraph");
	case EFreeUltraCodeBluePrintModeCapabilityId::BlueprintNodeDiscovery: return TEXT("Blueprint.NodeDiscovery");
	case EFreeUltraCodeBluePrintModeCapabilityId::BlueprintKeySpawn: return TEXT("Blueprint.KeySpawn");
	case EFreeUltraCodeBluePrintModeCapabilityId::BlueprintAutoLayout: return TEXT("Blueprint.AutoLayout");
	case EFreeUltraCodeBluePrintModeCapabilityId::BlueprintDispatcher: return TEXT("Blueprint.Dispatcher");
	case EFreeUltraCodeBluePrintModeCapabilityId::BlueprintTimeline: return TEXT("Blueprint.Timeline");
	case EFreeUltraCodeBluePrintModeCapabilityId::UMGWidgetTree: return TEXT("UMG.WidgetTree");
	case EFreeUltraCodeBluePrintModeCapabilityId::AnimGraphEdit: return TEXT("AnimGraph.Edit");
	case EFreeUltraCodeBluePrintModeCapabilityId::StateTreeEdit: return TEXT("StateTree.Edit");
	case EFreeUltraCodeBluePrintModeCapabilityId::NiagaraStackEdit: return TEXT("Niagara.StackEdit");
	default: return TEXT("Unknown");
	}
}

FFreeUltraCodeBluePrintModeCapability FFreeUltraCodeBluePrintModeCapabilityRegistry::MakeCapability(
	EFreeUltraCodeBluePrintModeCapabilityId CapabilityId,
	bool bSupported,
	bool bImplemented,
	const FString& Reason,
	const FString& MinVersion,
	const FString& Backend,
	bool bExperimental)
{
	FFreeUltraCodeBluePrintModeCapability Capability;
	Capability.CapabilityId = CapabilityId;
	Capability.Name = CapabilityIdToName(CapabilityId);
	Capability.bSupported = bSupported;
	Capability.bImplemented = bImplemented;
	Capability.bEnabled = bSupported && bImplemented;
	Capability.Reason = Reason;
	Capability.MinVersion = MinVersion;
	Capability.Backend = Backend;
	Capability.bExperimental = bExperimental;
	return Capability;
}

bool FFreeUltraCodeBluePrintModeCapabilityRegistry::IsEngineAtLeast(int32 Major, int32 Minor)
{
	if (ENGINE_MAJOR_VERSION != Major)
	{
		return ENGINE_MAJOR_VERSION > Major;
	}
	return ENGINE_MINOR_VERSION >= Minor;
}

bool FFreeUltraCodeBluePrintModeCapabilityRegistry::ModuleExists(const TCHAR* ModuleName)
{
	return FModuleManager::Get().ModuleExists(ModuleName);
}

bool FFreeUltraCodeBluePrintModeCapabilityRegistry::PluginEnabled(const FString& PluginName)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	return Plugin.IsValid() && Plugin->IsEnabled();
}

bool FFreeUltraCodeBluePrintModeCapabilityRegistry::ClassExists(const FString& ClassPath, const FString& ShortClassName)
{
	if (!ClassPath.IsEmpty())
	{
		if (LoadClass<UObject>(nullptr, *ClassPath))
		{
			return true;
		}
	}

	if (ShortClassName.IsEmpty())
	{
		return false;
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		const UClass* Candidate = *It;
		if (Candidate && Candidate->GetName().Equals(ShortClassName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

FString FFreeUltraCodeBluePrintModeCapabilityRegistry::EngineVersionString()
{
	return FString::Printf(TEXT("%d.%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION);
}
