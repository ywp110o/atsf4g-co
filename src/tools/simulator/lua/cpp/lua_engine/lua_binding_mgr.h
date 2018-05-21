﻿/**
 * @note 多线程多lua虚拟机情况下，应该在每个线程单独对虚拟机执行lua_binding_mgr::instance()->proc(L);以防止多线程冲突
 */

#ifndef _SCRIPT_LUA_LUABINDINGMGR_
#define _SCRIPT_LUA_LUABINDINGMGR_

#pragma once

#include <assert.h>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>

#include <design_pattern/singleton.h>
#include <std/explicit_declare.h>

#include "lua_binding_utils.h"

namespace script {
    namespace lua {
        class lua_binding_class_mgr_base {
        protected:
            lua_binding_class_mgr_base();

        public:
            virtual ~lua_binding_class_mgr_base() = 0;

            virtual int proc(lua_State *L) = 0;

            virtual void add_lua_state(lua_State *L) = 0;
        };


        template <typename TC>
        class lua_binding_class_mgr_inst : public lua_binding_class_mgr_base, public util::design_pattern::singleton<lua_binding_class_mgr_inst<TC> > {
        public:
            virtual int proc(lua_State *L) {
                if (NULL == L) {
                    cache_maps_.clear();
                    return 0;
                }

                intptr_t index = reinterpret_cast<intptr_t>(L);
                auto iter = cache_maps_.find(index);
                if (cache_maps_.end() == iter) {
                    return -1;
                }

                iter->second.clear();
                return 0;
            }

            bool add_ref(lua_State *L, const std::shared_ptr<TC> &ptr) {
                if (NULL == L) {
                    return false;
                }

                intptr_t index = reinterpret_cast<intptr_t>(L);
                auto iter = cache_maps_.find(index);
                if (cache_maps_.end() == iter) {
                    return false;
                }

                iter->second.push_back(ptr);
                return true;
            }

            virtual void add_lua_state(lua_State *L) {
                intptr_t index = reinterpret_cast<intptr_t>(L);
                cache_maps_[index];
            }

        private:
            std::map<intptr_t, std::list<std::shared_ptr<TC> > > cache_maps_;
        };

        class lua_binding_mgr : public util::design_pattern::singleton<lua_binding_mgr> {
        public:
            typedef std::function<void()> func_type;

        protected:
            lua_binding_mgr();
            ~lua_binding_mgr();

        public:
            int init(lua_State *L);

            /**
             * 自动更新/清理入口
             * @param 指定要清理的lua虚拟机，不指定为清理全部
             */
            int proc(lua_State *L = NULL);

            void add_bind(func_type fn);

        public:
            template <typename TC>
            bool add_ref(lua_State *L, const std::shared_ptr<TC> &ptr) {
                return lua_binding_class_mgr_inst<TC>::instance()->add_ref(L, ptr);
            }

            inline bool isInited() const { return inited_; }

        private:
            bool inited_;
            std::list<func_type> auto_bind_list_;
            std::list<lua_binding_class_mgr_base *> lua_states_;
            friend class lua_binding_class_mgr_base;
        };


        class lua_binding_wrapper {
        public:
            lua_binding_wrapper(lua_binding_mgr::func_type);
            ~lua_binding_wrapper();
        };
    } // namespace lua
} // namespace script

#define LUA_BIND_VAR_NAME(name) script_lua_LuaBindMgr_Var_##name
#define LUA_BIND_FN_NAME(name) script_lua_LuaBindMgr_Fn_##name

#define LUA_BIND_OBJECT(name)                                                                \
    static void LUA_BIND_FN_NAME(name)();                                                    \
    static script::lua::lua_binding_wrapper LUA_BIND_VAR_NAME(name)(LUA_BIND_FN_NAME(name)); \
    void LUA_BIND_FN_NAME(name)()

#endif
