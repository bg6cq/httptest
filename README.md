## httptest 测试http/https get请求

httptest [ -d ] [ -p ] [ -4 ] [ -6 ] [ -w wait_time ] [ -r check_string ] URL

如果获取到200应答，并且返回的内容中有check_string，退出值为0

显示 DNS解析时间 TCP连接时间 第一次应答时间 传输时间 传输速度

前4个参数单位是秒，最后1个参数单位是 字节/秒

运行测试：

```
# ./httptest -w 2 https://www.sjtu.edu.cn/
0.0010 0.0093 0.0331 0.0374 1981961
# ./httptest -w 2 https://www.pku.edu.cn/
0.0283 0.0257 0.0814 0.0273 2335198
# ./httptest -w 2 https://www.ustc.edu.cn/
0.0009 0.0003 0.0065 0.0007 36624668
```
