  Darwin流媒体服务器代码分析
  2017年12月04日 20:49:03
  阅读数：69
  Darwin Streaming Server（即DSS）是Apple公司提供的开源实时流媒体播放服务器程序。整个程序使用C++编写，在设计上遵循高性能，简单，模块化等程序设计原则，务求做到程序高效，可扩充性好。并且DSS是一个开源的基于标准的流媒体服务器，可以运行在Windows NT和Windows 2000，以及几个UNIX实现上，包括Mac OS X，Linux，FreeBSD和Solaris操作系统上。
  
  
  
  一、DSS 代码分析【启动、初始化流程】
  
  二、DSS 代码分析【服务器架构】
  
  三、DSS 代码分析【EventThread与EventContext】
  
  四、DSS 代码分析【TaskThread与Task】
  
  五、DSS 代码分析【TimeoutTask】
  
  六、DSS 代码分析【点播请求】
  
  七、DSS 代码分析【RTSP消息交互过程】
  
  八、DSS 代码分析【学习资料分享】
  
  九、DSS 代码分析【Reliable UDP之数据重传】
  
  十、DSS 代码分析【Reliable UDP之超时时间计算】
  
  十一、DSS 代码分析【Reliable UDP之拥塞控制】
  
  十二、DSS 代码分析【BufferWindow实现】
  
  十三、DSS 代码分析【SR包发送】
  
  十四、H.264视频流的传输与载荷
  
  十五、DSS 代码分析【RTP over tcp实现】
  
  十七、DSS 代码分析【reflector反射之推流转发分析】
  
  十八、DSS 代码分析【RTSP announce推流报文分析】
  
  十九、EasyDarwin 中使用epoll网络模型替换原来的select模型