#include "ARPrototype.h"
#include "ARProgression.h"
#include "AREnemyVariants.h"
#include "ARDungeon.h"
#include "AREquipmentComponent.h"
#include "ARHealthComponent.h"
#include "ARInventoryComponent.h"
#include "ARRunGameInstance.h"
#include "ARProfile.h"
#include "ARGameWidget.h"
#include "ARCombatFeedback.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimInstance.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "InputCoreTypes.h"

namespace
{
    UARRunGameInstance* Run(const UObject* Object)
    { return Cast<UARRunGameInstance>(UGameplayStatics::GetGameInstance(Object)); }
    void SetupMannequin(USkeletalMeshComponent* Mesh)
    {
        static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
        static ConstructorHelpers::FClassFinder<UAnimInstance> Animation(TEXT("/Game/Mannequin/Animations/ThirdPerson_AnimBP"));
        if (Body.Succeeded()) Mesh->SetSkeletalMesh(Body.Object);
        if (Animation.Succeeded()) Mesh->SetAnimInstanceClass(Animation.Class);
        Mesh->SetRelativeLocation(FVector(0, 0, -96));
        Mesh->SetRelativeRotation(FRotator(0, -90, 0));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

AARPlayerCharacter::AARPlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42, 96);
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 720, 0);
    GetCharacterMovement()->MaxWalkSpeed = 520;
    GetCharacterMovement()->JumpZVelocity = 500;
    SetupMannequin(GetMesh());
    Health = CreateDefaultSubobject<UARHealthComponent>(TEXT("Health"));
    Inventory = CreateDefaultSubobject<UARInventoryComponent>(TEXT("Inventory"));
    Equipment = CreateDefaultSubobject<UAREquipmentComponent>(TEXT("Equipment"));
    Feedback = CreateDefaultSubobject<UARCombatFeedbackComponent>(TEXT("CombatFeedback"));
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 440;
    CameraBoom->SocketOffset = FVector(0, 45, 75);
    CameraBoom->bUsePawnControlRotation = true;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom);
    Sword = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sword"));
    Sword->SetupAttachment(GetMesh(), TEXT("hand_r"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Sword->SetStaticMesh(Cube.Object);
    Sword->SetRelativeScale3D(FVector(.07f, .07f, .85f));
    Sword->SetRelativeLocation(FVector(0, 0, 35));
    Sword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AARPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    Health->OnDied.AddDynamic(this, &AARPlayerCharacter::Died);
    Health->OnDamaged.AddDynamic(this, &AARPlayerCharacter::Damaged);
    ResetCombatForRun();
    if (Controller) Controller->SetControlRotation(FRotator(-15, 0, 0));
}
void AARPlayerCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("MoveForward", this, &AARPlayerCharacter::MoveForward);
    Input->BindAxis("MoveRight", this, &AARPlayerCharacter::MoveRight);
    Input->BindAxis("Turn", this, &AARPlayerCharacter::LookYaw);
    Input->BindAxis("LookUp", this, &AARPlayerCharacter::LookPitch);
    Input->BindAction("Jump", IE_Pressed, this, &AARPlayerCharacter::SafeJump);
    Input->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
    Input->BindAction("Attack", IE_Pressed, this, &AARPlayerCharacter::Attack);
    Input->BindAction("Interact", IE_Pressed, this, &AARPlayerCharacter::Interact);
    Input->BindAction("Potion", IE_Pressed, this, &AARPlayerCharacter::UsePotion);
    Input->BindAction("Return", IE_Pressed, this, &AARPlayerCharacter::ReturnOrRetry);
    Input->BindAction("Inventory", IE_Pressed, this, &AARPlayerCharacter::ToggleInventory);
    Input->BindAction("Skill", IE_Pressed, this, &AARPlayerCharacter::UseSkill);
    Input->BindAction("OptionOne", IE_Pressed, this, &AARPlayerCharacter::OptionOne);
    Input->BindAction("OptionTwo", IE_Pressed, this, &AARPlayerCharacter::OptionTwo);
    Input->BindAction("EquipWeapon", IE_Pressed, this, &AARPlayerCharacter::EquipWeapon);
    Input->BindAction("EquipArmor", IE_Pressed, this, &AARPlayerCharacter::EquipArmor);
    Input->BindAction("EquipCharm", IE_Pressed, this, &AARPlayerCharacter::EquipCharm);
    Input->BindAction("Camp", IE_Pressed, this, &AARPlayerCharacter::ToggleCamp);
    Input->BindAction("CampPrevious", IE_Pressed, this, &AARPlayerCharacter::CampPrevious);
    Input->BindAction("CampNext", IE_Pressed, this, &AARPlayerCharacter::CampNext);
    Input->BindAction("CampAdd", IE_Pressed, this, &AARPlayerCharacter::CampAdd);
    Input->BindAction("CampRemove", IE_Pressed, this, &AARPlayerCharacter::CampRemove);
    Input->BindAction("Vitality", IE_Pressed, this, &AARPlayerCharacter::BuyVitality);
    Input->BindAction("Might", IE_Pressed, this, &AARPlayerCharacter::BuyMight);
    Input->BindAction("Menu", IE_Pressed, this, &AARPlayerCharacter::TogglePauseMenu).bExecuteWhenPaused = true;
}
bool AARPlayerCharacter::CanPlay() const
{
    const auto* State = Run(this);
    return State && !Health->bDead && (State->Phase == EARRunPhase::InDungeon || State->Phase == EARRunPhase::Extracting);
}
void AARPlayerCharacter::MoveForward(float Value)
{
    const auto* State = Run(this);
    if (Controller && !IsUIBlockingInput() && !Health->bDead && State && State->Phase != EARRunPhase::Result && State->Phase != EARRunPhase::Settling)
        AddMovementInput(FRotationMatrix(FRotator(0, Controller->GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X), Value);
}
void AARPlayerCharacter::MoveRight(float Value)
{
    const auto* State = Run(this);
    if (Controller && !IsUIBlockingInput() && !Health->bDead && State && State->Phase != EARRunPhase::Result && State->Phase != EARRunPhase::Settling)
        AddMovementInput(FRotationMatrix(FRotator(0, Controller->GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), Value);
}
void AARPlayerCharacter::SafeJump()
{ const auto* State = Run(this); if (State && !IsUIBlockingInput() && State->Phase != EARRunPhase::Result && State->Phase != EARRunPhase::Settling && !Health->bDead) Jump(); }
void AARPlayerCharacter::ShowNotice(const FString& Text)
{ Notice = Text; NoticeUntil = GetWorld()->GetTimeSeconds() + 2.5f; }
void AARPlayerCharacter::ToggleInventory() { bInventoryVisible = !bInventoryVisible; if (bInventoryVisible) bCampOpen = false; }
void AARPlayerCharacter::Attack()
{
    auto* State = Run(this);
    if (!CanPlay() || IsUIBlockingInput() || bShopOpen || State->Phase != EARRunPhase::InDungeon || GetWorld()->GetTimeSeconds() < NextAttackTime) return;
    if (SelectedClass == EARPlayerClass::Mage && Mana < 8) { ShowNotice(TEXT("Not enough mana")); return; }
    NextAttackTime = GetWorld()->GetTimeSeconds() + FMath::Max(.1f, AttackCooldown);
    if (SelectedClass == EARPlayerClass::Mage) { FireSpell(false); return; }
    Feedback->PlayAction(EARCombatAction::Slash);
    if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Swing, GetActorLocation());
    if (Controller) SetActorRotation(FRotator(0, Controller->GetControlRotation().Yaw, 0));
    const FVector Origin = GetActorLocation();
    for (TActorIterator<AAREnemy> It(GetWorld()); It; ++It)
    {
        AAREnemy* Enemy = *It;
        const FVector Offset = Enemy->GetActorLocation() - Origin;
        if (Enemy->Health->bDead || Offset.Size() > 215 || FVector::DotProduct(Offset.GetSafeNormal2D(), GetActorForwardVector()) < .25f) continue;
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ARAttack), false, this);
        const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Enemy->GetActorLocation(), ECC_Visibility, Query);
        if (!bBlocked || Hit.GetActor() == Enemy)
        {
            UGameplayStatics::ApplyDamage(Enemy, AttackDamage + TotalDamageBonus(), GetController(), this, nullptr);
            ShowNotice(TEXT("Hit!"));
        }
    }
}
void AARPlayerCharacter::UsePotion()
{
    if (!CanPlay() || UGameplayStatics::IsGamePaused(this) || Inventory->Count(TEXT("Potion")) <= 0) return;
    if (Health->Heal(30) > 0)
    {
        Inventory->TryRemove(TEXT("Potion"), 1);
        if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Heal, GetActorLocation());
        ShowNotice(TEXT("Potion: restored health"));
    }
}
void AARPlayerCharacter::Interact()
{
    auto* State = Run(this);
    if (!State) return;
    auto* PC = Cast<APlayerController>(Controller);
    auto* HUD = PC ? Cast<AARPrototypeHUD>(PC->GetHUD()) : nullptr;
    if (HUD && HUD->GameUI && HUD->GameUI->IsMenuOpen()) return;
    if (bShopOpen) { bShopOpen = false; ActiveShop = nullptr; return; }
    if (State->Phase == EARRunPhase::Hub)
    {
        if (FVector::Dist2D(GetActorLocation(), FVector(-700, 0, 0)) > 650)
        { ShowNotice(TEXT("Return to the camp to enter the dungeon")); return; }
        if (auto* Mode = Cast<AARPrototypeGameMode>(GetWorld()->GetAuthGameMode())) Mode->StartDungeon(this);
        return;
    }
    if (!CanPlay() || State->Phase != EARRunPhase::InDungeon) return;
    for (TActorIterator<AARShop> It(GetWorld()); It; ++It)
    {
        if (It->CanTrade(this)) { ActiveShop = *It; bShopOpen = true; bInventoryVisible = false; return; }
    }
    for (TActorIterator<AARPortal> It(GetWorld()); It; ++It)
    {
        if (FVector::Dist2D(GetActorLocation(), It->GetActorLocation()) <= 250 && State->BeginExtraction())
        {
            ExtractLocation = It->GetActorLocation();
            ExtractUntil = GetWorld()->GetTimeSeconds() + 3.f;
            ShowNotice(TEXT("Extracting: stay inside the portal for 3 seconds"));
            return;
        }
    }
    ShowNotice(TEXT("Press E near the trader or inside the blue extraction portal"));
}
float AARPlayerCharacter::ExtractionRemaining() const
{ return FMath::Max(0.f, ExtractUntil - GetWorld()->GetTimeSeconds()); }
void AARPlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetWorld()->GetTimeSeconds() > NoticeUntil) Notice.Empty();
    auto* State = Run(this);
    if (CanPlay() && SelectedClass == EARPlayerClass::Mage) Mana = FMath::Min(100.f, Mana + DeltaSeconds * 12.f);
    if (bCampOpen && !IsAtCamp()) bCampOpen = false;
    if (bShopOpen && (!IsValid(ActiveShop) || !ActiveShop->CanTrade(this))) { bShopOpen = false; ActiveShop = nullptr; }
    if (State && State->Phase == EARRunPhase::Extracting)
    {
        if (Health->bDead || FVector::Dist2D(GetActorLocation(), ExtractLocation) > 250)
        { State->CancelExtraction(); ExtractUntil = 0; ShowNotice(TEXT("Extraction cancelled")); }
        else if (ExtractionRemaining() <= 0) State->SettleRun(true, Inventory);
    }
    if (State && CanPlay() && GetActorLocation().Z < -500) Health->ReceiveDamage(Health->MaxHealth);
}
void AARPlayerCharacter::Damaged()
{
    bShopOpen = false;
    ActiveShop = nullptr;
    if (auto* State = Run(this)) State->CancelExtraction();
    ExtractUntil = 0;
    ShowNotice(TEXT("Taking damage!"));
}
void AARPlayerCharacter::Died()
{
    bShopOpen = false;
    ActiveShop = nullptr;
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    if (auto* State = Run(this)) State->SettleRun(false, Inventory);
}
void AARPlayerCharacter::ReturnOrRetry()
{
    auto* State = Run(this);
    if (!State) return;
    if (State->Phase == EARRunPhase::Settling) { State->RetrySave(); return; }
    if (!State->ReturnToHub()) return;
    if (auto* Mode = Cast<AARPrototypeGameMode>(GetWorld()->GetAuthGameMode())) Mode->ClearRunActors();
    for (TActorIterator<AARDungeonDirector> It(GetWorld()); It; ++It)
    {
        It->EnsureCampLayout();
        break;
    }
    ResetForHub();
}
void AARPlayerCharacter::ResetForHub()
{
    Inventory->Clear();
    ResetCombatForRun();
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->StopMovementImmediately();
    SetActorLocationAndRotation(FVector(-700, 0, 100), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
    if (Controller) Controller->SetControlRotation(FRotator(-15, 0, 0));
    NextAttackTime = ExtractUntil = 0;
    bInventoryVisible = false;
    bCampOpen = false;
}

AAREnemy::AAREnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42, 96);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    SetupMannequin(GetMesh());
    Health = CreateDefaultSubobject<UARHealthComponent>(TEXT("Health"));
    Health->MaxHealth = 60;
    Feedback = CreateDefaultSubobject<UARCombatFeedbackComponent>(TEXT("CombatFeedback"));
    GetCharacterMovement()->MaxWalkSpeed = 180;
    // Greybox pursuit uses character collision, without requiring a baked NavMesh.
    GetCharacterMovement()->bRunPhysicsWithNoController = true;
}
void AAREnemy::BeginPlay()
{
    Super::BeginPlay();
    Health->OnDied.AddDynamic(this, &AAREnemy::Died);
    if (auto* Mode = Cast<AARPrototypeGameMode>(GetWorld()->GetAuthGameMode())) bNavigationEnabled = Mode->bUseNavigation;
}
bool AAREnemy::ClearSight(AActor* Target) const
{
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AREnemySight), false, this);
    return !GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query) || Hit.GetActor() == Target;
}
void AAREnemy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* State = Run(this);
    auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!State || !Player || Health->bDead || !Player->CanPlay())
    { bHasTarget = bWindingAttack = false; GetCharacterMovement()->StopMovementImmediately(); return; }
    const float Time = GetWorld()->GetTimeSeconds();
    const FVector Offset = Player->GetActorLocation() - GetActorLocation();
    const float Distance = Offset.Size();
    if (bWindingAttack)
    {
        if (Time >= HitAt)
        {
            bWindingAttack = false;
            if (Distance <= MeleeReach && FVector::DotProduct(Offset.GetSafeNormal2D(), GetActorForwardVector()) >= AttackFacingDot && ClearSight(Player))
                UGameplayStatics::ApplyDamage(Player, Damage, GetController(), this, nullptr);
        }
        return;
    }
    if (!CanPursue(Player)) return;
    SetActorRotation(Offset.Rotation());
    SetActorRotation(FRotator(0, GetActorRotation().Yaw, 0));
    if (Distance > StopRange || !ClearSight(Player)) Pursue(Player);
    else
    {
        GetCharacterMovement()->StopMovementImmediately();
        if (Time >= NextAttackAt)
        {
            bWindingAttack = true; HitAt = Time + FMath::Max(.1f, AttackWindup); NextAttackAt = Time + FMath::Max(.5f, AttackInterval);
            Feedback->PlayAction(AttackWindup > .6f ? EARCombatAction::Heavy : EARCombatAction::Slash, AttackWindup);
            if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Swing, GetActorLocation());
        }
    }
}
void AAREnemy::Died()
{
    SetActorEnableCollision(false);
    GetCharacterMovement()->StopMovementImmediately();
    auto* Loot = GetWorld()->SpawnActor<AARLoot>(AARLoot::StaticClass(), GetActorLocation() - FVector(0, 0, 40), FRotator::ZeroRotator);
    if (Loot) { Loot->ItemID = TEXT("Gold"); Loot->Quantity = GoldReward; }
    if (RuneReward > 0)
    {
        const FTransform Transform(GetActorLocation() + FVector(-65, 0, -40));
        auto* Rune = GetWorld()->SpawnActorDeferred<AARLoot>(AARLoot::StaticClass(), Transform);
        if (Rune) { Rune->ItemID = TEXT("Rune"); Rune->Quantity = RuneReward; UGameplayStatics::FinishSpawningActor(Rune, Transform); }
    }
    if (!DropItemID.IsNone())
    {
        const FTransform Transform(GetActorLocation() + FVector(65, 0, -40));
        auto* Gear = GetWorld()->SpawnActorDeferred<AARLoot>(AARLoot::StaticClass(), Transform);
        if (Gear) { Gear->ItemID = DropItemID; Gear->Quantity = 1; UGameplayStatics::FinishSpawningActor(Gear, Transform); }
    }
    Destroy();
}

