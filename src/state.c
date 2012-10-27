/*
 * =====================================================================================
 *
 *       Filename:  state.c
 *
 *    Description:  简单的状态机，暂时够用
 *
 *        Version:  1.0
 *        Created:  2012年10月22日 21时41分49秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  zhanglei (zhanglei), zhangleibruce@gmail.com
 *        Company:  zlb.me
 *
 * =====================================================================================
 */
#include <stdio.h>

#include "state.h"
#include "xevent.h"
#include "xlock.h"
#include "config.h"

enum state_e
{
    STATE_INIT = 0,
    STATE_ACTIVE,
    STATE_IDLE,
    STATE_XLOCK,
    STATE_MAX,
};

struct state_t
{
    /* 是否可以从from_st进入该状态
     * @param from_st 前一个状态
     * @return TRUE or FALSE
     */
    gboolean (*pre_enter)(enum state_e from_st);
    /* 进入该状态
     * @param from_st 前一个状态
     */
    void (*enter)(enum state_e from_st);
    /* 退出该状态
     */
    void (*leave)();

    /* 定时超时回调
     * @param time 超时时间
     */
    void (*timeout_cb)(guint time);
};


// ACTIVE STATE
static gboolean state_active_pre_enter(enum state_e from_st);
static void state_active_enter(enum state_e from_st);
static void state_active_leave();
static void state_active_timeout_cb(guint time);

// IDLE STATE
static gboolean state_idle_pre_enter(enum state_e from_st);
static void state_idle_enter(enum state_e from_st);
static void state_idle_leave();
static void state_idle_timeout_cb(guint time);

// XLOCK STATE
static gboolean state_xlock_pre_enter(enum state_e from_st);
static void state_xlock_enter(enum state_e from_st);
static void state_xlock_leave();
static void state_xlock_timeout_cb(guint time);

static struct state_t s_all_st[STATE_MAX] = 
{
    [STATE_INIT] = 
    {
        .pre_enter  = NULL,
        .enter      = NULL,
        .leave      = NULL,
        .timeout_cb = NULL
    },
    [STATE_ACTIVE] = 
    {
        .pre_enter  = state_active_pre_enter,
        .enter      = state_active_enter,
        .leave      = state_active_leave,
        .timeout_cb = state_active_timeout_cb 
    },
    [STATE_IDLE] = 
    {
        .pre_enter  = state_idle_pre_enter,
        .enter      = state_idle_enter,
        .leave      = state_idle_leave,
        .timeout_cb = state_idle_timeout_cb 
    },
    [STATE_XLOCK] = 
    {
        .pre_enter  = state_xlock_pre_enter,
        .enter      = state_xlock_enter,
        .leave      = state_xlock_leave,
        .timeout_cb = state_xlock_timeout_cb 
    },
};

static enum state_e s_curret_state = STATE_INIT;

static struct state_t* get_st(enum state_e st)
{
    return s_all_st + st;
}

static gboolean state_change(enum state_e st)
{
    struct state_t* curret_st = get_st(s_curret_state);
    struct state_t* change_st = get_st(st);

    // 判断是否可以切换到目的状态
    if(change_st->pre_enter && !change_st->pre_enter(s_curret_state))
    {
        g_error("can not change to st(%d -> %d)\n", s_curret_state, st);
        return FALSE;
    }

    if(curret_st->leave)
    {
        curret_st->leave();
    }

    if(change_st->enter)
    {
        change_st->enter(s_curret_state);
    }

    s_curret_state = st;

    return TRUE;
}

gboolean state_init()
{
    return state_change(STATE_ACTIVE);
}

void state_timeout_cb(guint time)
{
    struct state_t* curret_st = get_st(s_curret_state);

    g_debug("curret_st = %d\n", s_curret_state);

    if(curret_st->timeout_cb)
    {
        curret_st->timeout_cb(time);
    }
}

// ACTIVE STATE
static guint s_work_time_left = 0;
static gboolean s_user_pause = FALSE;

void state_active_pause()
{
    s_user_pause = TRUE;
}

void state_active_unpause()
{
    s_user_pause = FALSE;
}

static gboolean state_active_pre_enter(enum state_e from_st)
{
    return TRUE;
}

static void state_active_enter(enum state_e from_st)
{
    s_work_time_left = g_config.interval;
}


static void state_active_leave()
{
}

static void state_active_timeout_cb(guint time)
{
    // 记录是否有用户空闲
    static guint s_idle_time = 0;

    g_debug("work time left = %d\n", s_work_time_left);

    // 判断是否需要进入 idle 状态
    if(s_work_time_left <= 0)
    {
        state_change(STATE_XLOCK);
    }

    if(!s_user_pause)
    {
        s_work_time_left -= time;
    }

    // 判断是否满足进入 idle 状态
    if(!xevent_has_event())
    {
        s_idle_time += time;
    }
    else
    {
        s_idle_time = 0;
    }
    xevent_clear_event();

    if(s_idle_time >= g_config.max_idle_time)
    {
        state_change(STATE_IDLE);
    }

}


// IDLE STATE
static gboolean state_idle_pre_enter(enum state_e from_st)
{
    return TRUE;
}

static void state_idle_enter(enum state_e from_st)
{
}

static void state_idle_leave()
{
}

static void state_idle_timeout_cb(guint time)
{
    // 如果有鼠标键盘事件，则进入active状态
    if(xevent_has_event())
    {
        state_change(STATE_ACTIVE);
    }
    xevent_clear_event();
}


// XLOCK STATE
static guint s_rest_time = 0;
static gboolean state_xlock_pre_enter(enum state_e from_st)
{
    return TRUE;
}

static void state_xlock_enter(enum state_e from_st)
{
    s_rest_time = 0;
    xlock_lockscreen();
}

static void state_xlock_leave()
{
    xlock_unlockscreen();
}

static void state_xlock_timeout_cb(guint time)
{
    // TODO: 显示剩余时间
    s_rest_time += time;
    if(s_rest_time >= g_config.rest_time)
    {
        state_change(STATE_ACTIVE);
    }

}

