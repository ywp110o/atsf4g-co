do
    local info = '============================= Load Lua Start ============================='
    if _G.jit then
        info = info .. '\n========== Lua JIT Version:' .. tostring(_G.jit.version) .. ' ==========\n\n\n'
    end

    if _G.lua_log then
        lua_log(0, 1, info)
    else
        print(info)
    end
end

math.randomseed(os.time())

local loader = require('utils.loader')
require('utils.event')

-- 那啥cocos2d会关闭标准输入输出函数, 会导致vardump无输出
do
    -- _G.vardump_default.ostream = log_stream(0, logc_fatal)
end

-- 加载bootstrap
loader.load_list('bootstrap')


--require "script/testunit/mainMenu"
log_info('============================= Load Lua End =============================')
