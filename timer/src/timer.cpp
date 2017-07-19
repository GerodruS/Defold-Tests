// copyright britzl
// Extension lib defines
#define LIB_NAME "Timer"
#define MODULE_NAME "timer"

// include the Defold SDK
#include <dmsdk/sdk.h>

struct Listener {
	Listener() {
		m_L = 0;
		m_Callback = LUA_NOREF;
		m_Self = LUA_NOREF;
	}
	lua_State* m_L;
	int        m_Callback;
	int        m_Self;
};

struct Timer {
	int repeating;
	unsigned int id;
	Listener listener;
};

static unsigned int g_SequenceId = 0;
static const int TIMERS_CAPACITY = 128;
static dmArray<Timer*> g_Timers;

/**
 * Create a listener instance from a function on the stack
 */
static Listener CreateListener(lua_State* L, int index) {
	luaL_checktype(L, index, LUA_TFUNCTION);
	lua_pushvalue(L, index);
	int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

	Listener listener;
	listener.m_L = dmScript::GetMainThread(L);
	listener.m_Callback = cb;
	dmScript::GetInstance(L);
	listener.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);
	return listener;
}

/**
 * Create a new timer
 */
static Timer* CreateTimer(Listener listener, int repeating) {
	Timer *timer = (Timer*)malloc(sizeof(Timer));
	timer->id = g_SequenceId++;
	timer->listener = listener;
	timer->repeating = repeating;

	if (g_Timers.Full()) {
		g_Timers.SetCapacity(g_Timers.Capacity() + TIMERS_CAPACITY);
	}
	g_Timers.Push(timer);
	return timer;
}

/**
 * Get a timer
 */
static Timer* GetTimer(int id) {
	for (int i = g_Timers.Size() - 1; i >= 0; i--) {
		Timer* timer = g_Timers[i];
		if (timer->id == id) {
			return timer;
		}
	}
	return 0;
}

static int Repeating(lua_State* L) {
	int top = lua_gettop(L);

	const Listener listener = CreateListener(L, 1);

	Timer *timer = CreateTimer(listener, 1);

	lua_pushinteger(L, timer->id);

	assert(top + 1 == lua_gettop(L));
	return 1;
}

static void Remove(int id) {
	for (int i = g_Timers.Size() - 1; i >= 0; i--) {
		Timer* timer = g_Timers[i];
		if (timer->id == id) {
			g_Timers.EraseSwap(i);
			free(timer);
			break;
		}
	}
}

static int Cancel(lua_State* L) {
	int top = lua_gettop(L);

	int id = luaL_checkint(L, 1);

	Remove(id);

	assert(top + 0 == lua_gettop(L));
	return 0;
}

/**
 * Cancel all timers
 */
static int CancelAll(lua_State* L) {
	int top = lua_gettop(L);

	for (int i = g_Timers.Size() - 1; i >= 0; i--) {
		Timer* timer = g_Timers[i];
		g_Timers.EraseSwap(i);
		free(timer);
	}

	assert(top + 0 == lua_gettop(L));
	return 0;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] = {
	{ "repeating", Repeating },
	{ "cancel", Cancel },
	{ "cancel_all", CancelAll },
	{ 0, 0 }
};

static void LuaInit(lua_State* L) {
	int top = lua_gettop(L);

	luaL_register(L, MODULE_NAME, Module_methods);

	lua_pop(L, 1);
	assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeTimerExtension(dmExtension::AppParams* params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeTimerExtension(dmExtension::Params* params) {
	LuaInit(params->m_L);
	printf("Registered %s Extension\n", MODULE_NAME);
	return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeTimerExtension(dmExtension::AppParams* params) {
	return dmExtension::RESULT_OK;
}

dmExtension::Result UpdateTimerExtension(dmExtension::Params* params)
{
	for (int i = g_Timers.Size() - 1; i >= 0; i--) {
		Timer* timer = g_Timers[i];
		if (timer) {
			lua_State* L = timer->listener.m_L;
			int top = lua_gettop(L);

			lua_rawgeti(L, LUA_REGISTRYINDEX, timer->listener.m_Callback);
			lua_rawgeti(L, LUA_REGISTRYINDEX, timer->listener.m_Self);
			lua_pushvalue(L, -1);
			dmScript::SetInstance(L);
			if (!dmScript::IsInstanceValid(L)) {
				lua_pop(L, 2);
			} else {
				lua_pushinteger(L, timer->id);
				int ret = lua_pcall(L, 2, LUA_MULTRET, 0);
				if (ret != 0) {
					dmLogError("Error running timer callback: %s", lua_tostring(L, -1));
					lua_pop(L, 1);
				}
			}
			assert(top == lua_gettop(L));
		}
	}

	return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeTimerExtension(dmExtension::Params* params) {
	return dmExtension::RESULT_OK;
}


// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

DM_DECLARE_EXTENSION(Timer, LIB_NAME, AppInitializeTimerExtension, AppFinalizeTimerExtension, InitializeTimerExtension, UpdateTimerExtension, 0, FinalizeTimerExtension)
