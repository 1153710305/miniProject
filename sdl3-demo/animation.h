#pragma once
#include "timer.h"

// ============================================================
// Animation：简单的帧动画类
// 通过一个 Timer（计时器）配合“动画总时长 / 帧数”换算出当前应显示第几帧，
// 而不是显式记录“当前帧序号”，这样帧的推进完全由时间驱动，天然支持不同帧率下的平滑播放。
// ============================================================
class Animation
{
	Timer timer;      // 内部计时器，累计已播放的时间，并保存动画的总时长（length）
	int frameCount;    // 该动画总共包含多少帧（即精灵表中横向排列的帧图数量）

public:
	// 默认构造函数：帧数为 0、时长为 0（表示一个“空”动画，通常不会被实际播放，仅作占位用）
	Animation() : timer(0), frameCount(0) {}

	// 构造函数：指定帧数与动画播放一轮所需的总时长（单位：秒）
	// 注意：成员初始化列表的实际执行顺序按照成员在类中声明的顺序（timer 先声明，frameCount 后声明），
	// 因此实际初始化顺序是 timer 先于 frameCount，与代码书写顺序（frameCount(frameCount), timer(length)）不同，
	// 但由于两者互不依赖，不影响最终结果
	Animation(int frameCount, float length) : frameCount(frameCount), timer(length)
	{
	}

	// 获取动画播放一轮的总时长（秒）
	float getLength() const { return timer.getLength(); }

	// 计算当前应显示的帧序号（从 0 开始）：
	// 用“已播放时间 / 总时长”得到当前播放进度（0.0 ~ 1.0 之间的比例），
	// 再乘以总帧数并向下取整（static_cast<int> 截断小数部分），即可得到当前帧的索引。
	// 例如：总时长 1.0 秒、共 4 帧，播放到 0.6 秒时，进度为 0.6，0.6 * 4 = 2.4，取整后为第 2 帧（从0开始数，即第3帧图）
	int currentFrame() const
	{
		return static_cast<int>(timer.getTime() / timer.getLength() * frameCount);
	}

	// 每帧调用：推进内部计时器，累加已经过去的时间（deltaTime，单位：秒）
	void step(float deltaTime)
	{
		timer.step(deltaTime);
	}

	// 判断动画是否已经播放完毕（计时器超时，即已到达/超过总时长）
	// 常用于一次性播放的动画（如受击、死亡动画），播放完后不再循环，停留在最后一帧
	bool isDone() const { return timer.isTimeout(); }
};