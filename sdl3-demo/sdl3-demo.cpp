#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <vector>
#include <string>
#include <array>
#include <format>

// STB_IMAGE_IMPLEMENTATION 宏必须在包含 stb_image.h 之前定义，
// 且只能在一个 .cpp 文件中定义一次，用于生成 stb_image 的实现代码（而不仅仅是声明）
#define STB_IMAGE_IMPLEMENTATION
#include <../vendored/stb/stb_image.h>

#include "gameobject.h"

using namespace std;

// ============================================================
// SDLState：保存与 SDL 窗口/渲染器相关的全局状态
// ============================================================
struct SDLState
{
	SDL_Window* window;      // SDL 窗口句柄
	SDL_Renderer* renderer;  // SDL 渲染器句柄
	int width, height;       // 实际窗口的宽高（像素）
	int logW, logH;          // 逻辑分辨率的宽高（用于像素风格游戏的固定分辨率渲染）
	const bool* keys;        // 指向 SDL 键盘状态数组的指针，可直接查询某按键是否按下
	bool fullscreen;         // 是否处于全屏模式

	// 构造函数：初始化时获取键盘状态数组，并默认开启全屏
	SDLState() : keys(SDL_GetKeyboardState(nullptr))
	{
		fullscreen = true;
	}
};

// ============================================================
// 常量定义
// ============================================================
const size_t LAYER_IDX_LEVEL = 0;      // layers 数组中“关卡地形”层的索引
const size_t LAYER_IDX_CHARACTERS = 1; // layers 数组中“角色（玩家/敌人）”层的索引
const int MAP_ROWS = 5;                // 地图的行数
const int MAP_COLS = 50;               // 地图的列数
const int TILE_SIZE = 32;              // 每个瓦片（tile）的像素大小

// ============================================================
// GameState：保存游戏运行时的全部动态数据
// ============================================================
struct GameState
{
	// layers[0] = 关卡地形对象；layers[1] = 角色对象（玩家、敌人）
	std::array<std::vector<GameObject>, 2> layers;
	std::vector<GameObject> backgroundTiles; // 背景装饰瓦片（例如砖块），渲染在角色之后、地形之前
	std::vector<GameObject> foregroundTiles; // 前景装饰瓦片（例如草），渲染在最上层，遮挡角色
	std::vector<GameObject> bullets;         // 子弹对象池（用对象池而非频繁 new/delete 提升性能）
	int playerIndex;                         // 玩家对象在 layers[LAYER_IDX_CHARACTERS] 中的索引
	SDL_FRect mapViewport;                   // 当前摄像机/视口在地图中的位置和大小
	float bg2Scroll, bg3Scroll, bg4Scroll;   // 三层视差背景各自的滚动偏移量
	bool debugMode;                          // 是否开启调试模式（显示碰撞盒、调试文本等）

	// 构造函数：根据窗口逻辑分辨率初始化视口，并将各滚动量、玩家索引清零
	GameState(const SDLState& state)
	{
		playerIndex = -1; // -1 表示尚未找到/创建玩家对象
		mapViewport = SDL_FRect{
			.x = 0, .y = 0,
			.w = static_cast<float>(state.logW),
			.h = static_cast<float>(state.logH)
		};
		bg2Scroll = bg3Scroll = bg4Scroll = 0;
		debugMode = false;
	}

	// 便捷方法：直接获取玩家对象的引用
	GameObject& player() { return layers[LAYER_IDX_CHARACTERS][playerIndex]; }
};

// ============================================================
// Resources：集中管理所有贴图、动画、音频资源的加载与释放
// ============================================================
struct Resources
{
	// ---- 玩家动画帧序号常量 ----
	const int ANIM_PLAYER_IDLE = 0;
	const int ANIM_PLAYER_RUN = 1;
	const int ANIM_PLAYER_SLIDE = 2;
	const int ANIM_PLAYER_SHOOT = 3;
	const int ANIM_PLAYER_SLIDE_SHOOT = 4;
	std::vector<Animation> playerAnims; // 玩家所有动画的集合，索引对应上面的常量

	// ---- 子弹动画帧序号常量 ----
	const int ANIM_BULLET_MOVING = 0;
	const int ANIM_BULLET_HIT = 1;
	std::vector<Animation> bulletAnims;

	// ---- 敌人动画帧序号常量 ----
	const int ANIM_ENEMY = 0;
	const int ANIM_ENEMY_HIT = 1;
	const int ANIM_ENEMY_DIE = 2;
	std::vector<Animation> enemyAnims;

	std::vector<SDL_Texture*> textures; // 保存所有已加载贴图，方便统一释放
	// 各个具体用途的贴图指针
	SDL_Texture* texIdle, * texRun, * texBrick, * texGrass, * texGround, * texPanel,
		* texSlide, * texBg1, * texBg2, * texBg3, * texBg4, * texBullet, * texBulletHit,
		* texShoot, * texRunShoot, * texSlideShoot, * texEnemy, * texEnemyHit, * texEnemyDie;

	std::vector<MIX_Audio*> audioBuffers; // 保存所有已加载音频，方便统一释放
	MIX_Audio* audioShoot, * audioShootHit, * audioEnemyHit; // 音效
	MIX_Audio* musicMain; // 背景音乐

