#include "CvarScopesModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "CvarScopes"

const FName FCvarScopesModule::TabName(TEXT("CvarScopes"));
const FString FCvarScopesModule::DefaultScopeName(TEXT("Default"));

void FCvarScopesModule::StartupModule()
{
	LoadScopes();
	const int32 LastActiveScopeIndex = Scopes.IndexOfByPredicate([this](const FScope& Scope)
	{
		return Scope.Name.Equals(LastActivatedScopeName, ESearchCase::CaseSensitive);
	});
	SelectedScopeIndex = LastActiveScopeIndex != INDEX_NONE ? LastActiveScopeIndex : 0;
	if (LastActiveScopeIndex == INDEX_NONE)
	{
		LastActivatedScopeName = DefaultScopeName;
	}
	ActiveScopeName.Empty();
	ActiveValues.Empty();
	ActivateSelectedScope();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName, FOnSpawnTab::CreateRaw(this, &FCvarScopesModule::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Cvar Scopes"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsMiscCategory());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCvarScopesModule::RegisterMenus));
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCvarScopesModule::RegisterToolbar));
}

void FCvarScopesModule::ShutdownModule()
{
	SaveScopes();
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabName);
}

void FCvarScopesModule::RegisterMenus()
{
	FToolMenuOwnerScoped Owner(this);
	UToolMenu* MenuBar = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu");
	FToolMenuSection& Section = MenuBar->FindOrAddSection("CvarScopes");
	FToolMenuEntry& PluginsMenu = Section.AddSubMenu(
		"CvarScopesPluginsMenu",
		LOCTEXT("PluginsMenu", "Plugins"),
		LOCTEXT("PluginsMenuTooltip", "Project plugin tools."),
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
		{
			FToolMenuSection& SubMenuSection = SubMenu->FindOrAddSection("CvarScopes");
			SubMenuSection.AddMenuEntry(
				"OpenCvarScopes",
				LOCTEXT("OpenMenu", "Cvar Scopes"),
				LOCTEXT("OpenMenuTooltip", "Open named Console Variable profiles."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(TabName); })));
		}));
	PluginsMenu.InsertPosition = FToolMenuInsert("Help", EToolMenuInsertType::Before);
}

void FCvarScopesModule::RegisterToolbar()
{
	FToolMenuOwnerScoped Owner(this);
	UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.AssetsToolBar");
	FToolMenuSection& Section = Toolbar->FindOrAddSection("CvarScopes");
	FToolMenuEntry& ProfileSelector = Section.AddEntry(FToolMenuEntry::InitComboButton(
		"CvarScopesProfileSelector",
		FToolUIActionChoice(FUIAction()),
		FNewToolMenuChoice(FNewToolMenuDelegate::CreateRaw(this, &FCvarScopesModule::PopulateProfileToolbarMenu)),
		TAttribute<FText>::CreateRaw(this, &FCvarScopesModule::GetToolbarProfileText),
		LOCTEXT("ProfileSelectorTooltip", "Activate a Cvar Scopes profile."),
		TAttribute<FSlateIcon>(),
		false));
	ProfileSelector.ToolBarData.StyleNameOverride = "CalloutToolbar";
	ProfileSelector.ToolBarData.LabelOverride = TAttribute<FText>::CreateRaw(this, &FCvarScopesModule::GetToolbarProfileText);
}

