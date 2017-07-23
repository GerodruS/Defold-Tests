// copyright britzl
// Extension lib defines
#define LIB_NAME "Timer"
#define MODULE_NAME "timer"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <stdlib.h>
#include <math.h>

#ifdef DM_PLATFORM_WINDOWS
#include <windows.h>
// Timestamp calculation from luasocket timeout.c
static double GetTimestamp(void)
{
	FILETIME ft;
	double t;

	GetSystemTimeAsFileTime(&ft);
	/* Windows file time (time since January 1, 1601 (UTC)) */
	t = ft.dwLowDateTime / 1.0e7 + ft.dwHighDateTime * (4294967296.0 / 1.0e7);
	/* convert to Unix Epoch time (time since January 1, 1970 (UTC)) */
	return t - 11644473600.0;
}
#else
#include <sys/time.h>
static double GetTimestamp(void)
{
	struct timeval v;

	gettimeofday(&v, (struct timezone *)NULL);
	/* Unix Epoch time (time since January 1, 1970 (UTC)) */
	return v.tv_sec + v.tv_usec / 1.0e6;
}
#endif

struct Listener {
	Listener()
		: m_L(0)
		, m_Self(LUA_NOREF)
	{
	}
	lua_State *	m_L;
	int		m_Self;
};

struct MovingObject {
	MovingObject()
	{
		const int maxi = 100;
		const float maxf = maxi;

		PositionX = (MinX + ((rand() % maxi) / maxf) * (MaxX - MinX));
		PositionY = (MinY + ((rand() % maxi) / maxf) * (MaxY - MinY));

		DirectionX = ((rand() % maxi) / maxf) - 0.5f;
		DirectionY = ((rand() % maxi) / maxf) - 0.5f;
		const float k = sqrtf(DirectionX * DirectionX + DirectionY * DirectionY);
		DirectionX *= (MinVelocity + ((rand() % maxi) / maxf) * (MaxVelocity - MinVelocity)) / k;
		DirectionY *= (MinVelocity + ((rand() % maxi) / maxf) * (MaxVelocity - MinVelocity)) / k;
		// const float speed = MinVelocity + (rand() % (int)(MaxVelocity - MinVelocity));
		printf("x=%f y=%f\ndx=%f dy=%f\n", PositionX, PositionY, DirectionX, DirectionY);
	}

	void Refresh(float deltaTime)
	{
		PositionX += DirectionX * deltaTime;
		PositionY += DirectionY * deltaTime;

		if (PositionX < MinX && DirectionX < 0)
			DirectionX = -DirectionX;

		if (MaxX < PositionX && 0 < DirectionX)
			DirectionX = -DirectionX;

		if (PositionY < MinY && DirectionY < 0)
			DirectionY = -DirectionY;

		if (MaxY < PositionY && 0 < DirectionY)
			DirectionY = -DirectionY;
	}

	const float	MinX = 0.0f;
	const float	MaxX = 960.0f;
	const float	MinY = 0.0f;
	const float	MaxY = 640.0f;
	const float	MinVelocity = 100.0f;
	const float	MaxVelocity = 300.0f;

	float		DirectionX;
	float		DirectionY;

	float		PositionX;
	float		PositionY;
};

struct Timer {
	int		repeating;
	unsigned int	id;
	Listener	listener;
	MovingObject	movingObject;
};

static unsigned int g_SequenceId = 0;
static const int TIMERS_CAPACITY = 128;
static dmArray<Timer *> g_Timers;

/**
 * Create a listener instance from a function on the stack
 */
static Listener CreateListener(lua_State *L, int index)
{
	Listener listener;
	listener.m_L = dmScript::GetMainThread(L);
	dmScript::GetInstance(L);
	listener.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
	return listener;
}

/**
 * Create a new timer
 */
static Timer *CreateTimer(Listener listener, int repeating)
{
	Timer *timer = new Timer();

	timer->id = g_SequenceId++;
	timer->listener = listener;
	timer->repeating = repeating;

	if (g_Timers.Full())
		g_Timers.SetCapacity(g_Timers.Capacity() + TIMERS_CAPACITY);
	g_Timers.Push(timer);
	return timer;
}

/**
 * Get a timer
 */
static Timer *GetTimer(int id)
{
	for (int i = g_Timers.Size() - 1; i >= 0; i--) {
		Timer *timer = g_Timers[i];
		if (timer->id == id)
			return timer;
	}
	return 0;
}

static int Repeating(lua_State *L)
{
	int top = lua_gettop(L);

	const Listener listener = CreateListener(L, 1);

	Timer *timer = CreateTimer(listener, 1);

	lua_pushinteger(L, timer->id);

	assert(top + 1 == lua_gettop(L));
	return 1;
}

static void Remove(int id)
{
	for (int i = g_Timers.Size() - 1; i >= 0; i--) {
		Timer *timer = g_Timers[i];
		if (timer->id == id) {
			g_Timers.EraseSwap(i);
			delete timer;
			break;
		}
	}
}