AARLoot::AARLoot()
{
    PrimaryActorTick.bCanEverTick = true;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    // Sphere is available in every UE4 installation; no external art dependency.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Mesh->SetStaticMesh(Sphere.Object);
    Mesh->SetRelativeScale3D(FVector(.28f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AARLoot::BeginPlay()
{
    Super::BeginPlay();
    const TCHAR* Path = ItemID == TEXT("Gold") ? TEXT("/Game/AshenRift/Materials/M_Gold.M_Gold") : ItemID == TEXT("Rune") ? TEXT("/Game/AshenRift/Materials/M_Rune.M_Rune") : TEXT("/Game/AshenRift/Materials/M_Potion.M_Potion");
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, Path)) Mesh->SetMaterial(0, Material);
    if (UAREquipmentComponent::IsEquipment(ItemID))
    {
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetRelativeScale3D(FVector(.4f));
        if (auto* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_Rune.M_Rune"))) Mesh->SetMaterial(0, Mat);
    }
}
bool AARLoot::TryCollect(AARPlayerCharacter* Player)
{
    if (IsActorBeingDestroyed() || !Player || !Player->CanPlay()) return false;
    auto* State = Run(this);
    if (!State || State->Phase != EARRunPhase::InDungeon) return false;
    const bool bAdded = ItemID == TEXT("Gold") ? State->AddCurrency(Quantity, 0) : ItemID == TEXT("Rune") ? State->AddCurrency(0, Quantity) : Player->Inventory->TryAdd(ItemID, Quantity);
    if (!bAdded) return false;
    if (auto* Audio = UARFeedbackSubsystem::Get(this)) Audio->PlayCue(EARFeedbackCue::Pickup, Player->GetActorLocation());
    Player->ShowNotice(FString::Printf(TEXT("Collected %d %s"), Quantity, *ItemID.ToString()));
    SetActorTickEnabled(false);
    Destroy();
    return true;
}
void AARLoot::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AddActorLocalRotation(FRotator(0, 90 * DeltaSeconds, 0));
    auto* Player = Cast<AARPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < 120) TryCollect(Player);
}
AARPortal::AARPortal()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Mesh->SetStaticMesh(Shape.Object);
    Mesh->SetRelativeScale3D(FVector(5, 5, .08f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

AARPrototypeGameMode::AARPrototypeGameMode()
{
    DefaultPawnClass = AARPlayerCharacter::StaticClass();
    HUDClass = AARPrototypeHUD::StaticClass();
    EnemyClass = AAREnemy::StaticClass();
    RangedEnemyClass = AARRangedEnemy::StaticClass();
    EliteEnemyClass = AAREliteEnemy::StaticClass();
}
void AARPrototypeGameMode::ClearRunActors()
{
    for (TActorIterator<AAREnemy> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AARLoot> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AARSpell> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AARBlastZone> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AAREnemyBolt> It(GetWorld()); It; ++It) It->Destroy();
    for (TActorIterator<AARImpactEffect> It(GetWorld()); It; ++It) It->Destroy();
}
bool AARPrototypeGameMode::StartDungeon(AARPlayerCharacter* Player)
{
    auto* State = Run(this);
    AARDungeonDirector* Director = nullptr;
    for (TActorIterator<AARDungeonDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
    const int32 Seed = Director && Director->bUseFixedSeed ? Director->FixedSeed : FMath::Rand();
    if (!Player || !State || !Player->IsAtCamp() || !State->BeginRun(Seed, Player->Inventory)) return false;
    ClearRunActors();
    if (Director) Director->Generate(Seed);
    Player->ResetCombatForRun();
    Player->bCampOpen = Player->bInventoryVisible = false;
    const FName Weapon = Player->SelectedClass == EARPlayerClass::Mage ? FName(TEXT("OakStaff")) : FName(TEXT("IronSword"));
    for (FName Item : { Weapon, FName(TEXT("LeatherArmor")), FName(TEXT("RunicCharm")) })
        if (Player->Inventory->Count(Item) > 0) Player->Equipment->ToggleEquip(Item);
    // A prepared armor starts the run at full health; toggling armor during combat never heals.
    Player->Health->ResetHealth();
    for (TActorIterator<AARShop> It(GetWorld()); It; ++It) It->ResetStock();
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Player->GetCharacterMovement()->StopMovementImmediately();
    Player->SetActorLocationAndRotation(FVector(1450, 0, 100), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
    if (Player->Controller) Player->Controller->SetControlRotation(FRotator(-15, 0, 0));
    if (Director) Director->Populate(Player, EnemyClass, RangedEnemyClass, EliteEnemyClass, BossClass);
    else
    {
        for (const FVector& Position : { FVector(2400, 0, 100), FVector(3100, 250, 100), FVector(4200, 0, 100) })
            GetWorld()->SpawnActor<AAREnemy>(EnemyClass, Position, FRotator(0, 180, 0));
        if (BossClass) GetWorld()->SpawnActor<AARBoss>(BossClass, FVector(7400, 0, 150), FRotator(0, 180, 0));
    }
    const FTransform PotionTransform(FVector(1650, 0, 55));
    auto* Potion = GetWorld()->SpawnActorDeferred<AARLoot>(AARLoot::StaticClass(), PotionTransform);
    if (Potion)
    {
        Potion->ItemID = TEXT("Potion"); Potion->Quantity = 2;
        UGameplayStatics::FinishSpawningActor(Potion, PotionTransform);
    }
    Player->ShowNotice(TEXT("Dungeon started. Defeat enemies or reach the blue extraction portal."));
    return true;
}

