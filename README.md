## httptest 测试http/https get请求

httptest [ -d ] [ -p ] [ -4 ] [ -6 ] [ -w wait_time ] [ -r check_string ] URL

注意，最多下载4MB字节。

程序退出值：
```
0 如果获取到200应答，并且返回的内容中有check_string
1 DNS查询错误
2 TCP连接建立错误
3 读应答错误
4 非200应答
5 提供了check_string, 但是应答中无check_string
8 命令行错误
9 https协商错误
10 url错误
```

stdout输出
```
退出值 DNS解析时间 TCP连接时间 第一次应答时间 传输时间 传输字节 传输速度
```

第1个参数是整数，第2-5个参数单位是秒，第6个参数单位是字节，最后1个参数单位是 字节/秒

运行测试：

```
# ./httptest -w 2 https://www.sjtu.edu.cn/
0 0.0109 0.0093 0.0322 0.0375 74006 1972652 https://www.sjtu.edu.cn/
# ./httptest -w 2 https://www.pku.edu.cn/
0 0.0007 0.0257 0.0811 0.0780 63863 819229 https://www.pku.edu.cn/
# ./httptest -w 2 https://www.ustc.edu.cn/
0 0.0008 0.0003 0.0066 0.0008 27392 35163028 https://www.ustc.edu.cn/
```