TSharedRef<SDockTab> FCvarScopesModule::SpawnTab(const FSpawnTabArgs& Args)
{
	RebuildScopeItems();

	TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	Tab->SetContent(
		SNew(SSplitter)
		+ SSplitter::Slot().Value(0.28f)
		[
			SNew(SBorder)
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SAssignNew(ScopeListView, SListView<TSharedPtr<FString>>)
					.ListItemsSource(&ScopeItems)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow_Lambda([](TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& Owner)
					{
						return SNew(STableRow<TSharedPtr<FString>>, Owner)
						[
							SNew(STextBlock).Text(FText::FromString(*Item))
						];
					})
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type)
					{
						if (Item.IsValid()) { SelectScope(ScopeItems.IndexOfByKey(Item)); }
					})
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)
					[
						SNew(SButton).Text(LOCTEXT("Add", "Add")).HAlign(HAlign_Center).OnClicked_Lambda([this] { AddScope(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton).Text(LOCTEXT("Delete", "Delete"))
						.HAlign(HAlign_Center)
						.IsEnabled_Lambda([this] { return GetSelectedScopeIndex() > 0; })
						.OnClicked_Lambda([this] { RequestDeleteSelectedScope(); return FReply::Handled(); })
					]
				]
			]
		]
		+ SSplitter::Slot().Value(0.72f)
		[
			SNew(SBorder)
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SEditableTextBox)
					.Text_Lambda([this] { const FScope* Scope = GetSelectedScope(); return Scope ? FText::FromString(Scope->Name) : FText::GetEmpty(); })
					.IsEnabled_Lambda([this] { return GetSelectedScopeIndex() > 0; })
					.OnTextCommitted_Raw(this, &FCvarScopesModule::RenameSelectedScope)
				]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 8.0f)
				[
					SAssignNew(CommandsEditorContainer, SBox)
					[
						CurrentMode == TEXT("Table mode") ? BuildTableEditor() : BuildCommandsEditor()
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton).Text(LOCTEXT("Activate", "Activate"))
						.IsEnabled_Lambda([this] { return !IsSelectedScopeActive(); })
						.OnClicked_Lambda([this] { ActivateSelectedScope(); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f)
					[
						SNew(STextBlock).Text_Lambda([this] { return GetActiveScopeText(); })
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SComboButton)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text_Lambda([this] { return FText::FromString(CurrentMode); })
						]
						.OnGetMenuContent_Lambda([this]()
						{
							FMenuBuilder MenuBuilder(true, nullptr);
							MenuBuilder.AddMenuEntry(
								LOCTEXT("TableMode", "Table mode"),
								FText::GetEmpty(),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([this] { SetEditorMode(TEXT("Table mode")); })));
							MenuBuilder.AddMenuEntry(
								LOCTEXT("NotepadMode", "Notepad mode"),
								FText::GetEmpty(),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([this] { SetEditorMode(TEXT("Notepad mode")); })));
							return MenuBuilder.MakeWidget();
						})
					]
				]
			]
		]);

	if (ScopeListView.IsValid() && ScopeItems.IsValidIndex(SelectedScopeIndex))
	{
		ScopeListView->SetSelection(ScopeItems[SelectedScopeIndex]);
	}
	return Tab;
}

void FCvarScopesModule::LoadScopes()
{
	Scopes.Empty();
	FString DefaultCommands;
	bool bHasSavedDefault = false;
	FString JsonText;
	if (FFileHelper::LoadFileToString(JsonText, *GetStoragePath()))
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			Root->TryGetStringField(TEXT("LastActivatedScope"), LastActivatedScopeName);
			const TArray<TSharedPtr<FJsonValue>>* SavedScopes;
			if (Root->TryGetArrayField(TEXT("Scopes"), SavedScopes))
			{
				for (const TSharedPtr<FJsonValue>& Value : *SavedScopes)
				{
					const TSharedPtr<FJsonObject> Object = Value->AsObject();
					if (!Object.IsValid()) { continue; }
					FScope Scope;
					Object->TryGetStringField(TEXT("Name"), Scope.Name);
					Object->TryGetStringField(TEXT("Commands"), Scope.Commands);
					if (Scope.Name.Equals(DefaultScopeName, ESearchCase::IgnoreCase))
					{
						if (!bHasSavedDefault)
						{
							DefaultCommands = MoveTemp(Scope.Commands);
							bHasSavedDefault = true;
						}
					}
					else if (!Scope.Name.IsEmpty())
					{
						Scopes.Add(MoveTemp(Scope));
					}
				}
			}
		}
	}

	Scopes.Insert({ DefaultScopeName, MoveTemp(DefaultCommands) }, 0);
	SelectedScopeIndex = 0;
	SaveScopes();
}

void FCvarScopesModule::SaveScopes() const
{
	TArray<TSharedPtr<FJsonValue>> SavedScopes;
	for (const FScope& Scope : Scopes)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("Name"), Scope.Name);
		Object->SetStringField(TEXT("Commands"), Scope.Commands);
		SavedScopes.Add(MakeShared<FJsonValueObject>(Object));
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("Scopes"), SavedScopes);
	Root->SetStringField(TEXT("LastActivatedScope"), LastActivatedScopeName);
	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	FJsonSerializer::Serialize(Root, Writer);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(GetStoragePath()), true);
	FFileHelper::SaveStringToFile(JsonText, *GetStoragePath());
}

