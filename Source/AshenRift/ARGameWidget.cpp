#include "ARGameWidget.h"
#include "ARPrototype.h"
#include "ARProgression.h"
#include "AREnemyVariants.h"
#include "ARDungeon.h"
#include "ARHealthComponent.h"
#include "ARInventoryComponent.h"
#include "AREquipmentComponent.h"
#include "ARRunGameInstance.h"
#include "ARProfile.h"
#include "ARCombatFeedback.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/Spacer.h"
#include "Components/ScrollBox.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

namespace
{
    const FLinearColor Gold(.86f, .67f, .36f);
    const FLinearColor Muted(.63f, .69f, .75f);
    const FLinearColor Cyan(.36f, .76f, .86f);
    const FLinearColor Ink(.018f, .027f, .041f, .97f);
}
void UARActionButton::InvokeAction() { if (IsValid(Screen)) Screen->ExecuteAction(Action, Payload); }
AARPlayerCharacter* UARGameWidget::Player() const { return Cast<AARPlayerCharacter>(GetOwningPlayerPawn()); }
UARRunGameInstance* UARGameWidget::Run() const { return Cast<UARRunGameInstance>(GetGameInstance()); }
bool UARGameWidget::BlocksGameplayInput() const
{
    auto* P = Player(); auto* S = Run();
    return bMainMenu || bPaused || (S && (S->Phase == EARRunPhase::Result || S->Phase == EARRunPhase::Settling)) ||
        (P && (P->bCampOpen || P->bInventoryVisible || P->bShopOpen));
}

