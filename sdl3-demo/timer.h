#pragma once

// ============================================================
// Timer：简单的循环计时器
// 用于跟踪“经过了多长时间”，并在达到指定时长（length）时触发一次性的“超时”标记。
// 常用于：动画帧切换、技能冷却、受伤硬直、闪烁效果等需要“定时触发一次”的场景。
// ============================================================
class Timer
{
	float length; // 计时器的目标时长（秒），到达该时长即视为“超时”
	float time;   // 当前已经累计经过的时间（秒），随 step() 调用不断增加
	bool timeout; // 是否已经超时的标记；注意此标记不会自动清除，需要外部通过 reset() 重置

public:
	// 构造函数：指定计时器的目标时长，初始已用时间为 0，初始未超时
	Timer(float length) : length(length), time(0), timeout(false)
	{
	}

	// 每帧调用：推进计时器，累加本帧经过的时间（deltaTime，单位：秒）
	// 返回值：本次调用是否“恰好触发了一次超时”（即时间刚刚跨过 length 这一时刻）
	bool step(float deltaTime)
	{
		time += deltaTime;
		if (time >= length)
		{
			// 达到或超过目标时长：
			// 用“减去 length”而非直接置 0，是为了保留超出的那部分时间（例如因为帧间隔较大，
			// 一次 step 可能超过 length 不止一点点），从而让计时器可以直接循环使用，
			// 不会因为“多出来的时间”被丢弃而导致长期运行后计时逐渐产生偏差
			time -= length;
			timeout = true; // 标记为已超时（此标记会一直保持 true，直到调用 reset()）
			return true;    // 告知调用者：本次 step 触发了一次超时事件
		}
		return false; // 尚未达到目标时长，本次未触发超时
	}

	// 查询是否已经超时（一旦置为 true，会一直保持，直到手动 reset()）
	bool isTimeout() const { return timeout; }

	// 获取当前已经累计的时间（秒）
	float getTime() const { return time; }

	// 获取计时器的目标时长（秒）
	float getLength() const { return length; }

	// 重置计时器：清零已用时间，并清除超时标记，通常在“消费”完一次超时事件后调用，
	// 以便计时器可以重新开始下一轮计时（例如武器冷却计时器在发射一次子弹后调用 reset()）
	void reset() { time = 0, timeout = false; }
};