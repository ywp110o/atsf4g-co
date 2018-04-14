#include <log/log_wrapper.h>
#include <proto_base.h>
#include <time/time_utility.h>


#include "protocol/pbdesc/svr.const.err.pb.h"

#include <config/logic_config.h>
#include <dispatcher/task_manager.h>
#include <rpc/db/login.h>
#include <rpc/db/player.h>


#include <logic/action/task_action_auto_save_players.h>
#include <logic/action/task_action_player_cache_expired.h>

#include "player_manager.h"
#include "session_manager.h"

player_manager::player_ptr_t player_manager::make_default_player(uint64_t user_id, const std::string &openid) {
    player_ptr_t ret = create(user_id, openid);
    WLOGDEBUG("create player %s with default data", openid.c_str());

    if (!ret) {
        return ret;
    }

    ret->create_init(hello::EN_VERSION_DEFAULT);
    return ret;
}

int player_manager::init() {
    last_proc_time_ = 0;
    return 0;
}

bool player_manager::remove(player_manager::player_ptr_t u, bool force) {
    if (!u || u->is_removing()) {
        return false;
    }
    u->set_removing(true);

    // 如果不是强制移除则进入缓存队列即可
    time_t expire_time = logic_config::me()->get_cfg_logic().player_cache_expire_time;
    if (force || expire_time <= 0) {
        do {
            int res = 0;

            // 先检测登入信息，防止缓存过期
            // 这意味着这个函数必须在Task中使用
            hello::table_login user_lg;
            std::string version;
            res = save(u, &user_lg, &version);
            if (res < 0) {
                break;
            }

            user_lg.set_login_pd(0);
            user_lg.set_logout_time(util::time::time_utility::get_now());
            // RPC save to db
            res = rpc::db::login::set(u->get_open_id().c_str(), user_lg, version);
            if (res < 0) {
                WLOGERROR("player %s(%llu) try logout load db failed, res: %d version:%s.", u->get_open_id().c_str(), u->get_user_id_llu(), res,
                          version.c_str());
            }

        } while (false);

        // 先执行同步操作
        // 释放用户索引
        WLOGINFO("player %s(%llu) removed", u->get_open_id().c_str(), u->get_user_id_llu());
        force_erase(u->get_user_id());
    }

    // 释放本地数据, 下线相关Session
    session::ptr_t s = u->get_session();
    if (s) {
        u->set_session(NULL);
        s->set_player(NULL);
        session_manager::me()->remove(s, ::atframe::gateway::close_reason_t::EN_CRT_KICKOFF);
    }

    u->set_removing(false);
    return true;
}

int player_manager::save(player_ptr_t u, hello::table_login *login_tb, std::string *login_version) {
    if (!u) {
        WLOGERROR("user is null");
        return hello::err::EN_SYS_UNKNOWN;
    }

    // 没初始化的不保存数据  缓存数据不保存
    if (!u->is_inited()) {
        return hello::err::EN_SUCCESS;
    }

    // 尝试保存用户数据
    hello::table_login user_lg;
    if (NULL == login_tb) {
        login_tb = &user_lg;
    }

    std::string version;
    if (NULL == login_version) {
        login_version = &version;
    }

    uint64_t self_bus_id = logic_config::me()->get_self_bus_id();
    // RPC read from DB
    int res = rpc::db::login::get(u->get_open_id().c_str(), *login_tb, *login_version);
    if (res < 0) {
        WLOGERROR("player %s(%llu) try load login data failed.", u->get_open_id().c_str(), u->get_user_id_llu());
        return res;
    }

    // 异常的玩家数据记录，自动修复一下
    if (0 == login_tb->login_pd()) {
        WLOGERROR("player %s(%llu) login bus id error(expected: 0x%llx, real: 0x%llx)", u->get_open_id().c_str(), u->get_user_id_llu(),
                  static_cast<unsigned long long>(self_bus_id), static_cast<unsigned long long>(login_tb->login_pd()));

        login_tb->set_login_pd(self_bus_id);
        // RPC save to db
        res = rpc::db::login::set(u->get_open_id().c_str(), *login_tb, *login_version);
        if (res < 0) {
            WLOGERROR("user %s(%llu) try set login data failed.", u->get_open_id().c_str(), u->get_user_id_llu());
            return res;
        }
    }

    if (login_tb->login_pd() != self_bus_id) {
        WLOGERROR("user %s(%llu) login pd error(expected: 0x%llx, real: 0x%llx)", u->get_open_id().c_str(), u->get_user_id_llu(),
                  static_cast<unsigned long long>(self_bus_id), static_cast<unsigned long long>(login_tb->login_pd()));

        // 在其他设备登入的要把这里的Session踢下线
        if (u->get_session()) {
            u->get_session()->send_kickoff(::atframe::gateway::close_reason_t::EN_CRT_KICKOFF);
        }

        return hello::EN_ERR_LOGIN_OTHER_DEVICE;
    }


    // 尝试保存用户数据
    hello::table_user user_gu;
    u->dump(user_gu, true);

    WLOGDEBUG("player %s(%llu) save curr data version:%s", u->get_open_id().c_str(), u->get_user_id_llu(), u->get_version().c_str());

    // RPC save to DB
    res = rpc::db::player::set(u->get_user_id(), user_gu, u->get_version());

    // CAS 序号错误（可能是先超时再返回成功）,重试一次
    // 前面已经确认了当前用户在此处登入并且已经更新了版本号到版本信息
    // RPC save to DB again
    if (hello::err::EN_DB_OLD_VERSION == res) {
        res = rpc::db::player::set(u->get_user_id(), user_gu, u->get_version());
    }

    if (res < 0) {
        WLOGERROR("player %s(%llu) try save db failed. res:%d version:%s", u->get_open_id().c_str(), u->get_user_id_llu(), res, u->get_version().c_str());
    }

    return res;
}