FString FCvarScopesModule::GetStoragePath() const { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CvarScopes"), TEXT("Profiles.json")); }

void FCvarScopesModule::RebuildScopeItems()
{
	ScopeItems.Empty();
	for (const FScope& Scope : Scopes) { ScopeItems.Add(MakeShared<FString>(Scope.Name)); }
}

void FCvarScopesModule::SelectScope(int32 Index)
{
	if (Scopes.IsValidIndex(Index))
	{
		SelectedScopeIndex = Index;
		if (CurrentMode == TEXT("Table mode") && CommandsEditorContainer.IsValid()) { CommandsEditorContainer->SetContent(BuildTableEditor()); }
		SaveScopes();
	}
}

int32 FCvarScopesModule::GetSelectedScopeIndex() const { return SelectedScopeIndex; }
FCvarScopesModule::FScope* FCvarScopesModule::GetSelectedScope() { return Scopes.IsValidIndex(SelectedScopeIndex) ? &Scopes[SelectedScopeIndex] : nullptr; }
const FCvarScopesModule::FScope* FCvarScopesModule::GetSelectedScope() const { return Scopes.IsValidIndex(SelectedScopeIndex) ? &Scopes[SelectedScopeIndex] : nullptr; }

FString FCvarScopesModule::MakeUniqueName(const FString& DesiredName, int32 IgnoreIndex) const
{
	FString Base = DesiredName.TrimStartAndEnd();
	if (Base.IsEmpty()) { Base = TEXT("NewScope"); }
	if (Base.Equals(DefaultScopeName, ESearchCase::IgnoreCase)) { Base = TEXT("Default1"); }
	auto IsTaken = [this, IgnoreIndex](const FString& Candidate)
	{
		for (int32 Index = 0; Index < Scopes.Num(); ++Index)
		{
			if (Index != IgnoreIndex && Scopes[Index].Name.Equals(Candidate, ESearchCase::IgnoreCase)) { return true; }
		}
		return false;
	};
	if (!IsTaken(Base)) { return Base; }
	for (int32 Suffix = 1; ; ++Suffix)
	{
		const FString Candidate = FString::Printf(TEXT("%s%d"), *Base, Suffix);
		if (!IsTaken(Candidate)) { return Candidate; }
	}
}

void FCvarScopesModule::AddScope()
{
	Scopes.Add({ MakeUniqueName(TEXT("NewScope")), FString() });
	SelectedScopeIndex = Scopes.Num() - 1;
	RebuildScopeItems();
	SaveScopes();
	if (ScopeListView.IsValid()) { ScopeListView->RequestListRefresh(); ScopeListView->SetSelection(ScopeItems[SelectedScopeIndex]); }
}

void FCvarScopesModule::RequestDeleteSelectedScope() { if (GetSelectedScopeIndex() > 0) { ShowDeleteConfirmation(); } }

void FCvarScopesModule::ShowDeleteConfirmation()
{
	const FString ScopeName = GetSelectedScope()->Name;
	const TSharedRef<SWindow> Dialog = SNew(SWindow)
		.Title(LOCTEXT("DeleteConfirmTitle", "Delete Cvar Scope"))
		.ClientSize(FVector2D(360.0f, 120.0f))
		.SupportsMinimize(false).SupportsMaximize(false);
	const TWeakPtr<SWindow> WeakDialog = Dialog;
	Dialog->SetContent(
		SNew(SBorder).Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(STextBlock).Text(FText::Format(LOCTEXT("DeleteConfirm", "Delete profile '{0}'?"), FText::FromString(ScopeName)))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton).Text(LOCTEXT("Yes", "Yes")).HAlign(HAlign_Center).OnClicked_Lambda([this, WeakDialog] { DeleteSelectedScope(); if (const TSharedPtr<SWindow> PinnedDialog = WeakDialog.Pin()) { PinnedDialog->RequestDestroyWindow(); } return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton).Text(LOCTEXT("Cancel", "Cancel")).HAlign(HAlign_Center).OnClicked_Lambda([WeakDialog] { if (const TSharedPtr<SWindow> PinnedDialog = WeakDialog.Pin()) { PinnedDialog->RequestDestroyWindow(); } return FReply::Handled(); })
					]
				]
			]);
	FSlateApplication::Get().AddModalWindow(Dialog, FSlateApplication::Get().GetActiveTopLevelWindow());
}