	// 加载贴图：使用 stb_image 读取像素数据，再转换为 SDL_Texture
	SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& filepath)
	{
		// 获取像素数据及图片信息（强制转换为 4 通道 RGBA）
		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_uc* pixData = stbi_load(filepath.c_str(), &width, &height, &channels, 4);

		// 用像素数据创建 SDL_Surface，再由 Surface 创建 Texture
		SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixData, width * 4);
		SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
		textures.push_back(tex); // 记录下来以便后续统一释放
		// 使用最近邻缩放，保持像素风格的清晰边缘（不做双线性模糊）
		SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

		// 先释放 stb_image 分配的像素数据，再销毁 Surface（Surface 不再需要，Texture 已经拥有数据）
		stbi_image_free(pixData);
		SDL_DestroySurface(surface);

		return tex;
	}

	// 加载音频文件（wav/mp3等），使用 SDL_mixer 的新版 MIX_LoadAudio 接口
	MIX_Audio* loadAudio(const std::string& filepath)
	{
		// 第二个参数为文件路径，第三个参数 true 表示以“可预测/可播放”方式加载（具体语义取决于 SDL_mixer 版本）
		MIX_Audio* audio = MIX_LoadAudio(nullptr, filepath.c_str(), true);
		audioBuffers.push_back(audio); // 记录下来以便统一释放
		//MIX_VolumeChunk(chunk, MIX_MAX_VOLUME / 2); // （已注释）可用于设置音量
		return audio;
	}

	// 统一加载所有游戏资源：动画参数、贴图、音效、音乐
	void load(SDLState& state)
	{
		// ---- 初始化玩家动画：参数为（帧数，每帧持续时间/整体时长，具体取决于 Animation 实现）----
		playerAnims.resize(5);
		playerAnims[ANIM_PLAYER_IDLE] = Animation(8, 1.6f);
		playerAnims[ANIM_PLAYER_RUN] = Animation(4, 0.5f);
		playerAnims[ANIM_PLAYER_SLIDE] = Animation(1, 1.0f);
		playerAnims[ANIM_PLAYER_SHOOT] = Animation(4, 0.5f);
		playerAnims[ANIM_PLAYER_SLIDE_SHOOT] = Animation(4, 0.5f);

		// ---- 初始化子弹动画 ----
		bulletAnims.resize(2);
		bulletAnims[ANIM_BULLET_MOVING] = Animation(4, 0.05f); // 飞行动画，帧切换很快
		bulletAnims[ANIM_BULLET_HIT] = Animation(4, 0.15f);    // 命中/爆炸动画

		// ---- 初始化敌人动画 ----
		enemyAnims.resize(3);
		enemyAnims[ANIM_ENEMY] = Animation(8, 1.0f);
		enemyAnims[ANIM_ENEMY_HIT] = Animation(8, 1.0f);
		enemyAnims[ANIM_ENEMY_DIE] = Animation(18, 2.0f); // 死亡动画帧数较多，时长也更长

		// ---- 加载所有贴图文件 ----
		texIdle = loadTexture(state.renderer, "data/idle.png");
		texRun = loadTexture(state.renderer, "data/run.png");
		texSlide = loadTexture(state.renderer, "data/slide.png");
		texBrick = loadTexture(state.renderer, "data/tiles/brick.png");
		texGrass = loadTexture(state.renderer, "data/tiles/grass.png");
		texGround = loadTexture(state.renderer, "data/tiles/ground.png");
		texPanel = loadTexture(state.renderer, "data/tiles/panel.png");
		texBg1 = loadTexture(state.renderer, "data/bg/bg_layer1.png"); // 最远景（静止不动）
		texBg2 = loadTexture(state.renderer, "data/bg/bg_layer2.png"); // 视差层，滚动系数最大
		texBg3 = loadTexture(state.renderer, "data/bg/bg_layer3.png"); // 视差层，滚动系数中等
		texBg4 = loadTexture(state.renderer, "data/bg/bg_layer4.png"); // 视差层，滚动系数最小（离摄像机最远）
		texBullet = loadTexture(state.renderer, "data/bullet.png");
		texBulletHit = loadTexture(state.renderer, "data/bullet_hit.png");
		texShoot = loadTexture(state.renderer, "data/shoot.png");
		texRunShoot = loadTexture(state.renderer, "data/shoot_run.png");
		texSlideShoot = loadTexture(state.renderer, "data/slide_shoot.png");
		texEnemy = loadTexture(state.renderer, "data/enemy.png");
		texEnemyHit = loadTexture(state.renderer, "data/enemy_hit.png");
		texEnemyDie = loadTexture(state.renderer, "data/enemy_die.png");

		// ---- 加载音效与音乐 ----
		audioShoot = loadAudio("data/audio/shoot.wav");         // 射击音效
		audioShootHit = loadAudio("data/audio/wall_hit.wav");   // 子弹击中墙壁音效
		audioEnemyHit = loadAudio("data/audio/shoot_hit.wav");  // 子弹击中敌人音效
		musicMain = loadAudio("data/audio/Juhani Junkala [Retro Game Music Pack] Level 1.mp3"); // 背景音乐
	}

	// 释放所有已加载的贴图和音频资源，防止内存/显存泄漏
	void unload()
	{
		for (SDL_Texture* tex : textures)
		{
			SDL_DestroyTexture(tex);
		}

		for (MIX_Audio* audio : audioBuffers)
		{
			MIX_DestroyAudio(audio);
		}
	}
};

// ============================================================
// 函数前置声明
// ============================================================
bool initialize(SDLState& state);
void cleanup(SDLState& state);
// 绘制单个游戏对象（含精灵动画帧的选取、翻转、受伤闪烁效果、调试碰撞框显示）
void drawObject(const SDLState& state, GameState& gs, GameObject& obj,
	float width, float height, float deltaTime);
// 每帧更新单个游戏对象的逻辑（状态机、物理、输入响应、碰撞检测触发）
void update(const SDLState& state, GameState& gs, Resources& res, GameObject& obj, float deltaTime);
// 根据地图数组数据生成关卡中的所有瓦片/角色对象
void createTiles(const SDLState& state, GameState& gs, const Resources& res);
// 轴对齐包围盒（AABB）相交检测，overlap 输出两个矩形在 x/y 方向上的重叠量
bool intersectAABB(const SDL_FRect& a, const SDL_FRect& b, glm::vec2& overlap);
// 检测两个对象是否碰撞，若碰撞则触发碰撞响应
void checkCollision(const SDLState& state, GameState& gs, Resources& res,
	GameObject& a, GameObject& b, float deltaTime);
// 处理键盘按键事件（跳跃等）
void handleKeyInput(const SDLState& state, GameState& gs, GameObject& obj,
	SDL_Scancode key, bool keyDown);
// 绘制一层视差滚动背景（根据玩家横向速度和滚动系数计算偏移，并平铺渲染）
void drawParalaxBackground(SDL_Renderer* renderer, SDL_Texture* texture,
	float xVelocity, float& scrollPos, float scrollFactor, float deltaTime);

