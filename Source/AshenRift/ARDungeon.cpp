#include "ARDungeon.h"
#include "ARPrototype.h"
#include "ARProgression.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "NavigationSystem.h"
#if WITH_EDITOR
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "ObjectTools.h"
#include "Builders/CubeBuilder.h"
#include "Model.h"
#include "Components/BrushComponent.h"
#endif

AARDungeonRoom::AARDungeonRoom()
{
    RoomRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = RoomRoot;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Pillar(TEXT("/Game/AshenRift/KayKit/Architecture/Columns/pillar.pillar"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Barrier(TEXT("/Game/AshenRift/KayKit/Architecture/Barriers/barrier.barrier"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wall(TEXT("/Game/AshenRift/Materials/M_Wall.M_Wall"));
    Cube = Shape.Succeeded() ? Shape.Object : nullptr;
    PillarMesh = Pillar.Succeeded() ? Pillar.Object : nullptr;
    BarrierMesh = Barrier.Succeeded() ? Barrier.Object : nullptr;
    WallMaterial = Wall.Succeeded() ? Wall.Object : nullptr;
}

UStaticMeshComponent* AARDungeonRoom::CreateBlock(FName Name, const FVector& RelativeLocation, const FVector& SizeUU)
{
    auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->bEditableWhenInherited = true;
    if (Cube) Mesh->SetStaticMesh(Cube);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetCanEverAffectNavigation(true);
    Mesh->SetRelativeLocation(RelativeLocation);
    Mesh->SetRelativeScale3D(SizeUU / 100.f);
    if (WallMaterial) Mesh->SetMaterial(0, WallMaterial);
    return Mesh;
}

UStaticMeshComponent* AARDungeonRoom::CreateProp(FName Name, UStaticMesh* MeshAsset, const FVector& RelativeLocation, const FRotator& RelativeRotation, const FVector& Scale)
{
    auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->bEditableWhenInherited = true;
    if (MeshAsset) Mesh->SetStaticMesh(MeshAsset);
    else if (Cube) Mesh->SetStaticMesh(Cube);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetCanEverAffectNavigation(true);
    Mesh->SetRelativeLocation(RelativeLocation);
    Mesh->SetRelativeRotation(RelativeRotation);
    Mesh->SetRelativeScale3D(Scale);
    return Mesh;
}

void AARDungeonRoom::Block(const FVector& Position, const FVector& Size)
{
    auto* Mesh = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetMobility(EComponentMobility::Movable);
    if (Cube) Mesh->SetStaticMesh(Cube);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetCanEverAffectNavigation(true);
    Mesh->SetRelativeLocation(Position);
    Mesh->SetRelativeScale3D(Size / 100);
    Mesh->RegisterComponent();
    if (WallMaterial) Mesh->SetMaterial(0, WallMaterial);
    else if (auto* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AshenRift/Materials/M_Wall.M_Wall"))) Mesh->SetMaterial(0, Mat);
}

void AARDungeonRoom::BuildInterior(int32 Kind)
{
    RoomKind = Kind;
    if (Kind == 0)
        for (float X : {-450.f, 450.f}) for (float Y : {-500.f, 500.f}) Block(FVector(X, Y, 120), FVector(200, 200, 240));
    else if (Kind == 1)
        Block(FVector(0, 0, 120), FVector(200, 1000, 240));
    else
    {
        Block(FVector(-450, -350, 120), FVector(200, 800, 240));
        Block(FVector(450, 350, 120), FVector(200, 800, 240));
    }
}

AARDungeonRoom_Pillars::AARDungeonRoom_Pillars()
{
    RoomKind = 0;
    DisplayName = TEXT("Pillars");
    const bool bKay = PillarMesh != nullptr;
    const float Z = bKay ? 0.f : 120.f;
    const FVector S = bKay ? FVector(1.f) : FVector(2.f, 2.f, 2.4f);
    Pillar_NW = CreateProp(TEXT("Pillar_NW"), PillarMesh, FVector(-450.f, -500.f, Z), FRotator::ZeroRotator, S);
    Pillar_NE = CreateProp(TEXT("Pillar_NE"), PillarMesh, FVector(-450.f,  500.f, Z), FRotator::ZeroRotator, S);
    Pillar_SW = CreateProp(TEXT("Pillar_SW"), PillarMesh, FVector( 450.f, -500.f, Z), FRotator::ZeroRotator, S);
    Pillar_SE = CreateProp(TEXT("Pillar_SE"), PillarMesh, FVector( 450.f,  500.f, Z), FRotator::ZeroRotator, S);
}

AARDungeonRoom_Barricade::AARDungeonRoom_Barricade()
{
    RoomKind = 1;
    DisplayName = TEXT("Barricade");
    const bool bKay = BarrierMesh != nullptr;
    const float Z = bKay ? 0.f : 120.f;
    const FRotator Yaw90(0.f, 90.f, 0.f);
    const FVector S = bKay ? FVector(1.f) : FVector(2.f, 4.f, 2.4f);
    Barricade_A = CreateProp(TEXT("Barricade_A"), BarrierMesh, FVector(0.f, -400.f, Z), Yaw90, S);
    Barricade_B = CreateProp(TEXT("Barricade_B"), BarrierMesh, FVector(0.f,    0.f, Z), Yaw90, S);
    Barricade_C = CreateProp(TEXT("Barricade_C"), BarrierMesh, FVector(0.f,  400.f, Z), Yaw90, S);
}

AARDungeonRoom_Cross::AARDungeonRoom_Cross()
{
    RoomKind = 2;
    DisplayName = TEXT("Cross");
    const bool bKay = BarrierMesh != nullptr;
    const float Z = bKay ? 0.f : 120.f;
    const FRotator Yaw90(0.f, 90.f, 0.f);
    const FVector S = bKay ? FVector(1.f) : FVector(2.f, 4.f, 2.4f);
    Cross_A1 = CreateProp(TEXT("Cross_A1"), BarrierMesh, FVector(-450.f, -550.f, Z), Yaw90, S);
    Cross_A2 = CreateProp(TEXT("Cross_A2"), BarrierMesh, FVector(-450.f, -150.f, Z), Yaw90, S);
    Cross_B1 = CreateProp(TEXT("Cross_B1"), BarrierMesh, FVector( 450.f,  150.f, Z), Yaw90, S);
    Cross_B2 = CreateProp(TEXT("Cross_B2"), BarrierMesh, FVector( 450.f,  550.f, Z), Yaw90, S);
}

AARDungeonDirector::AARDungeonDirector()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RoomLayouts.Reset();
    RoomLayouts.Add(AARDungeonRoom_Pillars::StaticClass());
    RoomLayouts.Add(AARDungeonRoom_Barricade::StaticClass());
    RoomLayouts.Add(AARDungeonRoom_Cross::StaticClass());
    static ConstructorHelpers::FClassFinder<AActor> CampBP(TEXT("/Game/AshenRift/Rooms/BP_Camp.BP_Camp_C"));
    if (CampBP.Succeeded()) CampLayout = CampBP.Class;
    CampOrigin = FVector(0.f, 0.f, 0.f);
}

TArray<int32> AARDungeonDirector::MakeRoomOrder(int32 Seed)
{
    return MakeLayoutOrder(Seed, 3, 3);
}

TArray<int32> AARDungeonDirector::MakeLayoutOrder(int32 Seed, int32 PoolSize, int32 PickCount)
{
    TArray<int32> Result;
    if (PoolSize <= 0) return Result;
    for (int32 I = 0; I < PoolSize; ++I) Result.Add(I);
    FRandomStream Stream(Seed);
    for (int32 I = Result.Num() - 1; I > 0; --I) Result.Swap(I, Stream.RandRange(0, I));
    const int32 Keep = FMath::Clamp(PickCount, 0, Result.Num());
    Result.SetNum(Keep);
    return Result;
}

FString AARDungeonDirector::RoomName(int32 Kind)
{
    return Kind == 0 ? TEXT("Pillars") : Kind == 1 ? TEXT("Barricade") : Kind == 2 ? TEXT("Cross") : FString::Printf(TEXT("Layout%d"), Kind);
}

FString AARDungeonDirector::SlotDisplayName(int32 SlotIndex) const
{
    if (Rooms.IsValidIndex(SlotIndex) && IsValid(Rooms[SlotIndex]))
    {
        if (const AARDungeonRoom* Typed = Cast<AARDungeonRoom>(Rooms[SlotIndex]))
        {
            if (!Typed->DisplayName.IsEmpty()) return Typed->DisplayName;
        }
        const FString Label = Rooms[SlotIndex]->GetActorLabel();
        if (!Label.IsEmpty() && Label != Rooms[SlotIndex]->GetClass()->GetName()) return Label;
    }
    if (RoomOrder.IsValidIndex(SlotIndex))
        return RoomName(RoomOrder[SlotIndex]);
    return TEXT("Room");
}

void AARDungeonDirector::BeginPlay()
{
    Super::BeginPlay();
    EnsureCampLayout();
}

AActor* AARDungeonDirector::EnsureCampLayout()
{
    if (!GetWorld()) return nullptr;
    if (!CampLayout)
    {
        // Late load if constructor soft-ref missed (BP created after module load).
        if (UClass* Loaded = LoadClass<AActor>(nullptr, TEXT("/Game/AshenRift/Rooms/BP_Camp.BP_Camp_C")))
            CampLayout = Loaded;
    }
    if (!CampLayout) return SpawnedCamp;

    if (IsValid(SpawnedCamp) && SpawnedCamp->GetClass() == *CampLayout)
    {
        SpawnedCamp->SetActorLocationAndRotation(CampOrigin, FRotator::ZeroRotator);
        return SpawnedCamp;
    }
    if (IsValid(SpawnedCamp))
    {
        SpawnedCamp->Destroy();
        SpawnedCamp = nullptr;
    }

    SpawnedCamp = GetWorld()->SpawnActor<AActor>(CampLayout, CampOrigin, FRotator::ZeroRotator);
    if (SpawnedCamp)
    {
        SpawnedCamp->SetActorLabel(TEXT("CampLayout"));
#if WITH_EDITOR
        SpawnedCamp->SetFolderPath(TEXT("AshenRift/Camp"));
#endif
    }
    return SpawnedCamp;
}

void AARDungeonDirector::Generate(int32 Seed)
{
    for (AActor* Room : Rooms) if (IsValid(Room)) Room->Destroy();
    Rooms.Empty();
    CurrentSeed = Seed;

    TArray<TSubclassOf<AActor>> Pool = RoomLayouts;
    if (Pool.Num() == 0)
    {
        Pool.Add(AARDungeonRoom_Pillars::StaticClass());
        Pool.Add(AARDungeonRoom_Barricade::StaticClass());
        Pool.Add(AARDungeonRoom_Cross::StaticClass());
    }

    RoomOrder = MakeLayoutOrder(Seed, Pool.Num(), 3);
    const int32 Slots[] = {1, 2, 4};
    for (int32 I = 0; I < RoomOrder.Num() && I < 3; ++I)
    {
        const int32 LayoutIndex = RoomOrder[I];
        TSubclassOf<AActor> Class = Pool.IsValidIndex(LayoutIndex) && Pool[LayoutIndex]
            ? Pool[LayoutIndex]
            : AARDungeonRoom::StaticClass();
        AActor* Spawned = GetWorld()->SpawnActor<AActor>(Class, FVector(Slots[I] * 2400, 0, 0), FRotator::ZeroRotator);
        if (!Spawned) continue;
        Spawned->SetActorLabel(RoomName(LayoutIndex));
        if (AARDungeonRoom* Typed = Cast<AARDungeonRoom>(Spawned))
        {
            Typed->RoomKind = LayoutIndex;
            if (Class == AARDungeonRoom::StaticClass()) Typed->BuildInterior(LayoutIndex);
        }
        Rooms.Add(Spawned);
    }
    if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld())) Nav->Build();
}

void AARDungeonDirector::Populate(AARPlayerCharacter* Player, TSubclassOf<AAREnemy> EnemyClass, TSubclassOf<AAREnemy> RangedClass, TSubclassOf<AAREnemy> EliteClass, TSubclassOf<AARBoss> BossClass)
{
    const int32 Slots[] = {1, 2, 4};
    for (int32 I = 0; I < 3; ++I)
    {
        const float X = Slots[I] * 2400;
        for (int32 J = 0; J < 2; ++J)
        {
            const FTransform Transform(FRotator(0, 180, 0), FVector(X + (J == 0 ? 650 : 800), J == 0 ? -300 : 500, 100));
            const TSubclassOf<AAREnemy> SpawnClass = J == 1 && RangedClass ? RangedClass : I == 2 && J == 0 && EliteClass ? EliteClass : EnemyClass;
            auto* Enemy = GetWorld()->SpawnActorDeferred<AAREnemy>(SpawnClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
            if (!Enemy) continue;
            if (J == 0) Enemy->DropItemID = I == 0 ? TEXT("LeatherArmor") : I == 1 ? TEXT("RunicCharm") : Player->SelectedClass == EARPlayerClass::Mage ? TEXT("OakStaff") : TEXT("IronSword");
            UGameplayStatics::FinishSpawningActor(Enemy, Transform);
        }
    }
    if (BossClass) GetWorld()->SpawnActor<AARBoss>(BossClass, FVector(12000, 0, 150), FRotator(0, 180, 0));
    const FTransform WeaponTransform(FVector(1750, -600, 55));
    auto* Weapon = GetWorld()->SpawnActorDeferred<AARLoot>(AARLoot::StaticClass(), WeaponTransform);
    if (Weapon)
    {
        Weapon->ItemID = Player->SelectedClass == EARPlayerClass::Mage ? TEXT("OakStaff") : TEXT("IronSword");
        Weapon->Quantity = 1;
        UGameplayStatics::FinishSpawningActor(Weapon, WeaponTransform);
    }
}

AActor* UARDungeonEditorLibrary::CreateNavigationBounds(UObject* WorldContext, FVector Center, FVector Extent)
{
#if WITH_EDITOR
    if (!WorldContext || !WorldContext->GetWorld() || Extent.GetMin() <= 0) return nullptr;
    UWorld* World = WorldContext->GetWorld();
    auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>(ANavMeshBoundsVolume::StaticClass(), Center, FRotator::ZeroRotator);
    if (!Bounds) return nullptr;
    if (!ResizeNavigationBounds(Bounds, Center, Extent)) { Bounds->Destroy(); return nullptr; }
    return Bounds;
#else
    return nullptr;
#endif
}

bool UARDungeonEditorLibrary::ResizeNavigationBounds(AActor* Actor, FVector Center, FVector Extent)
{
#if WITH_EDITOR
    auto* Bounds = Cast<ANavMeshBoundsVolume>(Actor);
    if (!Bounds || Extent.GetMin() <= 0) return false;
    UWorld* World = Bounds->GetWorld();
    Bounds->Modify();
    if (!Bounds->Brush)
    {
        Bounds->Brush = NewObject<UModel>(Bounds, NAME_None, RF_Transactional);
        Bounds->Brush->Initialize(Bounds, true);
    }
    Bounds->GetBrushComponent()->Brush = Bounds->Brush;
    auto* Builder = NewObject<UCubeBuilder>(Bounds);
    Builder->X = Extent.X * 2; Builder->Y = Extent.Y * 2; Builder->Z = Extent.Z * 2;
    Bounds->BrushBuilder = Builder;
    if (!Builder->Build(World, Bounds)) return false;
    Bounds->GetBrushComponent()->BuildSimpleBrushCollision();
    Bounds->SetActorLocation(Center);
    Bounds->SetActorHiddenInGame(true);
    Bounds->ReregisterAllComponents();
    Bounds->GetBrushComponent()->UpdateBounds();
    if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)) Nav->OnNavigationBoundsUpdated(Bounds);
    return Bounds->GetComponentsBoundingBox(true).GetExtent().GetMin() > 0;
#else
    return false;
#endif
}

#if WITH_EDITOR
namespace ARDungeonRoomBP
{
struct FMeshSpec
{
    FName Name;
    FVector Location;
    FRotator Rotation;
    FVector Scale;
    bool bUsePillar;
};

static void CollectSpecs(int32 LayoutKind, bool bKayKit, TArray<FMeshSpec>& Out)
{
    Out.Reset();
    const float Z = bKayKit ? 0.f : 120.f;
    const FRotator Yaw90(0.f, 90.f, 0.f);
    if (LayoutKind == 0)
    {
        const FVector S = bKayKit ? FVector(1.f) : FVector(2.f, 2.f, 2.4f);
        Out.Add({TEXT("Pillar_NW"), FVector(-450.f, -500.f, Z), FRotator::ZeroRotator, S, true});
        Out.Add({TEXT("Pillar_NE"), FVector(-450.f,  500.f, Z), FRotator::ZeroRotator, S, true});
        Out.Add({TEXT("Pillar_SW"), FVector( 450.f, -500.f, Z), FRotator::ZeroRotator, S, true});
        Out.Add({TEXT("Pillar_SE"), FVector( 450.f,  500.f, Z), FRotator::ZeroRotator, S, true});
    }
    else if (LayoutKind == 1)
    {
        const FVector S = bKayKit ? FVector(1.f) : FVector(2.f, 4.f, 2.4f);
        Out.Add({TEXT("Barricade_A"), FVector(0.f, -400.f, Z), Yaw90, S, false});
        Out.Add({TEXT("Barricade_B"), FVector(0.f,    0.f, Z), Yaw90, S, false});
        Out.Add({TEXT("Barricade_C"), FVector(0.f,  400.f, Z), Yaw90, S, false});
    }
    else
    {
        const FVector S = bKayKit ? FVector(1.f) : FVector(2.f, 4.f, 2.4f);
        Out.Add({TEXT("Cross_A1"), FVector(-450.f, -550.f, Z), Yaw90, S, false});
        Out.Add({TEXT("Cross_A2"), FVector(-450.f, -150.f, Z), Yaw90, S, false});
        Out.Add({TEXT("Cross_B1"), FVector( 450.f,  150.f, Z), Yaw90, S, false});
        Out.Add({TEXT("Cross_B2"), FVector( 450.f,  550.f, Z), Yaw90, S, false});
    }
}
}
#endif

bool UARDungeonEditorLibrary::CreateEditableRoomBlueprint(const FString& PackagePath, const FString& AssetName, int32 LayoutKind)
{
#if WITH_EDITOR
    if (PackagePath.IsEmpty() || AssetName.IsEmpty() || LayoutKind < 0 || LayoutKind > 2)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateEditableRoomBlueprint: bad args path=%s name=%s kind=%d"), *PackagePath, *AssetName, LayoutKind);
        return false;
    }

    const FString ObjectPath = PackagePath / AssetName;
    if (UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *(ObjectPath + TEXT(".") + AssetName)))
    {
        TArray<UObject*> ToDelete;
        ToDelete.Add(Existing);
        ObjectTools::ForceDeleteObjects(ToDelete, false);
    }

    UPackage* Package = CreatePackage(*ObjectPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateEditableRoomBlueprint: CreatePackage failed %s"), *ObjectPath);
        return false;
    }
    Package->FullyLoad();

    // Parent MUST be AActor â€?C++ AARDungeonRoom_* parents open as an empty black BP shell in 4.27.
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(),
        Package,
        FName(*AssetName),
        EBlueprintType::BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        NAME_None);
    if (!BP || !BP->SimpleConstructionScript)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateEditableRoomBlueprint: CreateBlueprint failed %s"), *ObjectPath);
        return false;
    }

    UStaticMesh* PillarMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/AshenRift/KayKit/Architecture/Columns/pillar.pillar"));
    UStaticMesh* BarrierMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/AshenRift/KayKit/Architecture/Barriers/barrier.barrier"));
    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    const bool bKay = (LayoutKind == 0) ? (PillarMesh != nullptr) : (BarrierMesh != nullptr);

    TArray<ARDungeonRoomBP::FMeshSpec> Specs;
    ARDungeonRoomBP::CollectSpecs(LayoutKind, bKay, Specs);

    USCS_Node* RootNode = BP->SimpleConstructionScript->GetDefaultSceneRootNode();
    int32 Added = 0;
    for (const ARDungeonRoomBP::FMeshSpec& Spec : Specs)
    {
        USCS_Node* Node = BP->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(), Spec.Name);
        if (!Node) continue;
        UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Node->ComponentTemplate);
        if (!SMC) continue;

        UStaticMesh* MeshAsset = Spec.bUsePillar ? (PillarMesh ? PillarMesh : CubeMesh) : (BarrierMesh ? BarrierMesh : CubeMesh);
        if (MeshAsset) SMC->SetStaticMesh(MeshAsset);
        SMC->SetMobility(EComponentMobility::Movable);
        SMC->SetCollisionProfileName(TEXT("BlockAll"));
        SMC->SetCanEverAffectNavigation(true);
        SMC->SetRelativeLocation(Spec.Location);
        SMC->SetRelativeRotation(Spec.Rotation);
        SMC->SetRelativeScale3D(Spec.Scale);
        SMC->bEditableWhenInherited = true;

        if (RootNode) RootNode->AddChildNode(Node);
        else BP->SimpleConstructionScript->AddNode(Node);
        ++Added;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    FAssetRegistryModule::AssetCreated(BP);
    Package->MarkPackageDirty();

    const FString FileName = FPackageName::LongPackageNameToFilename(ObjectPath, FPackageName::GetAssetPackageExtension());
