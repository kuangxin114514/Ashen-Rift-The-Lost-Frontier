#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARDungeon.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UMaterialInterface;
class AARPlayerCharacter;
class AAREnemy;
class AARBoss;

// Legacy C++ room (still spawnable). Editable room prefabs are plain Actor BPs
// under Content/AshenRift/Rooms with SCS meshes â€?NOT children of these classes.
UCLASS(Blueprintable, BlueprintType)
class ASHENRIFT_API AARDungeonRoom : public AActor
{
    GENERATED_BODY()
public:
    AARDungeonRoom();

    UFUNCTION(BlueprintCallable, Category="Dungeon")
    void BuildInterior(int32 Kind);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon")
    int32 RoomKind = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon")
    FString DisplayName;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    USceneComponent* RoomRoot = nullptr;

    UPROPERTY() UStaticMesh* Cube = nullptr;
    UPROPERTY() UStaticMesh* PillarMesh = nullptr;
    UPROPERTY() UStaticMesh* BarrierMesh = nullptr;
    UPROPERTY() UMaterialInterface* WallMaterial = nullptr;

    UStaticMeshComponent* CreateBlock(FName Name, const FVector& RelativeLocation, const FVector& SizeUU);
    UStaticMeshComponent* CreateProp(FName Name, UStaticMesh* Mesh, const FVector& RelativeLocation, const FRotator& RelativeRotation = FRotator::ZeroRotator, const FVector& Scale = FVector(1.f));
    void Block(const FVector& Position, const FVector& Size);
};

UCLASS(Blueprintable, BlueprintType, meta=(DisplayName="AR Dungeon Room Pillars"))
class ASHENRIFT_API AARDungeonRoom_Pillars : public AARDungeonRoom
{
    GENERATED_BODY()
public:
    AARDungeonRoom_Pillars();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Pillar_NW = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Pillar_NE = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Pillar_SW = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Pillar_SE = nullptr;
};

UCLASS(Blueprintable, BlueprintType, meta=(DisplayName="AR Dungeon Room Barricade"))
class ASHENRIFT_API AARDungeonRoom_Barricade : public AARDungeonRoom
{
    GENERATED_BODY()
public:
    AARDungeonRoom_Barricade();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Barricade_A = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Barricade_B = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Barricade_C = nullptr;
};

UCLASS(Blueprintable, BlueprintType, meta=(DisplayName="AR Dungeon Room Cross"))
class ASHENRIFT_API AARDungeonRoom_Cross : public AARDungeonRoom
{
    GENERATED_BODY()
public:
    AARDungeonRoom_Cross();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Cross_A1 = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Cross_A2 = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Cross_B1 = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Room|Meshes")
    UStaticMeshComponent* Cross_B2 = nullptr;
};

UCLASS(Blueprintable, BlueprintType)
class ASHENRIFT_API AARDungeonDirector : public AActor
{
    GENERATED_BODY()
public:
    AARDungeonDirector();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseFixedSeed = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FixedSeed = 1337;

    // Editable room prefabs: prefer plain Actor BPs (AshenRift/Rooms/BP_Room_*) with
    // Blueprint-authored StaticMesh components. C++ AARDungeonRoom_* still work as fallback.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon")
    TArray<TSubclassOf<AActor>> RoomLayouts;

    UPROPERTY(BlueprintReadOnly) TArray<int32> RoomOrder;
    UPROPERTY(BlueprintReadOnly) int32 CurrentSeed = 0;
    UFUNCTION(BlueprintPure, Category="Dungeon") static TArray<int32> MakeRoomOrder(int32 Seed);
    UFUNCTION(BlueprintPure, Category="Dungeon") static TArray<int32> MakeLayoutOrder(int32 Seed, int32 PoolSize, int32 PickCount = 3);
    UFUNCTION(BlueprintPure, Category="Dungeon") static FString RoomName(int32 Kind);
    UFUNCTION(BlueprintCallable, Category="Dungeon") void Generate(int32 Seed);
    void Populate(AARPlayerCharacter* Player, TSubclassOf<AAREnemy> EnemyClass, TSubclassOf<AAREnemy> RangedClass, TSubclassOf<AAREnemy> EliteClass, TSubclassOf<AARBoss> BossClass);

    /** Editable camp prefab (plain Actor BP, e.g. AshenRift/Rooms/BP_Camp). Spawned at CampOrigin on BeginPlay and when returning to hub. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon|Camp")
    TSubclassOf<AActor> CampLayout;

    /** World location of the camp cell (slot 0 center). Matches create_random_dungeon floor index 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon|Camp")
    FVector CampOrigin = FVector(0.f, 0.f, 0.f);

    UFUNCTION(BlueprintCallable, Category="Dungeon|Camp")
    AActor* EnsureCampLayout();

    virtual void BeginPlay() override;

    UPROPERTY()
    AActor* SpawnedCamp = nullptr;
    FVector Entry() const { return FVector(1450, 0, 100); }
    UPROPERTY() TArray<AActor*> Rooms;

    UFUNCTION(BlueprintPure, Category="Dungeon")
    FString SlotDisplayName(int32 SlotIndex) const;
};

UCLASS()
class ASHENRIFT_API UARDungeonEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="AshenRift|Editor")
    static AActor* CreateNavigationBounds(UObject* WorldContext, FVector Center, FVector Extent);
    UFUNCTION(BlueprintCallable, Category="AshenRift|Editor")
    static bool ResizeNavigationBounds(AActor* Actor, FVector Center, FVector Extent);

    /**
     * Creates a normal Actor Blueprint (parent = AActor) with SCS StaticMesh components
     * for the given layout kind: 0=Pillars, 1=Barricade, 2=Cross.
     * These open with Viewport + Components (unlike C++-parent room BPs).
     */
    UFUNCTION(BlueprintCallable, Category="AshenRift|Editor")
    static bool CreateEditableRoomBlueprint(const FString& PackagePath, const FString& AssetName, int32 LayoutKind);

    /** Creates a plain Actor Blueprint with KayKit camp starter props (table/chest/barrel/torch/chair). */
    UFUNCTION(BlueprintCallable, Category="AshenRift|Editor")
    static bool CreateEditableCampBlueprint(const FString& PackagePath, const FString& AssetName);
};