// ============================================================
// 程序入口
// ============================================================
int main(int argc, char* argv[])
{
	// ---- 初始化窗口/逻辑分辨率参数 ----
	SDLState state;
	state.width = 1600;
	state.height = 900;
	state.logW = 640;  // 游戏内部逻辑渲染分辨率（像素风格游戏常用较低分辨率再放大显示）
	state.logH = 320;

	if (!initialize(state))
	{
		return 1; // 初始化失败，直接退出
	}

	// ---- 加载游戏资源（贴图、音频、动画配置）----
	Resources res;
	res.load(state);

	// ---- 初始化游戏数据（生成地图、角色等）----
	GameState gs(state);
	createTiles(state, gs, res);
	uint64_t prevTime = SDL_GetTicks(); // 记录上一帧的时间戳，用于计算 deltaTime

	// 开始播放背景音乐（循环由具体实现决定）
	MIX_PlayAudio(nullptr, res.musicMain);

	// ============================================================
	// 主游戏循环
	// ============================================================
	bool running = true;
	while (running)
	{
		// ---- 计算本帧与上一帧的时间差（秒），用于帧率无关的物理/动画更新 ----
		uint64_t nowTime = SDL_GetTicks();
		float deltaTime = (nowTime - prevTime) / 1000.0f;

		// ---- 处理 SDL 事件队列 ----
		SDL_Event event{ 0 };
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_QUIT: // 用户点击关闭按钮等退出请求
			{
				running = false;
				break;
			}
			case SDL_EVENT_WINDOW_RESIZED: // 窗口大小改变，更新记录的宽高
			{
				state.width = event.window.data1;
				state.height = event.window.data2;
				break;
			}
			case SDL_EVENT_KEY_DOWN: // 按键按下
			{
				handleKeyInput(state, gs, gs.player(), event.key.scancode, true);
				break;
			}
			case SDL_EVENT_KEY_UP: // 按键松开
			{
				handleKeyInput(state, gs, gs.player(), event.key.scancode, false);
				// F12 切换调试模式（显示碰撞框、调试信息）
				if (event.key.scancode == SDL_SCANCODE_F12)
				{
					gs.debugMode = !gs.debugMode;
				}
				// F11 切换全屏/窗口模式
				else if (event.key.scancode == SDL_SCANCODE_F11)
				{
					state.fullscreen = !state.fullscreen;
					SDL_SetWindowFullscreen(state.window, state.fullscreen);
				}
				break;
			}
			}
		}

		// ---- 更新所有“地形层”和“角色层”对象（物理、动画、状态机、碰撞）----
		for (auto& layer : gs.layers)
		{
			for (GameObject& obj : layer)
			{
				update(state, gs, res, obj, deltaTime);
			}
		}

		// ---- 更新所有子弹对象 ----
		for (GameObject& bullet : gs.bullets)
		{
			update(state, gs, res, bullet, deltaTime);
		}

		// ---- 根据玩家位置重新计算摄像机视口的 x 坐标（水平跟随玩家，居中显示）----
		gs.mapViewport.x = (gs.player().position.x + TILE_SIZE / 2) - gs.mapViewport.w / 2;

		// ============================================================
		// 渲染阶段
		// ============================================================
		// 清屏，填充深紫色背景色
		SDL_SetRenderDrawColor(state.renderer, 20, 10, 30, 255);
		SDL_RenderClear(state.renderer);

		// ---- 绘制背景图层（从远到近：静止天空 -> 三层视差滚动）----
		SDL_RenderTexture(state.renderer, res.texBg1, nullptr, nullptr); // 最底层天空，铺满整个屏幕，不滚动
		// 滚动系数越小代表离摄像机越“远”，移动越慢，营造景深感（视差滚动）
		drawParalaxBackground(state.renderer, res.texBg4, gs.player().velocity.x,
			gs.bg4Scroll, 0.075f, deltaTime);
		drawParalaxBackground(state.renderer, res.texBg3, gs.player().velocity.x,
			gs.bg3Scroll, 0.150f, deltaTime);
		drawParalaxBackground(state.renderer, res.texBg2, gs.player().velocity.x,
			gs.bg2Scroll, 0.3f, deltaTime);

		// ---- 绘制背景装饰瓦片（例如砖块），位置需要减去视口 x 偏移以实现摄像机跟随 ----
		for (GameObject& obj : gs.backgroundTiles)
		{
			SDL_FRect dst{
				.x = obj.position.x - gs.mapViewport.x,
				.y = obj.position.y,
				.w = static_cast<float>(obj.texture->w),
				.h = static_cast<float>(obj.texture->h)
			};
			SDL_RenderTexture(state.renderer, obj.texture, nullptr, &dst);
		}

		// ---- 绘制所有地形/角色对象（关卡瓦片层 + 角色层）----
		for (auto& layer : gs.layers)
		{
			for (GameObject& obj : layer)
			{
				drawObject(state, gs, obj, TILE_SIZE, TILE_SIZE, deltaTime);
			}
		}

		// ---- 绘制所有活跃状态的子弹 ----
		for (GameObject& bullet : gs.bullets)
		{
			if (bullet.data.bullet.state != BulletState::inactive)
			{
				drawObject(state, gs, bullet, bullet.collider.w, bullet.collider.h, deltaTime);
			}
		}

		// ---- 绘制前景装饰瓦片（例如草），会遮挡在角色和地形之上 ----
		for (GameObject& obj : gs.foregroundTiles)
		{
			SDL_FRect dst{
				.x = obj.position.x - gs.mapViewport.x,
				.y = obj.position.y,
				.w = static_cast<float>(obj.texture->w),
				.h = static_cast<float>(obj.texture->h)
			};
			SDL_RenderTexture(state.renderer, obj.texture, nullptr, &dst);
		}

		// ---- 调试模式下，在左上角显示调试文本（玩家状态、子弹数量、是否着地）----
		if (gs.debugMode)
		{
			SDL_SetRenderDrawColor(state.renderer, 255, 255, 255, 255);
			SDL_RenderDebugText(state.renderer, 5, 5,
				std::format("S: {}, B: {}, G: {}",
					static_cast<int>(gs.player().data.player.state), gs.bullets.size(), gs.player().grounded).c_str());
		}

		// ---- 交换缓冲区，将本帧内容显示到屏幕上 ----
		SDL_RenderPresent(state.renderer);
		prevTime = nowTime; // 更新“上一帧时间”，供下一帧计算 deltaTime
	}

	// ---- 游戏结束，释放资源 ----
	res.unload();
	cleanup(state);
	return 0;
}