#if ENGINE_MAJOR_VERSION >= 5
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    const bool bSaved = UPackage::SavePackage(Package, BP, *FileName, SaveArgs);
#else
    const bool bSaved = UPackage::SavePackage(Package, BP, RF_Public | RF_Standalone, *FileName);
#endif

    const int32 ScsCount = BP->SimpleConstructionScript ? BP->SimpleConstructionScript->GetAllNodes().Num() : 0;
    UE_LOG(LogTemp, Log, TEXT("CreateEditableRoomBlueprint: %s saved=%d parent=%s scsNodes=%d meshes=%d kay=%d"),
        *ObjectPath, bSaved ? 1 : 0, *GetNameSafe(BP->ParentClass), ScsCount, Added, bKay ? 1 : 0);
    return bSaved && Added > 0 && BP->ParentClass == AActor::StaticClass();
#else
    return false;
#endif
}
bool UARDungeonEditorLibrary::CreateEditableCampBlueprint(const FString& PackagePath, const FString& AssetName)
{
#if WITH_EDITOR
    if (PackagePath.IsEmpty() || AssetName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("CreateEditableCampBlueprint: bad args path=%s name=%s"), *PackagePath, *AssetName);
        return false;
    }

    const FString ObjectPath = PackagePath / AssetName;
    if (UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *(ObjectPath + TEXT(".") + AssetName)))
    {
        TArray<UObject*> ToDelete;
        ToDelete.Add(Existing);
        ObjectTools::ForceDeleteObjects(ToDelete, false);
    }

    UPackage* Package = CreatePackage(*ObjectPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateEditableCampBlueprint: CreatePackage failed %s"), *ObjectPath);
        return false;
    }
    Package->FullyLoad();

    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(),
        Package,
        FName(*AssetName),
        EBlueprintType::BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        NAME_None);
    if (!BP || !BP->SimpleConstructionScript)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateEditableCampBlueprint: CreateBlueprint failed %s"), *ObjectPath);
        return false;
    }

    struct FCampMesh
    {
        FName Name;
        const TCHAR* Path;
        FVector Location;
        FRotator Rotation;
        FVector Scale;
    };

    // Relative to CampOrigin (0,0,0). Keep walkable space around CampSpawn (-700,0,100).
    const FCampMesh Specs[] = {
        { TEXT("Table"),   TEXT("/Game/AshenRift/KayKit/Props/Furniture/table_medium.table_medium"), FVector(-320.f,  180.f, 0.f), FRotator(0.f, 15.f, 0.f), FVector(1.f) },
        { TEXT("Chair"),   TEXT("/Game/AshenRift/KayKit/Props/Furniture/chair.chair"),               FVector(-220.f,  280.f, 0.f), FRotator(0.f,-40.f, 0.f), FVector(1.f) },
        { TEXT("Chest"),   TEXT("/Game/AshenRift/KayKit/Props/Containers/chest.chest"),               FVector(-480.f, -260.f, 0.f), FRotator(0.f, 90.f, 0.f), FVector(1.f) },
        { TEXT("Barrel"),  TEXT("/Game/AshenRift/KayKit/Props/Containers/barrel_large.barrel_large"), FVector(-560.f,  340.f, 0.f), FRotator::ZeroRotator, FVector(1.f) },
        { TEXT("Crate"),   TEXT("/Game/AshenRift/KayKit/Props/Containers/crates_stacked.crates_stacked"), FVector(-620.f, -120.f, 0.f), FRotator(0.f, 25.f, 0.f), FVector(1.f) },
        { TEXT("Torch"),   TEXT("/Game/AshenRift/KayKit/Props/Lights/torch_lit.torch_lit"),           FVector(-180.f, -520.f, 0.f), FRotator::ZeroRotator, FVector(1.f) },
        { TEXT("Banner"),  TEXT("/Game/AshenRift/KayKit/Props/Decor/banner_red.banner_red"),          FVector(-900.f,  400.f, 0.f), FRotator(0.f, 90.f, 0.f), FVector(1.f) },
    };

    USCS_Node* RootNode = BP->SimpleConstructionScript->GetDefaultSceneRootNode();
    int32 Added = 0;
    for (const FCampMesh& Spec : Specs)
    {
        UStaticMesh* MeshAsset = LoadObject<UStaticMesh>(nullptr, Spec.Path);
        if (!MeshAsset)
        {
            UE_LOG(LogTemp, Warning, TEXT("CreateEditableCampBlueprint: missing mesh %s"), Spec.Path);
            continue;
        }
        USCS_Node* Node = BP->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(), Spec.Name);
        if (!Node) continue;
        UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Node->ComponentTemplate);
        if (!SMC) continue;
        SMC->SetStaticMesh(MeshAsset);
        SMC->SetMobility(EComponentMobility::Movable);
        SMC->SetCollisionProfileName(TEXT("BlockAll"));
        SMC->SetCanEverAffectNavigation(true);
        SMC->SetRelativeLocation(Spec.Location);
        SMC->SetRelativeRotation(Spec.Rotation);
        SMC->SetRelativeScale3D(Spec.Scale);
        SMC->bEditableWhenInherited = true;
        if (RootNode) RootNode->AddChildNode(Node);
        else BP->SimpleConstructionScript->AddNode(Node);
        ++Added;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    FAssetRegistryModule::AssetCreated(BP);
    Package->MarkPackageDirty();

    const FString FileName = FPackageName::LongPackageNameToFilename(ObjectPath, FPackageName::GetAssetPackageExtension());
#if ENGINE_MAJOR_VERSION >= 5
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    const bool bSaved = UPackage::SavePackage(Package, BP, *FileName, SaveArgs);
#else
    const bool bSaved = UPackage::SavePackage(Package, BP, RF_Public | RF_Standalone, *FileName);
#endif

    UE_LOG(LogTemp, Log, TEXT("CreateEditableCampBlueprint: %s saved=%d meshes=%d parent=%s"),
        *ObjectPath, bSaved ? 1 : 0, Added, *GetNameSafe(BP->ParentClass));
    return bSaved && Added > 0 && BP->ParentClass == AActor::StaticClass();
#else
    return false;
#endif
}