UTextBlock* UARGameWidget::Text(UPanelWidget* Parent, const FString& Value, int32 Size, FLinearColor Color, FName Key)
{
    auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(FText::FromString(Value));
    Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
    Label->SetColorAndOpacity(FSlateColor(Color));
    Label->SetAutoWrapText(true);
    auto* ChildSlot = Parent->AddChild(Label);
    if (auto* Vertical = Cast<UVerticalBoxSlot>(ChildSlot)) Vertical->SetPadding(FMargin(0, 3));
    if (auto* Horizontal = Cast<UHorizontalBoxSlot>(ChildSlot))
    { Horizontal->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Horizontal->SetVerticalAlignment(VAlign_Center); Horizontal->SetPadding(FMargin(0, 3, 8, 3)); }
    if (!Key.IsNone()) Labels.Add(Key, Label);
    return Label;
}
UARActionButton* UARGameWidget::Button(UPanelWidget* Parent, const FString& Value, FName Action, int32 Payload, FName LabelKey)
{
    auto* Box = WidgetTree->ConstructWidget<USizeBox>();
    Box->SetHeightOverride(42);
    auto* ChildSlot = Parent->AddChild(Box);
    if (auto* Vertical = Cast<UVerticalBoxSlot>(ChildSlot)) Vertical->SetPadding(FMargin(0, 4));
    if (auto* Horizontal = Cast<UHorizontalBoxSlot>(ChildSlot))
    {
        Horizontal->SetPadding(FMargin(3, 4));
        if (Action == TEXT("PreparePlus") || Action == TEXT("PrepareMinus")) Box->SetWidthOverride(56);
        else Horizontal->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    auto* Click = WidgetTree->ConstructWidget<UARActionButton>();
    Box->AddChild(Click);
    Click->Screen = this; Click->Action = Action; Click->Payload = Payload;
    Click->SetBackgroundColor(FLinearColor(.12f, .19f, .24f));
    Click->OnClicked.AddDynamic(Click, &UARActionButton::InvokeAction);
    auto* Caption = Text(Click, Value, 16, Gold, LabelKey);
    Caption->SetAutoWrapText(false);
    Caption->SetJustification(ETextJustify::Center);
    if (auto* ContentSlot = Cast<UButtonSlot>(Caption->Slot)) { ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Center); }
    Actions.Add(FName(*(Action.ToString() + FString::FromInt(Payload))), Click);
    return Click;
}
void UARGameWidget::Gap(UPanelWidget* Parent, float Height)
{
    auto* Space = WidgetTree->ConstructWidget<USpacer>();
    Space->SetSize(FVector2D(1, Height));
    Parent->AddChild(Space);
}
UBorder* UARGameWidget::Panel(UCanvasPanel* Parent, FVector2D Position, FVector2D Size, FLinearColor Color)
{
    auto* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrushColor(Color);
    Border->SetPadding(FMargin(16));
    auto* ChildSlot = Parent->AddChildToCanvas(Border);
    ChildSlot->SetPosition(Position); ChildSlot->SetSize(Size);
    return Border;
}
void UARGameWidget::BuildRoot()
{
    WidgetTree = NewObject<UWidgetTree>(this, TEXT("GameWidgetTree"));
    auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    WidgetTree->RootWidget = Scale;
    auto* Design = WidgetTree->ConstructWidget<USizeBox>();
    Design->SetWidthOverride(1280); Design->SetHeightOverride(720);
    Scale->AddChild(Design);
    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Design->AddChild(RootCanvas);
    StatusPanel = Panel(RootCanvas, FVector2D(22, 20), FVector2D(374, 205), Ink);
    auto* Stats = WidgetTree->ConstructWidget<UVerticalBox>(); StatusPanel->AddChild(Stats);
    Text(Stats, TEXT("ASHEN RIFT  /  失落边境"), 17, Gold);
    Text(Stats, TEXT(""), 16, FLinearColor::White, TEXT("HudHealth"));
    HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
    HealthBar->SetFillColorAndOpacity(FLinearColor(.74f, .12f, .12f));
    auto* HealthSize = WidgetTree->ConstructWidget<USizeBox>(); HealthSize->SetHeightOverride(8); HealthSize->AddChild(HealthBar); Stats->AddChild(HealthSize);
    Text(Stats, TEXT(""), 14, Cyan, TEXT("HudCombat"));
    ManaBar = WidgetTree->ConstructWidget<UProgressBar>(); ManaBar->SetFillColorAndOpacity(Cyan);
    auto* ManaSize = WidgetTree->ConstructWidget<USizeBox>(); ManaSize->SetHeightOverride(5); ManaSize->AddChild(ManaBar); Stats->AddChild(ManaSize);
    Text(Stats, TEXT(""), 14, FLinearColor::White, TEXT("HudWallet"));
    Text(Stats, TEXT(""), 13, Muted, TEXT("HudSkill"));
    PromptPanel = Panel(RootCanvas, FVector2D(22, 570), FVector2D(720, 88), FLinearColor(.018f, .027f, .041f, .82f));
    auto* PromptBody = WidgetTree->ConstructWidget<UVerticalBox>(); PromptPanel->AddChild(PromptBody);
    Text(PromptBody, TEXT(""), 15, Gold, TEXT("HudNotice"));
    Text(PromptBody, TEXT(""), 14, Cyan, TEXT("HudContext"));
    FooterPanel = Panel(RootCanvas, FVector2D(22, 668), FVector2D(1236, 40), Ink); FooterPanel->SetPadding(FMargin(10, 5));
    Text(FooterPanel, TEXT(""), 12, Muted, TEXT("HudRoute"));
    BossPanel = Panel(RootCanvas, FVector2D(440, 22), FVector2D(600, 76), Ink);
    auto* BossBody = WidgetTree->ConstructWidget<UVerticalBox>(); BossPanel->AddChild(BossBody);
    Text(BossBody, TEXT(""), 17, Gold, TEXT("HudBoss"));
    BossBar = WidgetTree->ConstructWidget<UProgressBar>(); BossBar->SetFillColorAndOpacity(FLinearColor(.7f, .09f, .13f));
    auto* BossSize = WidgetTree->ConstructWidget<USizeBox>(); BossSize->SetHeightOverride(10); BossSize->AddChild(BossBar); BossBody->AddChild(BossSize);
    // The modal layer is last so its buttons always receive clicks above the HUD.
    Dimmer = Panel(RootCanvas, FVector2D::ZeroVector, FVector2D(1280, 720), FLinearColor(.003f, .006f, .012f, .7f));
    Modal = Panel(RootCanvas, FVector2D(110, 40), FVector2D(1060, 640), FLinearColor(.018f, .027f, .041f, 1));
    Modal->SetPadding(FMargin(24));
    ModalBody = WidgetTree->ConstructWidget<UVerticalBox>(); Modal->AddChild(ModalBody);
    bBuilt = true;
}
void UARGameWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bIsFocusable = true;
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    BuildScreen(bMainMenu ? EARUIScreen::MainMenu : EARUIScreen::Game);
    Refresh();
}
TSharedRef<SWidget> UARGameWidget::RebuildWidget()
{
    if (!bBuilt) BuildRoot();
    return Super::RebuildWidget();
}
void UARGameWidget::BuildScreen(EARUIScreen Screen)
{
    View = Screen;
    for (auto It = Labels.CreateIterator(); It; ++It) if (!It.Key().ToString().StartsWith(TEXT("Hud"))) It.RemoveCurrent();
    Actions.Empty(); ModalBody->ClearChildren();
    Modal->SetVisibility(Screen == EARUIScreen::Game ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    Dimmer->SetVisibility(Screen == EARUIScreen::Game ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    const bool bLarge = Screen == EARUIScreen::MainMenu || Screen == EARUIScreen::Camp;
    const bool bSide = Screen == EARUIScreen::Inventory || Screen == EARUIScreen::Shop;
    Dimmer->SetBrushColor(FLinearColor(.003f, .006f, .012f, bSide ? .3f : .7f));
    const FVector2D Size = bLarge ? FVector2D(1060, 640) : bSide ? FVector2D(500, 660) : FVector2D(640, 600);
    auto* ModalSlot = Cast<UCanvasPanelSlot>(Modal->Slot);
    ModalSlot->SetPosition(bSide ? FVector2D(758, 30) : (FVector2D(1280, 720) - Size) * .5f); ModalSlot->SetSize(Size);
    if (Screen == EARUIScreen::MainMenu)
    {
        Text(ModalBody, TEXT("ASHEN RIFT"), 42, Gold);
        Text(ModalBody, TEXT("失 落 边 境"), 28, FLinearColor::White);
        Text(ModalBody, TEXT("深入地牢，获取战利品；选择撤离，把成长带回营地。"), 17, Muted);
        Gap(ModalBody, 20);
        auto* Classes = WidgetTree->ConstructWidget<UHorizontalBox>(); ModalBody->AddChild(Classes);
        Button(Classes, TEXT("战士"), TEXT("Warrior"), 0, TEXT("ClassWarrior"));
        Button(Classes, TEXT("法师"), TEXT("Mage"), 0, TEXT("ClassMage"));
        Text(ModalBody, TEXT(""), 16, Cyan, TEXT("ClassDescription"));
        Gap(ModalBody, 18);
        Button(ModalBody, TEXT("进入营地  ·  准备远征"), TEXT("Start"));
        Text(ModalBody, TEXT(""), 14, Gold, TEXT("ModalNotice"));
        Text(ModalBody, TEXT("死亡会失去本局物品；仓库中未携带的装备与永久天赋会保留。"), 16, Muted);
        Text(ModalBody, TEXT("WASD 移动 · 鼠标观察 · 左键攻击 · F 技能 · Q 药水 · Esc 暂停"), 15, Muted);
        Gap(ModalBody, 16);
        Button(ModalBody, TEXT(""), TEXT("Sound"), 0, TEXT("SoundLabel"));
        Button(ModalBody, TEXT("退出游戏"), TEXT("Quit"));
        Text(ModalBody, TEXT("0.8 课程原型  /  单人地牢探索与撤离"), 13, Muted);
    }
    else if (Screen == EARUIScreen::Camp)
    {
        Text(ModalBody, TEXT("营地  /  远征准备"), 28, Gold);
        Text(ModalBody, TEXT(""), 16, Cyan, TEXT("CampBank"));
        Gap(ModalBody, 8);
        auto* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(); ModalBody->AddChild(Columns);
        auto* Left = WidgetTree->ConstructWidget<UVerticalBox>();
        auto* LeftSlot = Columns->AddChildToHorizontalBox(Left); LeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LeftSlot->SetPadding(FMargin(0, 0, 30, 0));
        auto* Right = WidgetTree->ConstructWidget<UVerticalBox>(); Columns->AddChildToHorizontalBox(Right)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Text(Left, TEXT("选择职业"), 20, FLinearColor::White);
        auto* Classes = WidgetTree->ConstructWidget<UHorizontalBox>(); Left->AddChild(Classes);
        Button(Classes, TEXT("战士 [1]"), TEXT("Warrior"), 0, TEXT("ClassWarrior")); Button(Classes, TEXT("法师 [2]"), TEXT("Mage"), 0, TEXT("ClassMage"));
        Text(Left, TEXT(""), 14, Cyan, TEXT("ClassDescription"));
        Gap(Left, 8);
        Text(Left, TEXT("永久天赋  /  两职业共享"), 20, FLinearColor::White);
        Button(Left, TEXT(""), TEXT("Vitality"), 0, TEXT("VitalityLabel"));
        Button(Left, TEXT(""), TEXT("Might"), 0, TEXT("MightLabel"));
        Text(Left, TEXT(""), 15, Cyan, TEXT("TalentBonus"));
        Text(Left, TEXT("每级生命 +10 或伤害 +2。升级花费已存符文，死亡后保留。"), 14, Muted);
        Gap(Left, 8);
        Text(Left, TEXT("出发时才扣除携带物品；成功撤离会存回剩余物品，死亡不会退还。"), 14, Gold);
        Text(Right, TEXT("仓库与携带清单"), 20, FLinearColor::White);
        Text(Right, TEXT("装备每种 1 件，药水最多 5 瓶；仅携带本职业武器。"), 14, Muted);
        for (int32 Row = 0; Row < 5; ++Row)
        {
            auto* Line = WidgetTree->ConstructWidget<UHorizontalBox>(); Right->AddChild(Line);
            Text(Line, TEXT(""), 16, FLinearColor::White, FName(*FString::Printf(TEXT("Stash%d"), Row)));
            Button(Line, TEXT("−"), TEXT("PrepareMinus"), Row); Button(Line, TEXT("+"), TEXT("PreparePlus"), Row);
        }
        Gap(Right, 10);
        Text(Right, TEXT("↑↓ 选物品，←→ 调整数量；T / Y 升级天赋。"), 14, Muted);
        Text(Right, TEXT(""), 13, Gold, TEXT("ModalNotice"));
        Gap(ModalBody, 12);
        auto* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(); ModalBody->AddChild(Footer);
        Button(Footer, TEXT("关闭准备 [B]"), TEXT("Close")); Button(Footer, TEXT("携带所选物品出发 [E]"), TEXT("Deploy"));
    }
    else if (Screen == EARUIScreen::Inventory)
    {
        Text(ModalBody, TEXT("装备与背包"), 28, Gold);
        Text(ModalBody, TEXT(""), 16, Cyan, TEXT("GearBonus"));
        Button(ModalBody, TEXT(""), TEXT("Equip"), 0, TEXT("GearWeapon"));
        Button(ModalBody, TEXT(""), TEXT("Equip"), 1, TEXT("GearArmor"));
        Button(ModalBody, TEXT(""), TEXT("Equip"), 2, TEXT("GearCharm"));
        Text(ModalBody, TEXT("持有物品"), 18, FLinearColor::White);
        auto* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(); ScrollSize->SetHeightOverride(110); ModalBody->AddChild(ScrollSize);
        auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>(); ScrollSize->AddChild(Scroll);
        Text(Scroll, TEXT(""), 16, FLinearColor::White, TEXT("OwnedItems"));
        Button(ModalBody, TEXT("使用药水 [Q]  ·  恢复 30 生命"), TEXT("Potion"));
        Text(ModalBody, TEXT("背包不会暂停战斗。装备按钮可装卸持有的对应装备。"), 14, Gold);
        Button(ModalBody, TEXT("关闭 [Tab / Esc]"), TEXT("Close"));
    }
    else if (Screen == EARUIScreen::Shop)
    {
        Text(ModalBody, TEXT("地牢商人"), 28, Gold);
        Text(ModalBody, TEXT(""), 18, Cyan, TEXT("ShopGold"));
        Gap(ModalBody, 16);
        Button(ModalBody, TEXT(""), TEXT("BuyPotion"), 0, TEXT("ShopPotion"));
        Button(ModalBody, TEXT(""), TEXT("BuyPower"), 0, TEXT("ShopPower"));
        Gap(ModalBody, 16);
        Text(ModalBody, TEXT("强化仅在本局有效，花费本局金币。商店不暂停战斗，受伤或走远会关闭。"), 16, Muted);
        Button(ModalBody, TEXT("离开商店 [E / Esc]"), TEXT("Close"));
    }
    else if (Screen == EARUIScreen::Result)
    {
        Text(ModalBody, TEXT(""), 28, Gold, TEXT("ResultTitle"));
        Text(ModalBody, TEXT(""), 17, FLinearColor::White, TEXT("ResultMoney"));
        Text(ModalBody, TEXT(""), 16, Cyan, TEXT("ResultBank"));
        Text(ModalBody, TEXT(""), 16, Muted, TEXT("ResultBoss"));
        Gap(ModalBody, 12);
        auto* ScrollSize = WidgetTree->ConstructWidget<USizeBox>(); ScrollSize->SetHeightOverride(170); ModalBody->AddChild(ScrollSize);
        auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>(); ScrollSize->AddChild(Scroll);
        Text(Scroll, TEXT(""), 16, FLinearColor::White, TEXT("ResultLoot"));
        Text(ModalBody, TEXT(""), 15, Gold, TEXT("ResultHint"));
        Button(ModalBody, TEXT("返回营地 / 重试保存 [R]"), TEXT("Return"));
    }
    else if (Screen == EARUIScreen::Pause)
    {
        Text(ModalBody, TEXT("游戏已暂停"), 30, Gold);
        Text(ModalBody, TEXT("战斗、弹体与技能冷却均已冻结。"), 16, Cyan);
        Gap(ModalBody, 16);
        Button(ModalBody, TEXT("继续游戏 [Esc]"), TEXT("Resume"));
        Text(ModalBody, TEXT("WASD 移动 · 鼠标观察 · 空格跳跃\n左键攻击 · F 职业技能 · Q 药水\nE 交互 / 撤离 · Tab 背包 · Z/X/C 装卸\n营地 B 准备 · 1/2 选职业 · T/Y 天赋"), 17, Muted);
        Gap(ModalBody, 18);
        Text(ModalBody, TEXT("退出时不会保存未撤离物品，已带入装备不会退还。"), 16, Gold);
        Button(ModalBody, TEXT(""), TEXT("Sound"), 0, TEXT("SoundLabel"));
        Button(ModalBody, TEXT("退出游戏"), TEXT("Quit"));
    }
    ApplyInputMode();
}
void UARGameWidget::ApplyInputMode()
{
    auto* Controller = GetOwningPlayer(); if (!Controller) return;
    Controller->bShowMouseCursor = View != EARUIScreen::Game;
    if (View == EARUIScreen::Game)
    { Controller->SetInputMode(FInputModeGameOnly()); UWidgetBlueprintLibrary::SetFocusToGameViewport(); }
    else
    {
        FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(TakeWidget()); Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Controller->SetInputMode(Mode); SetUserFocus(Controller);
    }
}
void UARGameWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{ Super::NativeTick(Geometry, DeltaSeconds); Refresh(); }

void UARGameWidget::Refresh()
{
    auto* P = Player(); auto* S = Run(); if (!bBuilt || !P || !S) return;
    if (S->Phase != EARRunPhase::Hub) bMainMenu = false;
    if (bPaused && (S->Phase == EARRunPhase::Result || S->Phase == EARRunPhase::Settling))
    { bPaused = false; UGameplayStatics::SetGamePaused(this, false); }
    const EARUIScreen Desired = S->Phase == EARRunPhase::Result || S->Phase == EARRunPhase::Settling ? EARUIScreen::Result :
        bMainMenu ? EARUIScreen::MainMenu : bPaused ? EARUIScreen::Pause : P->bCampOpen ? EARUIScreen::Camp :
        P->bShopOpen ? EARUIScreen::Shop : P->bInventoryVisible ? EARUIScreen::Inventory : EARUIScreen::Game;
    if (View != Desired) BuildScreen(Desired);
    for (const auto& Pair : Labels)
    {
        const FString Value = LabelText(Pair.Key);
        if (Pair.Value->GetText().ToString() != Value) Pair.Value->SetText(FText::FromString(Value));
    }
    HealthBar->SetPercent(P->Health->CurrentHealth / P->Health->MaxHealth); ManaBar->SetPercent(P->Mana / 100);
    const bool bShowStatus = View == EARUIScreen::Game || View == EARUIScreen::Inventory || View == EARUIScreen::Shop;
    StatusPanel->SetVisibility(bShowStatus ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
    PromptPanel->SetVisibility(bShowStatus ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
    FooterPanel->SetVisibility(View == EARUIScreen::Game ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
    BossPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (View == EARUIScreen::Game && P->CanPlay())
        for (TActorIterator<AARBoss> It(GetWorld()); It; ++It)
            if (FVector::Dist(P->GetActorLocation(), It->GetActorLocation()) < 1800)
            { BossPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible); BossBar->SetPercent(It->Health->CurrentHealth / It->Health->MaxHealth); break; }
}

FString UARGameWidget::ItemName(FName Item)
{
    if (Item.IsNone()) return TEXT("未装备");
    if (Item == TEXT("IronSword")) return TEXT("铁剑");
    if (Item == TEXT("OakStaff")) return TEXT("橡木法杖");
    if (Item == TEXT("LeatherArmor")) return TEXT("皮甲");
    if (Item == TEXT("RunicCharm")) return TEXT("符文护符");
    if (Item == TEXT("Potion")) return TEXT("治疗药水");
    if (Item == TEXT("Rune")) return TEXT("符文");
    if (Item == TEXT("Gold")) return TEXT("金币");
    return Item.ToString();
}
FString UARGameWidget::LabelText(FName Key) const
{
    auto* P = Player(); auto* S = Run(); if (!P || !S) return FString();
    const bool Mage = P->SelectedClass == EARPlayerClass::Mage;
    const int32 BankGold = S->Profile ? S->Profile->BankGold : 0;
    const int32 BankRunes = S->Profile ? S->Profile->BankRunes : 0;
    const FString K = Key.ToString();
    if (Key == TEXT("SoundLabel")) { auto* Audio = UARFeedbackSubsystem::Get(this); return Audio && Audio->bSoundEnabled ? TEXT("音效：开启  ·  点击关闭") : TEXT("音效：关闭  ·  点击开启"); }
    auto ItemList = [](const TMap<FName, int32>& Items)
    {
        TArray<FName> Keys; Items.GetKeys(Keys); Keys.Sort(FNameLexicalLess());
        FString Result;
        for (FName Item : Keys) Result += FString::Printf(TEXT("%s  × %d\n"), *ItemName(Item), Items.FindRef(Item));
        return Result.IsEmpty() ? FString(TEXT("暂无物品")) : Result;
    };
    if (Key == TEXT("HudHealth")) return FString::Printf(TEXT("生命  %.0f / %.0f"), P->Health->CurrentHealth, P->Health->MaxHealth);
    if (Key == TEXT("HudCombat")) return FString::Printf(TEXT("%s  ·  伤害加成 +%d  ·  法力 %.0f"), Mage ? TEXT("法师") : TEXT("战士"), P->TotalDamageBonus(), P->Mana);
    if (Key == TEXT("HudWallet")) return FString::Printf(TEXT("金币 %d    符文 %d    药水 %d"), S->RunGold, S->RunRunes, P->Inventory->Count(TEXT("Potion")));
    if (Key == TEXT("HudSkill")) return FString::Printf(TEXT("[F] %s  ·  %s"), Mage ? TEXT("奥术爆发") : TEXT("旋风斩"), P->SkillRemaining() > 0 ? *FString::Printf(TEXT("冷却 %.1f 秒"), P->SkillRemaining()) : TEXT("就绪"));
    if (Key == TEXT("HudNotice") || Key == TEXT("ModalNotice")) return !S->LastError.IsEmpty() ? TranslateNotice(S->LastError) : TranslateNotice(P->Notice);
    if (Key == TEXT("HudContext"))
    {
        if (S->Phase == EARRunPhase::Hub) return TEXT("[B] 营地准备  ·  [1/2] 选择职业  ·  [E] 出发");
        if (S->Phase == EARRunPhase::Extracting) return FString::Printf(TEXT("正在撤离 %.1f 秒  ·  离开传送阵或受伤会中断"), P->ExtractionRemaining());
        if (S->Phase == EARRunPhase::Result || S->Phase == EARRunPhase::Settling) return TEXT("[R] 返回营地或重试保存");
        for (TActorIterator<AARPortal> It(GetWorld()); It; ++It) if (FVector::Dist2D(P->GetActorLocation(), It->GetActorLocation()) < 250) return TEXT("[E] 撤离  ·  在蓝色传送阵内停留 3 秒");
        for (TActorIterator<AARShop> It(GetWorld()); It; ++It) if (It->CanTrade(P)) return TEXT("[E] 与商人交易  ·  使用本局金币补给");
        return TEXT("[F] 技能  ·  [Q] 药水  ·  [Tab] 背包  ·  [Esc] 暂停");
    }
    if (Key == TEXT("HudRoute"))
    {
        for (TActorIterator<AARDungeonDirector> It(GetWorld()); It; ++It)
            if (P->CanPlay() && It->RoomOrder.Num() == 3)
            {
                auto Label = [&](int32 CombatSlot) -> FString
                {
                    const FString Raw = It->SlotDisplayName(CombatSlot);
                    if (Raw == TEXT("Pillars") || Raw == TEXT("石柱厅")) return TEXT("石柱厅");
                    if (Raw == TEXT("Barricade") || Raw == TEXT("屏障厅")) return TEXT("屏障厅");
                    if (Raw == TEXT("Cross") || Raw == TEXT("交错厅")) return TEXT("交错厅");
                    return Raw.IsEmpty() ? TEXT("战斗厅") : Raw;
                };
                const FString A = Label(0), B = Label(1), C = Label(2);
                return FString::Printf(TEXT("种子 %d  /  %s → %s → 补给与撤离 → %s → 灰烬守望者"), S->Seed, *A, *B, *C);
            }
        return TEXT("WASD 移动 · 鼠标观察 · 左键攻击 · 空格跳跃   /   成功撤离才会保存本局战利品");
    }
    if (Key == TEXT("HudBoss"))
        for (TActorIterator<AARBoss> It(GetWorld()); It; ++It) return It->bCasting ? TEXT("灰烬守望者  /  重击将至，离开红圈") : TEXT("灰烬守望者");
    if (Key == TEXT("ClassWarrior")) return Mage ? TEXT("选择战士 [1]") : TEXT("✓ 战士 [1]");
    if (Key == TEXT("ClassMage")) return Mage ? TEXT("✓ 法师 [2]") : TEXT("选择法师 [2]");
    if (Key == TEXT("ClassDescription")) return FString::Printf(TEXT("%s  ·  基础生命 %.0f"), Mage ? TEXT("法师：远程法术与奥术爆发") : TEXT("战士：近战攻击与旋风斩"), P->BaseMaxHealth());
    if (Key == TEXT("CampBank")) return FString::Printf(TEXT("银行  %d 金币 / %d 符文  ·  本次准备暂未扣除仓库物品"), BankGold, BankRunes);
    if (Key == TEXT("VitalityLabel") || Key == TEXT("MightLabel"))
    {
        const bool Vitality = Key == TEXT("VitalityLabel"); const FName Talent = Vitality ? FName(TEXT("Vitality")) : FName(TEXT("Might"));
        return FString::Printf(TEXT("%s [%d/3]  ·  %s"), Vitality ? TEXT("生命天赋 [T]") : TEXT("攻击天赋 [Y]"), S->TalentRank(Talent),
            S->TalentRank(Talent) == 3 ? TEXT("已满级") : *FString::Printf(TEXT("升级 %d 符文"), S->TalentCost(Talent)));
    }
    if (Key == TEXT("TalentBonus")) return FString::Printf(TEXT("永久加成：生命 +%d / 伤害 +%d"), S->PermanentHealthBonus(), S->PermanentDamageBonus());
    if (K.StartsWith(TEXT("Stash")))
    {
        const int32 Row = FCString::Atoi(*K.Mid(5)); const FName Item = AARPlayerCharacter::CampItem(Row);
        return FString::Printf(TEXT("%s%s  库存 %d / 携带 %d"), Row == P->CampRow ? TEXT("› ") : TEXT(""), *ItemName(Item), S->Profile ? S->Profile->Stash.FindRef(Item) : 0, S->PreparedLoadout.FindRef(Item));
    }
    if (Key == TEXT("GearBonus")) return FString::Printf(TEXT("生命 %.0f/%.0f · 装备伤害 +%d / 生命 +%d"), P->Health->CurrentHealth, P->Health->MaxHealth, P->Equipment->DamageBonus(), P->Equipment->HealthBonus());
    if (Key == TEXT("GearWeapon")) return TEXT("武器 [Z]  ·  ") + ItemName(P->Equipment->Weapon);
    if (Key == TEXT("GearArmor")) return TEXT("护甲 [X]  ·  ") + ItemName(P->Equipment->Armor);
    if (Key == TEXT("GearCharm")) return TEXT("饰品 [C]  ·  ") + ItemName(P->Equipment->Charm);
    if (Key == TEXT("OwnedItems")) return ItemList(P->Inventory->Items);
    if (Key == TEXT("ShopGold")) return FString::Printf(TEXT("本局金币：%d  ·  攻击强化 +%d"), S->RunGold, P->RunPowerBonus);
    if (Key == TEXT("ShopPotion")) return FString::Printf(TEXT("[1] 治疗药水  ·  8 金币  ·  剩余 %d"), IsValid(P->ActiveShop) ? 5 - P->ActiveShop->PotionsSold : 0);
    if (Key == TEXT("ShopPower")) return FString::Printf(TEXT("[2] 攻击 +5  ·  15 金币  ·  剩余 %d"), IsValid(P->ActiveShop) ? 3 - P->ActiveShop->UpgradesSold : 0);
    if (Key == TEXT("ResultTitle")) return S->Phase == EARRunPhase::Settling ? TEXT("正在等待保存") : S->Summary.bSuccess ? TEXT("撤离成功") : TEXT("挑战失败  /  本局物品已失去");
    if (Key == TEXT("ResultMoney")) return FString::Printf(TEXT("获得 %d  ·  花费 %d  ·  带出 %d  ·  损失 %d 金币"), S->Summary.EarnedGold, S->Summary.SpentGold, S->Summary.BankedGold, S->Summary.LostGold);
    if (Key == TEXT("ResultBank")) return FString::Printf(TEXT("银行：%d 金币 / %d 符文  ·  本局%s符文 %d"), BankGold, BankRunes, S->Summary.bSuccess ? TEXT("带出") : TEXT("损失"), S->Summary.EarnedRunes);
    if (Key == TEXT("ResultBoss")) return S->Summary.bBossDefeated ? TEXT("灰烬守望者已击败") : TEXT("灰烬守望者未击败");
    if (Key == TEXT("ResultLoot")) return FString(S->Summary.bSuccess ? TEXT("带出物品\n") : TEXT("损失物品\n")) + ItemList(S->Summary.Loot);
    if (Key == TEXT("ResultHint")) return S->Phase == EARRunPhase::Settling ? TranslateNotice(S->LastError) : S->Summary.bSuccess ? TEXT("战利品已保存，可在营地准备下一次挑战。") : TEXT("未携带的仓库物品与永久天赋仍然保留。");
    return FString();
}

bool UARGameWidget::ExecuteAction(FName Action, int32 Payload)
{
    auto* P = Player(); auto* S = Run(); if (!P || !S) return false;
    bool Done = false;
    if (Action == TEXT("Start") && View == EARUIScreen::MainMenu && S->Profile)
    { bMainMenu = false; P->bCampOpen = true; Done = true; }
    else if ((Action == TEXT("Warrior") || Action == TEXT("Mage")) && (View == EARUIScreen::MainMenu || View == EARUIScreen::Camp))
        Done = P->SelectClass(Action == TEXT("Mage") ? EARPlayerClass::Mage : EARPlayerClass::Warrior);
    else if (Action == TEXT("Deploy") && View == EARUIScreen::Camp)
    { P->Interact(); Done = S->Phase == EARRunPhase::InDungeon; }
    else if ((Action == TEXT("PreparePlus") || Action == TEXT("PrepareMinus")) && View == EARUIScreen::Camp && Payload >= 0 && Payload < 5)
    {
        P->CampRow = Payload;
        Done = S->AdjustLoadout(AARPlayerCharacter::CampItem(Payload), Action == TEXT("PreparePlus") ? 1 : -1, P);
        if (!Done) P->ShowNotice(TEXT("无法准备：请检查库存、职业与携带上限。"));
    }
    else if ((Action == TEXT("Vitality") || Action == TEXT("Might")) && View == EARUIScreen::Camp)
    {
        Done = S->PurchaseTalent(Action, P);
        P->ShowNotice(Done ? TEXT("永久天赋已升级并保存。") : TEXT("无法升级：符文不足或天赋已满级。"));
    }
    else if (Action == TEXT("Equip") && View == EARUIScreen::Inventory && Payload >= 0 && Payload < 3)
    {
        const FName Item = Payload == 0 ? (P->SelectedClass == EARPlayerClass::Mage ? FName(TEXT("OakStaff")) : FName(TEXT("IronSword"))) : Payload == 1 ? FName(TEXT("LeatherArmor")) : FName(TEXT("RunicCharm"));
        Done = P->Equipment->ToggleEquip(Item);
        if (!Done) P->ShowNotice(TEXT("背包中没有可装备的对应物品。"));
    }
    else if (Action == TEXT("Potion") && View == EARUIScreen::Inventory)
    { const int32 Before = P->Inventory->Count(TEXT("Potion")); P->UsePotion(); Done = Before > P->Inventory->Count(TEXT("Potion")); }
    else if ((Action == TEXT("BuyPotion") || Action == TEXT("BuyPower")) && View == EARUIScreen::Shop && IsValid(P->ActiveShop))
        Done = P->ActiveShop->Purchase(P, Action == TEXT("BuyPotion") ? 1 : 2);
    else if (Action == TEXT("Close"))
    {
        if (View == EARUIScreen::Camp) { P->bCampOpen = false; Done = true; }
        if (View == EARUIScreen::Inventory) { P->bInventoryVisible = false; Done = true; }
        if (View == EARUIScreen::Shop) { P->bShopOpen = false; P->ActiveShop = nullptr; Done = true; }
    }
    else if (Action == TEXT("Return") && View == EARUIScreen::Result)
    { P->ReturnOrRetry(); Done = S->Phase != EARRunPhase::Settling; }
    else if (Action == TEXT("Resume") && View == EARUIScreen::Pause) { TogglePause(); return true; }
    else if (Action == TEXT("Sound") && (View == EARUIScreen::MainMenu || View == EARUIScreen::Pause))
    { if (auto* Audio = UARFeedbackSubsystem::Get(this)) { Audio->SetSoundEnabled(!Audio->bSoundEnabled); Done = true; } }
    else if (Action == TEXT("Quit") && (View == EARUIScreen::MainMenu || View == EARUIScreen::Pause))
    { UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false); return true; }
    Refresh();
    if (Done) if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Click, FVector::ZeroVector, true);
    return Done;
}
bool UARGameWidget::OpenMainMenu()
{
    auto* S = Run(); auto* P = Player(); if (!S || !P || S->Phase != EARRunPhase::Hub) return false;
    if (bPaused) UGameplayStatics::SetGamePaused(this, false);
    bPaused = false; bMainMenu = true; P->bCampOpen = P->bInventoryVisible = P->bShopOpen = false; P->ActiveShop = nullptr;
    Refresh(); return true;
}
void UARGameWidget::TogglePause()
{
    auto* S = Run(); if (!S || bMainMenu || (View != EARUIScreen::Game && View != EARUIScreen::Pause) || S->Phase == EARRunPhase::Result || S->Phase == EARRunPhase::Settling) return;
    bPaused = !bPaused; UGameplayStatics::SetGamePaused(this, bPaused); Refresh();
}
FReply UARGameWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FKey Key = Event.GetKey(); auto* P = Player(); if (!P) return Super::NativeOnKeyDown(Geometry, Event);
    if (Key == EKeys::Escape)
    {
        if (View == EARUIScreen::Pause) ExecuteAction(TEXT("Resume"));
        else if (View == EARUIScreen::Camp || View == EARUIScreen::Shop || View == EARUIScreen::Inventory) ExecuteAction(TEXT("Close"));
        else if (View == EARUIScreen::Game) TogglePause();
        return FReply::Handled();
    }
    if (View == EARUIScreen::MainMenu)
    {
        if (Key == EKeys::Enter) ExecuteAction(TEXT("Start"));
        if (Key == EKeys::One) ExecuteAction(TEXT("Warrior")); if (Key == EKeys::Two) ExecuteAction(TEXT("Mage"));
        return FReply::Handled();
    }
    if (View == EARUIScreen::Pause) return FReply::Handled();
    if (View == EARUIScreen::Camp)
    {
        if (Key == EKeys::B) ExecuteAction(TEXT("Close")); else if (Key == EKeys::E) ExecuteAction(TEXT("Deploy"));
        else if (Key == EKeys::One) ExecuteAction(TEXT("Warrior")); else if (Key == EKeys::Two) ExecuteAction(TEXT("Mage"));
        else if (Key == EKeys::T) ExecuteAction(TEXT("Vitality")); else if (Key == EKeys::Y) ExecuteAction(TEXT("Might"));
        else if (Key == EKeys::Up) P->CampPrevious(); else if (Key == EKeys::Down) P->CampNext();
        else if (Key == EKeys::Left) ExecuteAction(TEXT("PrepareMinus"), P->CampRow); else if (Key == EKeys::Right) ExecuteAction(TEXT("PreparePlus"), P->CampRow);
        Refresh(); return FReply::Handled();
    }
    if (View == EARUIScreen::Inventory)
    {
        if (Key == EKeys::Tab) ExecuteAction(TEXT("Close")); else if (Key == EKeys::Z) ExecuteAction(TEXT("Equip"), 0);
        else if (Key == EKeys::X) ExecuteAction(TEXT("Equip"), 1); else if (Key == EKeys::C) ExecuteAction(TEXT("Equip"), 2); else if (Key == EKeys::Q) ExecuteAction(TEXT("Potion"));
        return FReply::Handled();
    }
    if (View == EARUIScreen::Shop)
    {
        if (Key == EKeys::E) ExecuteAction(TEXT("Close")); else if (Key == EKeys::One) ExecuteAction(TEXT("BuyPotion")); else if (Key == EKeys::Two) ExecuteAction(TEXT("BuyPower"));
        return FReply::Handled();
    }
    if (View == EARUIScreen::Result && Key == EKeys::R) { ExecuteAction(TEXT("Return")); return FReply::Handled(); }
    return Super::NativeOnKeyDown(Geometry, Event);
}

int32 UARGameWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
    FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
    const int32 Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bParentEnabled) + 1;
    auto* P = Player(); if (!P || !P->CanPlay() || View != EARUIScreen::Game) return Layer;
    const float Scale = FMath::Max(.01f, UWidgetLayoutLibrary::GetViewportScale(GetWorld()));
    const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(13.f / Scale));
    for (TActorIterator<AAREnemy> It(GetWorld()); It; ++It)
    {
        if (Cast<AARBoss>(*It) || FVector::Dist(P->GetActorLocation(), It->GetActorLocation()) > 1800) continue;
        FVector2D Screen;
        if (!GetOwningPlayer()->ProjectWorldLocationToScreen(It->GetActorLocation() + FVector(0, 0, 125), Screen, true)) continue;
        Screen /= Scale;
        const FString Name = Cast<AARRangedEnemy>(*It) ? TEXT("术士") : Cast<AAREliteEnemy>(*It) ? TEXT("精英守卫") : TEXT("掠夺者");
        FSlateDrawElement::MakeText(Elements, Layer, Geometry.ToPaintGeometry(Screen + FVector2D(-40, -23) / Scale, FVector2D(180, 24) / Scale),
            FText::FromString(Name + (It->bWindingAttack ? TEXT(" · 蓄势") : TEXT(""))), Font, ESlateDrawEffect::None, It->bWindingAttack ? Gold : FLinearColor::White);
        FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Screen + FVector2D(-40, 0) / Scale, FVector2D(80, 6) / Scale), White, ESlateDrawEffect::None, FLinearColor(.12f, .03f, .03f));
        FSlateDrawElement::MakeBox(Elements, Layer + 1, Geometry.ToPaintGeometry(Screen + FVector2D(-40, 0) / Scale, FVector2D(80 * It->Health->CurrentHealth / It->Health->MaxHealth, 6) / Scale), White,
            ESlateDrawEffect::None, It->bWindingAttack ? Gold : FLinearColor(.78f, .12f, .1f));
    }
    return Layer + 1;
}