void FCvarScopesModule::DeleteSelectedScope()
{
	if (SelectedScopeIndex <= 0) { return; }
	const int32 ScopeIndexToDelete = SelectedScopeIndex;
	if (ActiveScopeName.Equals(Scopes[ScopeIndexToDelete].Name, ESearchCase::CaseSensitive)
		|| LastActivatedScopeName.Equals(Scopes[ScopeIndexToDelete].Name, ESearchCase::CaseSensitive))
	{
		SelectedScopeIndex = 0;
		ActivateSelectedScope();
	}
	Scopes.RemoveAt(ScopeIndexToDelete);
	SelectedScopeIndex = 0;
	RebuildScopeItems();
	SaveScopes();
	if (ScopeListView.IsValid()) { ScopeListView->RequestListRefresh(); ScopeListView->SetSelection(ScopeItems[0]); }
}

void FCvarScopesModule::RenameSelectedScope(const FText& NewName, ETextCommit::Type)
{
	if (FScope* Scope = GetSelectedScope(); Scope && SelectedScopeIndex > 0)
	{
		const FString OldName = Scope->Name;
		Scope->Name = MakeUniqueName(NewName.ToString(), SelectedScopeIndex);
		if (ActiveScopeName.Equals(OldName, ESearchCase::CaseSensitive)) { ActiveScopeName.Empty(); }
		if (LastActivatedScopeName.Equals(OldName, ESearchCase::CaseSensitive)) { LastActivatedScopeName = Scope->Name; }
		RebuildScopeItems();
		SaveScopes();
		if (ScopeListView.IsValid()) { ScopeListView->RequestListRefresh(); ScopeListView->SetSelection(ScopeItems[SelectedScopeIndex]); }
	}
}

void FCvarScopesModule::UpdateSelectedCommands(const FText& NewCommands)
{
	if (FScope* Scope = GetSelectedScope())
	{
		if (Scope->Commands != NewCommands.ToString() && ActiveScopeName.Equals(Scope->Name, ESearchCase::CaseSensitive)) { ActiveScopeName.Empty(); }
		Scope->Commands = NewCommands.ToString();
	}
}

TSharedRef<SWidget> FCvarScopesModule::BuildCommandsEditor()
{
	return SNew(SMultiLineEditableTextBox)
		.Text_Lambda([this] { const FScope* Scope = GetSelectedScope(); return Scope ? FText::FromString(Scope->Commands) : FText::GetEmpty(); })
		.OnTextChanged_Raw(this, &FCvarScopesModule::UpdateSelectedCommands)
		.HintText(LOCTEXT("CommandsHint", "One CVar assignment per line, for example: r.Nanite 0"));
}

TSharedRef<SWidget> FCvarScopesModule::BuildTableEditor()
{
	RebuildTableItems();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("TableNameColumn", "CVar"))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("TableValueColumn", "Value"))
			]
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SAssignNew(TableListView, SListView<TSharedPtr<FTableEntry>>)
			.ListItemsSource(&TableItems)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow_Lambda([this](TSharedPtr<FTableEntry> Entry, const TSharedRef<STableViewBase>& Owner)
			{
				return SNew(STableRow<TSharedPtr<FTableEntry>>, Owner)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
					[
						SNew(SEditableTextBox)
						.Text_Lambda([Entry] { return FText::FromString(Entry->Name); })
						.OnTextChanged_Lambda([this, Entry](const FText& NewText) { UpdateTableEntry(Entry, NewText, true); })
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
					[
						SNew(SEditableTextBox)
						.Text_Lambda([Entry] { return FText::FromString(Entry->Value); })
						.OnTextChanged_Lambda([this, Entry](const FText& NewText) { UpdateTableEntry(Entry, NewText, false); })
					]
				];
			})
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("AddTableRow", "Add row"))
				.OnClicked_Lambda([this] { AddTableEntry(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton).Text(LOCTEXT("DeleteTableRow", "Delete row"))
				.IsEnabled_Lambda([this] { return TableListView.IsValid() && TableListView->GetNumItemsSelected() > 0; })
				.OnClicked_Lambda([this] { DeleteSelectedTableEntry(); return FReply::Handled(); })
			]
		];
}