// ============================================================
// initialize：初始化 SDL、创建窗口/渲染器、初始化音频库
// ============================================================
bool initialize(SDLState& state)
{
	bool initSuccess = true;

	// 初始化 SDL 视频子系统
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initializing SDL3", nullptr);
		initSuccess = false;
	}

	// 创建可调整大小的窗口
	state.window = SDL_CreateWindow("SDL3 Demo", state.width, state.height, SDL_WINDOW_RESIZABLE);
	if (!state.window)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating window", nullptr);
		cleanup(state);
		initSuccess = false;
	}

	// 创建渲染器（第二个参数为 nullptr 表示让 SDL 自动选择合适的渲染驱动）
	state.renderer = SDL_CreateRenderer(state.window, nullptr);
	if (!state.renderer)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating renderer", state.window);
		cleanup(state);
		initSuccess = false;
	}
	// 开启垂直同步，避免画面撕裂
	SDL_SetRenderVSync(state.renderer, 1);

	// 设置逻辑分辨率与呈现方式：LETTERBOX 表示按比例缩放并在多余空间加黑边，
	// 保证游戏内容始终以固定的 logW x logH 分辨率渲染，不受窗口实际大小影响
	SDL_SetRenderLogicalPresentation(state.renderer, state.logW, state.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	// 初始化 SDL_mixer 音频库
	if (!MIX_Init())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating audio device", state.window);
		cleanup(state);
		initSuccess = false;
	}

	// 根据配置设置是否全屏
	SDL_SetWindowFullscreen(state.window, state.fullscreen);

	return initSuccess;
}

// ============================================================
// cleanup：释放渲染器、窗口，并退出 SDL
// ============================================================
void cleanup(SDLState& state)
{
	SDL_DestroyRenderer(state.renderer);
	SDL_DestroyWindow(state.window);
	SDL_Quit();
}

// ============================================================
// drawObject：绘制一个游戏对象的精灵动画帧
// ============================================================
void drawObject(const SDLState& state, GameState& gs, GameObject& obj,
	float width, float height, float deltaTime)
{
	// 计算精灵表（sprite sheet）中当前帧的 x 坐标：
	// 若对象正在播放动画，则取动画当前帧序号 * 单帧宽度；
	// 否则使用固定的 spriteFrame（例如死亡后停在最后一帧）
	float srcX = obj.currentAnimation != -1
		? obj.animations[obj.currentAnimation].currentFrame() * width
		: (obj.spriteFrame - 1) * width;
	SDL_FRect src{
		.x = srcX,
		.y = 0,
		.w = width,
		.h = height
	};

	// 目标绘制位置：世界坐标减去摄像机视口的 x 偏移，得到屏幕坐标
	SDL_FRect dst{
		.x = obj.position.x - gs.mapViewport.x,
		.y = obj.position.y,
		.w = width,
		.h = height
	};

	// 根据对象朝向决定是否水平翻转贴图（-1 表示朝左，需要翻转）
	SDL_FlipMode flipMode = obj.direction == -1 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

	if (!obj.shouldFlash)
	{
		// 正常绘制
		SDL_RenderTextureRotated(state.renderer, obj.texture, &src, &dst, 0, nullptr, flipMode);
	}
	else
	{
		// 受伤/闪烁效果：临时叠加偏红的颜色调制（放大红色通道），绘制后立即恢复原色
		SDL_SetTextureColorModFloat(obj.texture, 2.5f, 1.0f, 1.0f);
		SDL_RenderTextureRotated(state.renderer, obj.texture, &src, &dst, 0, nullptr, flipMode);
		SDL_SetTextureColorModFloat(obj.texture, 1.0f, 1.0f, 1.0f);

		// 闪烁计时器计时结束后，关闭闪烁效果
		if (obj.flashTimer.step(deltaTime))
		{
			obj.shouldFlash = false;
		}
	}

	// ---- 调试模式：绘制碰撞盒和“地面检测传感器”----
	if (gs.debugMode)
	{
		// 对象的碰撞矩形（红色半透明）
		SDL_FRect rectA{
			.x = obj.position.x + obj.collider.x - gs.mapViewport.x,
			.y = obj.position.y + obj.collider.y,
			.w = obj.collider.w,
			.h = obj.collider.h
		};
		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);

		SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 150);
		SDL_RenderFillRect(state.renderer, &rectA);

		// 碰撞盒底部的一条细线（蓝色），用于直观展示“地面检测线”位置
		SDL_FRect sensor{
			.x = obj.position.x + obj.collider.x - gs.mapViewport.x,
			.y = obj.position.y + obj.collider.y + obj.collider.h,
			.w = obj.collider.w, .h = 1
		};
		SDL_SetRenderDrawColor(state.renderer, 0, 0, 255, 150);
		SDL_RenderFillRect(state.renderer, &sensor);

		SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);
	}
}