void player_manager::force_erase(uint64_t user_id) {
    player_index_t::iterator iter = all_players_.find(user_id);
    if (iter != all_players_.end()) {
        iter->second->on_remove();
        all_players_.erase(iter);
    }
}

player_manager::player_cache_t *player_manager::set_offline_cache(player_ptr_t user, bool is_save) {
    if (!user) {
        WLOGERROR("user can not be null");
        return NULL;
    }

    time_t cache_expire_time = logic_config::me()->get_cfg_logic().player_cache_expire_time;
    cache_expire_list_.push_back(player_cache_t());
    player_cache_t &new_cache = cache_expire_list_.back();
    new_cache.failed_times = 0;
    new_cache.operation_sequence = ++user->schedule_data_.cache_sequence;
    new_cache.player_inst = user;
    new_cache.expire_time = util::time::time_utility::get_now() + cache_expire_time;
    new_cache.save = is_save;

    return &new_cache;
}

player_manager::player_ptr_t player_manager::load(uint64_t user_id, bool force) {
    player_ptr_t user = find(user_id);
    if (force || !user) {
        hello::table_user tbu;
        std::string version;

        // 这个函数必须在task中运行
        // 尝试从数据库读数据
        // RPC get from DB
        int res = rpc::db::player::get_all(user_id, tbu, version);
        if (res) {
            WLOGERROR("load game user data for %llu failed, error code:%d", static_cast<unsigned long long>(user_id), res);
            return NULL;
        }

        user = find(user_id);
        if (!user) {
            user = create(user_id, tbu.open_id());
        }

        if (user && !user->is_inited() && user->get_version() != version && !version.empty()) {
            user->set_version(version);

            user->init_from_table_data(tbu);

            // 只是load的数据不要保存，不然出现版本错误。
            set_offline_cache(user, false);
        }
    }

    return user;
}

size_t player_manager::size() const { return all_players_.size(); }

player_manager::player_ptr_t player_manager::create(uint64_t user_id, const std::string &openid) {
    if (0 == user_id || openid.empty()) {
        WLOGERROR("can not create player without user id or open id");
        return player_ptr_t();
    }

    if (find(user_id)) {
        WLOGERROR("player %llu already exists, can not create again", static_cast<unsigned long long>(user_id));
        return player_ptr_t();
    }

    // online user number limit
    if (all_players_.size() > logic_config::me()->get_cfg_logic().player_max_online_number) {
        WLOGERROR("online number extended");
        return player_ptr_t();
    }

    player_ptr_t ret = std::make_shared<player>();
    if (!ret) {
        WLOGERROR("malloc player %s(%llu) failed", openid.c_str(), static_cast<unsigned long long>(user_id));
        return ret;
    }
    ret->init(user_id, openid);
    all_players_[user_id] = ret;

    return ret;
}

const player_manager::player_ptr_t player_manager::find(uint64_t user_id) const {
    player_index_t::const_iterator iter = all_players_.find(user_id);
    if (all_players_.end() == iter) {
        return player_ptr_t();
    }

    return iter->second;
}

player_manager::player_ptr_t player_manager::find(uint64_t user_id) {
    player_index_t::iterator iter = all_players_.find(user_id);
    if (all_players_.end() == iter) {
        return player_ptr_t();
    }

    return iter->second;
}

