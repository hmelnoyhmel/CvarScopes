#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class SWindow;
class SBox;
class SScrollBar;
class SScrollBox;
template <typename ItemType> class SListView;

class FCvarScopesModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	enum class EDiagnosticSeverity : uint8
	{
		None,
		Warning,
		Error
	};

	enum class ECvarValueType : uint8
	{
		Unknown,
		Bool,
		Int,
		Float,
		String
	};

	struct FDiagnostic
	{
		EDiagnosticSeverity Severity = EDiagnosticSeverity::None;
		ECvarValueType ValueType = ECvarValueType::Unknown;
		FString Message;
		bool bIsDuplicate = false;
	};

	struct FLineDiagnostic
	{
		int32 LineIndex = INDEX_NONE;
		FDiagnostic Diagnostic;
	};


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
		bool bIsCommentOrEmpty = false;
		FDiagnostic Diagnostic;
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
	void ScheduleValidation();
	void RefreshValidation();
	FDiagnostic ValidateAssignment(const FString& Name, const FString& Value) const;
	TArray<FLineDiagnostic> ValidateCommands(const FString& Commands) const;
	static bool IsBooleanValue(const FString& Value, bool& OutValue);
	static FString GetValueTypeHint(ECvarValueType ValueType);
	static FSlateColor GetDiagnosticColor(const FDiagnostic& Diagnostic);
	static FString GetDiagnosticSymbol(const FDiagnostic& Diagnostic);
	TSharedRef<class SWidget> BuildNotepadDiagnosticsGutter();
	static void GetCvarSuggestions(const FString& Input, TArray<FString>& OutSuggestions);
	void UpdateTableBoolEntry(TSharedPtr<FTableEntry> Entry, ECheckBoxState NewState);
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
	TSharedPtr<SBox> NotepadDiagnosticsContainer;
	TSharedPtr<SScrollBar> NotepadScrollBar;
	TArray<FLineDiagnostic> NotepadDiagnostics;
	FTSTicker::FDelegateHandle ValidationTickerHandle;
	float NotepadLineHeight = 18.0f;
	int32 SelectedScopeIndex = 0;
	FString ActiveScopeName;
	FString LastActivatedScopeName;
	FCvarValueMap ActiveValues;
	FString CurrentMode = TEXT("Table mode");
};