// ============================================================
// update：每帧更新一个游戏对象（动画、物理、状态机、输入、碰撞）
// ============================================================
void update(const SDLState& state, GameState& gs, Resources& res, GameObject& obj, float deltaTime)
{
	// ---- 推进动画播放进度 ----
	if (obj.currentAnimation != -1)
	{
		obj.animations[obj.currentAnimation].step(deltaTime);
	}

	// ---- 重力：动态对象且未着地时施加向下加速度 ----
	if (obj.dynamic && !obj.grounded)
	{
		obj.velocity += glm::vec2(0, 500) * deltaTime;
	}

	float currentDirection = 0; // 本帧由输入/AI 决定的水平移动方向（-1 左，1 右，0 无）

	// ============================================================
	// 玩家逻辑
	// ============================================================
	if (obj.type == ObjectType::player)
	{
		// 读取 A/D 键状态，得到期望移动方向
		if (state.keys[SDL_SCANCODE_A])
		{
			currentDirection += -1;
		}
		if (state.keys[SDL_SCANCODE_D])
		{
			currentDirection += 1;
		}

		// 推进射击冷却计时器
		Timer& weaponTimer = obj.data.player.weaponTimer;
		weaponTimer.step(deltaTime);

		// lambda：处理射击逻辑，根据是否按下 J 键决定使用普通贴图/动画还是射击贴图/动画，
		// 并在冷却结束时生成一颗新子弹
		const auto handleShooting = [&state, &gs, &res, &obj, &weaponTimer](
			SDL_Texture* tex, SDL_Texture* shootTex, int animIndex, int shootAnimIndex)
			{
				if (state.keys[SDL_SCANCODE_J])
				{
					// 切换为射击贴图/动画
					obj.texture = shootTex;
					obj.currentAnimation = shootAnimIndex;

					// 冷却结束才允许真正发射子弹
					if (weaponTimer.isTimeout())
					{
						weaponTimer.reset();

						// ---- 构造新的子弹对象 ----
						GameObject bullet;
						bullet.data.bullet = BulletData();
						bullet.type = ObjectType::bullet;
						bullet.direction = gs.player().direction; // 子弹方向与玩家朝向一致
						bullet.texture = res.texBullet;
						bullet.currentAnimation = res.ANIM_BULLET_MOVING;
						bullet.collider = SDL_FRect{
							.x = 0, .y = 0,
							.w = static_cast<float>(res.texBullet->h), // 用贴图高度作为碰撞盒宽高（假设子弹为正方形贴图）
							.h = static_cast<float>(res.texBullet->h)
						};

						// 给子弹的垂直速度加入一点随机偏移，让弹道看起来更自然
						const int yVariation = 40;
						const float yVelocity = SDL_rand(yVariation) - yVariation / 2.0f;
						bullet.velocity = glm::vec2(
							obj.velocity.x + 600.0f * obj.direction, // 玩家自身速度 + 固定射速，沿玩家朝向
							yVelocity
						);
						bullet.maxSpeedX = 1000.0f;
						bullet.animations = res.bulletAnims;

						// 根据玩家朝向，在玩家左侧或右侧一定偏移处生成子弹（避免子弹出现在玩家身体正中间）
						const float left = 4;
						const float right = 24;
						const float t = (obj.direction + 1) / 2.0f; // 将 direction(-1..1) 映射到 0..1
						const float xOffset = left + right * t;     // 在 left 和 right 之间做线性插值
						bullet.position = glm::vec2(
							obj.position.x + xOffset,
							obj.position.y + TILE_SIZE / 2 + 1
						);

						// ---- 对象池复用：优先寻找一个“非活跃”的子弹槽位并覆盖，避免频繁分配内存 ----
						bool foundInactive = false;
						for (int i = 0; i < gs.bullets.size() && !foundInactive; i++)
						{
							if (gs.bullets[i].data.bullet.state == BulletState::inactive)
							{
								foundInactive = true;
								gs.bullets[i] = bullet;
							}
						}
						// 若没有空闲槽位，则在池子末尾新增一个
						if (!foundInactive)
						{
							gs.bullets.push_back(bullet);
						}

						// 播放射击音效
						MIX_PlayAudio(nullptr, res.audioShoot);
					}
				}
				else
				{
					// 未按射击键，使用普通贴图/动画
					obj.texture = tex;
					obj.currentAnimation = animIndex;
				}
			};

		// ---- 玩家状态机：idle（待机）/ running（奔跑）/ jumping（跳跃）----
		switch (obj.data.player.state)
		{
		case PlayerState::idle:
		{
			// 有方向输入 -> 切换到奔跑状态
			if (currentDirection)
			{
				obj.data.player.state = PlayerState::running;
			}
			else
			{
				// 无输入时做减速处理（模拟摩擦力，使角色平滑停下而非瞬间停止）
				if (obj.velocity.x)
				{
					const float factor = obj.velocity.x > 0 ? -1.5f : 1.5f;
					float amount = factor * obj.acceleration.x * deltaTime;
					// 若本次减速量超过当前速度，直接归零，避免因减速过量导致反向运动
					if (std::abs(obj.velocity.x) < std::abs(amount))
					{
						obj.velocity.x = 0;
					}
					else
					{
						obj.velocity.x += amount;
					}
				}
			}
			handleShooting(res.texIdle, res.texShoot, res.ANIM_PLAYER_IDLE, res.ANIM_PLAYER_SHOOT);
			break;
		}
		case PlayerState::running:
		{
			// 无方向输入 -> 切换回待机状态
			if (!currentDirection)
			{
				obj.data.player.state = PlayerState::idle;
			}

			// 若当前速度方向与朝向相反（急转向）且处于地面上，则播放“滑行”动画
			if (obj.velocity.x * obj.direction < 0 && obj.grounded)
			{
				handleShooting(res.texSlide, res.texSlideShoot, res.ANIM_PLAYER_SLIDE, res.ANIM_PLAYER_SLIDE_SHOOT);
			}
			else
			{
				handleShooting(res.texRun, res.texRunShoot, res.ANIM_PLAYER_RUN, res.ANIM_PLAYER_RUN);
			}
			break;
		}
		case PlayerState::jumping:
		{
			// 跳跃时使用奔跑贴图/动画（简化处理，未做专门的跳跃贴图）
			handleShooting(res.texRun, res.texRunShoot, res.ANIM_PLAYER_RUN, res.ANIM_PLAYER_RUN);
			break;
		}
		}
	}
	// ============================================================
	// 子弹逻辑
	// ============================================================
	else if (obj.type == ObjectType::bullet)
	{
		switch (obj.data.bullet.state)
		{
		case BulletState::moving:
		{
			// 若子弹飞出屏幕视口范围（上下左右任一方向），标记为“非活跃”，供对象池回收复用
			if (obj.position.x - gs.mapViewport.x < 0 || // 超出左边界
				obj.position.x - gs.mapViewport.x > state.logW || // 超出右边界
				obj.position.y - gs.mapViewport.y < 0 || // 超出上边界
				obj.position.y - gs.mapViewport.y > state.logH) // 超出下边界
			{
				obj.data.bullet.state = BulletState::inactive;
			}
			break;
		}
		case BulletState::colliding:
		{
			// 命中动画播放完毕后，将子弹标记为非活跃
			if (obj.animations[obj.currentAnimation].isDone())
			{
				obj.data.bullet.state = BulletState::inactive;
			}
			break;
		}
		}
	}
	// ============================================================
	// 敌人逻辑
	// ============================================================
	else if (obj.type == ObjectType::enemy)
	{
		EnemyData& d = obj.data.enemy;
		switch (d.state)
		{
		case EnemyState::shambling: // 蹒跚追击状态
		{
			glm::vec2 playerDir = gs.player().position - obj.position;
			// 玩家在附近（距离小于100像素）时才会主动追击
			if (glm::length(playerDir) < 100)
			{
				currentDirection = playerDir.x < 0 ? -1 : 1;
				obj.acceleration = glm::vec2(30, 0);
			}
			else
			{
				// 玩家不在感知范围内，停止移动
				obj.acceleration = glm::vec2(0);
				obj.velocity.x = 0;
			}
			break;
		}
		case EnemyState::damaged: // 受伤硬直状态
		{
			// 硬直计时结束后恢复到蹒跚状态，并切回正常贴图/动画
			if (d.damageTimer.step(deltaTime))
			{
				d.state = EnemyState::shambling;
				obj.texture = res.texEnemy;
				obj.currentAnimation = res.ANIM_ENEMY;
			}
			break;
		}
		case EnemyState::dead: // 死亡状态
		{
			obj.velocity.x = 0; // 死亡后不再移动
			// 死亡动画播放完毕后，停止动画播放，固定停留在最后一帧（第18帧）
			if (obj.currentAnimation != -1 &&
				obj.animations[obj.currentAnimation].isDone())
			{
				obj.currentAnimation = -1;
				obj.spriteFrame = 18;
			}
			break;
		}
		}
	}

	// ---- 若本帧有有效的移动方向输入，则更新对象的朝向（用于翻转贴图、子弹发射方向等）----
	if (currentDirection)
	{
		obj.direction = currentDirection;
	}

	// ---- 物理积分：加速度 -> 速度 ----
	obj.velocity += currentDirection * obj.acceleration * deltaTime;
	// 限制水平速度不超过最大速度（超出时直接钳制为“方向 * 最大速度”）
	if (std::abs(obj.velocity.x) > obj.maxSpeedX)
	{
		obj.velocity.x = currentDirection * obj.maxSpeedX;
	}

	// ---- 物理积分：速度 -> 位置 ----
	obj.position += obj.velocity * deltaTime;

	// ============================================================
	// 碰撞检测：将当前对象与“地形层”“角色层”中所有其他对象逐一检测
	// ============================================================
	bool wasGrounded = obj.grounded;
	obj.grounded = false; // 每帧先假设未着地，若检测到从上方碰撞则在 collisionResponse 中重新置为 true
	for (auto& layer : gs.layers)
	{
		for (GameObject& objB : layer)
		{
			if (&obj != &objB) // 避免自己与自己检测碰撞
			{
				checkCollision(state, gs, res, obj, objB, deltaTime);
			}
		}
	}

	// 碰撞响应函数会更新 obj.grounded；此处检测“本帧刚刚从空中落地”的时刻，触发状态切换
	if (obj.grounded && !wasGrounded)
	{
		if (obj.grounded && obj.type == ObjectType::player)
		{
			obj.data.player.state = PlayerState::running;
		}
	}
}