void FCvarScopesModule::SetEditorMode(const FString& NewMode)
{
	if (CurrentMode == NewMode || !CommandsEditorContainer.IsValid()) { return; }
	CurrentMode = NewMode;
	CommandsEditorContainer->SetContent(CurrentMode == TEXT("Table mode") ? BuildTableEditor() : BuildCommandsEditor());
}

void FCvarScopesModule::RebuildTableItems()
{
	TableItems.Empty();
	const FScope* Scope = GetSelectedScope();
	if (!Scope) { return; }

	TArray<FString> Lines;
	Scope->Commands.ParseIntoArrayLines(Lines, false);
	for (FString Line : Lines)
	{
		Line.TrimStartAndEndInline();
		TSharedPtr<FTableEntry> Entry = MakeShared<FTableEntry>();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT(";")))
		{
			Entry->Name = MoveTemp(Line);
		}
		else
		{
			int32 SplitIndex = INDEX_NONE;
			for (int32 Index = 0; Index < Line.Len(); ++Index)
			{
				if (FChar::IsWhitespace(Line[Index])) { SplitIndex = Index; break; }
			}
			Entry->Name = SplitIndex == INDEX_NONE ? Line : Line.Left(SplitIndex);
			Entry->Value = SplitIndex == INDEX_NONE ? FString() : Line.Mid(SplitIndex).TrimStartAndEnd();
		}
		TableItems.Add(MoveTemp(Entry));
	}
}

void FCvarScopesModule::UpdateTableEntry(TSharedPtr<FTableEntry> Entry, const FText& NewText, bool bIsName)
{
	if (!Entry.IsValid()) { return; }
	if (bIsName) { Entry->Name = NewText.ToString(); }
	else { Entry->Value = NewText.ToString(); }
	CommitTableEntries();
}

void FCvarScopesModule::CommitTableEntries()
{
	TArray<FString> Lines;
	Lines.Reserve(TableItems.Num());
	for (const TSharedPtr<FTableEntry>& Entry : TableItems)
	{
		const FString Name = Entry->Name.TrimStartAndEnd();
		const FString Value = Entry->Value.TrimStartAndEnd();
		Lines.Add(Value.IsEmpty() ? Name : FString::Printf(TEXT("%s %s"), *Name, *Value));
	}
	UpdateSelectedCommands(FText::FromString(FString::Join(Lines, TEXT("\n"))));
}

void FCvarScopesModule::AddTableEntry()
{
	const TSharedPtr<FTableEntry> Entry = MakeShared<FTableEntry>();
	TableItems.Add(Entry);
	CommitTableEntries();
	if (TableListView.IsValid())
	{
		TableListView->RequestListRefresh();
		TableListView->SetSelection(Entry);
	}
}

void FCvarScopesModule::DeleteSelectedTableEntry()
{
	if (!TableListView.IsValid()) { return; }
	const TArray<TSharedPtr<FTableEntry>> SelectedEntries = TableListView->GetSelectedItems();
	if (SelectedEntries.Num() == 0) { return; }
	TableItems.Remove(SelectedEntries[0]);
	CommitTableEntries();
	TableListView->RequestListRefresh();
}

void FCvarScopesModule::ActivateSelectedScope()
{
	const FScope* Scope = GetSelectedScope();
	if (!Scope) { return; }
	const FCvarValueMap NextValues = ParseCommands(Scope->Commands);
	const FCvarValueMap DefaultValues = ParseCommands(Scopes[0].Commands);
	for (const TPair<FString, FString>& OldPair : ActiveValues)
	{
		const FString* NextValue = NextValues.Find(OldPair.Key);
		if (!NextValue) { ResetToDefaultValue(OldPair.Key, DefaultValues); }
	}
	for (const TPair<FString, FString>& NextPair : NextValues)
	{
		const FString* OldValue = ActiveValues.Find(NextPair.Key);
		if (!OldValue || *OldValue != NextPair.Value) { ApplyValue(NextPair.Key, NextPair.Value); }
	}
	ActiveValues = NextValues;
	ActiveScopeName = Scope->Name;
	LastActivatedScopeName = Scope->Name;
	SaveScopes();
}

