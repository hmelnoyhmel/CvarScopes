#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class SWindow;
class SBox;
template <typename ItemType> class SListView;

class FCvarScopesModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	struct FScope
	{
		FString Name;
		FString Commands;
	};

	struct FTableEntry
	{
		FString Name;
		FString Value;
	};

	using FCvarValueMap = TMap<FString, FString>;

	static const FName TabName;
	static const FString DefaultScopeName;

	void RegisterMenus();
	void RegisterToolbar();
	TSharedRef<SDockTab> SpawnTab(const class FSpawnTabArgs& Args);
	void LoadScopes();
	void SaveScopes() const;
	FString GetStoragePath() const;

	void RebuildScopeItems();
	void SelectScope(int32 Index);
	int32 GetSelectedScopeIndex() const;
	FScope* GetSelectedScope();
	const FScope* GetSelectedScope() const;
	FString MakeUniqueName(const FString& DesiredName, int32 IgnoreIndex = INDEX_NONE) const;

	void AddScope();
	void RequestDeleteSelectedScope();
	void ShowDeleteConfirmation();
	void DeleteSelectedScope();
	void RenameSelectedScope(const FText& NewName, ETextCommit::Type CommitType);
	void UpdateSelectedCommands(const FText& NewCommands);
	TSharedRef<class SWidget> BuildCommandsEditor();
	TSharedRef<class SWidget> BuildTableEditor();
	void SetEditorMode(const FString& NewMode);
	void RebuildTableItems();
	void UpdateTableEntry(TSharedPtr<FTableEntry> Entry, const FText& NewText, bool bIsName);
	void CommitTableEntries();
	void AddTableEntry();
	void DeleteSelectedTableEntry();
	void ActivateSelectedScope();
	void ActivateScopeByName(const FString& ScopeName);
	void PopulateProfileToolbarMenu(class UToolMenu* Menu);

	FCvarValueMap ParseCommands(const FString& Commands) const;
	void ApplyValue(const FString& Name, const FString& Value) const;
	void ResetToDefaultValue(const FString& Name, const FCvarValueMap& DefaultValues) const;
	bool IsSelectedScopeActive() const;
	FText GetActiveScopeText() const;
	FText GetToolbarProfileText() const;

	TArray<FScope> Scopes;
	TArray<TSharedPtr<FString>> ScopeItems;
	TSharedPtr<SListView<TSharedPtr<FString>>> ScopeListView;
	TArray<TSharedPtr<FTableEntry>> TableItems;
	TSharedPtr<SListView<TSharedPtr<FTableEntry>>> TableListView;
	TSharedPtr<SBox> CommandsEditorContainer;
	int32 SelectedScopeIndex = 0;
	FString ActiveScopeName;
	FString LastActivatedScopeName;
	FCvarValueMap ActiveValues;
	FString CurrentMode = TEXT("Table mode");
};