// ============================================================
// collisionResponse：根据碰撞双方的类型，执行相应的碰撞响应逻辑
// ============================================================
void collisionResponse(const SDLState& state, GameState& gs, Resources& res,
	const SDL_FRect& rectA, const SDL_FRect& rectB, const glm::vec2& overlap,
	GameObject& objA, GameObject& objB, float deltaTime)
{
	// lambda：通用的“推出”碰撞响应——将 objA 沿重叠量最小的轴推出 objB，并清零对应方向速度
	const auto genericResponse = [&]()
		{
			// x 轴方向重叠量更小 -> 判定为水平方向碰撞
			if (overlap.x < overlap.y)
			{
				if (objA.position.x < objB.position.x) // objA 在 objB 左侧，从左边被推出
				{
					objA.position.x -= overlap.x;
				}
				else // objA 在 objB 右侧，从右边被推出
				{
					objA.position.x += overlap.x;
				}
				objA.velocity.x = 0;
			}
			else // y 轴方向重叠量更小或相等 -> 判定为垂直方向碰撞
			{
				if (objA.position.y < objB.position.y) // objA 在 objB 上方，从上面被推出（即“站在”objB 上）
				{
					objA.position.y -= overlap.y;
					objA.grounded = true; // 标记为已着地
				}
				else // objA 在 objB 下方，从下面被推出（例如撞到天花板）
				{
					objA.position.y += overlap.y;
				}
				objA.velocity.y = 0;
			}
		};

	// ---- 根据 objA 的类型分派不同的碰撞处理逻辑 ----
	if (objA.type == ObjectType::player)
	{
		switch (objB.type)
		{
		case ObjectType::level: // 玩家与地形碰撞：直接做通用推出处理
		{
			genericResponse();
			break;
		}
		case ObjectType::enemy: // 玩家与存活敌人碰撞：反弹玩家（类似受击弹开效果）
		{
			if (objB.data.enemy.state != EnemyState::dead)
			{
				glm::vec2 prevVel = objA.velocity;
				genericResponse();
				objA.velocity = -prevVel; // 速度反向，形成“弹开”效果
			}
			// 若敌人已死亡，则不做任何处理，玩家可自由穿过尸体
			break;
		}
		}
	}
	else if (objA.type == ObjectType::bullet)
	{
		bool passthrough = false; // 是否“穿透”本次碰撞（不触发命中效果、不停止子弹）
		switch (objA.data.bullet.state)
		{
		case BulletState::moving:
		{
			switch (objB.type)
			{
			case ObjectType::level: // 子弹击中地形：播放击墙音效
			{
				MIX_PlayAudio(nullptr, res.audioShootHit);
				break;
			}
			case ObjectType::enemy: // 子弹击中敌人
			{
				EnemyData& d = objB.data.enemy;
				if (d.state != EnemyState::dead)
				{
					// 敌人被击退方向与子弹方向相反
					objB.direction = -objA.direction;
					// 触发受伤闪烁效果
					objB.shouldFlash = true;
					objB.flashTimer.reset();
					objB.texture = res.texEnemyHit;
					objB.currentAnimation = res.ANIM_ENEMY_HIT;
					d.state = EnemyState::damaged;

					// 扣血，若血量降至0以下则切换为死亡状态并播放死亡动画
					d.healthPoints -= 10;
					if (d.healthPoints <= 0)
					{
						d.state = EnemyState::dead;
						objB.texture = res.texEnemyDie;
						objB.currentAnimation = res.ANIM_ENEMY_DIE;
					}
					MIX_PlayAudio(nullptr, res.audioEnemyHit);
				}
				else
				{
					// 敌人已死亡：子弹直接穿过，不触发命中效果
					passthrough = true;
				}
				break;
			}
			}

			// 未被标记为“穿透”时，才让子弹真正停止并播放命中动画
			if (!passthrough)
			{
				genericResponse();
				objA.velocity *= 0; // 子弹停止移动
				objA.data.bullet.state = BulletState::colliding;
				objA.texture = res.texBulletHit;
				objA.currentAnimation = res.ANIM_BULLET_HIT;
			}
			break;
		}
		// 注：BulletState::colliding / inactive 状态下不再处理新的碰撞
		}
	}
	else if (objA.type == ObjectType::enemy)
	{
		// 敌人与其他对象（通常是地形）碰撞：使用通用推出处理
		genericResponse();
	}
}