FString UARGameWidget::TranslateNotice(const FString& Message)
{
    static const TMap<FString, FString> Messages = {
        {TEXT("Hit!"), TEXT("命中！")},
        {TEXT("Not enough mana"), TEXT("法力不足。")},
        {TEXT("Potion: restored health"), TEXT("已使用药水，恢复生命。")},
        {TEXT("Return to the camp to enter the dungeon"), TEXT("请回到营地附近再出发。")},
        {TEXT("Extracting: stay inside the portal for 3 seconds"), TEXT("正在撤离：请在传送阵内停留 3 秒。")},
        {TEXT("Press E near the trader or inside the blue extraction portal"), TEXT("在商人旁或蓝色撤离阵内按 E。")},
        {TEXT("Extraction cancelled"), TEXT("撤离已中断。")},
        {TEXT("Taking damage!"), TEXT("正在受到攻击！")},
        {TEXT("Dungeon started. Defeat enemies or reach the blue extraction portal."), TEXT("远征开始：击败敌人，并及时前往蓝色传送阵撤离。")},
        {TEXT("Mage selected: ranged bolts, mana and Arcane Burst [F]"), TEXT("已选择法师：远程法术与奥术爆发 [F]。")},
        {TEXT("Warrior selected: sword and Whirlwind [F]"), TEXT("已选择战士：近战攻击与旋风斩 [F]。")},
        {TEXT("Whirlwind!"), TEXT("旋风斩！")},
        {TEXT("Arcane impact"), TEXT("奥术命中！")},
        {TEXT("WARDEN SLAM - leave the red circle!"), TEXT("守望者即将重击，请离开红色预警圈！")},
        {TEXT("Purchase complete"), TEXT("购买成功。")},
        {TEXT("Cannot buy: check gold, stock or inventory space"), TEXT("无法购买：请检查金币、商店库存和背包空间。")},
        {TEXT("Equipment updated"), TEXT("装备已更新。")},
        {TEXT("Collect your class weapon, then press Z"), TEXT("拾取对应职业武器后按 Z 装备。")},
        {TEXT("Collect LeatherArmor, then press X"), TEXT("拾取皮甲后按 X 装备。")},
        {TEXT("Collect RunicCharm, then press C"), TEXT("拾取符文护符后按 C 装备。")},
        {TEXT("HEXER AIMING - move sideways to dodge the bolt"), TEXT("术士正在瞄准，横向移动躲避弹体！")},
        {TEXT("Return to camp to manage talents and supplies"), TEXT("请回到营地管理天赋与补给。")},
        {TEXT("Cannot prepare: check stash, class or carry limit"), TEXT("无法准备：请检查库存、职业和携带上限。")},
        {TEXT("Vitality permanently upgraded"), TEXT("永久生命天赋已升级。")},
        {TEXT("Might permanently upgraded"), TEXT("永久攻击天赋已升级。")},
        {TEXT("Cannot upgrade: check banked runes or rank cap"), TEXT("无法升级：已存符文不足或天赋已满级。")}
    };
    if (const FString* Localized = Messages.Find(Message)) return *Localized;
    if (Message.StartsWith(TEXT("Collected ")))
    {
        TArray<FString> Parts; Message.ParseIntoArrayWS(Parts);
        if (Parts.Num() == 3) return FString::Printf(TEXT("获得 %s × %s"), *ItemName(FName(*Parts[2])), *Parts[1]);
    }
    if (Message.Contains(TEXT("Profile")) || Message.Contains(TEXT("Invalid stash or talents"))) return TEXT("存档读取失败或数据不兼容，原存档未被覆盖。");
    if (Message.Contains(TEXT("overflow"))) return TEXT("存档数值超出范围，结算已锁定。");
    if (Message.Contains(TEXT("save")) || Message.Contains(TEXT("Save"))) return TEXT("保存未完成。请重试当前操作；尚未提交的扣除不会生效。");
    if (Message.StartsWith(TEXT("Cannot deploy"))) return TEXT("无法出发：请检查携带物品、职业、空背包与容量。");
    if (Message.Contains(TEXT("Invalid stash quantity"))) return TEXT("仓库数量异常，结算已锁定。");
    return Message;
}
