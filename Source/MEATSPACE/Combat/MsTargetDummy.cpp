#include "Combat/MsTargetDummy.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AMsTargetDummy::AMsTargetDummy()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Engine primitive - placeholder until clankers exist.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	// Must block the Visibility channel or the weapon trace passes straight through it.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void AMsTargetDummy::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
}

void AMsTargetDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMsTargetDummy, Health);
}

float AMsTargetDummy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Server only. Clients find out via Health replication.
	if (!HasAuthority() || Health <= 0.0f)
	{
		return 0.0f;
	}

	Health = FMath::Max(Health - DamageAmount, 0.0f);

	// OnRep does not fire on the server, so drive the feedback here too.
	ShowHealthFeedback();

	if (Health <= 0.0f)
	{
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);

		if (bRespawnOnDeath)
		{
			GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMsTargetDummy::Revive, RespawnDelay, false);
		}
		else
		{
			SetLifeSpan(0.1f);
		}
	}

	return Applied;
}

void AMsTargetDummy::OnRep_Health()
{
	ShowHealthFeedback();

	const bool bDead = Health <= 0.0f;
	SetActorHiddenInGame(bDead);
	SetActorEnableCollision(!bDead);
}

void AMsTargetDummy::ShowHealthFeedback()
{
	// Debug only, never shown to a player - so a plain FString is fine here. Anything a
	// player actually reads must be FText in a String Table (see CLAUDE.md).
	if (GEngine)
	{
		// Not named 'Role' - AActor already has a member by that name.
		const FString NetRoleLabel = HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT");

		// Cast the key explicitly: GetUniqueID() is uint32, which is ambiguous between the
		// uint64 and int32 overloads of AddOnScreenDebugMessage.
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()), 1.5f,
			Health > 0.0f ? FColor::Yellow : FColor::Red,
			FString::Printf(TEXT("[%s] %s  HP %.0f / %.0f"), *NetRoleLabel, *GetName(), Health, MaxHealth));
	}
}

void AMsTargetDummy::Revive()
{
	if (!HasAuthority())
	{
		return;
	}

	Health = MaxHealth;
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	ShowHealthFeedback();
}