const player_manager::player_ptr_t player_manager::find(const session::key_t &sess_key) const {
    session::ptr_t sess = session_manager::me()->find(sess_key);
    if (!sess) {
        return player_ptr_t();
    }

    return sess->get_player();
}

player_manager::player_ptr_t player_manager::find(const session::key_t &sess_key) {
    session::ptr_t sess = session_manager::me()->find(sess_key);
    if (!sess) {
        return player_ptr_t();
    }

    return sess->get_player();
}

int player_manager::proc() {
    int ret = 0;

    // 每秒只需要判定一次
    if (last_proc_time_ == ::util::time::time_utility::get_now()) {
        return ret;
    }
    last_proc_time_ = ::util::time::time_utility::get_now();

    // 自动保存
    do {
        if (auto_save_list_.empty()) {
            break;
        }

        player_ptr_t user = auto_save_list_.front().lock();

        // 如果已下线并且用户缓存失效则跳过
        if (!user) {
            auto_save_list_.pop_front();
            continue;
        }

        // 没有初始化完成的直接移除
        if (!user->is_inited()) {
            force_erase(user->get_user_id());
            auto_save_list_.pop_front();
            continue;
        }

        // 如果没有开启自动保存则跳过
        if (0 == user->schedule_data_.save_pending_time) {
            auto_save_list_.pop_front();
            continue;
        }

        // 如果最近自动保存用户的保存时间大于当前时间，则没有用户需要保存数据
        if (util::time::time_utility::get_now() <= user->schedule_data_.save_pending_time) {
            break;
        }

        // 启动用户数据保存Task
        // 使用默认的类似CS消息的短期的超时时间,以便减少数据缓存时间
        task_manager::id_t task_id = 0;
        int res = task_manager::me()->create_task<task_action_auto_save_players>(task_id, task_action_auto_save_players::ctor_param_t());
        if (hello::err::EN_SUCCESS != res || 0 == task_id) {
            WLOGERROR("create task to auto save failed.");
            break;
        }

        dispatcher_start_data_t start_data;
        start_data.private_data = NULL;
        start_data.message.msg_addr = NULL;
        start_data.message.msg_type = 0;

        task_manager::me()->start_task(task_id, start_data);

        ++ret;
        break;
    } while (true);


    // 缓存失效定时器
    do {
        if (cache_expire_list_.empty()) {
            break;
        }

        player_cache_t &cache = cache_expire_list_.front();

        // 如果没到时间，后面的全没到时间
        if (util::time::time_utility::get_now() <= cache.expire_time) {
            break;
        }

        // 不需要保存则跳过
        if (false == cache.save) {
            cache_expire_list_.pop_front();
            continue;
        }

        // 如果已下线并且用户缓存失效则跳过
        player_ptr_t user = cache.player_inst.lock();
        if (!user) {
            cache_expire_list_.pop_front();
            continue;
        }

        // 如果操作序列失效则跳过
        if (false == user->check_logout_cache(cache.operation_sequence)) {
            cache_expire_list_.pop_front();
            continue;
        }

        // 启动用户缓存失效登出Task
        // 使用默认的类似CS消息的短期的超时时间,以便减少数据缓存时间
        task_manager::id_t task_id = 0;
        int res = task_manager::me()->create_task<task_action_player_cache_expired>(task_id, task_action_player_cache_expired::ctor_param_t());
        if (hello::err::EN_SUCCESS != res || 0 == task_id) {
            WLOGERROR("create task to expire cache failed.");
            break;
        }

        dispatcher_start_data_t start_data;
        start_data.private_data = NULL;
        start_data.message.msg_addr = NULL;
        start_data.message.msg_type = 0;
        task_manager::me()->start_task(task_id, start_data);

        ++ret;
        break;
    } while (true);

    return ret;
}

void player_manager::update_auto_save(player_ptr_t &user) {
    if (!user) {
        WLOGERROR("this function can not be call with user=null");
        return;
    }

    // 没有设置定时保存限制则跳过
    time_t auto_save_interval = logic_config::me()->get_cfg_logic().player_auto_save_interval;
    if (auto_save_interval <= 0) {
        return;
    }

    time_t now_tm = util::time::time_utility::get_now();
    time_t update_tm = auto_save_interval + now_tm;

    // 没有设置定时保存则跳过
    if (update_tm <= now_tm) {
        return;
    }

    // 如果有未完成的定时保存任务则不需要再设一个
    if (0 != user->schedule_data_.save_pending_time && now_tm <= user->schedule_data_.save_pending_time) {
        return;
    }

    user->schedule_data_.save_pending_time = update_tm;
    auto_save_list_.push_back(std::weak_ptr<player>(user));
}