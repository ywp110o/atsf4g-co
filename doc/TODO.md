TODO 
======

待测试项和计划的功能
------

1. ~~[测试] atgateway: openssl+DH密钥交换流程~~
2. ~~[测试] atgateway: openssl+直接发送密钥流程~~
3. ~~[测试] atgateway: 无加密传输流程~~
4. ~~[测试] atgateway: mbedtls+DH密钥交换流程~~
5. ~~[测试] atgateway: mbedtls+直接发送密钥流程~~
6. ~~[测试] atgateway: openssl服务器+mbedtls客户端+DH密钥交换流程~~
7. ~~[测试] atgateway: mbedtls服务器+openssl客户端+DH密钥交换流程~~
8. ~~[测试] atgateway: 逻辑服务器主动踢下线~~
9. ~~[测试] atgateway: 断线重连+无加密~~
10. ~~[测试] atgateway: 断线重连+DH密钥交换~~
11. ~~[测试] atgateway: 主动踢出，禁止重连~~
12. ~~[测试] atgateway: 掉线和主动踢出的Remove通知~~
13. [测试] atgateway: 连接数过载保护
14. [测试] atgateway: 总流量过载保护
15. [测试] atgateway: 每小时流量过载保护
16. [测试] atgateway: 每分钟流量过载保护
17. ~~[测试] atgateway: 握手超时保护(DDOS)~~
18. [测试] valgrind --leak-check=full --tool=memcheck --show-leak-kinds=all --log-file=memcheck.log --malloc-fill=0x5E
19. [测试] cppcheck 无ERROR
20. ~~[功能] atgateway: 踢出Session（允许和不允许断线重连）~~
21. [功能] atgateway: 关闭状态，通知所有client后退出
22. ~~[测试] atgateway: DH密钥交换，定期更换密钥~~
23. ~~[测试] atgateway: 直接发送密钥，定期更换密钥~~
24. ~~[测试] atgateway: XXTEA加密算法~~
25. ~~[测试] atgateway: openssl+AES256加密算法~~
26. ~~[测试] atgateway: mbedtls+AES256加密算法~~
27. ~~[测试] clang-analyzer无warning无error~~