void FCvarScopesModule::ActivateScopeByName(const FString& ScopeName)
{
	const int32 ScopeIndex = Scopes.IndexOfByPredicate([&ScopeName](const FScope& Scope)
	{
		return Scope.Name.Equals(ScopeName, ESearchCase::CaseSensitive);
	});
	if (ScopeIndex == INDEX_NONE) { return; }

	const int32 PreviouslySelectedScopeIndex = SelectedScopeIndex;
	SelectedScopeIndex = ScopeIndex;
	ActivateSelectedScope();
	SelectedScopeIndex = PreviouslySelectedScopeIndex;
}

void FCvarScopesModule::PopulateProfileToolbarMenu(UToolMenu* Menu)
{
	FToolMenuSection& Section = Menu->FindOrAddSection("CvarScopesProfiles");
	for (const FScope& Scope : Scopes)
	{
		const FString ScopeName = Scope.Name;
		Section.AddMenuEntry(
			FName(*FString::Printf(TEXT("Activate_%s"), *ScopeName)),
			FText::FromString(ScopeName),
			FText::Format(LOCTEXT("ActivateProfileTooltip", "Activate profile '{0}'."), FText::FromString(ScopeName)),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, ScopeName] { ActivateScopeByName(ScopeName); }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, ScopeName] { return LastActivatedScopeName.Equals(ScopeName, ESearchCase::CaseSensitive); })),
			EUserInterfaceActionType::RadioButton);
	}
}

FCvarScopesModule::FCvarValueMap FCvarScopesModule::ParseCommands(const FString& Commands) const
{
	FCvarValueMap Result;
	TArray<FString> Lines;
	Commands.ParseIntoArrayLines(Lines, true);
	for (FString Line : Lines)
	{
		Line = Line.TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT(";"))) { continue; }
		int32 SplitIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Line.Len(); ++Index) { if (FChar::IsWhitespace(Line[Index])) { SplitIndex = Index; break; } }
		if (SplitIndex == INDEX_NONE) { continue; }
		const FString Name = Line.Left(SplitIndex);
		const FString Value = Line.Mid(SplitIndex).TrimStartAndEnd();
		if (!Name.IsEmpty() && !Value.IsEmpty()) { Result.Add(Name, Value); }
	}
	return Result;
}

void FCvarScopesModule::ApplyValue(const FString& Name, const FString& Value) const
{
	if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name)) { Variable->Set(*Value, ECVF_SetByConsole); }
	else { UE_LOG(LogTemp, Warning, TEXT("CvarScopes: Console variable '%s' was not found."), *Name); }
}

void FCvarScopesModule::ResetToDefaultValue(const FString& Name, const FCvarValueMap& DefaultValues) const
{
	if (const FString* DefaultValue = DefaultValues.Find(Name)) { ApplyValue(Name, *DefaultValue); }
	else if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name)) { Variable->Set(*Variable->GetDefaultValue(), ECVF_SetByConsole); }
}

bool FCvarScopesModule::IsSelectedScopeActive() const
{
	const FScope* Scope = GetSelectedScope();
	return Scope && ActiveScopeName.Equals(Scope->Name, ESearchCase::CaseSensitive);
}

FText FCvarScopesModule::GetActiveScopeText() const
{
	return ActiveScopeName.IsEmpty() ? LOCTEXT("ActiveModified", "Active profile has unsaved editor changes") : FText::Format(LOCTEXT("ActiveScope", "Active: {0}"), FText::FromString(ActiveScopeName));
}

FText FCvarScopesModule::GetToolbarProfileText() const
{
	return FText::FromString(LastActivatedScopeName.IsEmpty() ? DefaultScopeName : LastActivatedScopeName);
}

IMPLEMENT_MODULE(FCvarScopesModule, CvarScopes)

#undef LOCTEXT_NAMESPACE