// ============================================================
// intersectAABB：轴对齐包围盒相交测试
// 返回是否相交；若相交，overlap 输出 x/y 两个方向上的重叠深度
// ============================================================
bool intersectAABB(const SDL_FRect& a, const SDL_FRect& b, glm::vec2& overlap)
{
	// 分别计算两个矩形在 x、y 方向上的最小值/最大值边界
	const float minXA = a.x;
	const float maxXA = a.x + a.w;
	const float minYA = a.y;
	const float maxYA = a.y + a.h;
	const float minXB = b.x;
	const float maxXB = b.x + b.w;
	const float minYB = b.y;
	const float maxYB = b.y + b.h;

	// x 方向区间相交 且 y 方向区间相交（y 方向用 <=/>= 是为了让“贴合”在地面上时也能判定为接触）
	if ((minXA < maxXB && maxXA > minXB) &&
		(minYA <= maxYB && maxYA >= minYB))
	{
		// 重叠深度 = 两个方向上“较小的那个穿透距离”
		overlap.x = std::min(maxXA - minXB, maxXB - minXA);
		overlap.y = std::min(maxYA - minYB, maxYB - minYA);
		return true;
	}
	return false;
}

// ============================================================
// checkCollision：构造两个对象的世界坐标碰撞矩形，检测相交并触发响应
// ============================================================
void checkCollision(const SDLState& state, GameState& gs, Resources& res,
	GameObject& a, GameObject& b, float deltaTime)
{
	// 对象的碰撞矩形 = 对象位置 + collider 的局部偏移/大小
	SDL_FRect rectA{
		.x = a.position.x + a.collider.x,
		.y = a.position.y + a.collider.y,
		.w = a.collider.w,
		.h = a.collider.h
	};
	SDL_FRect rectB{
		.x = b.position.x + b.collider.x,
		.y = b.position.y + b.collider.y,
		.w = b.collider.w,
		.h = b.collider.h
	};

	glm::vec2 resolution{ 0 };
	if (intersectAABB(rectA, rectB, resolution))
	{
		// 检测到相交，调用碰撞响应函数进行具体处理
		collisionResponse(state, gs, res, rectA, rectB, resolution, a, b, deltaTime);
	}
}

