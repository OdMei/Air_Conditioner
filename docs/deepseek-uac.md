# DeepSeek API 与 UAC 音频输出接口

## DeepSeek

`deepseek_api` 只负责构造受限长度的 Chat Completions JSON，并通过注入的 transport 回调发送。它不保存 API Key，也不直接调用网络栈；TLS、DNS、HTTPS 和密钥存储由应用的 NetX/安全传输适配器实现。`deepseek_api_submit()` 应在线程或消息队列任务中调用，不能放在音频 DMA 回调或 DSP 临界路径。

请求包含 `model`、system prompt、user text 和 `max_tokens`，字符串会处理引号、反斜杠、换行和控制字符。传输回调必须返回响应长度并遵守提供的响应缓冲区容量；失败会返回 `DEEPSEEK_API_TRANSPORT_ERROR`。

## UAC

`audio_uac_output` 提供 16 kHz、单声道、16-bit UAC1.0 streaming 描述符片段和容量为 2 的非阻塞处理帧队列。当前方向是“板端增强语音作为 USB Audio IN 发送给主机”，每次 `poll()` 通过 `send` 回调发送一个 32 字节等时包，10 个包组成一个 10 ms 音频帧。默认端点地址为 `0x81`，每 1 ms 包大小为 32 字节。

USBX 设备控制器、PCD 初始化、端点打开和等时传输调度尚未在当前工程配置；底层回调需要在 USB 帧节拍内发送该 32 字节包，不能阻塞等待主机。描述符函数不等于设备已枚举，实板验证需要 USB 端口、PHY/PCD 和主机枚举日志。
