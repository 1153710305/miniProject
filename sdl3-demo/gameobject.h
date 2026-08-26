#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <SDL3/SDL.h>
#include "animation.h"

// ============================================================
// PlayerState：玩家角色的行为状态
// ============================================================
enum class PlayerState
{
	idle,    // 待机（无水平移动输入）
	running, // 奔跑（有水平移动输入，或正在减速滑行）
	jumping  // 跳跃（离开地面、处于空中）
};

// ============================================================
// BulletState：子弹的生命周期状态
// ============================================================
enum class BulletState
{
	moving,    // 飞行中，正常移动并检测碰撞
	colliding, // 已命中目标，正在播放命中/爆炸动画
	inactive   // 非活跃，可被对象池回收复用（不参与更新与渲染）
};

// ============================================================
// EnemyState：敌人的行为状态
// ============================================================
enum class EnemyState
{
	shambling, // 蹒跚移动/追击状态（正常存活状态）
	damaged,   // 受伤硬直状态（被子弹击中后短暂僵直，期间不可再次追击）
	dead       // 死亡状态（播放死亡动画后保持静止，不再参与碰撞逻辑判断）
};

// ============================================================
// PlayerData：玩家专属的数据（仅当 GameObject.type == player 时有效）
// ============================================================
struct PlayerData
{
	PlayerState state;   // 当前玩家状态
	Timer weaponTimer;   // 射击冷却计时器，控制连续射击的间隔

	// 构造函数：初始化武器冷却时间为 0.1 秒（即每 0.1 秒最多发射一颗子弹），初始状态为待机
	PlayerData() : weaponTimer(0.1f)
	{
		state = PlayerState::idle;
	}
};

// ============================================================
// LevelData：地形对象的专属数据
// 地形本身没有额外状态需要维护，因此是一个空结构体（仅作类型占位）
// ============================================================
struct LevelData {};

// ============================================================
// EnemyData：敌人专属的数据（仅当 GameObject.type == enemy 时有效）
// ============================================================
struct EnemyData
{
	EnemyState state;      // 当前敌人状态
	Timer damageTimer;     // 受伤硬直计时器，控制“damaged”状态持续多久后恢复
	int healthPoints;      // 当前生命值，降至 0 或以下时进入 dead 状态

	// 构造函数：初始状态为蹒跚移动，硬直时长 0.5 秒，初始生命值 100
	EnemyData() : state(EnemyState::shambling), damageTimer(0.5f)
	{
		healthPoints = 100;
	}
};

// ============================================================
// BulletData：子弹专属的数据（仅当 GameObject.type == bullet 时有效）
// ============================================================
struct BulletData
{
	BulletState state; // 当前子弹状态

	// 构造函数：子弹一经生成即处于飞行状态
	BulletData() : state(BulletState::moving)
	{
	}
};

// ============================================================
// ObjectData：联合体（union），根据 GameObject.type 的不同，
// 复用同一块内存空间存储不同类型对象的专属数据，节省内存。
// 注意：union 中同一时刻只有一个成员是“有效”的，具体是哪个由外部的 ObjectType 决定，
// 使用时必须确保访问的成员与当前对象的实际类型一致，否则会读到未定义的数据。
// ============================================================
union ObjectData
{
	PlayerData player;
	LevelData level;
	EnemyData enemy;
	BulletData bullet;
};

// ============================================================
// ObjectType：游戏对象的大类类型，决定了 ObjectData 中应访问哪个成员，
// 以及 update/collisionResponse 等函数中应该走哪一分支的逻辑
// ============================================================
enum class ObjectType
{
	player, level, enemy, bullet
};

// ============================================================
// GameObject：所有游戏实体（玩家、地形瓦片、敌人、子弹）的统一表示。
// 采用“单一结构体 + 联合体数据 + 类型标签”的设计（有点类似简化版的 ECS/多态），
// 避免为每种对象单独定义类并使用虚函数/继承，减少间接调用开销，便于统一存储在同一容器中。
// ============================================================
struct GameObject
{
	ObjectType type;               // 对象的具体类型（玩家/地形/敌人/子弹）
	ObjectData data;                // 与 type 对应的专属数据（联合体）

	glm::vec2 position;             // 世界坐标系中的位置（左上角）
	glm::vec2 velocity;             // 当前速度（像素/秒）
	glm::vec2 acceleration;         // 加速度，用于逐帧改变速度（例如玩家的加速/敌人的追击加速度）

	float direction;                // 朝向：1 表示朝右，-1 表示朝左，用于贴图翻转和子弹发射方向
	float maxSpeedX;                // 水平方向最大速度限制

	std::vector<Animation> animations; // 该对象拥有的所有动画（例如玩家的待机/奔跑/射击等各动画）
	int currentAnimation;              // 当前正在播放的动画在 animations 中的索引，-1 表示不播放动画（使用固定帧）

	SDL_Texture* texture;           // 当前使用的精灵表贴图指针

	bool dynamic;                   // 是否是“动态”对象，即是否受重力影响（玩家、敌人为 true；地形通常为 false）
	bool grounded;                  // 是否处于“着地”状态（用于判断是否可以跳跃、是否需要施加重力等）

	SDL_FRect collider;              // 碰撞盒：相对于 position 的局部偏移量(x,y)与大小(w,h)

	Timer flashTimer;                // 受伤闪烁效果的计时器，控制闪烁持续时长
	bool shouldFlash;                // 是否需要绘制受伤闪烁效果（红色高亮）

	int spriteFrame;                 // 固定帧序号（当 currentAnimation == -1 时使用，例如死亡后停在指定帧）

	// 默认构造函数：将所有字段初始化为安全的默认值
	// data 显式初始化为 LevelData（因为 union 必须显式指定初始化其中一个成员），
	// 对应默认 type 也设为 level，保证两者初始状态是一致的
	GameObject() : data{ .level = LevelData() }, collider{ 0 }, flashTimer(0.05f)
	{
		type = ObjectType::level;
		direction = 1;                          // 默认朝右
		maxSpeedX = 0;
		position = velocity = acceleration = glm::vec2(0);
		currentAnimation = -1;                  // 默认不播放动画
		texture = nullptr;
		dynamic = false;                        // 默认不受重力影响（如地形）
		grounded = false;
		shouldFlash = false;
		spriteFrame = 1;                        // 默认使用第 1 帧
	}
};