// ============================================================
// createTiles：根据硬编码的地图数组数据，生成关卡中的地形/背景/前景/角色对象
// ============================================================
void createTiles(const SDLState& state, GameState& gs, const Resources& res)
{
	/*
		数字含义：
		1 - 地面 (Ground)
		2 - 面板/平台 (Panel)
		3 - 敌人 (Enemy)
		4 - 玩家出生点 (Player)
		5 - 草（前景装饰）(Grass)
		6 - 砖块（背景装饰）(Brick)
	*/

	// 主图层：包含地面、平台、敌人出生点、玩家出生点
	short map[MAP_ROWS][MAP_COLS] = {
		4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 3, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 3, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 0, 2, 2, 2, 2, 0, 2, 2, 2, 0, 0, 3, 2, 2, 2, 2, 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 2, 0, 2, 2, 0, 0, 0, 3, 0, 0, 3, 0, 2, 2, 2, 2, 2, 0, 0, 2, 2, 0, 3, 0, 0, 3, 0, 2, 3, 3, 3, 0, 2, 0, 3, 3, 0, 0, 3, 0, 3, 0, 3, 0, 0, 0, 3,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1

	};

	// 背景装饰图层：砖块（在角色之后渲染，不参与碰撞逻辑，仅作视觉装饰）
	short background[MAP_ROWS][MAP_COLS] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 6, 6, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};

	// 前景装饰图层：草（在最上层渲染，会遮挡角色和地形，仅作视觉装饰）
	short foreground[MAP_ROWS][MAP_COLS] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};

	// lambda：遍历给定的二维数组，根据每个格子的数值创建对应的对象并加入相应容器
	const auto loadMap = [&state, &gs, &res](short layer[MAP_ROWS][MAP_COLS])
		{
			// 内部 lambda：根据行列坐标和贴图/类型构造一个基础 GameObject
			// 注意：世界坐标的 y 是从底部向上计算的（MAP_ROWS - r 行数），使地图“贴地”对齐
			const auto createObject = [&state](int r, int c, SDL_Texture* tex, ObjectType type)
				{
					GameObject o;
					o.type = type;
					o.position = glm::vec2(c * TILE_SIZE, state.logH - (MAP_ROWS - r) * TILE_SIZE);
					o.texture = tex;
					o.collider = { .x = 0, .y = 0, .w = TILE_SIZE, .h = TILE_SIZE };
					return o;
				};

			// 遍历地图的每一个格子
			for (int r = 0; r < MAP_ROWS; r++)
			{
				for (int c = 0; c < MAP_COLS; c++)
				{
					switch (layer[r][c])
					{
					case 1: // 地面：加入地形层，参与碰撞
					{
						GameObject o = createObject(r, c, res.texGround, ObjectType::level);
						gs.layers[LAYER_IDX_LEVEL].push_back(o);
						break;
					}
					case 2: // 平台/面板：同样加入地形层
					{
						GameObject o = createObject(r, c, res.texPanel, ObjectType::level);
						gs.layers[LAYER_IDX_LEVEL].push_back(o);
						break;
					}
					case 3: // 敌人：初始化敌人专属数据、动画、碰撞盒（比整格 TILE_SIZE 小，贴合角色轮廓）
					{
						GameObject o = createObject(r, c, res.texEnemy, ObjectType::enemy);
						o.data.enemy = EnemyData();
						o.currentAnimation = res.ANIM_ENEMY;
						o.animations = res.enemyAnims;
						o.collider = SDL_FRect{
							.x = 10, .y = 4, .w = 12, .h = 28
						};
						o.maxSpeedX = 15;
						o.dynamic = true; // 受重力影响
						gs.layers[LAYER_IDX_CHARACTERS].push_back(o);
						break;
					}
					case 4: // 玩家出生点：初始化玩家专属数据、动画、物理参数、碰撞盒
					{
						GameObject player = createObject(r, c, res.texIdle, ObjectType::player);
						player.data.player = PlayerData();
						player.animations = res.playerAnims;
						player.currentAnimation = res.ANIM_PLAYER_IDLE;
						player.acceleration = glm::vec2(300, 0);
						player.maxSpeedX = 100;
						player.dynamic = true; // 受重力影响
						player.collider = {
							.x = 11, .y = 6,
							.w = 10, .h = 26
						};
						gs.layers[LAYER_IDX_CHARACTERS].push_back(player);
						// 记录玩家在角色层数组中的索引，供 gs.player() 快速访问
						gs.playerIndex = gs.layers[LAYER_IDX_CHARACTERS].size() - 1;
						break;
					}
					case 5: // 草：纯视觉前景装饰，不参与碰撞
					{
						GameObject o = createObject(r, c, res.texGrass, ObjectType::level);
						gs.foregroundTiles.push_back(o);
						break;
					}
					case 6: // 砖块：纯视觉背景装饰，不参与碰撞
					{
						GameObject o = createObject(r, c, res.texBrick, ObjectType::level);
						gs.backgroundTiles.push_back(o);
						break;
					}
					}
				}
			}
		};

	// 依次加载三张地图数据（主图层、背景装饰层、前景装饰层）
	loadMap(map);
	loadMap(background);
	loadMap(foreground);

	// 断言：确保地图中一定包含玩家出生点，否则说明关卡数据配置有误
	assert(gs.playerIndex != -1);
}

// ============================================================
// handleKeyInput：处理键盘输入，目前仅实现跳跃逻辑
// ============================================================
void handleKeyInput(const SDLState& state, GameState& gs, GameObject& obj,
	SDL_Scancode key, bool keyDown)
{
	const float JUMP_FORCE = -200.0f; // 负值表示向上（SDL/屏幕坐标系 y 轴向下为正）

	// lambda：K 键按下 且 当前处于地面上时，触发一次跳跃
	const auto jump = [&]()
		{
			if (key == SDL_SCANCODE_K && keyDown && obj.grounded)
			{
				obj.data.player.state = PlayerState::jumping;
				obj.velocity.y += JUMP_FORCE; // 瞬间给予一个向上的初速度
			}
		};

	if (obj.type == ObjectType::player)
	{
		// 仅在 idle 或 running 状态下允许起跳（避免空中二段跳等）
		switch (obj.data.player.state)
		{
		case PlayerState::idle:
		{
			jump();
			break;
		}
		case PlayerState::running:
		{
			jump();
			break;
		}
		// 注意：jumping 状态未在此处理，意味着跳跃状态下再次按 K 不会触发新的跳跃（防止二段跳）
		}
	}
}

// ============================================================
// drawParalaxBackground：绘制单层视差滚动背景
// ============================================================
void drawParalaxBackground(SDL_Renderer* renderer, SDL_Texture* texture,
	float xVelocity, float& scrollPos, float scrollFactor, float deltaTime)
{
	// 根据玩家水平速度和该层的滚动系数，反向移动背景，制造“摄像机跟随玩家”的错觉
	// scrollFactor 越小，背景移动越慢，视觉上显得越“远”（视差效果的核心）
	scrollPos -= xVelocity * scrollFactor * deltaTime;

	// 当背景滚动超过一张贴图的宽度后，重置滚动位置为 0，实现无缝循环平铺
	if (scrollPos <= -texture->w)
	{
		scrollPos = 0;
	}

	SDL_FRect dst{
		.x = scrollPos, .y = 30,
		.w = texture->w * 2.0f, // 宽度设为贴图两倍，配合平铺渲染保证无缝衔接、覆盖整个可视范围
		.h = static_cast<float>(texture->h)
	};

	// 使用平铺渲染（Tiled）在 dst 矩形范围内重复绘制贴图，参数 1 为平铺缩放比例
	SDL_RenderTextureTiled(renderer, texture, nullptr, 1, &dst);
}
