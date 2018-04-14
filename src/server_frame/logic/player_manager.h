#ifndef LOGIC_PLAYER_MANAGER_H
#define LOGIC_PLAYER_MANAGER_H

#pragma once

#include <list>

#include <design_pattern/singleton.h>

#include <protocol/pbdesc/svr.table.pb.h>

#include <utility/environment_helper.h>

#include <data/player.h>
#include <data/session.h>

class player_manager : public util::design_pattern::singleton<player_manager> {
public:
    typedef player::ptr_t player_ptr_t;
    typedef UTIL_ENV_AUTO_MAP(uint64_t, player_ptr_t) player_index_t;

    struct player_cache_t {
        uint32_t operation_sequence;
        time_t expire_time;
        std::weak_ptr<player> player_inst;
        uint32_t failed_times;
        bool save;
    };

public:
    player_ptr_t make_default_player(uint64_t user_id, const std::string &openid);

    int init();

    /**
     * @brief 移除用户
     * @param user user指针
     * @param force 强制移除，不进入离线缓存
     */
    bool remove(player_ptr_t user, bool force = false);


    /**
     * @brief 保存用户数据
     * @param user user指针
     * @param login_tb 如果不为空，传出login表数据
     * @param login_version 如果不为空，传出login表版本号
     */
    int save(player_ptr_t u, hello::table_login *login_tb, std::string *login_version);

    /**
     * @brief 强制释放用户数据
     * @note 慎用，要手动处理关联的数据释放
     * @param user user指针
     */
    void force_erase(uint64_t user_id);

    /**
     * @brief 设置用户离线缓存
     * @param user user指针
     * @param is_save 是否需要保存到数据库中
     * @note 如果离线缓存列表里已有改单位，则会延长缓存时间
     */
    player_cache_t *set_offline_cache(player_ptr_t user, bool is_save = true);

    /**
     * @brief 加载指定玩家数据。
     * @note 注意这个函数只是读数据库做缓存。
     * @note gamesvr 请不要强制拉去数据 会冲掉玩家数据
     * @note 返回的 user 指针不能用于改写玩家数据，不做保存。
     * @param user_id
     * @return null 或者 user指针
     */
    player_ptr_t load(uint64_t user_id, bool force = false);

    size_t size() const;

    player_ptr_t create(uint64_t user_id, const std::string &openid);

    const player_ptr_t find(uint64_t user_id) const;
    player_ptr_t find(uint64_t user_id);

    const player_ptr_t find(const session::key_t &sess_key) const;
    player_ptr_t find(const session::key_t &sess_key);

    int proc();

    void update_auto_save(player_ptr_t &user);

    std::list<std::weak_ptr<player> > &get_auto_save_list() { return auto_save_list_; }

    const std::list<std::weak_ptr<player> > &get_auto_save_list() const { return auto_save_list_; }

    std::list<player_cache_t> &get_cache_expire_list() { return cache_expire_list_; }

    const std::list<player_cache_t> &get_cache_expire_list() const { return cache_expire_list_; }

private:
    player_index_t all_players_;
    std::list<std::weak_ptr<player> > auto_save_list_;
    std::list<player_cache_t> cache_expire_list_;
    time_t last_proc_time_;
};

#endif