static int Cancel(lua_State *L)
{
	int top = lua_gettop(L);

	int id = luaL_checkint(L, 1);

	Remove(id);

	assert(top + 0 == lua_gettop(L));
	return 0;
}

/**
 * Cancel all timers
 */
static int CancelAll(lua_State *L)
{
	int top = lua_gettop(L);

	for (int i = g_Timers.Size() - 1; i >= 0; i--) {
		Timer *timer = g_Timers[i];
		g_Timers.EraseSwap(i);
		free(timer);
	}

	assert(top + 0 == lua_gettop(L));
	return 0;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] = {
	{ "repeating",	Repeating },
	{ "cancel",	Cancel	  },
	{ "cancel_all", CancelAll },
	{ 0,		0	  }
};

static void LuaInit(lua_State *L)
{
	int top = lua_gettop(L);

	luaL_register(L, MODULE_NAME, Module_methods);

	lua_pop(L, 1);
	assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeTimerExtension(dmExtension::AppParams *params)
{
	return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeTimerExtension(dmExtension::Params *params)
{
	LuaInit(params->m_L);
	printf("Registered %s Extension\n", MODULE_NAME);
	return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeTimerExtension(dmExtension::AppParams *params)
{
	return dmExtension::RESULT_OK;
}

double g_PreviousTime = 0;

void PrintPosition(lua_State* L)
{
	const int top = lua_gettop(L);
	lua_getglobal(L, "go");
	lua_getfield(L, -1, "get_position");
	if (lua_pcall(L, 0, 1, 0) != 0) {
		printf("error running function `f2': %s\n", lua_tostring(L, -1));
	} else {
		if (!lua_isuserdata(L, -1)) {
			printf("function `f' must return a userdata\n");
		} else {
			double x, y, z;

			lua_getfield(L, -1, "x");
			if (lua_isnumber(L, -1)) {
				x = lua_tonumber(L, -1);
			} else {
				printf("not a number\n");
			}
			lua_pop(L, 1);

			lua_getfield(L, -1, "y");
			if (lua_isnumber(L, -1)) {
				y = lua_tonumber(L, -1);
			} else {
				printf("not a number\n");
			}
			lua_pop(L, 1);

			lua_getfield(L, -1, "z");
			if (lua_isnumber(L, -1)) {
				z = lua_tonumber(L, -1);
			} else {
				printf("not a number\n");
			}
			lua_pop(L, 1);

			printf("x=%f y=%f z=%f\n", x, y, z);
		}
	}
	lua_settop(L, top);
}

void SetPosition(lua_State* L, int vectorIndex)
{
	const int top = lua_gettop(L);
	lua_getglobal(L, "go");
	lua_getfield(L, -1, "set_position");
	lua_pushvalue(L, vectorIndex);
	if (lua_pcall(L, 1, 0, 0) != 0) {
		printf("error running function `f2': %s\n", lua_tostring(L, -1));
	}
	lua_settop(L, top);
}

void CreateVector3(lua_State* L, double x, double y, double z)
{
	lua_getglobal(L, "vmath");
	lua_getfield(L, -1, "vector3");

	lua_pushnumber(L, x);
	lua_pushnumber(L, y);
	lua_pushnumber(L, z);

	if (lua_pcall(L, 3, 1, 0) != 0)
		printf("error running function `f': %s\n", lua_tostring(L, -1));
}

dmExtension::Result UpdateTimerExtension(dmExtension::Params *params)
{
	const double currentTime = GetTimestamp();
	const double dt = currentTime - g_PreviousTime;

	g_PreviousTime = currentTime;

	for (int i = g_Timers.Size() - 1; i >= 0; i--) {
		Timer *timer = g_Timers[i];
		if (timer) {
			MovingObject& movingObject = timer->movingObject;
			movingObject.Refresh(dt);

			lua_State *L = timer->listener.m_L;
			int top = lua_gettop(L);

			{
				int top2 = lua_gettop(L);

				CreateVector3(L, movingObject.PositionX, movingObject.PositionY, 0.0);
				const int vectorIndex = lua_gettop(L);

				lua_rawgeti(L, LUA_REGISTRYINDEX, timer->listener.m_Self);
				lua_pushvalue(L, -1);
				dmScript::SetInstance(L);

				SetPosition(L, vectorIndex);

				lua_settop(L, top2);
			}
			assert(top == lua_gettop(L));
		}
	}


	return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeTimerExtension(dmExtension::Params *params)
{
	return dmExtension::RESULT_OK;
}


// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

DM_DECLARE_EXTENSION(Timer, LIB_NAME, AppInitializeTimerExtension, AppFinalizeTimerExtension, InitializeTimerExtension, UpdateTimerExtension, 0, FinalizeTimerExtension)
